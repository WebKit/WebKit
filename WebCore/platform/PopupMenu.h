/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#include "IntRect.h"
#include "PopupMenuClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class NSPopUpButtonCell;
#else
class NSPopUpButtonCell;
#endif
#elif PLATFORM(WIN)
#include "Scrollbar.h"
#include "ScrollbarClient.h"
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
#elif PLATFORM(WX)
#ifdef __WXMSW__
#include <wx/msw/winundef.h>
#endif
class wxMenu;
#include <wx/defs.h>
#include <wx/event.h>
#elif PLATFORM(CHROMIUM)
#include "PopupMenuPrivate.h"
#elif PLATFORM(HAIKU)
class BMenu;
#endif

namespace WebCore {

class FrameView;
class Scrollbar;

class PopupMenu : public RefCounted<PopupMenu>
#if PLATFORM(WIN)
                , private ScrollbarClient
#endif
#if PLATFORM(WX)
                , public wxEvtHandler
#endif
{
public:
    static PassRefPtr<PopupMenu> create(PopupMenuClient* client) { return adoptRef(new PopupMenu(client)); }
    ~PopupMenu();
    
    void disconnectClient() { m_popupClient = 0; }

    void show(const IntRect&, FrameView*, int index);
    void hide();

    void updateFromElement();
    
    PopupMenuClient* client() const { return m_popupClient; }

    static bool itemWritingDirectionIsNatural();

#if PLATFORM(WIN)
    Scrollbar* scrollbar() const { return m_scrollbar.get(); }

    static LPCTSTR popupClassName();

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
#endif

protected:
    PopupMenu(PopupMenuClient*);
    
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
    // ScrollBarClient
    virtual void valueChanged(Scrollbar*);
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&);
    virtual bool isActive() const { return true; }
    virtual bool scrollbarCornerPresent() const { return false; }

    void calculatePositionAndSize(const IntRect&, FrameView*);
    void invalidateItem(int index);

    static LRESULT CALLBACK PopupMenuWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void registerClass();

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
#elif PLATFORM(GTK)
    IntPoint m_menuPosition;
    GtkMenu* m_popup;
    HashMap<GtkWidget*, int> m_indexMap;
    static void menuItemActivated(GtkMenuItem* item, PopupMenu*);
    static void menuUnmapped(GtkWidget*, PopupMenu*);
    static void menuPositionFunction(GtkMenu*, gint*, gint*, gboolean*, PopupMenu*);
    static void menuRemoveItem(GtkWidget*, PopupMenu*);
#elif PLATFORM(WX)
    wxMenu* m_menu;
    void OnMenuItemSelected(wxCommandEvent&);
#elif PLATFORM(CHROMIUM)
    PopupMenuPrivate p;
#elif PLATFORM(HAIKU)
    BMenu* m_menu;
#endif

};

}

#endif
