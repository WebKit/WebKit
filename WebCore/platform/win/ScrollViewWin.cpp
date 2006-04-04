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
        , suppressScrollBars(false)
        , vScrollBarMode(ScrollBarAuto)
        , hScrollBarMode(ScrollBarAuto)
    {
    }
    IntSize scrollOffset;
    IntSize contentsSize;
    bool hasStaticBackground;
    bool suppressScrollBars;
    ScrollBarMode vScrollBarMode;
    ScrollBarMode hScrollBarMode;
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
    HDC dc = GetDC(windowHandle());
    FillRect(dc, &dirtyRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    ReleaseDC(windowHandle(), dc);
#endif

    InvalidateRect(windowHandle(), &dirtyRect, true);
    if (now)
        UpdateWindow(windowHandle());
}

int ScrollView::visibleWidth() const
{
    RECT bounds;
    GetClientRect(windowHandle(), &bounds);
    return (bounds.right - bounds.left);
}

int ScrollView::visibleHeight() const
{
    RECT bounds;
    GetClientRect(windowHandle(), &bounds);
    return (bounds.bottom - bounds.top);
}

FloatRect ScrollView::visibleContentRect() const
{
    RECT bounds;
    GetClientRect(windowHandle(), &bounds);
    FloatRect contentRect = bounds;
    contentRect.move(m_data->scrollOffset);
    return contentRect;
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
        updateScrollBars();
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

IntPoint ScrollView::viewportToContents(const IntPoint& contentsPoint)
{
    POINT point = contentsPoint;
    MapWindowPoints(GetAncestor(windowHandle(), GA_ROOT), windowHandle(), &point, 1);
    return IntPoint(point) + scrollOffset();
}

IntPoint ScrollView::contentsToViewport(const IntPoint& viewportPoint)
{
    POINT point = viewportPoint - scrollOffset();
    MapWindowPoints(windowHandle(), GetAncestor(windowHandle(), GA_ROOT), &point, 1);
    return point;
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
        updateScrollBars();
        // ScrollBar updates can fail, so we check the final delta before scrolling
        IntSize scrollDelta = m_data->scrollOffset - scrollOffset;
        if (scrollDelta == IntSize())
            return;
        if (!m_data->hasStaticBackground)
            // FIXME: This could be made more efficient by passing a valid clip rect for only the document content.
            ScrollWindowEx(windowHandle(), -scrollDelta.width(), -scrollDelta.height(), 0, 0, 0, 0, SW_INVALIDATE);
        else
            InvalidateRect(windowHandle(), 0, true);
    }
}

WebCore::ScrollBarMode ScrollView::hScrollBarMode() const
{
    return m_data->hScrollBarMode;
}

WebCore::ScrollBarMode ScrollView::vScrollBarMode() const
{
    return m_data->vScrollBarMode;
}

void ScrollView::suppressScrollBars(bool suppressed, bool repaintOnSuppress)
{
    m_data->suppressScrollBars = suppressed;
    if (repaintOnSuppress)
        updateScrollBars();
}

void ScrollView::setHScrollBarMode(ScrollBarMode newMode)
{
    if (m_data->hScrollBarMode != newMode) {
        m_data->hScrollBarMode = newMode;
        updateScrollBars();
    }
}

void ScrollView::setVScrollBarMode(ScrollBarMode newMode)
{
    if (m_data->vScrollBarMode != newMode) {
        m_data->vScrollBarMode = newMode;
        updateScrollBars();
    }
}

void ScrollView::setScrollBarsMode(ScrollBarMode newMode)
{
    m_data->hScrollBarMode = m_data->vScrollBarMode = newMode;
    updateScrollBars();
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->hasStaticBackground = flag;
}

int ScrollView::updateScrollInfo(short type, int current, int max, int pageSize)
{
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = max;
    si.nPage  = pageSize;
    si.nPos   = current;
    SetScrollInfo(windowHandle(), type, &si, TRUE);
    GetScrollInfo(windowHandle(), type, &si);
    return si.nPos;
}

void ScrollView::updateScrollBars()
{ 
    IntSize maxScrollPosition(contentsWidth(), contentsHeight());
    IntSize scroll = scrollOffset().shrunkTo(maxScrollPosition);
    scroll.clampNegativeToZero();

    m_data->scrollOffset = 
        IntSize(updateScrollInfo(SB_HORZ, scroll.width(), contentsWidth() - 1, width()),
                updateScrollInfo(SB_VERT, scroll.height(), contentsHeight() - 1, height()));

    if (m_data->hScrollBarMode != ScrollBarAuto || m_data->suppressScrollBars)
        ShowScrollBar(windowHandle(), SB_HORZ, (m_data->hScrollBarMode != ScrollBarAlwaysOff) && !m_data->suppressScrollBars);
    if (m_data->vScrollBarMode != ScrollBarAuto || m_data->suppressScrollBars)
        ShowScrollBar(windowHandle(), SB_VERT, (m_data->vScrollBarMode != ScrollBarAlwaysOff) && !m_data->suppressScrollBars);
}

}
