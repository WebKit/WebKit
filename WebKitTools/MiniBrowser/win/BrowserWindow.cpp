/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "StdAfx.h"
#include "BrowserWindow.h"
#include "MiniBrowser.h"
#include "Resource.h"

#include <assert.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <vector>
#include <wininet.h>

using namespace std;

BrowserWindow::BrowserWindow()
    : m_window(0)
    , m_rebarWindow(0)
    , m_comboBoxWindow(0)
{
}

LRESULT CALLBACK BrowserWindow::BrowserWindowWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = ::GetWindowLongPtr(window, 0);
    
    if (BrowserWindow* browserView = reinterpret_cast<BrowserWindow*>(longPtr))
        return browserView->wndProc(window, message, wParam, lParam);

    if (message == WM_CREATE) {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        BrowserWindow* browserWindow = static_cast<BrowserWindow*>(createStruct->lpCreateParams);
        browserWindow->m_window = window;

        ::SetWindowLongPtr(window, 0, (LONG_PTR)browserWindow);

        browserWindow->onCreate(createStruct);
        return 0;
    }

    return ::DefWindowProc(window, message, wParam, lParam);
}

LRESULT BrowserWindow::wndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    bool handled = true;

    switch (message) {
    case WM_ERASEBKGND:
        lResult = 1;
        break;

    case WM_COMMAND:
        lResult = onCommand(LOWORD(wParam), handled);
        break;

    case WM_SIZE:
        onSize(LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_DESTROY:
        onDestroy();
        break;

    case WM_NCDESTROY:
        onNCDestroy();
        break;

    default:
        handled = false;
    }

    if (!handled)
        lResult = ::DefWindowProc(window, message, wParam, lParam);

    return lResult;
}

void BrowserWindow::createWindow(int x, int y, int width, int height)
{
    assert(!m_window);

    // Register the class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = 0;
    windowClass.lpfnWndProc = BrowserWindowWndProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = sizeof(this);
    windowClass.hInstance = MiniBrowser::shared().instance();
    windowClass.hIcon = 0;
    windowClass.hCursor = ::LoadCursor(0, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)::GetStockObject(WHITE_BRUSH);
    windowClass.lpszMenuName = MAKEINTRESOURCE(IDR_MAINFRAME);
    windowClass.lpszClassName = L"MiniBrowser";
    windowClass.hIconSm = 0;

    ::RegisterClassEx(&windowClass);

    ::CreateWindowW(L"MiniBrowser", L"MiniBrowser", WS_OVERLAPPEDWINDOW, x, y, width, height, 0, 0, MiniBrowser::shared().instance(), this);
}

void BrowserWindow::showWindow()
{
    assert(m_window);

    ::ShowWindow(m_window, SW_SHOWNORMAL);
}

void BrowserWindow::goToURL(const std::wstring& url)
{
    m_browserView.goToURL(url);
}

