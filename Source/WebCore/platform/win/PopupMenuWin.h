/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef PopupMenuWin_h
#define PopupMenuWin_h

#include "IntRect.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include "Scrollbar.h"
#include "ScrollbarClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

typedef struct HWND__* HWND;
typedef struct HDC__* HDC;
typedef struct HBITMAP__* HBITMAP;

namespace WebCore {

class FrameView;
class Scrollbar;

class PopupMenuWin : public PopupMenu, private ScrollbarClient {
public:
    PopupMenuWin(PopupMenuClient*);
    ~PopupMenuWin();

    virtual void show(const IntRect&, FrameView*, int index);
    virtual void hide();
    virtual void updateFromElement();
    virtual void disconnectClient();

    static LPCTSTR popupClassName();

private:
    PopupMenuClient* client() const { return m_popupClient; }

    Scrollbar* scrollbar() const { return m_scrollbar.get(); }

    bool up(unsigned lines = 1);
    bool down(unsigned lines = 1);

    int itemHeight() const { return m_itemHeight; }
    const IntRect& windowRect() const { return m_windowRect; }
    IntRect clientRect() const;

    int visibleItems() const;

    int listIndexAtPoint(const IntPoint&) const;

    bool setFocusedIndex(int index, bool hotTracking = false);
    int focusedIndex() const;
    void focusFirst();
    void focusLast();

    void paint(const IntRect& damageRect, HDC = 0);

    HWND popupHandle() const { return m_popup; }

    void setWasClicked(bool b = true) { m_wasClicked = b; }
    bool wasClicked() const { return m_wasClicked; }

    void setScrollOffset(int offset) { m_scrollOffset = offset; }
    int scrollOffset() const { return m_scrollOffset; }

    bool scrollToRevealSelection();

    void incrementWheelDelta(int delta);
    void reduceWheelDelta(int delta);
    int wheelDelta() const { return m_wheelDelta; }

    bool scrollbarCapturingMouse() const { return m_scrollbarCapturingMouse; }
    void setScrollbarCapturingMouse(bool b) { m_scrollbarCapturingMouse = b; }

    // ScrollBarClient
    virtual int scrollSize(ScrollbarOrientation orientation) const;
    virtual void setScrollOffsetFromAnimation(const IntPoint&);
    virtual void valueChanged(Scrollbar*);
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&);
    virtual bool isActive() const { return true; }
    virtual bool scrollbarCornerPresent() const { return false; }

    void calculatePositionAndSize(const IntRect&, FrameView*);
    void invalidateItem(int index);

    static LRESULT CALLBACK PopupMenuWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void registerClass();

    PopupMenuClient* m_popupClient;
    RefPtr<Scrollbar> m_scrollbar;
    HWND m_popup;
    HDC m_DC;
    HBITMAP m_bmp;
    bool m_wasClicked;
    IntRect m_windowRect;
    int m_itemHeight;
    int m_scrollOffset;
    int m_wheelDelta;
    int m_focusedIndex;
    bool m_scrollbarCapturingMouse;
    bool m_showPopup;
};

}

#endif // PopupMenuWin_h
