/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2008, 2011, 2014-2016 Apple Inc. All Rights Reserved.
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

#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "LayoutRect.h"
#include "Logging.h"
#include "PlatformWheelEvent.h"
#include "ScrollAnimator.h"
#include "ScrollAnimatorMock.h"
#include "ScrollbarTheme.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

struct SameSizeAsScrollableArea {
    virtual ~SameSizeAsScrollableArea();
#if !ASSERT_DISABLED
    bool weakPtrFactorWasConstructedOnMainThread;
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    void* pointers[3];
    unsigned currentIndices[2];
#else
    void* pointer[2];
#endif
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
    , m_currentScrollType(static_cast<unsigned>(ScrollType::User))
    , m_scrollShouldClearLatchedState(false)
{
}

ScrollableArea::~ScrollableArea() = default;

ScrollAnimator& ScrollableArea::scrollAnimator() const
{
    if (!m_scrollAnimator) {
        if (usesMockScrollAnimator()) {
            m_scrollAnimator = makeUnique<ScrollAnimatorMock>(const_cast<ScrollableArea&>(*this), [this](const String& message) {
                logMockScrollAnimatorMessage(message);
            });
        } else
            m_scrollAnimator = ScrollAnimator::create(const_cast<ScrollableArea&>(*this));
    }

    ASSERT(m_scrollAnimator);
    return *m_scrollAnimator.get();
}

void ScrollableArea::setScrollOrigin(const IntPoint& origin)
{
    if (m_scrollOrigin != origin) {
        m_scrollOrigin = origin;
        m_scrollOriginChanged = true;
    }
}

float ScrollableArea::adjustScrollStepForFixedContent(float step, ScrollbarOrientation, ScrollGranularity)
{
    return step;
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

    step = adjustScrollStepForFixedContent(step, orientation, granularity);
    return scrollAnimator().scroll(orientation, granularity, step, multiplier);
}

void ScrollableArea::scrollToOffsetWithoutAnimation(const FloatPoint& offset, ScrollClamping clamping)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollableArea " << this << " scrollToOffsetWithoutAnimation " << offset);
    scrollAnimator().scrollToOffsetWithoutAnimation(offset, clamping);
}

void ScrollableArea::scrollToOffsetWithoutAnimation(ScrollbarOrientation orientation, float offset)
{
    auto currentOffset = scrollOffsetFromPosition(IntPoint(scrollAnimator().currentPosition()));
    if (orientation == HorizontalScrollbar)
        scrollToOffsetWithoutAnimation(FloatPoint(offset, currentOffset.y()));
    else
        scrollToOffsetWithoutAnimation(FloatPoint(currentOffset.x(), offset));
}

void ScrollableArea::notifyScrollPositionChanged(const ScrollPosition& position)
{
    scrollPositionChanged(position);
    scrollAnimator().setCurrentPosition(position);
}

void ScrollableArea::scrollPositionChanged(const ScrollPosition& position)
{
    IntPoint oldPosition = scrollPosition();
    // Tell the derived class to scroll its contents.
    setScrollOffset(scrollOffsetFromPosition(position));

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
        scrollAnimator().notifyContentAreaScrolled(scrollPosition() - oldPosition);
}

bool ScrollableArea::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!isScrollableOrRubberbandable())
        return false;

    bool handledEvent = scrollAnimator().handleWheelEvent(wheelEvent);
#if ENABLE(CSS_SCROLL_SNAP)
    if (scrollAnimator().activeScrollSnapIndexDidChange()) {
        setCurrentHorizontalSnapPointIndex(scrollAnimator().activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal));
        setCurrentVerticalSnapPointIndex(scrollAnimator().activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical));
    }
#endif
    return handledEvent;
}

#if ENABLE(TOUCH_EVENTS)
bool ScrollableArea::handleTouchEvent(const PlatformTouchEvent& touchEvent)
{
    return scrollAnimator().handleTouchEvent(touchEvent);
}
#endif

// NOTE: Only called from Internals for testing.
void ScrollableArea::setScrollOffsetFromInternals(const ScrollOffset& offset)
{
    setScrollOffsetFromAnimation(offset);
}

void ScrollableArea::setScrollOffsetFromAnimation(const ScrollOffset& offset)
{
    ScrollPosition position = scrollPositionFromOffset(offset);
    if (requestScrollPositionUpdate(position))
        return;

    scrollPositionChanged(position);
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
        scrollAnimator->mouseExitedContentArea();
}

void ScrollableArea::mouseMovedInContentArea() const
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->mouseMovedInContentArea();
}

void ScrollableArea::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    scrollAnimator().mouseEnteredScrollbar(scrollbar);
}

void ScrollableArea::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    scrollAnimator().mouseExitedScrollbar(scrollbar);
}

