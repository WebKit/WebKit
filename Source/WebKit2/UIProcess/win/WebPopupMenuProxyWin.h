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

#ifndef WebPopupMenuProxyWin_h
#define WebPopupMenuProxyWin_h

#include "PlatformPopupMenuData.h"
#include "WebPopupItem.h"
#include "WebPopupMenuProxy.h"
#include <WebCore/Scrollbar.h>
#include <WebCore/ScrollableArea.h>

typedef struct HWND__* HWND;
typedef struct HDC__* HDC;
typedef struct HBITMAP__* HBITMAP;

namespace WebKit {

class WebView;

class WebPopupMenuProxyWin : public WebPopupMenuProxy, private WebCore::ScrollableArea  {
public:
    static PassRefPtr<WebPopupMenuProxyWin> create(WebView* webView, WebPopupMenuProxy::Client* client)
    {
        return adoptRef(new WebPopupMenuProxyWin(webView, client));
    }
    ~WebPopupMenuProxyWin();

    virtual void showPopupMenu(const WebCore::IntRect&, WebCore::TextDirection, double pageScaleFactor, const Vector<WebPopupItem>&, const PlatformPopupMenuData&, int32_t selectedIndex);
    virtual void hidePopupMenu();

    bool setFocusedIndex(int index, bool hotTracking = false);

    void hide() { hidePopupMenu(); }

private:
    WebPopupMenuProxyWin(WebView*, WebPopupMenuProxy::Client*);

    WebCore::Scrollbar* scrollbar() const { return m_scrollbar.get(); }

    // ScrollableArea
    virtual int scrollSize(WebCore::ScrollbarOrientation) const;
    virtual int scrollPosition(WebCore::Scrollbar*) const;
    virtual void setScrollOffset(const WebCore::IntPoint&);
    virtual void invalidateScrollbarRect(WebCore::Scrollbar*, const WebCore::IntRect&);
    virtual void invalidateScrollCornerRect(const WebCore::IntRect&) { }
    virtual bool isActive() const { return true; }
    virtual bool isScrollCornerVisible() const { return false; }
    virtual WebCore::IntRect scrollCornerRect() const { return WebCore::IntRect(); }
    virtual WebCore::Scrollbar* verticalScrollbar() const { return m_scrollbar.get(); }
    virtual WebCore::ScrollableArea* enclosingScrollableArea() const { return 0; }

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

    void calculatePositionAndSize(const WebCore::IntRect&);
    WebCore::IntRect clientRect() const;
    void invalidateItem(int index);

    int itemHeight() const { return m_itemHeight; }
    const WebCore::IntRect& windowRect() const { return m_windowRect; }
    int wheelDelta() const { return m_wheelDelta; }
    void setWasClicked(bool b = true) { m_wasClicked = b; }
    bool wasClicked() const { return m_wasClicked; }
    void setScrollOffset(int offset) { m_scrollOffset = offset; }
    int scrollOffset() const { return m_scrollOffset; }
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
    int m_newSelectedIndex;

    RefPtr<WebCore::Scrollbar> m_scrollbar;
    HWND m_popup;
    HDC m_DC;
    HBITMAP m_bmp;
    WebCore::IntRect m_windowRect;

    int m_itemHeight;
    int m_scrollOffset;
    int m_wheelDelta;
    int m_focusedIndex;
    bool m_wasClicked;
    bool m_scrollbarCapturingMouse;
    bool m_showPopup;
};

} // namespace WebKit

#endif // WebPopupMenuProxyWin_h