void BrowserWindow::onCreate(LPCREATESTRUCT createStruct)
{
    // Register our window.
    MiniBrowser::shared().registerWindow(this);

    // Create the rebar control.
    m_rebarWindow = ::CreateWindowEx(0, REBARCLASSNAME, 0, WS_VISIBLE | WS_BORDER | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_NODIVIDER | CCS_NOPARENTALIGN | RBS_VARHEIGHT | RBS_BANDBORDERS,
                                     0, 0, 0, 0, m_window, 0, createStruct->hInstance, 0);
    REBARINFO rebarInfo = { 0 };
    rebarInfo.cbSize = sizeof(rebarInfo);
    rebarInfo.fMask = 0;
    ::SendMessage(m_rebarWindow, RB_SETBARINFO, 0, (LPARAM)&rebarInfo);

    // Create the combo box control.
    m_comboBoxWindow = ::CreateWindowEx(0, L"combobox", 0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_VSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CBS_AUTOHSCROLL | CBS_DROPDOWN,
                                        0, 0, 0, 0, m_rebarWindow, 0, createStruct->hInstance, 0);
    SendMessage(m_comboBoxWindow, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));

    REBARBANDINFO bandInfo;
    bandInfo.cbSize = sizeof(bandInfo);
    bandInfo.fMask = RBBIM_STYLE | RBBIM_TEXT | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
    bandInfo.fStyle = RBBS_CHILDEDGE | RBBS_GRIPPERALWAYS;
    bandInfo.lpText = L"Address";
    bandInfo.hwndChild = m_comboBoxWindow;

    RECT comboBoxRect;
    ::GetWindowRect(m_comboBoxWindow, &comboBoxRect);
    bandInfo.cx = 100;
    bandInfo.cxMinChild = comboBoxRect.right - comboBoxRect.left;
    bandInfo.cyMinChild = comboBoxRect.bottom - comboBoxRect.top;

    // Add the band to the rebar.
    int result = ::SendMessage(m_rebarWindow, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&bandInfo);
    
    // Create the browser view.
    RECT webViewRect = { 0, 0, 0, 0};
    m_browserView.create(webViewRect, this);
}

void BrowserWindow::onDestroy()
{
    MiniBrowser::shared().unregisterWindow(this);

    // FIXME: Should we close the browser view here?
}

void BrowserWindow::onNCDestroy()
{
    delete this;
}

void BrowserWindow::onSize(int width, int height)
{
    RECT rebarRect;
    ::GetClientRect(m_rebarWindow, &rebarRect);

    // Resize the rebar.
    ::MoveWindow(m_rebarWindow, 0, 0, width, rebarRect.bottom - rebarRect.top, true);

    RECT webViewRect;
    webViewRect.top = rebarRect.bottom;
    webViewRect.left = 0;
    webViewRect.right = width;
    webViewRect.bottom = height;
    m_browserView.setFrame(webViewRect);
}

LRESULT BrowserWindow::onCommand(int commandID, bool& handled)
{
    switch (commandID) {
    case ID_FILE_NEW_WINDOW:
        MiniBrowser::shared().createNewWindow();
        break;
    case ID_FILE_CLOSE:
        ::PostMessage(m_window, WM_CLOSE, 0, 0);
        break;
    case ID_DEBUG_SHOW_WEB_VIEW: {
        HMENU menu = ::GetMenu(m_window);
        bool shouldHide = ::GetMenuState(menu, ID_DEBUG_SHOW_WEB_VIEW, MF_BYCOMMAND) & MF_CHECKED;

        ::CheckMenuItem(menu, ID_DEBUG_SHOW_WEB_VIEW, MF_BYCOMMAND | (shouldHide ? MF_UNCHECKED : MF_CHECKED));

        // Show or hide the web view.
        HWND webViewWindow = WKViewGetWindow(m_browserView.webView());
        ::ShowWindow(webViewWindow, shouldHide ? SW_HIDE : SW_SHOW);
        break;
    }
    default:
        handled = false;
    }

    return 0;
}

bool BrowserWindow::handleMessage(const MSG* message)
{
    if (message->hwnd != m_comboBoxWindow && !::IsChild(m_comboBoxWindow, message->hwnd))
        return false;

    // Look for a WM_KEYDOWN message.
    if (message->message != WM_KEYDOWN)
        return false;

    // Look for the VK_RETURN key.
    if (message->wParam != VK_RETURN)
        return false;

    std::vector<WCHAR> buffer;
    int textLength = ::GetWindowTextLength(m_comboBoxWindow);

    buffer.resize(textLength + 1);
    ::GetWindowText(m_comboBoxWindow, &buffer[0], buffer.size());

    std::wstring url(&buffer[0], buffer.size() - 1);

    if (url.find(L"http://"))
        url = L"http://" + url;

    m_browserView.goToURL(url);

    // We handled this message.
    return true;
}
