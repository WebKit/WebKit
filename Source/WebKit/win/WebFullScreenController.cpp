/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(FULLSCREEN_API)

#include "WebFullScreenController.h"

#include "WebView.h"
#include <wtf/RefPtr.h>
#include <WebCore/Element.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/MediaPlayerPrivateFullscreenWindow.h>
#include <WebCore/WebCoreInstanceHandle.h>

using namespace WebCore;

class WebFullScreenController::Private : public MediaPlayerPrivateFullscreenClient {
public:
    Private(WebFullScreenController* controller, WebView* webView) 
        : controller(controller)
        , webView(webView)
        , hostWindow(0)
        , originalHost(0)
        , isFullScreen(false)
    {
    }
    virtual ~Private() { }

    virtual LRESULT fullscreenClientWndProc(HWND, UINT, WPARAM, LPARAM);
    
    WebFullScreenController* controller;
    RefPtr<Element> element;
    WebView* webView;
    OwnPtr<MediaPlayerPrivateFullscreenWindow> fullScreenWindow;
    IntRect fullScreenFrame;
    HWND hostWindow;
    HWND originalHost;
    bool isFullScreen;
};

LRESULT WebFullScreenController::Private::fullscreenClientWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;

    switch (msg) {
    case WM_CREATE:
        hostWindow = hwnd;
        break;
    case WM_DESTROY:
        hostWindow = 0;
        break;
    case WM_MOVE:
        fullScreenFrame.setX(LOWORD(lParam));
        fullScreenFrame.setY(HIWORD(lParam));
        break;
    case WM_SIZE:
        fullScreenFrame.setWidth(LOWORD(lParam));
        fullScreenFrame.setHeight(HIWORD(lParam));
        if (webView->viewWindow())
            ::SetWindowPos(webView->viewWindow(), 0, 0, 0, fullScreenFrame.width(), fullScreenFrame.height(), SWP_NOREPOSITION  | SWP_NOMOVE);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            controller->exitFullScreen();
            break;
        }
        // Fall through.
    default:
        lResult = ::DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return lResult;
}

WebFullScreenController::WebFullScreenController(WebView* webView)
    : m_private(adoptPtr(new WebFullScreenController::Private(this, webView)))
{
    ASSERT(webView);
}

WebFullScreenController::~WebFullScreenController()
{
}

void WebFullScreenController::setElement(PassRefPtr<Element> element)
{
    m_private->element = element;
}

Element* WebFullScreenController::element() const
{
    return m_private->element.get();
}

bool WebFullScreenController::isFullScreen() const
{
    return m_private->isFullScreen;
}

void WebFullScreenController::enterFullScreen()
{
    if (m_private->isFullScreen)
        return;
    m_private->isFullScreen = true;

    ASSERT(m_private->element);
    m_private->element->document()->webkitWillEnterFullScreenForElement(m_private->element.get());

    ASSERT(!m_private->fullScreenWindow);
    m_private->fullScreenWindow = adoptPtr(new MediaPlayerPrivateFullscreenWindow(m_private.get()));
    ASSERT(m_private->fullScreenWindow);

    m_private->fullScreenWindow->createWindow(0);
    ASSERT(m_private->hostWindow);

    HWND originalHost = 0;
    if (FAILED(m_private->webView->hostWindow(reinterpret_cast<OLE_HANDLE*>(&originalHost))))
        return;

    if (FAILED(m_private->webView->setHostWindow(reinterpret_cast<OLE_HANDLE>(m_private->hostWindow))))
        return;

    IntRect viewFrame(IntPoint(), m_private->fullScreenFrame.size());
    ::SetWindowPos(m_private->webView->viewWindow(), HWND_TOP, 0, 0, viewFrame.width(), viewFrame.height(), SWP_NOREPOSITION);
    ::ShowWindow(m_private->hostWindow, SW_SHOW);
    m_private->webView->repaint(viewFrame, false, true);
    m_private->originalHost = originalHost;

    m_private->element->document()->webkitDidEnterFullScreenForElement(m_private->element.get());
}

void WebFullScreenController::exitFullScreen()
{
    if (!m_private->isFullScreen)
        return;
    m_private->isFullScreen = false;

    ASSERT(m_private->element);
    m_private->element->document()->webkitWillExitFullScreenForElement(m_private->element.get());

    ASSERT(m_private->hostWindow);
    if (FAILED(m_private->webView->setHostWindow(reinterpret_cast<OLE_HANDLE>(m_private->originalHost))))
        return;

    ::DestroyWindow(m_private->hostWindow);
    ASSERT(!m_private->hostWindow);
    m_private->fullScreenWindow = 0;

    m_private->element->document()->webkitDidExitFullScreenForElement(m_private->element.get());
    m_private->element = 0;
}

#endif
