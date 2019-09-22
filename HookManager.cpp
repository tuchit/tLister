/*
    tLister add tabs support to Lister.
    Copyright (C) 2011 Egor Vlaznev
    Copyright (C) 2019 Aleksei Ilin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "HookManager.h"
#include "TabWindow.h"

#include <algorithm>
#include <assert.h>

static HookManager* g_hookManager = NULL;

HookManager::HookManager()
{
    m_hKb = SetWindowsHookEx(WH_GETMESSAGE, &HookManager::KeySpyProc, hInst, GetCurrentThreadId());
    m_hSpy = SetWindowsHookEx(WH_CALLWNDPROCRET, &HookManager::SpyProc, hInst, GetCurrentThreadId());
}

HookManager& HookManager::instance()
{
    if (!g_hookManager) {
        g_hookManager = new HookManager();
    }
    return *g_hookManager;
}

HookManager::~HookManager()
{
    DeleteObject(m_hTabFont);
    UnhookWindowsHookEx(m_hKb);
    UnhookWindowsHookEx(m_hSpy);
}

TabWindow* HookManager::CreateNewWindow(HWND hWndParent)
{
    if (!m_windows.size()) {
        m_config.parse();
    }

    CreateTabFont();

    TabWindow* wnd = new TabWindow(hWndParent, m_hWndFree, m_hTabFont);
    m_hWndFree = NULL;
    m_windows.push_back(wnd);
    return wnd;
}

void HookManager::CreateTabFont()
{
    if (!m_hTabFont) {
        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(ncm);
        if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
            m_hTabFont = CreateFontIndirect(&ncm.lfMenuFont);
        }
    }
}

void HookManager::AddNewChild(HWND hWndChild)
{
    TabWindow* nextParent = nullptr;
    if (m_wndLastUsed && m_wndLastUsed->ChildCount() < static_cast<size_t>(m_config.MaxWinCount)) {
        nextParent = m_wndLastUsed;
    }
    else {
        for (auto it = m_windows.crbegin(); it != m_windows.crend(); ++it) {
            if ((*it)->ChildCount() < static_cast<size_t>(m_config.MaxWinCount)) {
                nextParent = *it;
            }
        }
    }

    if (nextParent) {
        nextParent->AddTab(hWndChild);
    }
    else {
        CreateNewWindow(hWndChild);
    }
}

TabWindow* HookManager::FindParentTabWindow(HWND hWndChild) const
{
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
        if ((*it)->isChild(hWndChild)) {
            return *it;
        }
    }
    return nullptr;
}

TabWindow* HookManager::FindTabWindow(HWND hWnd) const
{
    auto it = std::find_if(m_windows.cbegin(), m_windows.cend(), [=](TabWindow* wnd) {
        return wnd->hWnd() == hWnd;
    });
    return it == m_windows.cend() ? nullptr : *it;
}

LRESULT HookManager::SpyProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    const auto& windows = HookManager::instance().windows();
    for (auto it = windows.cbegin(); it != windows.cend(); ++it) {
        (*it)->SpyProc(nCode, wParam, lParam);
    }
    return CallNextHookEx(HookManager::instance().m_hSpy, nCode, wParam, lParam);
}

LRESULT HookManager::KeySpyProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    HWND hWndForeground = GetForegroundWindow();
    TabWindow* wnd = HookManager::instance().FindTabWindow(hWndForeground);
    if (wnd) {
        if (LRESULT res = wnd->KeySpyProc(nCode, wParam, lParam)) {
            return res;
        }
    }
    return CallNextHookEx(HookManager::instance().m_hKb, nCode, wParam, lParam);
}

void HookManager::AddTab(HWND window)
{
    if ((!(WS_CHILD & GetWindowLongPtr((HWND)window, GWL_STYLE)))
        && (m_config.AlwaysShowTab == 0)
        && ((!(m_hWndFree && IsWindow(m_hWndFree))) || m_hWndFree == window)
        && (!m_windows.size())) {

        m_hWndFree = window;
        return;
    }

    if (WS_CHILD & GetWindowLongPtr((HWND)window, GWL_STYLE)) {
        if (!FindParentTabWindow(window))
            return;

        LONG_PTR style = GetWindowLongPtr((HWND)window, GWL_STYLE);
        SetWindowLongPtr((HWND)window, GWL_STYLE, (style|WS_POPUP)&(~WS_CHILD));
        SetWindowText((HWND)window, _T("Lister"));
    }

    AddNewChild(window);
}

void HookManager::DestroyWindow(TabWindow* tabWindow)
{
    auto it = std::find(m_windows.cbegin(), m_windows.cend(), tabWindow);
    assert(it != m_windows.cend());
    if (it != m_windows.cend()) {
        delete *it;
        it = m_windows.erase(it);
    }

    if (m_windows.size() > 0) {
        if (m_wndLastUsed == tabWindow) {
            m_wndLastUsed = m_windows.back();
        }
    }
    else {
        g_hookManager = nullptr;
        delete this;
    }
}

void HookManager::SetLastUsedWindow(TabWindow * tabWindow)
{
    m_wndLastUsed = tabWindow;
}

HookManager::WindowCont& HookManager::windows()
{
    return m_windows;
}

const TabConfig& HookManager::config() const
{
    return m_config;
}

HWND HookManager::GetFreeWindow() const
{
    return m_hWndFree;
}

void HookManager::SetFreeWindow(HWND hWnd)
{
    assert(!m_hWndFree);
    m_hWndFree = hWnd;
}
