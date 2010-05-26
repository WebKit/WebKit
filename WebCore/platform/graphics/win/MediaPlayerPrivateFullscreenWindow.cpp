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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS “AS IS” 
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
#include "MediaPlayerPrivateFullscreenWindow.h"

#include "IntRect.h"
#include "WebCoreInstanceHandle.h"
#include <CoreGraphics/CGColor.h>
#include <windows.h>

namespace WebCore {

MediaPlayerPrivateFullscreenWindow::MediaPlayerPrivateFullscreenWindow(MediaPlayerPrivateFullscreenClient* client)
    : m_client(client)
    , m_hwnd(0)
#if USE(ACCELERATED_COMPOSITING)
    , m_layerRenderer(WKCACFLayerRenderer::create(0))
#endif
{
}

MediaPlayerPrivateFullscreenWindow::~MediaPlayerPrivateFullscreenWindow()
{
    if (m_hwnd)
        close();
}

void MediaPlayerPrivateFullscreenWindow::createWindow(HWND parentHwnd)
{
    static ATOM windowAtom;
    static LPCWSTR windowClassName = L"MediaPlayerPrivateFullscreenWindowClass";
    if (!windowAtom) {
        WNDCLASSEX wcex = {0};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = staticWndProc;
        wcex.hInstance = instanceHandle();
        wcex.lpszClassName = windowClassName;
        windowAtom = ::RegisterClassEx(&wcex);
    }

    if (m_hwnd)
        close();

    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(MonitorFromWindow(parentHwnd, MONITOR_DEFAULTTONEAREST), &mi))
        return;

    IntRect monitorRect = mi.rcMonitor;
    
    ::CreateWindowExW(WS_EX_TOOLWINDOW, windowClassName, L"", WS_POPUP | WS_VISIBLE, 
        monitorRect.x(), monitorRect.y(), monitorRect.width(), monitorRect.height(),
        parentHwnd, 0, WebCore::instanceHandle(), this);
    ASSERT(IsWindow(m_hwnd));

    ::SetFocus(m_hwnd);
}

void MediaPlayerPrivateFullscreenWindow::close()
{
    ::DestroyWindow(m_hwnd);
    ASSERT(!m_hwnd);
}

#if USE(ACCELERATED_COMPOSITING)
void MediaPlayerPrivateFullscreenWindow::setRootChildLayer(PassRefPtr<WKCACFLayer> rootChild)
{
    if (m_rootChild == rootChild)
        return;

    if (m_rootChild)
        m_rootChild->removeFromSuperlayer();

    m_rootChild = rootChild;

    if (!m_rootChild)
        return;

    m_layerRenderer->setRootChildLayer(m_rootChild.get());
    WKCACFLayer* rootLayer = m_rootChild->rootLayer();
    CGRect rootBounds = m_rootChild->rootLayer()->bounds();
    m_rootChild->setFrame(rootBounds);
    m_layerRenderer->setScrollFrame(IntPoint(rootBounds.origin), IntSize(rootBounds.size));
    m_rootChild->setBackgroundColor(CGColorGetConstantColor(kCGColorBlack));
#ifndef NDEBUG
    RetainPtr<CGColorRef> redColor(AdoptCF, CGColorCreateGenericRGB(1, 0, 0, 1));
    rootLayer->setBackgroundColor(redColor.get());
#else
    rootLayer->setBackgroundColor(CGColorGetConstantColor(kCGColorBlack));
#endif
}
#endif

LRESULT MediaPlayerPrivateFullscreenWindow::staticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!longPtr && message == WM_CREATE) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        longPtr = reinterpret_cast<LONG_PTR>(lpcs->lpCreateParams);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, longPtr);
    }

    if (MediaPlayerPrivateFullscreenWindow* window = reinterpret_cast<MediaPlayerPrivateFullscreenWindow*>(longPtr))
        return window->wndProc(hWnd, message, wParam, lParam);

    return ::DefWindowProc(hWnd, message, wParam, lParam);    
}

LRESULT MediaPlayerPrivateFullscreenWindow::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    switch (message) {
    case WM_CREATE:
        m_hwnd = hWnd;
#if USE(ACCELERATED_COMPOSITING)
        m_layerRenderer->setHostWindow(m_hwnd);
        m_layerRenderer->createRenderer();
        if (m_rootChild)
            m_layerRenderer->setNeedsDisplay();
#endif
        break;
    case WM_DESTROY:
        m_hwnd = 0;
#if USE(ACCELERATED_COMPOSITING)
        m_layerRenderer->destroyRenderer();
        m_layerRenderer->setHostWindow(0);
#endif
        break;
    case WM_WINDOWPOSCHANGED:
        {
            LPWINDOWPOS wp = reinterpret_cast<LPWINDOWPOS>(lParam);
            if (wp->flags & SWP_NOSIZE)
                break;
#if USE(ACCELERATED_COMPOSITING)
            m_layerRenderer->resize();
            WKCACFLayer* rootLayer = m_rootChild->rootLayer();
            CGRect rootBounds = m_rootChild->rootLayer()->bounds();
            m_rootChild->setFrame(rootBounds);
            m_rootChild->setNeedsLayout();
            m_layerRenderer->setScrollFrame(IntPoint(rootBounds.origin), IntSize(rootBounds.size));
#endif
        }
        break;
    case WM_PAINT:
#if USE(ACCELERATED_COMPOSITING)
        m_layerRenderer->renderSoon();
#endif
        break;
    }

    if (m_client)
        lResult = m_client->fullscreenClientWndProc(hWnd, message, wParam, lParam);

    return lResult;
}

}
