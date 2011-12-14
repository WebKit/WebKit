/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WKCACFViewWindow.h"

#if HAVE(WKQCA)

#include <WebCore/WebCoreInstanceHandle.h>
#include <WebKitQuartzCoreAdditions/WKCACFView.h>

using namespace WebCore;

namespace WebKit {

static LPCWSTR windowClassName = L"WKCACFViewWindowClass";

WKCACFViewWindow::WKCACFViewWindow(WKCACFViewRef view, HWND parentWindow, DWORD additionalStyles)
    : m_window(0)
    , m_view(view)
    , m_deletesSelfWhenWindowDestroyed(false)
{
    ASSERT_ARG(view, view);

    registerClass();

    UINT style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | additionalStyles;
    if (parentWindow)
        style |= WS_CHILD;
    else
        style |= WS_POPUP;

    m_window = ::CreateWindowExW(0, windowClassName, L"WKCACFViewWindow", style, 0, 0, 0, 0, parentWindow, 0, instanceHandle(), this);
    ASSERT_WITH_MESSAGE(m_window, "::CreateWindowExW failed with error %lu", ::GetLastError());
}

WKCACFViewWindow::~WKCACFViewWindow()
{
    if (!m_window)
        return;

    ASSERT(!m_deletesSelfWhenWindowDestroyed);
    ::DestroyWindow(m_window);
}

LRESULT WKCACFViewWindow::onCustomDestroy(WPARAM, LPARAM)
{
    ::DestroyWindow(m_window);
    return 0;
}

LRESULT WKCACFViewWindow::onDestroy(WPARAM, LPARAM)
{
    WKCACFViewUpdate(m_view.get(), 0, 0);
    return 0;
}

LRESULT WKCACFViewWindow::onEraseBackground(WPARAM, LPARAM)
{
    // Tell Windows not to erase us.
    return 1;
}

LRESULT WKCACFViewWindow::onNCDestroy(WPARAM, LPARAM)
{
    m_window = 0;

    if (!m_deletesSelfWhenWindowDestroyed)
        return 0;

    delete this;
    return 0;
}

LRESULT WKCACFViewWindow::onPaint(WPARAM, LPARAM)
{
    WKCACFViewDraw(m_view.get());
    ::ValidateRect(m_window, 0);
    return 0;
}

LRESULT WKCACFViewWindow::onPrintClient(WPARAM wParam, LPARAM lParam)
{
    if (!(lParam & PRF_CLIENT))
        return 0;

    WKCACFViewDrawIntoDC(m_view.get(), reinterpret_cast<HDC>(wParam));
    return 0;
}

void WKCACFViewWindow::registerClass()
{
    static bool didRegister;
    if (didRegister)
        return;
    didRegister = true;

    WNDCLASSW wndClass = {0};
    wndClass.lpfnWndProc = staticWndProc;
    wndClass.hInstance = instanceHandle();
    wndClass.lpszClassName = windowClassName;

    ::RegisterClassW(&wndClass);
}

LRESULT WKCACFViewWindow::staticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WKCACFViewWindow* window = reinterpret_cast<WKCACFViewWindow*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (!window) {
        if (message != WM_CREATE)
            return ::DefWindowProcW(hWnd, message, wParam, lParam);
        CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<WKCACFViewWindow*>(createStruct->lpCreateParams);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }

    return window->wndProc(message, wParam, lParam);
}

LRESULT WKCACFViewWindow::wndProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case customDestroyMessage:
        return onCustomDestroy(wParam, lParam);
    case WM_DESTROY:
        return onDestroy(wParam, lParam);
    case WM_ERASEBKGND:
        return onEraseBackground(wParam, lParam);
    case WM_NCDESTROY:
        return onNCDestroy(wParam, lParam);
    case WM_PAINT:
        return onPaint(wParam, lParam);
    case WM_PRINTCLIENT:
        return onPrintClient(wParam, lParam);
    default:
        return ::DefWindowProcW(m_window, message, wParam, lParam);
    }
}

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)
