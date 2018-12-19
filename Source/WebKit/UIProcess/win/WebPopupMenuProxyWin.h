/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include "PlatformPopupMenuData.h"
#include "WebPopupItem.h"
#include "WebPopupMenuProxy.h"
#include <OleAcc.h>
#include <WebCore/ScrollableArea.h>
#include <WebCore/Scrollbar.h>

namespace WebKit {

class WebView;

class WebPopupMenuProxyWin : public WebPopupMenuProxy, private WebCore::ScrollableArea {
public:
    static Ref<WebPopupMenuProxyWin> create(WebView* webView, WebPopupMenuProxy::Client& client)
    {
        return adoptRef(*new WebPopupMenuProxyWin(webView, client));
    }
    ~WebPopupMenuProxyWin();

    void showPopupMenu(const WebCore::IntRect&, WebCore::TextDirection, double pageScaleFactor, const Vector<WebPopupItem>&, const PlatformPopupMenuData&, int32_t selectedIndex) override;
    void hidePopupMenu() override;

    bool setFocusedIndex(int index, bool hotTracking = false);

    void hide() { hidePopupMenu(); }

private:
    WebPopupMenuProxyWin(WebView*, WebPopupMenuProxy::Client&);

    WebCore::Scrollbar* scrollbar() const { return m_scrollbar.get(); }

    // ScrollableArea
    int scrollSize(WebCore::ScrollbarOrientation) const override;
    void setScrollOffset(const WebCore::IntPoint&) override;
    int scrollOffset(WebCore::ScrollbarOrientation) const override { return m_scrollOffset; }

    void invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&) override;
    void invalidateScrollCornerRect(const WebCore::IntRect&) override { }
    bool isActive() const override { return true; }
    bool isScrollCornerVisible() const override { return false; }
    WebCore::IntRect scrollCornerRect() const override { return WebCore::IntRect(); }
    WebCore::Scrollbar* verticalScrollbar() const override { return m_scrollbar.get(); }
    WebCore::ScrollableArea* enclosingScrollableArea() const override { return 0; }
    WebCore::IntSize visibleSize() const override;
    WebCore::IntSize contentsSize() const override;
    WebCore::IntRect scrollableAreaBoundingBox(bool* = nullptr) const override;
    bool shouldPlaceBlockDirectionScrollbarOnLeft() const override { return false; }
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const override { return false; }
    bool isScrollableOrRubberbandable() override { return true; }
    bool hasScrollableOrRubberbandableAncestor() override { return true; }

    // NOTE: This should only be called by the overriden setScrollOffset from ScrollableArea.
    void scrollTo(int offset);

    static bool registerWindowClass();
    static LRESULT CALLBACK WebPopupMenuProxyWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Message pump messages.
    LRESULT onMouseActivate(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onSize(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onKeyDown(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onChar(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onMouseMove(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onLButtonDown(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onLButtonUp(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onMouseWheel(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onPaint(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onPrintClient(HWND, UINT message, WPARAM, LPARAM, bool& handled);
    LRESULT onGetObject(HWND, UINT message, WPARAM, LPARAM, bool &handled);

    void calculatePositionAndSize(const WebCore::IntRect&);
    WebCore::IntRect clientRect() const;
    void invalidateItem(int index);


    int itemHeight() const { return m_itemHeight; }
    const WebCore::IntRect& windowRect() const { return m_windowRect; }
    int wheelDelta() const { return m_wheelDelta; }
    void setWasClicked(bool b = true) { m_wasClicked = b; }
    bool wasClicked() const { return m_wasClicked; }
    bool scrollbarCapturingMouse() const { return m_scrollbarCapturingMouse; }
    void setScrollbarCapturingMouse(bool b) { m_scrollbarCapturingMouse = b; }

    bool up(unsigned lines = 1);
    bool down(unsigned lines = 1);

    void paint(const WebCore::IntRect& damageRect, HDC = 0);
    int visibleItems() const;
    int listIndexAtPoint(const WebCore::IntPoint&) const;
    int focusedIndex() const;
    void focusFirst();
    void focusLast();
    bool scrollToRevealSelection();
    void incrementWheelDelta(int delta);
    void reduceWheelDelta(int delta);

    WebView* m_webView;
    Vector<WebPopupItem> m_items;
    PlatformPopupMenuData m_data;
    int m_newSelectedIndex { 0 };

    RefPtr<WebCore::Scrollbar> m_scrollbar;
    GDIObject<HDC> m_DC;
    GDIObject<HBITMAP> m_bmp;
    HWND m_popup { nullptr };
    WebCore::IntRect m_windowRect;

    int m_itemHeight { 0 };
    int m_scrollOffset { 0 };
    int m_wheelDelta { 0 };
    int m_focusedIndex { 0 };
    int m_hoveredIndex { 0 };
    bool m_wasClicked { false };
    bool m_scrollbarCapturingMouse { false };
    bool m_showPopup { false };
    int m_scaleFactor { 1 };
};

} // namespace WebKit
