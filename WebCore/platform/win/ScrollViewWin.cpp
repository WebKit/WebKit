/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
    IntPoint scrollPoint;
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
    adjustedDirtyRect.move(-m_data->scrollPoint.x(), -m_data->scrollPoint.y());

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
    contentRect.move(m_data->scrollPoint);
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
    return scrollXOffset();
}

int ScrollView::contentsY() const
{
    return scrollYOffset();
}

int ScrollView::contentsWidth() const
{
    return m_data->contentsSize.width();
}

int ScrollView::contentsHeight() const
{
    return m_data->contentsSize.height();
}

void ScrollView::viewportToContents(int x1, int y1, int& x2, int& y2)
{
    POINT point = {x1, y1};
    MapWindowPoints(GetAncestor(windowHandle(), GA_ROOT), windowHandle(), &point, 1);
    x2 = point.x + scrollXOffset();
    y2 = point.y + scrollYOffset();
}

void ScrollView::contentsToViewport(int x1, int y1, int& x2, int& y2)
{
    POINT point = {x1 - scrollXOffset(), y1 - scrollYOffset()};
    MapWindowPoints(windowHandle(), GetAncestor(windowHandle(), GA_ROOT), &point, 1);
    x2 = point.x;
    y2 = point.y;
}

int ScrollView::scrollXOffset() const
{
    return m_data->scrollPoint.x();
}

int ScrollView::scrollYOffset() const
{
    return m_data->scrollPoint.y();
}

static inline int clampToRange(int value, int min, int max)
{
    return max(min(value, max), min);
}

IntPoint ScrollView::maximumScroll() const
{
    const IntSize& docSize = m_data->contentsSize;
    const IntPoint& scrollPoint = m_data->scrollPoint;
    return IntPoint(max(docSize.width() - scrollPoint.x(), 0), max(docSize.height() - scrollPoint.y(), 0));
}

void ScrollView::scrollBy(int dx, int dy)
{
    IntPoint scrollPoint = m_data->scrollPoint;
    const IntPoint& maxScroll = maximumScroll();
    int newX = max(0, min(scrollPoint.x() + dx, maxScroll.x()));
    int newY = max(0, min(scrollPoint.y() + dy, maxScroll.y()));

    IntPoint newPoint(newX, newY);
    if (newPoint != scrollPoint) {
        m_data->scrollPoint = newPoint;
        updateScrollBars();
        // ScrollBar updates can fail, so we check the final delta before scrolling
        IntPoint scrollDelta = m_data->scrollPoint - scrollPoint;
        if (scrollDelta == IntPoint(0,0))
            return;
        if (!m_data->hasStaticBackground)
            // FIXME: This could be made more efficient by passing a valid clip rect for only the document content.
            ScrollWindowEx(windowHandle(), -scrollDelta.x(), -scrollDelta.y(), 0, 0, 0, 0, SW_INVALIDATE);
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
    const IntPoint& maxScroll = maximumScroll();

    int xScroll = max(0, min(scrollXOffset(), maxScroll.x()));
    xScroll = updateScrollInfo(SB_HORZ, xScroll, contentsWidth()-1, width());
    m_data->scrollPoint.setX(xScroll);

    int yScroll = max(0, min(scrollYOffset(), maxScroll.y()));
    yScroll = updateScrollInfo(SB_VERT, yScroll, contentsHeight()-1, height());
    m_data->scrollPoint.setY(yScroll);

    if (m_data->hScrollBarMode != ScrollBarAuto || m_data->suppressScrollBars)
        ShowScrollBar(windowHandle(), SB_HORZ, (m_data->hScrollBarMode != ScrollBarAlwaysOff) && !m_data->suppressScrollBars);
    if (m_data->vScrollBarMode != ScrollBarAuto || m_data->suppressScrollBars)
        ShowScrollBar(windowHandle(), SB_VERT, (m_data->vScrollBarMode != ScrollBarAlwaysOff) && !m_data->suppressScrollBars);
}

}
