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

#include "config.h"
#include "FullScreenWindow.h"

#if ENABLE(FULLSCREEN_API)

#include "IntRect.h"
#include "WebCoreInstanceHandle.h"
#include <windows.h>

namespace WebCore {

FullScreenWindow::FullScreenWindow(FullScreenClient* client)
    : m_client(client)
    , m_hwnd(0)
{
}

FullScreenWindow::~FullScreenWindow()
{
    if (!m_hwnd)
        return;

    ::DestroyWindow(m_hwnd);
    ASSERT(!m_hwnd);
}

void FullScreenWindow::createWindow(HWND parentHwnd)
{
    static ATOM windowAtom;
    static LPCWSTR windowClassName = L"FullscreenWindowClass";
    if (!windowAtom) {
        WNDCLASSEX wcex { };
        wcex.cbSize = sizeof(wcex);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = staticWndProc;
        wcex.hInstance = instanceHandle();
        wcex.lpszClassName = windowClassName;
        windowAtom = ::RegisterClassEx(&wcex);
    }

    ASSERT(!m_hwnd);

    MONITORINFO mi { };
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(MonitorFromWindow(parentHwnd, MONITOR_DEFAULTTONEAREST), &mi))
        return;

    IntRect monitorRect = mi.rcMonitor;
    
    ::CreateWindowExW(WS_EX_TOOLWINDOW, windowClassName, L"", WS_POPUP, 
        monitorRect.x(), monitorRect.y(), monitorRect.width(), monitorRect.height(),
        parentHwnd, 0, instanceHandle(), this);
    ASSERT(IsWindow(m_hwnd));

    ::SetFocus(m_hwnd);
}

LRESULT FullScreenWindow::staticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!longPtr && message == WM_CREATE) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        longPtr = reinterpret_cast<LONG_PTR>(lpcs->lpCreateParams);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, longPtr);
    }

    if (FullScreenWindow* window = reinterpret_cast<FullScreenWindow*>(longPtr))
        return window->wndProc(hWnd, message, wParam, lParam);

    return ::DefWindowProc(hWnd, message, wParam, lParam);    
}

LRESULT FullScreenWindow::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    switch (message) {
    case WM_CREATE:
        m_hwnd = hWnd;
        break;
    case WM_DESTROY:
        m_hwnd = 0;
        break;
    case WM_WINDOWPOSCHANGED:
        {
            LPWINDOWPOS wp = reinterpret_cast<LPWINDOWPOS>(lParam);
            if (wp->flags & SWP_NOSIZE)
                break;
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = ::BeginPaint(m_hwnd, &ps);
            ::FillRect(hdc, &ps.rcPaint, (HBRUSH)::GetStockObject(BLACK_BRUSH));
            ::EndPaint(m_hwnd, &ps);
        }
        break;
    case WM_PRINTCLIENT:
        {
            RECT clientRect;
            HDC context = (HDC)wParam;
            ::GetClientRect(m_hwnd, &clientRect);
            ::FillRect(context, &clientRect, (HBRUSH)::GetStockObject(BLACK_BRUSH));
        }
    }
    if (m_client)
        lResult = m_client->fullscreenClientWndProc(hWnd, message, wParam, lParam);

    return lResult;
}

} // namespace WebCore

#endif // ENABLE(FULLSCREEN_API)
