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

#ifndef WebView_h
#define WebView_h

#include "APIObject.h"
#include "PageClient.h"
#include "WebPageProxy.h"
#include <WebCore/WindowMessageListener.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class DrawingAreaProxy;
class WebPageNamespace;

class WebView : public APIObject, public PageClient, WebCore::WindowMessageListener {
public:
    static PassRefPtr<WebView> create(RECT rect, WebPageNamespace* pageNamespace, HWND hostWindow)
    {
        return adoptRef(new WebView(rect, pageNamespace, hostWindow));
    }
    ~WebView();

    RECT rect() const { return m_rect; }

    HWND window() const { return m_window; }
    HWND hostWindow() const { return m_hostWindow; }
    void setHostWindow(HWND);
    void windowAncestryDidChange();

    WebPageProxy* page() const { return m_page.get(); }

private:
    WebView(RECT, WebPageNamespace*, HWND hostWindow);

    virtual Type type() const { return TypeView; }

    static bool registerWebViewWindowClass();
    static LRESULT CALLBACK WebViewWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT onMouseEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onWheelEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onKeyEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onPaintEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onPrintClientEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSizeEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onWindowPositionChangedEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSetFocusEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onKillFocusEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onTimerEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onShowWindowEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSetCursor(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);

    bool isActive();
    void updateActiveState();
    void updateActiveStateSoon();

    void initializeToolTipWindow();

    void startTrackingMouseLeave();
    void stopTrackingMouseLeave();

    void close();

    // PageClient
    virtual void processDidExit();
    virtual void processDidRevive();
    virtual void takeFocus(bool direction);
    virtual void toolTipChanged(const WTF::String&, const WTF::String&);
    virtual void setCursor(const WebCore::Cursor&);
#if USE(ACCELERATED_COMPOSITING)
    virtual void pageDidEnterAcceleratedCompositing();
    virtual void pageDidLeaveAcceleratedCompositing();
#endif

    // WebCore::WindowMessageListener
    virtual void windowReceivedMessage(HWND, UINT message, WPARAM, LPARAM);

    RECT m_rect;
    HWND m_window;
    HWND m_hostWindow;
    HWND m_topLevelParentWindow;
    HWND m_toolTipWindow;

    HCURSOR m_lastCursorSet;

    bool m_trackingMouseLeave;
    bool m_isBeingDestroyed;

    RefPtr<WebPageProxy> m_page;
};

} // namespace WebKit

#endif // WebView_h