void ScrollableArea::mouseIsDownInScrollbar(Scrollbar* scrollbar, bool mouseIsDown) const
{
    scrollAnimator().mouseIsDownInScrollbar(scrollbar, mouseIsDown);
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
        scrollAnimator().didAddVerticalScrollbar(scrollbar);
    else
        scrollAnimator().didAddHorizontalScrollbar(scrollbar);

    // <rdar://problem/9797253> AppKit resets the scrollbar's style when you attach a scrollbar
    setScrollbarOverlayStyle(scrollbarOverlayStyle());
}

void ScrollableArea::willRemoveScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    if (orientation == VerticalScrollbar)
        scrollAnimator().willRemoveVerticalScrollbar(scrollbar);
    else
        scrollAnimator().willRemoveHorizontalScrollbar(scrollbar);
}

void ScrollableArea::contentsResized()
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentsResized();
}

void ScrollableArea::availableContentSizeChanged(AvailableSizeChangeReason)
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentsResized(); // This flashes overlay scrollbars.
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
        ScrollbarTheme::theme().updateScrollbarOverlayStyle(*horizontalScrollbar());
        horizontalScrollbar()->invalidate();
        if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
            scrollAnimator->invalidateScrollbarPartLayers(horizontalScrollbar());
    }
    
    if (verticalScrollbar()) {
        ScrollbarTheme::theme().updateScrollbarOverlayStyle(*verticalScrollbar());
        verticalScrollbar()->invalidate();
        if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
            scrollAnimator->invalidateScrollbarPartLayers(verticalScrollbar());
    }
}

bool ScrollableArea::useDarkAppearanceForScrollbars() const
{
    // If dark appearance is used or the overlay style is light (because of a dark page background), set the dark appearance.
    return useDarkAppearance() || scrollbarOverlayStyle() == WebCore::ScrollbarOverlayStyleLight;
}

