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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PopupMenu_h
#define PopupMenu_h

#include <wtf/RefCounted.h>

#include "IntRect.h"
#include "PopupMenuClient.h"
#include <wtf/PassRefPtr.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class NSPopUpButtonCell;
#else
class NSPopUpButtonCell;
#endif
#elif PLATFORM(WIN)
#include "ScrollBar.h"
#include <wtf/RefPtr.h>
typedef struct HWND__* HWND;
typedef struct HDC__* HDC;
typedef struct HBITMAP__* HBITMAP;
#elif PLATFORM(QT)
namespace WebCore {
    class QWebPopup;
}
#elif PLATFORM(GTK)
typedef struct _GtkMenu GtkMenu;
typedef struct _GtkMenuItem GtkMenuItem;
typedef struct _GtkWidget GtkWidget;
#include <wtf/HashMap.h>
#include <glib.h>
#endif

namespace WebCore {

class FrameView;
class PlatformScrollbar;

class PopupMenu : public RefCounted<PopupMenu>
#if PLATFORM(WIN)
                , private ScrollbarClient
#endif
{
public:
    static PassRefPtr<PopupMenu> create(PopupMenuClient* client) { return new PopupMenu(client); }
    ~PopupMenu();
    
    void disconnectClient() { m_popupClient = 0; }

    void show(const IntRect&, FrameView*, int index);
    void hide();

    void updateFromElement();
    
    PopupMenuClient* client() const { return m_popupClient; }

    static bool itemWritingDirectionIsNatural();

#if PLATFORM(WIN)
    PlatformScrollbar* scrollBar() const { return m_scrollBar.get(); }

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

    void paint(const IntRect& damageRect, HDC hdc = 0);

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
#endif

protected:
    PopupMenu(PopupMenuClient* client);

#if PLATFORM(WIN)
    // ScrollBarClient
    virtual void valueChanged(Scrollbar*);
    virtual IntRect windowClipRect() const;
    virtual bool isActive() const { return true; }
#endif
    
private:
    PopupMenuClient* m_popupClient;
    
#if PLATFORM(MAC)
    void clear();
    void populate();

    RetainPtr<NSPopUpButtonCell> m_popup;
#elif PLATFORM(QT)
    void clear();
    void populate(const IntRect&);
    QWebPopup* m_popup;
#elif PLATFORM(WIN)
    void calculatePositionAndSize(const IntRect&, FrameView*);
    void invalidateItem(int index);

    RefPtr<PlatformScrollbar> m_scrollBar;
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
#elif PLATFORM(GTK)
    IntPoint m_menuPosition;
    GtkMenu* m_popup;
    HashMap<GtkWidget*, int> m_indexMap;
    static void menuItemActivated(GtkMenuItem* item, PopupMenu*);
    static void menuUnmapped(GtkWidget*, PopupMenu*);
    static void menuPositionFunction(GtkMenu*, gint*, gint*, gboolean*, PopupMenu*);
    static void menuRemoveItem(GtkWidget*, PopupMenu*);
#endif

};

}

#endif
