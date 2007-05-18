/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * All rights reserved.
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
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplementedGdk.h"
#include "RenderLayer.h"

#include <gdk/gdk.h>

using namespace std;

namespace WebCore {

//hack to simulate scroll bars
const int scrollbarSize = 10;

class ScrollView::ScrollViewPrivate
{
public:
    ScrollViewPrivate()
        : hasStaticBackground(false), suppressScrollbars(false)
        , vScrollbarMode(ScrollbarAuto), hScrollbarMode(ScrollbarAuto)
    { }

    IntSize scrollOffset;
    IntSize contentsSize;
    bool hasStaticBackground;
    bool suppressScrollbars;
    ScrollbarMode vScrollbarMode;
    ScrollbarMode hScrollbarMode;
    IntRect visibleContentArea;
    IntRect viewportArea;
    IntRect scrollViewArea;
};


ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate)
{}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::updateView(const IntRect& updateRect, bool now)
{
    GdkRectangle rect = { updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height() };
    GdkDrawable* gdkdrawable = Widget::drawable();
    if (GDK_IS_WINDOW(gdkdrawable)) {
        GdkWindow* window = GDK_WINDOW(gdkdrawable);
        gdk_window_invalidate_rect(window, &rect, true);
        if (now)
            gdk_window_process_updates(window, true);
    }
}

void ScrollView::updateContents(const IntRect& updateRect, bool now)
{
    IntRect adjustedDirtyRect(updateRect);
    adjustedDirtyRect.move(-m_data->scrollOffset);
    updateView(adjustedDirtyRect, now);
}

void ScrollView::update()
{
    notImplementedGdk();
}

int ScrollView::visibleWidth() const
{
    return m_data->viewportArea.width();
}

int ScrollView::visibleHeight() const
{
    return m_data->viewportArea.height();
}

// Region of the content currently visible in the viewport in the content view's coordinate system.
FloatRect ScrollView::visibleContentRect() const
{
    FloatRect contentRect = FloatRect(m_data->viewportArea);
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

void ScrollView::resizeContents(int w, int h)
{
    IntSize newSize(w, h);
    if (m_data->contentsSize != newSize) {
        m_data->contentsSize = newSize;
        updateScrollbars();
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
    newScrollOffset = newScrollOffset.shrunkTo(maxScroll);

    if (newScrollOffset != scrollOffset) {
        m_data->scrollOffset = newScrollOffset;
        updateScrollbars();
        // Scrollbar updates can fail, so we check the final delta before scrolling
        IntSize scrollDelta = m_data->scrollOffset - scrollOffset;
        if (scrollDelta == IntSize())
            return;
        if (isFrameView()) {
            FrameView* f = static_cast<FrameView*>(this);
            f->frame()->renderer()->layer()->scrollToOffset
                (newScrollOffset.width(), newScrollOffset.height(), true, true);
        } else {
            printf("FIXME ScrollViewGdk Unsupported Scroll operation !!!\n");
            updateView(m_data->viewportArea, true);
        }
    }
}

ScrollbarMode ScrollView::hScrollbarMode() const
{
    return m_data->hScrollbarMode;
}

ScrollbarMode ScrollView::vScrollbarMode() const
{
    return m_data->vScrollbarMode;
}

void ScrollView::suppressScrollbars(bool suppressed, bool repaintOnSuppress)
{
    m_data->suppressScrollbars = suppressed;
    if (repaintOnSuppress)
        updateScrollbars();
}

void ScrollView::setHScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->hScrollbarMode != newMode) {
        m_data->hScrollbarMode = newMode;
        updateScrollbars();
    }
}

void ScrollView::setVScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->vScrollbarMode != newMode) {
        m_data->vScrollbarMode = newMode;
        updateScrollbars();
    }
}

void ScrollView::setScrollbarsMode(ScrollbarMode newMode)
{
    m_data->hScrollbarMode = m_data->vScrollbarMode = newMode;
    updateScrollbars();
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->hasStaticBackground = flag;
}

void ScrollView::setFrameGeometry(const IntRect& r)
{
    Widget::setFrameGeometry(r);
    m_data->scrollViewArea = IntRect(0, 0, r.width(), r.height());
    m_data->viewportArea = IntRect(0, 0, r.width() - scrollbarSize, r.height() - scrollbarSize);
}

void ScrollView::updateGeometry()
{
    GdkDrawable* gdkdrawable = drawable();
    if (gdkdrawable) {
        GdkWindow* frame = GDK_WINDOW(gdkdrawable);
        gint x, y, width, height, depth;
        gdk_window_get_geometry(frame, &x, &y, &width, &height, &depth);
        m_data->scrollViewArea = IntRect(0, 0, width, height);
        m_data->viewportArea = IntRect(0, 0, width - scrollbarSize, height - scrollbarSize);
    }
}

void ScrollView::setGtkWidget(GtkWidget* widget)
{
    Widget::setGtkWidget(widget);
    if (!GDK_IS_WINDOW(drawable())) {
        LOG_ERROR("image scrollview not supported");
        return;
    }
    updateGeometry();
}


void ScrollView::addChild(Widget*)
{ 
    notImplementedGdk();
}

void ScrollView::removeChild(Widget*)
{
    notImplementedGdk();
}

void ScrollView::scrollRectIntoViewRecursively(const IntRect&)
{
    notImplementedGdk();
}

bool ScrollView::inWindow() const
{
    notImplementedGdk();
    return true;
}

void ScrollView::wheelEvent(PlatformWheelEvent&)
{
    notImplementedGdk();
}

void ScrollView::updateScrollbars()
{
    notImplementedGdk();
}

int ScrollView::updateScrollInfo(short type, int current, int max, int pageSize)
{ 
    notImplementedGdk();
    return 0;
}

IntPoint ScrollView::windowToContents(const IntPoint& point) const
{ 
    notImplementedGdk();
    return point;
}

IntPoint ScrollView::contentsToWindow(const IntPoint& point) const
{
    notImplementedGdk();
    return point;
}

PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent)
{ 
    return 0;
    notImplementedGdk();
}

}