void ScrollableArea::invalidateScrollbar(Scrollbar& scrollbar, const IntRect& rect)
{
    if (&scrollbar == horizontalScrollbar()) {
        if (GraphicsLayer* graphicsLayer = layerForHorizontalScrollbar()) {
            graphicsLayer->setNeedsDisplay();
            graphicsLayer->setContentsNeedsDisplay();
            return;
        }
    } else if (&scrollbar == verticalScrollbar()) {
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
    scrollAnimator().verticalScrollbarLayerDidChange();
}

void ScrollableArea::horizontalScrollbarLayerDidChange()
{
    scrollAnimator().horizontalScrollbarLayerDidChange();
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

#if ENABLE(CSS_SCROLL_SNAP)
ScrollSnapOffsetsInfo<LayoutUnit>& ScrollableArea::ensureSnapOffsetsInfo()
{
    if (!m_snapOffsetsInfo)
        m_snapOffsetsInfo = makeUnique<ScrollSnapOffsetsInfo<LayoutUnit>>();
    return *m_snapOffsetsInfo;
}

const Vector<LayoutUnit>* ScrollableArea::horizontalSnapOffsets() const
{
    if (!m_snapOffsetsInfo)
        return nullptr;

    return &m_snapOffsetsInfo->horizontalSnapOffsets;
}

const Vector<ScrollOffsetRange<LayoutUnit>>* ScrollableArea::horizontalSnapOffsetRanges() const
{
    if (!m_snapOffsetsInfo)
        return nullptr;

    return &m_snapOffsetsInfo->horizontalSnapOffsetRanges;
}

const Vector<ScrollOffsetRange<LayoutUnit>>* ScrollableArea::verticalSnapOffsetRanges() const
{
    if (!m_snapOffsetsInfo)
        return nullptr;

    return &m_snapOffsetsInfo->verticalSnapOffsetRanges;
}

const Vector<LayoutUnit>* ScrollableArea::verticalSnapOffsets() const
{
    if (!m_snapOffsetsInfo)
        return nullptr;

    return &m_snapOffsetsInfo->verticalSnapOffsets;
}

void ScrollableArea::setHorizontalSnapOffsets(const Vector<LayoutUnit>& horizontalSnapOffsets)
{
    // Consider having a non-empty set of snap offsets as a cue to initialize the ScrollAnimator.
    if (horizontalSnapOffsets.size())
        scrollAnimator();

    ensureSnapOffsetsInfo().horizontalSnapOffsets = horizontalSnapOffsets;
}

void ScrollableArea::setVerticalSnapOffsets(const Vector<LayoutUnit>& verticalSnapOffsets)
{
    // Consider having a non-empty set of snap offsets as a cue to initialize the ScrollAnimator.
    if (verticalSnapOffsets.size())
        scrollAnimator();

    ensureSnapOffsetsInfo().verticalSnapOffsets = verticalSnapOffsets;
}

void ScrollableArea::setHorizontalSnapOffsetRanges(const Vector<ScrollOffsetRange<LayoutUnit>>& horizontalRanges)
{
    ensureSnapOffsetsInfo().horizontalSnapOffsetRanges = horizontalRanges;
}

void ScrollableArea::setVerticalSnapOffsetRanges(const Vector<ScrollOffsetRange<LayoutUnit>>& verticalRanges)
{
    ensureSnapOffsetsInfo().verticalSnapOffsetRanges = verticalRanges;
}

void ScrollableArea::clearHorizontalSnapOffsets()
{
    if (!m_snapOffsetsInfo)
        return;

    m_snapOffsetsInfo->horizontalSnapOffsets = { };
    m_snapOffsetsInfo->horizontalSnapOffsetRanges = { };
    m_currentHorizontalSnapPointIndex = 0;
}

void ScrollableArea::clearVerticalSnapOffsets()
{
    if (!m_snapOffsetsInfo)
        return;

    m_snapOffsetsInfo->verticalSnapOffsets = { };
    m_snapOffsetsInfo->verticalSnapOffsetRanges = { };
    m_currentVerticalSnapPointIndex = 0;
}

IntPoint ScrollableArea::nearestActiveSnapPoint(const IntPoint& currentPosition)
{
    if (!horizontalSnapOffsets() && !verticalSnapOffsets())
        return currentPosition;
    
    if (!existingScrollAnimator())
        return currentPosition;
    
    IntPoint correctedPosition = currentPosition;
    
    if (horizontalSnapOffsets()) {
        const auto& horizontal = *horizontalSnapOffsets();
        
        size_t activeIndex = currentHorizontalSnapPointIndex();
        if (activeIndex < horizontal.size())
            correctedPosition.setX(horizontal[activeIndex].toInt());
    }
    
    if (verticalSnapOffsets()) {
        const auto& vertical = *verticalSnapOffsets();
        
        size_t activeIndex = currentVerticalSnapPointIndex();
        if (activeIndex < vertical.size())
            correctedPosition.setY(vertical[activeIndex].toInt());
    }

    return correctedPosition;
}

void ScrollableArea::updateScrollSnapState()
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->updateScrollSnapState();

    if (isScrollSnapInProgress())
        return;

    IntPoint currentPosition = scrollPosition();
    IntPoint correctedPosition = nearestActiveSnapPoint(currentPosition);
    
    if (correctedPosition != currentPosition)
        scrollToOffsetWithoutAnimation(correctedPosition);
}
#else
void ScrollableArea::updateScrollSnapState()
{
}
#endif


void ScrollableArea::serviceScrollAnimations()
{
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->serviceScrollAnimations();
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

int ScrollableArea::horizontalScrollbarIntrusion() const
{
    return verticalScrollbar() ? verticalScrollbar()->occupiedWidth() : 0;
}

int ScrollableArea::verticalScrollbarIntrusion() const
{
    return horizontalScrollbar() ? horizontalScrollbar()->occupiedHeight() : 0;
}

IntSize ScrollableArea::scrollbarIntrusion() const
{
    return { horizontalScrollbarIntrusion(), verticalScrollbarIntrusion() };
}

ScrollOffset ScrollableArea::scrollOffset() const
{
    return scrollOffsetFromPosition(scrollPosition());
}

ScrollPosition ScrollableArea::minimumScrollPosition() const
{
    return scrollPositionFromOffset(ScrollPosition());
}

ScrollPosition ScrollableArea::maximumScrollPosition() const
{
    return scrollPositionFromOffset(ScrollPosition(totalContentsSize() - visibleSize()));
}

ScrollOffset ScrollableArea::maximumScrollOffset() const
{
    return ScrollOffset(totalContentsSize() - visibleSize());
}

ScrollPosition ScrollableArea::scrollPositionFromOffset(ScrollOffset offset) const
{
    return scrollPositionFromOffset(offset, toIntSize(m_scrollOrigin));
}

ScrollOffset ScrollableArea::scrollOffsetFromPosition(ScrollPosition position) const
{
    return scrollOffsetFromPosition(position, toIntSize(m_scrollOrigin));
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

void ScrollableArea::scrollbarStyleChanged(ScrollbarStyle, bool)
{
    availableContentSizeChanged(AvailableSizeChangeReason::ScrollbarsChanged);
}

IntSize ScrollableArea::reachableTotalContentsSize() const
{
    return totalContentsSize();
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
            verticalScrollbarWidth = verticalBar->occupiedWidth();
        if (Scrollbar* horizontalBar = horizontalScrollbar())
            horizontalScrollbarHeight = horizontalBar->occupiedHeight();
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
            scrollRect.move(-(idealScrollRectSize.width() - scrollRect.width()), 0_lu);
        if (scrollRect.height() < idealScrollRectSize.height())
            scrollRect.move(0_lu, -(idealScrollRectSize.height() - scrollRect.height()));
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
