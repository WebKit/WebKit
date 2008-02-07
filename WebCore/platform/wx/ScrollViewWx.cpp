/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
#include "ScrollView.h"

#include "FloatRect.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformWheelEvent.h"
#include "ScrollBar.h"

#include <algorithm>
#include <stdio.h>

#include <wx/defs.h>
#include <wx/scrolwin.h>
#include <wx/event.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate : public wxEvtHandler {

public:
    ScrollViewPrivate(ScrollView* scrollView)
        : wxEvtHandler()
        , m_scrollView(scrollView)
        , hasStaticBackground(false)
        , suppressScrollbars(false)
        , vScrollbarMode(ScrollbarAuto)
        , hScrollbarMode(ScrollbarAuto)
        , viewStart(0, 0)
    {
    }

    void bindEvents(wxWindow* win)
    {
        // TODO: is there an easier way to Connect to a range of events? these are contiguous.
        win->Connect(wxEVT_SCROLLWIN_TOP,          wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
        win->Connect(wxEVT_SCROLLWIN_BOTTOM,       wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
        win->Connect(wxEVT_SCROLLWIN_LINEUP,       wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
        win->Connect(wxEVT_SCROLLWIN_LINEDOWN,     wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
        win->Connect(wxEVT_SCROLLWIN_PAGEUP,       wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
        win->Connect(wxEVT_SCROLLWIN_PAGEDOWN,     wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
        win->Connect(wxEVT_SCROLLWIN_THUMBTRACK,   wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
        win->Connect(wxEVT_SCROLLWIN_THUMBRELEASE, wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
        win->Connect(wxEVT_SCROLLWIN_TOP,          wxScrollWinEventHandler(ScrollViewPrivate::OnScrollWinEvents), NULL, this);
    }
    
    void OnScrollWinEvents(wxScrollWinEvent& e)
    {
        wxEventType scrollType(e.GetEventType());
        bool horiz = e.GetOrientation() == wxHORIZONTAL;

        wxPoint pos(viewStart);
 
        if (scrollType == wxEVT_SCROLLWIN_THUMBTRACK || scrollType == wxEVT_SCROLLWIN_THUMBRELEASE) {
            if (horiz) 
                pos.x = e.GetPosition();
            else       
                pos.y = e.GetPosition();
        }
        else if ( scrollType == wxEVT_SCROLLWIN_LINEDOWN ) {
            if (horiz) 
                pos.x += LINE_STEP;
            else       
                pos.y += LINE_STEP;
        }
        else if ( scrollType == wxEVT_SCROLLWIN_LINEUP ) {
            if (horiz) 
                pos.x -= LINE_STEP;
            else       
                pos.y -= LINE_STEP;
        }
        else
            return e.Skip();

        m_scrollView->setContentsPos(pos.x, pos.y);
        m_scrollView->update();
    }

    ScrollView* m_scrollView;

    HashSet<Widget*> m_children;
    bool hasStaticBackground;
    bool suppressScrollbars;
    ScrollbarMode vScrollbarMode;
    ScrollbarMode hScrollbarMode;
    wxPoint viewStart;
};

ScrollView::ScrollView()
{
    m_data = new ScrollViewPrivate(this);
}

void ScrollView::setNativeWindow(wxWindow* win)
{
    Widget::setNativeWindow(win);
    m_data->bindEvents(win);
}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::updateContents(const IntRect& updateRect, bool now)
{
    // we need to convert coordinates to scrolled position
    wxRect contentsRect = updateRect;
    contentsRect.Offset(-contentsX(), -contentsY());
    wxWindow* win = nativeWindow();
    if (win) {
        win->RefreshRect(contentsRect, true);
        if (now)
            win->Update();
    }
}

void ScrollView::update()
{
    wxWindow* win = nativeWindow();
    if (win)
        win->Update();
}

int ScrollView::visibleWidth() const
{
    int width = 0;
    wxWindow* win = nativeWindow();
    if (win)
        win->GetClientSize(&width, NULL);
    
    ASSERT(width >= 0);
    return width;
}

int ScrollView::visibleHeight() const
{
    int height = 0;
    wxWindow* win = nativeWindow();
    if (win)
        win->GetClientSize(NULL, &height);
    
    ASSERT(height >= 0);
    return height;
}

FloatRect ScrollView::visibleContentRect() const
{
    return FloatRect(contentsX(),contentsY(),visibleWidth(),visibleHeight());
}

void ScrollView::setContentsPos(int newX, int newY)
{
    int dx = newX - contentsX();
    int dy = newY - contentsY();
    scrollBy(dx, dy);
}

void ScrollView::scrollBy(int dx, int dy)
{
    wxWindow* win = nativeWindow();
    if (!win)
        return;

    wxPoint scrollOffset = m_data->viewStart;
    wxPoint orig(scrollOffset);
    wxPoint newScrollOffset = scrollOffset + wxPoint(dx, dy);

    wxRect vRect(win->GetVirtualSize());
    wxRect cRect(win->GetClientSize());

    // clamp to scroll area
    if (newScrollOffset.x < 0)
        newScrollOffset.x = 0;
    else if (newScrollOffset.x + cRect.width > vRect.width)
        newScrollOffset.x = max(0, vRect.width - cRect.width - 1);

    if (newScrollOffset.y < 0)
        newScrollOffset.y = 0;
    else if (newScrollOffset.y + cRect.height > vRect.height)
        newScrollOffset.y = max(0, vRect.height - cRect.height - 1);

    if (newScrollOffset == scrollOffset)
        return;

    m_data->viewStart = newScrollOffset;

    wxPoint delta(orig - newScrollOffset);

    if (m_data->hasStaticBackground)
        win->Refresh();
    else
        win->ScrollWindow(delta.x, delta.y);

    adjustScrollbars();
}

void ScrollView::resizeContents(int w,int h)
{
    wxWindow* win = nativeWindow();
    if (win) {
        win->SetVirtualSize(w, h);
        adjustScrollbars();
    }
}

int ScrollView::contentsX() const
{
    ASSERT(m_data->viewStart.x >= 0);
    return m_data->viewStart.x;
}

int ScrollView::contentsY() const
{
    ASSERT(m_data->viewStart.y >= 0);
    return m_data->viewStart.y;
}

int ScrollView::contentsWidth() const
{
    int width = 0;
    wxWindow* win = nativeWindow();
    if (win)
        win->GetVirtualSize(&width, NULL);
    ASSERT(width >= 0);
    return width;
}

int ScrollView::contentsHeight() const
{
    int height = 0;
    wxWindow* win = nativeWindow();
    if (win)
        win->GetVirtualSize(NULL, &height);
    ASSERT(height >= 0);
    return height;
}

FloatRect ScrollView::visibleContentRectConsideringExternalScrollers() const
{
    // FIXME: clip this rect if parent scroll views cut off the visible
    // area.
    return visibleContentRect();
}

IntSize ScrollView::scrollOffset() const
{
    return IntSize(contentsX(), contentsY());
}

void ScrollView::adjustScrollbars(int x, int y, bool refresh)
{
    wxWindow* win = nativeWindow();
    if (!win)
        return;

    wxRect crect(win->GetClientRect()), vrect(win->GetVirtualSize());

    if (x == -1) x = m_data->viewStart.x;
    if (y == -1) y = m_data->viewStart.y;

    long style = win->GetWindowStyle();

    // by setting the wxALWAYS_SHOW_SB wxWindow flag before
    // each SetScrollbar call, we can control the scrollbars
    // visibility individually.

    // horizontal scrollbar
    switch (m_data->hScrollbarMode) {
        case ScrollbarAlwaysOff:
            win->SetWindowStyleFlag(style & ~wxALWAYS_SHOW_SB);
            win->SetScrollbar(wxHORIZONTAL, 0, 0, 0, refresh);
            break;

        case ScrollbarAuto:
            win->SetWindowStyleFlag(style & ~wxALWAYS_SHOW_SB);
            win->SetScrollbar(wxHORIZONTAL, x, crect.width, vrect.width, refresh);
            break;

        default: // ScrollbarAlwaysOn
            win->SetWindowStyleFlag(style | wxALWAYS_SHOW_SB);
            win->SetScrollbar(wxHORIZONTAL, x, crect.width, vrect.width, refresh);
            break;
    }

    // vertical scrollbar
    switch (m_data->vScrollbarMode) {
        case ScrollbarAlwaysOff:
            win->SetWindowStyleFlag(style & ~wxALWAYS_SHOW_SB);
            win->SetScrollbar(wxVERTICAL, 0, 0, 0, refresh);
            break;

        case ScrollbarAlwaysOn:
            win->SetWindowStyleFlag(style | wxALWAYS_SHOW_SB);
            win->SetScrollbar(wxVERTICAL, y, crect.height, vrect.height, refresh);
            break;

        default: // case ScrollbarAuto:
            win->SetWindowStyleFlag(style & ~wxALWAYS_SHOW_SB);
            win->SetScrollbar(wxVERTICAL, y, crect.height, vrect.height, refresh);
    }
}

WebCore::ScrollbarMode ScrollView::hScrollbarMode() const
{
    return m_data->hScrollbarMode;
}

WebCore::ScrollbarMode ScrollView::vScrollbarMode() const
{
    return m_data->vScrollbarMode;
}

void ScrollView::setScrollbarsMode(ScrollbarMode newMode)
{
    bool needsAdjust = false;

    if (m_data->hScrollbarMode != newMode) {
        m_data->hScrollbarMode = newMode;
        needsAdjust = true;
    }

    if (m_data->vScrollbarMode != newMode) {
        m_data->vScrollbarMode = newMode;
        adjustScrollbars();
        needsAdjust = true;
    }

    if (needsAdjust)
        adjustScrollbars();
}

void ScrollView::setHScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->hScrollbarMode != newMode) {
        m_data->hScrollbarMode = newMode;
        adjustScrollbars();
    }
}

void ScrollView::setVScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->vScrollbarMode != newMode) {
        m_data->vScrollbarMode = newMode;
        adjustScrollbars();
    }
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->hasStaticBackground = flag;
}

void ScrollView::suppressScrollbars(bool suppressed, bool repaintOnSuppress)
{
    if ( m_data->suppressScrollbars != suppressed )
        m_data->suppressScrollbars = suppressed;
}

IntPoint ScrollView::contentsToWindow(const IntPoint& point) const
{
    return point - scrollOffset();
}

IntPoint ScrollView::windowToContents(const IntPoint& point) const
{
    return point + scrollOffset();
}

bool ScrollView::inWindow() const
{
    // NB: This is called from RenderObject::willRenderImage
    // and really seems to be more of a "is the window in a valid state" test,
    // despite the API name.
    return nativeWindow() != NULL;
}

void ScrollView::wheelEvent(PlatformWheelEvent& e)
{
    // Determine how much we want to scroll.  If we can move at all, we will accept the event.
    IntSize maxScrollDelta = maximumScroll();
    if ((e.deltaX() < 0 && maxScrollDelta.width() > 0) ||
        (e.deltaX() > 0 && scrollOffset().width() > 0) ||
        (e.deltaY() < 0 && maxScrollDelta.height() > 0) ||
        (e.deltaY() > 0 && scrollOffset().height() > 0)) {
        e.accept();
        scrollBy(-e.deltaX() * LINE_STEP, -e.deltaY() * LINE_STEP);
    }
}

// used for subframes support
void ScrollView::addChild(Widget* widget)
{
    m_data->m_children.add(widget);

    // NB: In all cases I'm aware of,
    // by the time this is called the ScrollView is already a child
    // of its parent Widget by wx port APIs, so I don't think
    // we need to do anything here.
}

void ScrollView::removeChild(Widget* widget)
{
    m_data->m_children.remove(widget);

    if (nativeWindow() && widget->nativeWindow()) {
        nativeWindow()->RemoveChild(widget->nativeWindow());
        // FIXME: Is this the right place to do deletion? I see
        // detachFromParent2/3/4, initiated by FrameLoader::detachFromParent,
        // but I'm not sure if it's better to handle there or not.
        widget->nativeWindow()->Destroy();
    }
}

HashSet<Widget*>* ScrollView::children()
{
    return &(m_data->m_children);
}

void ScrollView::scrollRectIntoViewRecursively(const IntRect& rect)
{
    setContentsPos(rect.x(), rect.y());
}

PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent)
{
    // AFAICT this is only used for platforms that provide
    // feedback when mouse is hovered over.
    return 0;
}

IntSize ScrollView::maximumScroll() const
{
    IntSize delta = (IntSize(contentsWidth(), contentsHeight()) - IntSize(visibleWidth(), visibleHeight())) - scrollOffset();
    delta.clampNegativeToZero();
    return delta;
}

}
