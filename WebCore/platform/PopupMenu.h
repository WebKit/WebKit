/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef PopupMenu_h
#define PopupMenu_h

#include "Shared.h"

#include "IntRect.h"
#include "PopupMenuClient.h"
#include <wtf/PassRefPtr.h>

#if PLATFORM(MAC)
#include "RetainPtr.h"
#ifdef __OBJC__
@class NSPopUpButtonCell;
#else
class NSPopUpButtonCell;
#endif
#elif PLATFORM(WIN)
typedef struct HWND__* HWND;
typedef struct HDC__* HDC;
typedef struct HBITMAP__* HBITMAP;
#elif PLATFORM(QT)
#include <QHash>
class QMenu;
class QListWidgetItem;
#endif

namespace WebCore {

class FrameView;

class PopupMenu : public Shared<PopupMenu> {
public:
    static PassRefPtr<PopupMenu> create(PopupMenuClient* client) { return new PopupMenu(client); }
    ~PopupMenu();
    
    void disconnectClient() { m_popupClient = 0; }

    void show(const IntRect&, FrameView*, int index);
    void hide();

    void updateFromElement();
    
    PopupMenuClient* client() const { return m_popupClient; }

#if PLATFORM(WIN)
    bool up(unsigned lines = 1);
    bool down(unsigned lines = 1);

    int itemHeight() const { return m_itemHeight; }
    const IntRect& windowRect() const { return m_windowRect; }
    IntRect clientRect() const;

    int listIndexAtPoint(const IntPoint& point) { return (point.y() + m_scrollOffset) / m_itemHeight; }

    bool setFocusedIndex(int index, bool hotTracking = false, bool fireOnChange = false);
    int focusedIndex() const;
    void focusFirst();
    void focusLast();

    void paint(const IntRect& damageRect, HDC hdc = 0);

    HWND popupHandle() const { return m_popup; }

    void setWasClicked(bool b = true) { m_wasClicked = b; }
    bool wasClicked() const { return m_wasClicked; }

    void setScrollOffset(int offset) { m_scrollOffset = offset; }
    int scrollOffset() const { return m_scrollOffset; }

    bool scrollTo(int);
    bool scrollToRevealSelection();

    void incrementWheelDelta(int delta);
    void reduceWheelDelta(int delta);
    int wheelDelta() const { return m_wheelDelta; }
#endif

protected:
    PopupMenu(PopupMenuClient* client);
    
private:
    PopupMenuClient* m_popupClient;
    
#if PLATFORM(MAC)
    void clear();
    void populate();

    RetainPtr<NSPopUpButtonCell> m_popup;
#elif PLATFORM(QT)
    void clear();
    void populate(const IntRect&);
    QMenu* m_popup;
    QHash<QListWidgetItem*, int> m_actions;
#elif PLATFORM(WIN)
    void calculatePositionAndSize(const IntRect&, FrameView*);
    void invalidateItem(int index);

    HWND m_popup;
    HDC m_DC;
    HBITMAP m_bmp;
    bool m_wasClicked;
    IntRect m_windowRect;
    int m_itemHeight;
    int m_scrollOffset;
    int m_wheelDelta;
#endif

};

}

#endif
