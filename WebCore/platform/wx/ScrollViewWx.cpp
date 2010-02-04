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
#include "Scrollbar.h"

#include <algorithm>
#include <stdio.h>

#include <wx/defs.h>
#include <wx/scrolbar.h>
#include <wx/scrolwin.h>
#include <wx/event.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate : public wxEvtHandler {

public:
    ScrollViewPrivate(ScrollView* scrollView)
        : wxEvtHandler()
        , m_scrollView(scrollView)
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
        else if (scrollType == wxEVT_SCROLLWIN_LINEDOWN) {
            if (horiz) 
                pos.x += Scrollbar::pixelsPerLineStep();
            else       
                pos.y += Scrollbar::pixelsPerLineStep();
        }
        else if (scrollType == wxEVT_SCROLLWIN_LINEUP) {
            if (horiz) 
                pos.x -= Scrollbar::pixelsPerLineStep();
            else       
                pos.y -= Scrollbar::pixelsPerLineStep();
        }
        else if (scrollType == wxEVT_SCROLLWIN_PAGEUP) {
            if (horiz) 
                pos.x -= max<int>(m_scrollView->visibleWidth() * Scrollbar::minFractionToStepWhenPaging(), m_scrollView->visibleWidth() - Scrollbar::maxOverlapBetweenPages());
            else       
                pos.y -= max<int>(m_scrollView->visibleHeight() * Scrollbar::minFractionToStepWhenPaging(), m_scrollView->visibleHeight() - Scrollbar::maxOverlapBetweenPages());
        }
        else if (scrollType == wxEVT_SCROLLWIN_PAGEDOWN) {
            if (horiz) 
                pos.x += max<int>(m_scrollView->visibleWidth() * Scrollbar::minFractionToStepWhenPaging(), m_scrollView->visibleWidth() - Scrollbar::maxOverlapBetweenPages());
            else       
                pos.y += max<int>(m_scrollView->visibleHeight() * Scrollbar::minFractionToStepWhenPaging(), m_scrollView->visibleHeight() - Scrollbar::maxOverlapBetweenPages());
        }
        else
            return e.Skip();

        m_scrollView->setScrollPosition(IntPoint(pos.x, pos.y));
    }

    ScrollView* m_scrollView;

    ScrollbarMode vScrollbarMode;
    ScrollbarMode hScrollbarMode;
    wxPoint viewStart;
};

void ScrollView::platformInit()
{
    m_data = new ScrollViewPrivate(this);
}


void ScrollView::platformDestroy()
{
    delete m_data;
}

void ScrollView::setPlatformWidget(wxWindow* win)
{
    Widget::setPlatformWidget(win);
    m_data->bindEvents(win);
}

void ScrollView::platformRepaintContentRectangle(const IntRect& updateRect, bool now)
{
    // we need to convert coordinates to scrolled position
    wxRect contentsRect = updateRect;
    contentsRect.Offset(-scrollX(), -scrollY());
    wxWindow* win = platformWidget();
    if (win) {
        win->RefreshRect(contentsRect, true);
        if (now)
            win->Update();
    }
}

IntRect ScrollView::platformVisibleContentRect(bool includeScrollbars) const
{
    wxWindow* win = platformWidget();
    if (!win)
        return IntRect();

    int width, height;

    if (includeScrollbars)
        win->GetSize(&width, &height);
    else
        win->GetClientSize(&width, &height);
        
    return IntRect(m_data->viewStart.x, m_data->viewStart.y, width, height);
}

IntSize ScrollView::platformContentsSize() const
{
    int width = 0;
    int height = 0;
    if (platformWidget()) {
        platformWidget()->GetVirtualSize(&width, &height);
        ASSERT(width >= 0 && height >= 0);
    }
    return IntSize(width, height);
}

void ScrollView::platformSetScrollPosition(const IntPoint& scrollPoint)
{
    wxWindow* win = platformWidget();

    wxPoint scrollOffset = m_data->viewStart;
    wxPoint orig(scrollOffset);
    wxPoint newScrollOffset(scrollPoint);

    wxRect vRect(win->GetVirtualSize());
    wxRect cRect(win->GetClientSize());

    // clamp to scroll area
    if (newScrollOffset.x < 0)
        newScrollOffset.x = 0;
    else if (newScrollOffset.x + cRect.width > vRect.width)
        newScrollOffset.x = max(0, vRect.width - cRect.width);

    if (newScrollOffset.y < 0)
        newScrollOffset.y = 0;
    else if (newScrollOffset.y + cRect.height > vRect.height)
        newScrollOffset.y = max(0, vRect.height - cRect.height);

    if (newScrollOffset == scrollOffset)
        return;

    m_data->viewStart = newScrollOffset;

    wxPoint delta(orig - newScrollOffset);

    if (canBlitOnScroll())
        win->ScrollWindow(delta.x, delta.y);
    else
        win->Refresh();

    adjustScrollbars();
}

bool ScrollView::platformScroll(ScrollDirection, ScrollGranularity)
{
    notImplemented();
    return true;
}

void ScrollView::platformSetContentsSize()
{
    wxWindow* win = platformWidget();
    if (!win)
        return;

    win->SetVirtualSize(m_contentsSize.width(), m_contentsSize.height());
    adjustScrollbars();
}

void ScrollView::adjustScrollbars(int x, int y, bool refresh)
{
    wxWindow* win = platformWidget();
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

void ScrollView::platformSetScrollbarModes()
{
    bool needsAdjust = false;

    if (m_data->hScrollbarMode != horizontalScrollbarMode() ) {
        m_data->hScrollbarMode = horizontalScrollbarMode();
        needsAdjust = true;
    }

    if (m_data->vScrollbarMode != verticalScrollbarMode() ) {
        m_data->vScrollbarMode = verticalScrollbarMode();
        needsAdjust = true;
    }

    if (needsAdjust)
        adjustScrollbars();
}

void ScrollView::platformScrollbarModes(ScrollbarMode& horizontal, ScrollbarMode& vertical) const
{
    horizontal = m_data->hScrollbarMode;
    vertical = m_data->vScrollbarMode;
}

void ScrollView::platformSetCanBlitOnScroll(bool canBlitOnScroll)
{
    m_canBlitOnScroll = canBlitOnScroll;
}

bool ScrollView::platformCanBlitOnScroll() const
{
    return m_canBlitOnScroll;
}

// used for subframes support
void ScrollView::platformAddChild(Widget* widget)
{
    // NB: In all cases I'm aware of,
    // by the time this is called the ScrollView is already a child
    // of its parent Widget by wx port APIs, so I don't think
    // we need to do anything here.
}

void ScrollView::platformRemoveChild(Widget* widget)
{
    if (platformWidget()) {
        platformWidget()->RemoveChild(widget->platformWidget());
        // FIXME: Is this the right place to do deletion? I see
        // detachFromParent2/3/4, initiated by FrameLoader::detachFromParent,
        // but I'm not sure if it's better to handle there or not.
        widget->platformWidget()->Destroy();
    }
}

IntRect ScrollView::platformContentsToScreen(const IntRect& rect) const
{
    if (platformWidget()) {
        wxRect wxrect = rect;
        platformWidget()->ClientToScreen(&wxrect.x, &wxrect.y);
        return wxrect;
    }
    return IntRect();
}

IntPoint ScrollView::platformScreenToContents(const IntPoint& point) const
{
    if (platformWidget()) {
        return platformWidget()->ScreenToClient(point);
    }
    return IntPoint();
}

bool ScrollView::platformIsOffscreen() const
{
    return !platformWidget() || !platformWidget()->IsShownOnScreen();
}

}
