/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#pragma once

#include "APIObject.h"
#include "PageClientImpl.h"
#include "WKView.h"
#include "WebPageProxy.h"
#include <WebCore/COMPtr.h>
#include <WebCore/WindowMessageListener.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class DrawingAreaProxy;

class WebView
    : public API::ObjectImpl<API::Object::Type::View>
    , WebCore::WindowMessageListener {
public:
    static Ref<WebView> create(RECT rect, const API::PageConfiguration& configuration, HWND parentWindow)
    {
        auto webView = adoptRef(*new WebView(rect, configuration, parentWindow));
        webView->initialize();
        return webView;
    }
    ~WebView();

    HWND window() const { return m_window; }
    void setParentWindow(HWND);
    void windowAncestryDidChange();
    void setIsInWindow(bool);
    void setIsVisible(bool);
    bool isWindowActive();
    bool isFocused();
    bool isVisible();
    bool isInWindow();
    void setCursor(const WebCore::Cursor&);
    void setOverrideCursor(HCURSOR);
    void setScrollOffsetOnNextResize(const WebCore::IntSize&);
    void initialize();
    void setToolTip(const String&);

    void setViewNeedsDisplay(const WebCore::Region&);

    WebPageProxy* page() const { return m_page.get(); }

    DrawingAreaProxy* drawingArea() { return page() ? page()->drawingArea() : nullptr; }

private:
    WebView(RECT, const API::PageConfiguration&, HWND parentWindow);

    static bool registerWebViewWindowClass();
    static LRESULT CALLBACK WebViewWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT onMouseEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onWheelEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onHorizontalScroll(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onVerticalScroll(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);
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
    LRESULT onMenuCommand(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled);

    void paint(HDC, const WebCore::IntRect& dirtyRect);
    void setWasActivatedByMouseEvent(bool flag) { m_wasActivatedByMouseEvent = flag; }

    void updateActiveState();
    void updateActiveStateSoon();

    void initializeToolTipWindow();

    void startTrackingMouseLeave();
    void stopTrackingMouseLeave();

    bool shouldInitializeTrackPointHack();

    void close();

    HCURSOR cursorToShow() const;
    void updateNativeCursor();

    void updateChildWindowGeometries();

    void didCommitLoadForMainFrame(bool useCustomRepresentation);
    void didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const IPC::DataReference&);
    virtual double customRepresentationZoomFactor();
    virtual void setCustomRepresentationZoomFactor(double);

    virtual void findStringInCustomRepresentation(const String&, FindOptions, unsigned maxMatchCount);
    virtual void countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned maxMatchCount);

    virtual HWND nativeWindow();

    // WebCore::WindowMessageListener
    virtual void windowReceivedMessage(HWND, UINT message, WPARAM, LPARAM);

    HWND m_window { nullptr };
    HWND m_topLevelParentWindow { nullptr };
    HWND m_toolTipWindow { nullptr };
    WTF::String m_toolTip;

    WebCore::IntSize m_nextResizeScrollOffset;

    HCURSOR m_lastCursorSet { nullptr };
    HCURSOR m_webCoreCursor { nullptr };
    HCURSOR m_overrideCursor { nullptr };

    bool m_isInWindow { false };
    bool m_isVisible { false };
    bool m_wasActivatedByMouseEvent { false };
    bool m_trackingMouseLeave { false };
    bool m_isBeingDestroyed { false };

    std::unique_ptr<WebKit::PageClientImpl> m_pageClient;
    RefPtr<WebPageProxy> m_page;
};

} // namespace WebKit
