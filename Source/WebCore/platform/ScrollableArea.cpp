/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollableArea.h"

#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "FloatPoint.h"
#include "PlatformWheelEvent.h"
#include "ScrollAnimator.h"
#include "ScrollbarTheme.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

ScrollableArea::ScrollableArea()
    : m_constrainsScrollingToContentEdge(true)
    , m_inLiveResize(false)
    , m_verticalScrollElasticity(ScrollElasticityNone)
    , m_horizontalScrollElasticity(ScrollElasticityNone)
    , m_scrollbarOverlayStyle(ScrollbarOverlayStyleDefault)
    , m_scrollOriginChanged(false)
{
}

ScrollableArea::~ScrollableArea()
{
}

ScrollAnimator* ScrollableArea::scrollAnimator() const
{
    if (!m_scrollAnimator)
        m_scrollAnimator = ScrollAnimator::create(const_cast<ScrollableArea*>(this));

    return m_scrollAnimator.get();
}

void ScrollableArea::setScrollOrigin(const IntPoint& origin)
{
    if (m_scrollOrigin != origin) {
        m_scrollOrigin = origin;
        m_scrollOriginChanged = true;
    }
}
 
void ScrollableArea::setScrollOriginX(int x)
{
    if (m_scrollOrigin.x() != x) {
        m_scrollOrigin.setX(x);
        m_scrollOriginChanged = true;
    }
}

void ScrollableArea::setScrollOriginY(int y)
{
    if (m_scrollOrigin.y() != y) {
        m_scrollOrigin.setY(y);
        m_scrollOriginChanged = true;
    }
}

bool ScrollableArea::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    ScrollbarOrientation orientation;
    Scrollbar* scrollbar;
    if (direction == ScrollUp || direction == ScrollDown) {
        orientation = VerticalScrollbar;
        scrollbar = verticalScrollbar();
    } else {
        orientation = HorizontalScrollbar;
        scrollbar = horizontalScrollbar();
    }

    if (!scrollbar)
        return false;

    float step = 0;
    switch (granularity) {
    case ScrollByLine:
        step = scrollbar->lineStep();
        break;
    case ScrollByPage:
        step = scrollbar->pageStep();
        break;
    case ScrollByDocument:
        step = scrollbar->totalSize();
        break;
    case ScrollByPixel:
        step = scrollbar->pixelStep();
        break;
    }

    if (direction == ScrollUp || direction == ScrollLeft)
        multiplier = -multiplier;

    return scrollAnimator()->scroll(orientation, granularity, step, multiplier);
}

void ScrollableArea::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    scrollAnimator()->scrollToOffsetWithoutAnimation(offset);
}

void ScrollableArea::scrollToOffsetWithoutAnimation(ScrollbarOrientation orientation, float offset)
{
    if (orientation == HorizontalScrollbar)
        scrollToXOffsetWithoutAnimation(offset);
    else
        scrollToYOffsetWithoutAnimation(offset);
}

void ScrollableArea::scrollToXOffsetWithoutAnimation(float x)
{
    scrollToOffsetWithoutAnimation(FloatPoint(x, scrollAnimator()->currentPosition().y()));
}

void ScrollableArea::scrollToYOffsetWithoutAnimation(float y)
{
    scrollToOffsetWithoutAnimation(FloatPoint(scrollAnimator()->currentPosition().x(), y));
}

void ScrollableArea::zoomAnimatorTransformChanged(float, float, float, ZoomAnimationState)
{
    // Requires FrameView to override this.
}

bool ScrollableArea::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    return scrollAnimator()->handleWheelEvent(wheelEvent);
}

#if ENABLE(GESTURE_EVENTS)
void ScrollableArea::handleGestureEvent(const PlatformGestureEvent& gestureEvent)
{
    scrollAnimator()->handleGestureEvent(gestureEvent);
}
#endif

// NOTE: Only called from Internals for testing.
void ScrollableArea::setScrollOffsetFromInternals(const IntPoint& offset)
{
    setScrollOffsetFromAnimation(offset);
}

void ScrollableArea::setScrollOffsetFromAnimation(const IntPoint& offset)
{
    // Tell the derived class to scroll its contents.
    setScrollOffset(offset);

    Scrollbar* verticalScrollbar = this->verticalScrollbar();

    // Tell the scrollbars to update their thumb postions.
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
        horizontalScrollbar->offsetDidChange();
        if (horizontalScrollbar->isOverlayScrollbar()) {
            if (!verticalScrollbar)
                horizontalScrollbar->invalidate();
            else {
                // If there is both a horizontalScrollbar and a verticalScrollbar,
                // then we must also invalidate the corner between them.
                IntRect boundsAndCorner = horizontalScrollbar->boundsRect();
                boundsAndCorner.setWidth(boundsAndCorner.width() + verticalScrollbar->width());
                horizontalScrollbar->invalidateRect(boundsAndCorner);
            }
        }
    }
    if (verticalScrollbar) {
        verticalScrollbar->offsetDidChange();
        if (verticalScrollbar->isOverlayScrollbar())
            verticalScrollbar->invalidate();
    }
}

