/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2008, 2011, 2014 Apple Inc. All Rights Reserved.
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
#include "LayoutRect.h"
#include "PlatformWheelEvent.h"
#include "ScrollAnimator.h"
#include "ScrollbarTheme.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

struct SameSizeAsScrollableArea {
    virtual ~SameSizeAsScrollableArea();
    void* pointer;
    IntPoint origin;
    unsigned bitfields : 16;
};

COMPILE_ASSERT(sizeof(ScrollableArea) == sizeof(SameSizeAsScrollableArea), ScrollableArea_should_stay_small);

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
    case ScrollByPrecisePixel:
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
        scrollToOffsetWithoutAnimation(FloatPoint(offset, scrollAnimator()->currentPosition().y()));
    else
        scrollToOffsetWithoutAnimation(FloatPoint(scrollAnimator()->currentPosition().x(), offset));
}

void ScrollableArea::notifyScrollPositionChanged(const IntPoint& position)
{
    scrollPositionChanged(position);
    scrollAnimator()->setCurrentPosition(position);
}

void ScrollableArea::scrollPositionChanged(const IntPoint& position)
{
    IntPoint oldPosition = scrollPosition();
    // Tell the derived class to scroll its contents.
    setScrollOffset(position);

    Scrollbar* verticalScrollbar = this->verticalScrollbar();

    // Tell the scrollbars to update their thumb postions.
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
        horizontalScrollbar->offsetDidChange();
        if (horizontalScrollbar->isOverlayScrollbar() && !hasLayerForHorizontalScrollbar()) {
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
        if (verticalScrollbar->isOverlayScrollbar() && !hasLayerForVerticalScrollbar())
            verticalScrollbar->invalidate();
    }

    if (scrollPosition() != oldPosition)
        scrollAnimator()->notifyContentAreaScrolled(scrollPosition() - oldPosition);
}

bool ScrollableArea::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    return scrollAnimator()->handleWheelEvent(wheelEvent);
}

#if ENABLE(TOUCH_EVENTS)
bool ScrollableArea::handleTouchEvent(const PlatformTouchEvent& touchEvent)
{
    return scrollAnimator()->handleTouchEvent(touchEvent);
}
#endif

// NOTE: Only called from Internals for testing.
void ScrollableArea::setScrollOffsetFromInternals(const IntPoint& offset)
{
    setScrollOffsetFromAnimation(offset);
}

void ScrollableArea::setScrollOffsetFromAnimation(const IntPoint& offset)
{
    if (requestScrollPositionUpdate(offset))
        return;

    scrollPositionChanged(offset);
}

void ScrollableArea::willStartLiveResize()
{
    if (m_inLiveResize)
        return;
    m_inLiveResize = true;
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->willStartLiveResize();
}

void ScrollableArea::willEndLiveResize()
{
    if (!m_inLiveResize)
        return;
    m_inLiveResize = false;
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->willEndLiveResize();
}    

void ScrollableArea::contentAreaWillPaint() const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentAreaWillPaint();
}

void ScrollableArea::mouseEnteredContentArea() const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->mouseEnteredContentArea();
}

void ScrollableArea::mouseExitedContentArea() const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->mouseEnteredContentArea();
}

void ScrollableArea::mouseMovedInContentArea() const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->mouseMovedInContentArea();
}

void ScrollableArea::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    scrollAnimator()->mouseEnteredScrollbar(scrollbar);
}

void ScrollableArea::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    scrollAnimator()->mouseExitedScrollbar(scrollbar);
}

void ScrollableArea::contentAreaDidShow() const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentAreaDidShow();
}

void ScrollableArea::contentAreaDidHide() const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentAreaDidHide();
}

void ScrollableArea::lockOverlayScrollbarStateToHidden(bool shouldLockState) const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->lockOverlayScrollbarStateToHidden(shouldLockState);
}

bool ScrollableArea::scrollbarsCanBeActive() const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->scrollbarsCanBeActive();
    return true;
}

