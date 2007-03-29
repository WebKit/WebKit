/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Justin Haygood <jhaygood@spsu.edu>.
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

#include <algorithm>
#include "FloatRect.h"
#include "IntRect.h"
#include <windows.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate {
public:
    ScrollViewPrivate()
        : hasStaticBackground(false)
        , suppressScrollbars(false)
        , vScrollbarMode(ScrollbarAuto)
        , hScrollbarMode(ScrollbarAuto)
    {
    }
    IntSize scrollOffset;
    IntSize contentsSize;
    bool hasStaticBackground;
    bool suppressScrollbars;
    ScrollbarMode vScrollbarMode;
    ScrollbarMode hScrollbarMode;
};

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate())
{
}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::updateContents(const IntRect& updateRect, bool now)
{
    IntRect adjustedDirtyRect(updateRect);
    adjustedDirtyRect.move(-m_data->scrollOffset);

    RECT dirtyRect = RECT(adjustedDirtyRect);
#if PAINT_FLASHING_DEBUG
    HDC dc = GetDC(containingWindow());
    FillRect(dc, &dirtyRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    ReleaseDC(containingWindow(), dc);
#endif

    InvalidateRect(containingWindow(), &dirtyRect, true);
    if (now)
        UpdateWindow(containingWindow());
}

int ScrollView::visibleWidth() const
{
    RECT bounds;
    GetClientRect(containingWindow(), &bounds);
    return (bounds.right - bounds.left);
}

int ScrollView::visibleHeight() const
{
    RECT bounds;
    GetClientRect(containingWindow(), &bounds);
    return (bounds.bottom - bounds.top);
}

FloatRect ScrollView::visibleContentRect() const
{
    RECT bounds;
    GetClientRect(containingWindow(), &bounds);
    FloatRect contentRect = bounds;
    contentRect.move(m_data->scrollOffset);
    return contentRect;
}

FloatRect ScrollView::visibleContentRectConsideringExternalScrollers() const
{
    // external scrollers not supported for now
    return visibleContentRect();
}

void ScrollView::setContentsPos(int newX, int newY)
{
    int dx = newX - contentsX();
    int dy = newY - contentsY();
    scrollBy(dx, dy);
}

void ScrollView::resizeContents(int w,int h)
{
    IntSize newSize(w,h);
    if (m_data->contentsSize != newSize) {
        m_data->contentsSize = newSize;
        updateScrollbars(m_data->scrollOffset);
    }    
}

int ScrollView::contentsX() const
{
    return scrollOffset().width();
}

int ScrollView::contentsY() const
{
    return scrollOffset().height();
}

int ScrollView::contentsWidth() const
{
    return m_data->contentsSize.width();
}

int ScrollView::contentsHeight() const
{
    return m_data->contentsSize.height();
}

IntPoint ScrollView::contentsToWindow(const IntPoint& point) const
{
    return point - scrollOffset();
}

IntPoint ScrollView::windowToContents(const IntPoint& point) const
{
    return point + scrollOffset();
}

IntSize ScrollView::scrollOffset() const
{
    return m_data->scrollOffset;
}

IntSize ScrollView::maximumScroll() const
{
    IntSize delta = m_data->contentsSize - m_data->scrollOffset;
    delta.clampNegativeToZero();
    return delta;
}

void ScrollView::scrollBy(int dx, int dy)
{
    IntSize scrollOffset = m_data->scrollOffset;
    IntSize maxScroll = maximumScroll();
    IntSize newScrollOffset = scrollOffset + IntSize(dx, dy);
    newScrollOffset.clampNegativeToZero();

    if (newScrollOffset != scrollOffset) {
        m_data->scrollOffset = newScrollOffset;
        updateScrollbars(m_data->scrollOffset);
        // Scrollbar updates can fail, so we check the final delta before scrolling
        IntSize scrollDelta = m_data->scrollOffset - scrollOffset;
        if (scrollDelta == IntSize())
            return;
        if (!m_data->hasStaticBackground)
            // FIXME: This could be made more efficient by passing a valid clip rect for only the document content.
            ScrollWindowEx(containingWindow(), -scrollDelta.width(), -scrollDelta.height(), 0, 0, 0, 0, SW_INVALIDATE);
        else
            InvalidateRect(containingWindow(), 0, true);
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

void ScrollView::suppressScrollbars(bool suppressed, bool repaintOnSuppress)
{
    m_data->suppressScrollbars = suppressed;
    if (repaintOnSuppress)
        updateScrollbars(m_data->scrollOffset);
}

void ScrollView::setHScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->hScrollbarMode != newMode) {
        m_data->hScrollbarMode = newMode;
        updateScrollbars(m_data->scrollOffset);
    }
}

void ScrollView::setVScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->vScrollbarMode != newMode) {
        m_data->vScrollbarMode = newMode;
        updateScrollbars(m_data->scrollOffset);
    }
}

void ScrollView::setScrollbarsMode(ScrollbarMode newMode)
{
    m_data->hScrollbarMode = m_data->vScrollbarMode = newMode;
    updateScrollbars(m_data->scrollOffset);
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->hasStaticBackground = flag;
}

static int updateScrollInfo(ScrollView* view, short type, int current, int max, int pageSize)
{
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = max;
    si.nPage  = pageSize;
    si.nPos   = current;
    SetScrollInfo(view->containingWindow(), type, &si, TRUE);
    GetScrollInfo(view->containingWindow(), type, &si);
    return si.nPos;
}

void ScrollView::updateScrollbars(const IntSize&)
{ 
    IntSize maxScrollPosition(contentsWidth(), contentsHeight());
    IntSize scroll = scrollOffset().shrunkTo(maxScrollPosition);
    scroll.clampNegativeToZero();

    m_data->scrollOffset = 
        IntSize(updateScrollInfo(this, SB_HORZ, scroll.width(), contentsWidth() - 1, width()),
                updateScrollInfo(this, SB_VERT, scroll.height(), contentsHeight() - 1, height()));

    if (m_data->hScrollbarMode != ScrollbarAuto || m_data->suppressScrollbars)
        ShowScrollBar(containingWindow(), SB_HORZ, (m_data->hScrollbarMode != ScrollbarAlwaysOff) && !m_data->suppressScrollbars);
    if (m_data->vScrollbarMode != ScrollbarAuto || m_data->suppressScrollbars)
        ShowScrollBar(containingWindow(), SB_VERT, (m_data->vScrollbarMode != ScrollbarAlwaysOff) && !m_data->suppressScrollbars);
}

}