void ScrollableArea::willStartLiveResize()
{
    if (m_inLiveResize)
        return;
    m_inLiveResize = true;
    scrollAnimator()->willStartLiveResize();
}

void ScrollableArea::willEndLiveResize()
{
    if (!m_inLiveResize)
        return;
    m_inLiveResize = false;
    scrollAnimator()->willEndLiveResize();
}    

void ScrollableArea::didAddVerticalScrollbar(Scrollbar* scrollbar)
{
    scrollAnimator()->didAddVerticalScrollbar(scrollbar);

    // <rdar://problem/9797253> AppKit resets the scrollbar's style when you attach a scrollbar
    setScrollbarOverlayStyle(scrollbarOverlayStyle());
}

void ScrollableArea::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
{
    scrollAnimator()->willRemoveVerticalScrollbar(scrollbar);
}

void ScrollableArea::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
    scrollAnimator()->didAddHorizontalScrollbar(scrollbar);

    // <rdar://problem/9797253> AppKit resets the scrollbar's style when you attach a scrollbar
    setScrollbarOverlayStyle(scrollbarOverlayStyle());
}

void ScrollableArea::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    scrollAnimator()->willRemoveHorizontalScrollbar(scrollbar);
}

bool ScrollableArea::hasOverlayScrollbars() const
{
    return (verticalScrollbar() && verticalScrollbar()->isOverlayScrollbar())
        || (horizontalScrollbar() && horizontalScrollbar()->isOverlayScrollbar());
}

void ScrollableArea::setScrollbarOverlayStyle(ScrollbarOverlayStyle overlayStyle)
{
    m_scrollbarOverlayStyle = overlayStyle;

    if (horizontalScrollbar()) {
        ScrollbarTheme::theme()->updateScrollbarOverlayStyle(horizontalScrollbar());
        horizontalScrollbar()->invalidate();
    }
    
    if (verticalScrollbar()) {
        ScrollbarTheme::theme()->updateScrollbarOverlayStyle(verticalScrollbar());
        verticalScrollbar()->invalidate();
    }
}

bool ScrollableArea::isPinnedInBothDirections(const IntSize& scrollDelta) const
{
    return isPinnedHorizontallyInDirection(scrollDelta.width()) && isPinnedVerticallyInDirection(scrollDelta.height());
}

bool ScrollableArea::isPinnedHorizontallyInDirection(int horizontalScrollDelta) const
{
    if (horizontalScrollDelta < 0 && isHorizontalScrollerPinnedToMinimumPosition())
        return true;
    if (horizontalScrollDelta > 0 && isHorizontalScrollerPinnedToMaximumPosition())
        return true;
    return false;
}

bool ScrollableArea::isPinnedVerticallyInDirection(int verticalScrollDelta) const
{
    if (verticalScrollDelta < 0 && isVerticalScrollerPinnedToMinimumPosition())
        return true;
    if (verticalScrollDelta > 0 && isVerticalScrollerPinnedToMaximumPosition())
        return true;
    return false;
}

void ScrollableArea::invalidateScrollbar(Scrollbar* scrollbar, const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING)
    if (scrollbar == horizontalScrollbar()) {
        if (GraphicsLayer* graphicsLayer = layerForHorizontalScrollbar()) {
            graphicsLayer->setNeedsDisplay();
            return;
        }
    } else if (scrollbar == verticalScrollbar()) {
        if (GraphicsLayer* graphicsLayer = layerForVerticalScrollbar()) {
            graphicsLayer->setNeedsDisplay();
            return;
        }
    }
#endif
    invalidateScrollbarRect(scrollbar, rect);
}

void ScrollableArea::invalidateScrollCorner(const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING)
    if (GraphicsLayer* graphicsLayer = layerForScrollCorner()) {
        graphicsLayer->setNeedsDisplay();
        return;
    }
#endif
    invalidateScrollCornerRect(rect);
}

bool ScrollableArea::hasLayerForHorizontalScrollbar() const
{
#if USE(ACCELERATED_COMPOSITING)
    return layerForHorizontalScrollbar();
#else
    return false;
#endif
}

bool ScrollableArea::hasLayerForVerticalScrollbar() const
{
#if USE(ACCELERATED_COMPOSITING)
    return layerForVerticalScrollbar();
#else
    return false;
#endif
}

bool ScrollableArea::hasLayerForScrollCorner() const
{
#if USE(ACCELERATED_COMPOSITING)
    return layerForScrollCorner();
#else
    return false;
#endif
}

} // namespace WebCore