void ScrollableArea::didAddScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    if (orientation == VerticalScrollbar)
        scrollAnimator()->didAddVerticalScrollbar(scrollbar);
    else
        scrollAnimator()->didAddHorizontalScrollbar(scrollbar);

    // <rdar://problem/9797253> AppKit resets the scrollbar's style when you attach a scrollbar
    setScrollbarOverlayStyle(scrollbarOverlayStyle());
}

void ScrollableArea::willRemoveScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    if (orientation == VerticalScrollbar)
        scrollAnimator()->willRemoveVerticalScrollbar(scrollbar);
    else
        scrollAnimator()->willRemoveHorizontalScrollbar(scrollbar);
}

void ScrollableArea::contentsResized()
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentsResized();
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

void ScrollableArea::invalidateScrollbar(Scrollbar* scrollbar, const IntRect& rect)
{
    if (scrollbar == horizontalScrollbar()) {
        if (GraphicsLayer* graphicsLayer = layerForHorizontalScrollbar()) {
            graphicsLayer->setNeedsDisplay();
            graphicsLayer->setContentsNeedsDisplay();
            return;
        }
    } else if (scrollbar == verticalScrollbar()) {
        if (GraphicsLayer* graphicsLayer = layerForVerticalScrollbar()) {
            graphicsLayer->setNeedsDisplay();
            graphicsLayer->setContentsNeedsDisplay();
            return;
        }
    }

    invalidateScrollbarRect(scrollbar, rect);
}

void ScrollableArea::invalidateScrollCorner(const IntRect& rect)
{
    if (GraphicsLayer* graphicsLayer = layerForScrollCorner()) {
        graphicsLayer->setNeedsDisplay();
        return;
    }

    invalidateScrollCornerRect(rect);
}

void ScrollableArea::verticalScrollbarLayerDidChange()
{
    scrollAnimator()->verticalScrollbarLayerDidChange();
}

void ScrollableArea::horizontalScrollbarLayerDidChange()
{
    scrollAnimator()->horizontalScrollbarLayerDidChange();
}

bool ScrollableArea::hasLayerForHorizontalScrollbar() const
{
    return layerForHorizontalScrollbar();
}

bool ScrollableArea::hasLayerForVerticalScrollbar() const
{
    return layerForVerticalScrollbar();
}

bool ScrollableArea::hasLayerForScrollCorner() const
{
    return layerForScrollCorner();
}

void ScrollableArea::serviceScrollAnimations()
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->serviceScrollAnimations();
}

#if PLATFORM(IOS)
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
#endif // PLATFORM(IOS)

IntPoint ScrollableArea::scrollPosition() const
{
    int x = horizontalScrollbar() ? horizontalScrollbar()->value() : 0;
    int y = verticalScrollbar() ? verticalScrollbar()->value() : 0;
    return IntPoint(x, y);
}

IntPoint ScrollableArea::minimumScrollPosition() const
{
    return IntPoint();
}

IntPoint ScrollableArea::maximumScrollPosition() const
{
    return IntPoint(totalContentsSize().width() - visibleWidth(), totalContentsSize().height() - visibleHeight());
}

bool ScrollableArea::scrolledToTop() const
{
    return scrollPosition().y() <= minimumScrollPosition().y();
}

bool ScrollableArea::scrolledToBottom() const
{
    return scrollPosition().y() >= maximumScrollPosition().y();
}

bool ScrollableArea::scrolledToLeft() const
{
    return scrollPosition().x() <= minimumScrollPosition().x();
}

bool ScrollableArea::scrolledToRight() const
{
    return scrollPosition().x() >= maximumScrollPosition().x();
}

IntSize ScrollableArea::totalContentsSize() const
{
    IntSize totalContentsSize = contentsSize();
    totalContentsSize.setHeight(totalContentsSize.height() + headerHeight() + footerHeight());
    return totalContentsSize;
}

IntRect ScrollableArea::visibleContentRect(VisibleContentRectBehavior visibleContentRectBehavior) const
{
    return visibleContentRectInternal(ExcludeScrollbars, visibleContentRectBehavior);
}

IntRect ScrollableArea::visibleContentRectIncludingScrollbars(VisibleContentRectBehavior visibleContentRectBehavior) const
{
    return visibleContentRectInternal(IncludeScrollbars, visibleContentRectBehavior);
}

IntRect ScrollableArea::visibleContentRectInternal(VisibleContentRectIncludesScrollbars scrollbarInclusion, VisibleContentRectBehavior) const
{
    int verticalScrollbarWidth = 0;
    int horizontalScrollbarHeight = 0;

    if (scrollbarInclusion == IncludeScrollbars) {
        if (Scrollbar* verticalBar = verticalScrollbar())
            verticalScrollbarWidth = !verticalBar->isOverlayScrollbar() ? verticalBar->width() : 0;
        if (Scrollbar* horizontalBar = horizontalScrollbar())
            horizontalScrollbarHeight = !horizontalBar->isOverlayScrollbar() ? horizontalBar->height() : 0;
    }

    return IntRect(scrollPosition().x(),
                   scrollPosition().y(),
                   std::max(0, visibleWidth() + verticalScrollbarWidth),
                   std::max(0, visibleHeight() + horizontalScrollbarHeight));
}

LayoutPoint ScrollableArea::constrainScrollPositionForOverhang(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize, const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, int headerHeight, int footerHeight)
{
    // The viewport rect that we're scrolling shouldn't be larger than our document.
    LayoutSize idealScrollRectSize(std::min(visibleContentRect.width(), totalContentsSize.width()), std::min(visibleContentRect.height(), totalContentsSize.height()));
    
    LayoutRect scrollRect(scrollPosition + scrollOrigin - LayoutSize(0, headerHeight), idealScrollRectSize);
    LayoutRect documentRect(LayoutPoint(), LayoutSize(totalContentsSize.width(), totalContentsSize.height() - headerHeight - footerHeight));

    // Use intersection to constrain our ideal scroll rect by the document rect.
    scrollRect.intersect(documentRect);

    if (scrollRect.size() != idealScrollRectSize) {
        // If the rect was clipped, restore its size, effectively pushing it "down" from the top left.
        scrollRect.setSize(idealScrollRectSize);

        // If we still clip, push our rect "up" from the bottom right.
        scrollRect.intersect(documentRect);
        if (scrollRect.width() < idealScrollRectSize.width())
            scrollRect.move(-(idealScrollRectSize.width() - scrollRect.width()), 0);
        if (scrollRect.height() < idealScrollRectSize.height())
            scrollRect.move(0, -(idealScrollRectSize.height() - scrollRect.height()));
    }

    return scrollRect.location() - toLayoutSize(scrollOrigin);
}

LayoutPoint ScrollableArea::constrainScrollPositionForOverhang(const LayoutPoint& scrollPosition)
{
    return constrainScrollPositionForOverhang(visibleContentRect(), totalContentsSize(), scrollPosition, scrollOrigin(), headerHeight(), footerHeight());
}

void ScrollableArea::computeScrollbarValueAndOverhang(float currentPosition, float totalSize, float visibleSize, float& doubleValue, float& overhangAmount)
{
    doubleValue = 0;
    overhangAmount = 0;
    float maximum = totalSize - visibleSize;

    if (currentPosition < 0) {
        // Scrolled past the top.
        doubleValue = 0;
        overhangAmount = -currentPosition;
    } else if (visibleSize + currentPosition > totalSize) {
        // Scrolled past the bottom.
        doubleValue = 1;
        overhangAmount = currentPosition + visibleSize - totalSize;
    } else {
        // Within the bounds of the scrollable area.
        if (maximum > 0)
            doubleValue = currentPosition / maximum;
        else
            doubleValue = 0;
    }
}

} // namespace WebCore
