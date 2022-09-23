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
#include "ScrollbarTheme.h"
#include "ScrollbarsControllerMock.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

struct SameSizeAsScrollableArea {
    virtual ~SameSizeAsScrollableArea();
#if ASSERT_ENABLED
    bool weakPtrFactorWasConstructedOnMainThread;
#endif
    void* pointer[3];
    IntPoint origin;
    bool bytes[9];
};

#if CPU(ADDRESS64)
static_assert(sizeof(ScrollableArea) == sizeof(SameSizeAsScrollableArea), "ScrollableArea should stay small");
#endif

ScrollableArea::ScrollableArea() = default;
ScrollableArea::~ScrollableArea() = default;

ScrollAnimator& ScrollableArea::scrollAnimator() const
{
    if (!m_scrollAnimator)
        m_scrollAnimator = ScrollAnimator::create(const_cast<ScrollableArea&>(*this));

    return *m_scrollAnimator.get();
}

ScrollbarsController& ScrollableArea::scrollbarsController() const
{
    if (!m_scrollbarsController) {
        if (mockScrollbarsControllerEnabled()) {
            m_scrollbarsController = makeUnique<ScrollbarsControllerMock>(const_cast<ScrollableArea&>(*this), [this](const String& message) {
                logMockScrollbarsControllerMessage(message);
            });
        } else
            m_scrollbarsController = ScrollbarsController::create(const_cast<ScrollableArea&>(*this));
    }

    return *m_scrollbarsController.get();
}

void ScrollableArea::setScrollOrigin(const IntPoint& origin)
{
    if (m_scrollOrigin != origin) {
        m_scrollOrigin = origin;
        m_scrollOriginChanged = true;
    }
}

float ScrollableArea::adjustVerticalPageScrollStepForFixedContent(float step)
{
    return step;
}

bool ScrollableArea::scroll(ScrollDirection direction, ScrollGranularity granularity, unsigned stepCount)
{
    auto* scrollbar = scrollbarForDirection(direction);
    if (!scrollbar)
        return false;

    float step = 0;
    switch (granularity) {
    case ScrollGranularity::Line:
        step = scrollbar->lineStep();
        break;
    case ScrollGranularity::Page:
        step = scrollbar->pageStep();
        break;
    case ScrollGranularity::Document:
        step = scrollbar->totalSize();
        break;
    case ScrollGranularity::Pixel:
        step = scrollbar->pixelStep();
        break;
    }

    auto axis = axisFromDirection(direction);

    if (granularity == ScrollGranularity::Page && axis == ScrollEventAxis::Vertical)
        step = adjustVerticalPageScrollStepForFixedContent(step);

    auto scrollDelta = step * stepCount;
    
    if (direction == ScrollUp || direction == ScrollLeft)
        scrollDelta = -scrollDelta;

    return scrollAnimator().singleAxisScroll(axis, scrollDelta, ScrollAnimator::ScrollBehavior::RespectScrollSnap);
}

void ScrollableArea::scrollToPositionWithoutAnimation(const FloatPoint& position, ScrollClamping clamping)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollableArea " << this << " scrollToPositionWithoutAnimation " << position);
    scrollAnimator().scrollToPositionWithoutAnimation(position, clamping);
}

void ScrollableArea::scrollToPositionWithAnimation(const FloatPoint& position, ScrollClamping clamping)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollableArea " << this << " scrollToPositionWithAnimation " << position);

    bool startedAnimation = requestAnimatedScrollToPosition(roundedIntPoint(position), clamping);
    if (!startedAnimation)
        startedAnimation = scrollAnimator().scrollToPositionWithAnimation(position, clamping);

    if (startedAnimation)
        setScrollAnimationStatus(ScrollAnimationStatus::Animating);
}

void ScrollableArea::scrollToOffsetWithoutAnimation(const FloatPoint& offset, ScrollClamping clamping)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollableArea " << this << " scrollToOffsetWithoutAnimation " << offset);

    auto position = scrollPositionFromOffset(offset, toFloatSize(scrollOrigin()));
    scrollAnimator().scrollToPositionWithoutAnimation(position, clamping);
}

void ScrollableArea::scrollToOffsetWithoutAnimation(ScrollbarOrientation orientation, float offset)
{
    auto currentPosition = scrollAnimator().currentPosition();
    if (orientation == ScrollbarOrientation::Horizontal)
        scrollAnimator().scrollToPositionWithoutAnimation(FloatPoint(offset, currentPosition.y()));
    else
        scrollAnimator().scrollToPositionWithoutAnimation(FloatPoint(currentPosition.x(), offset));
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

    auto* verticalScrollbar = this->verticalScrollbar();

    // Tell the scrollbars to update their thumb postions.
    if (auto* horizontalScrollbar = this->horizontalScrollbar()) {
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
        scrollbarsController().notifyContentAreaScrolled(scrollPosition() - oldPosition);
}

bool ScrollableArea::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, std::optional<WheelScrollGestureState>)
{
    if (!isScrollableOrRubberbandable())
        return false;

    bool handledEvent = scrollAnimator().handleWheelEvent(wheelEvent);
    
    LOG_WITH_STREAM(Scrolling, stream << "ScrollableArea (" << *this << ") handleWheelEvent - handled " << handledEvent);
    return handledEvent;
}

void ScrollableArea::stopKeyboardScrollAnimation()
{
    scrollAnimator().stopKeyboardScrollAnimation();
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
    setScrollPositionFromAnimation(scrollPositionFromOffset(offset));
}

void ScrollableArea::setScrollPositionFromAnimation(const ScrollPosition& position)
{
    // An early return here if the position hasn't changed breaks an iOS RTL scrolling test: webkit.org/b/230450.
    auto scrollType = currentScrollType();
    auto clamping = scrollType == ScrollType::User ? ScrollClamping::Unclamped : ScrollClamping::Clamped;
    if (requestScrollPositionUpdate(position, scrollType, clamping))
        return;

    scrollPositionChanged(position);
}

void ScrollableArea::willStartLiveResize()
{
    if (m_inLiveResize)
        return;

    m_inLiveResize = true;
    scrollbarsController().willStartLiveResize();
}

void ScrollableArea::willEndLiveResize()
{
    if (!m_inLiveResize)
        return;

    m_inLiveResize = false;
    scrollbarsController().willEndLiveResize();
    scrollAnimator().contentsSizeChanged();
}

void ScrollableArea::contentAreaWillPaint() const
{
    // This is used to flash overlay scrollbars in some circumstances.
    scrollbarsController().contentAreaWillPaint();
}

void ScrollableArea::mouseEnteredContentArea() const
{
    scrollbarsController().mouseEnteredContentArea();
}

void ScrollableArea::mouseExitedContentArea() const
{
    scrollbarsController().mouseExitedContentArea();
}

void ScrollableArea::mouseMovedInContentArea() const
{
    scrollbarsController().mouseMovedInContentArea();
}

void ScrollableArea::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    scrollbarsController().mouseEnteredScrollbar(scrollbar);
}

void ScrollableArea::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    scrollbarsController().mouseExitedScrollbar(scrollbar);
}

void ScrollableArea::mouseIsDownInScrollbar(Scrollbar* scrollbar, bool mouseIsDown) const
{
    scrollbarsController().mouseIsDownInScrollbar(scrollbar, mouseIsDown);
}

void ScrollableArea::contentAreaDidShow() const
{
    scrollbarsController().contentAreaDidShow();
}

void ScrollableArea::contentAreaDidHide() const
{
    scrollbarsController().contentAreaDidHide();
}

void ScrollableArea::lockOverlayScrollbarStateToHidden(bool shouldLockState) const
{
    scrollbarsController().lockOverlayScrollbarStateToHidden(shouldLockState);
}

bool ScrollableArea::scrollbarsCanBeActive() const
{
    return scrollbarsController().scrollbarsCanBeActive();
}

void ScrollableArea::didAddScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    if (orientation == ScrollbarOrientation::Vertical)
        scrollbarsController().didAddVerticalScrollbar(scrollbar);
    else
        scrollbarsController().didAddHorizontalScrollbar(scrollbar);

    scrollAnimator().contentsSizeChanged();

    // <rdar://problem/9797253> AppKit resets the scrollbar's style when you attach a scrollbar
    setScrollbarOverlayStyle(scrollbarOverlayStyle());
}

void ScrollableArea::willRemoveScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    if (orientation == ScrollbarOrientation::Vertical)
        scrollbarsController().willRemoveVerticalScrollbar(scrollbar);
    else
        scrollbarsController().willRemoveHorizontalScrollbar(scrollbar);
}

void ScrollableArea::contentsResized()
{
    scrollAnimator().contentsSizeChanged();
    scrollbarsController().contentsSizeChanged();
}

void ScrollableArea::availableContentSizeChanged(AvailableSizeChangeReason)
{
    scrollAnimator().contentsSizeChanged();
    scrollbarsController().contentsSizeChanged(); // This flashes overlay scrollbars.
}

bool ScrollableArea::hasOverlayScrollbars() const
{
    return (verticalScrollbar() && verticalScrollbar()->isOverlayScrollbar())
        || (horizontalScrollbar() && horizontalScrollbar()->isOverlayScrollbar());
}

bool ScrollableArea::canShowNonOverlayScrollbars() const
{
    return canHaveScrollbars() && !ScrollbarTheme::theme().usesOverlayScrollbars();
}

void ScrollableArea::setScrollbarOverlayStyle(ScrollbarOverlayStyle overlayStyle)
{
    m_scrollbarOverlayStyle = overlayStyle;

    if (auto* scrollbar = horizontalScrollbar())
        ScrollbarTheme::theme().updateScrollbarOverlayStyle(*scrollbar);

    if (auto* scrollbar = verticalScrollbar())
        ScrollbarTheme::theme().updateScrollbarOverlayStyle(*scrollbar);

    invalidateScrollbars();
}

void ScrollableArea::invalidateScrollbars()
{
    if (auto* scrollbar = horizontalScrollbar()) {
        scrollbar->invalidate();
        scrollbarsController().invalidateScrollbarPartLayers(scrollbar);
    }

    if (auto* scrollbar = verticalScrollbar()) {
        scrollbar->invalidate();
        scrollbarsController().invalidateScrollbarPartLayers(scrollbar);
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
    scrollbarsController().verticalScrollbarLayerDidChange();
}

void ScrollableArea::horizontalScrollbarLayerDidChange()
{
    scrollbarsController().horizontalScrollbarLayerDidChange();
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

bool ScrollableArea::allowsHorizontalScrolling() const
{
    auto* horizontalScrollbar = this->horizontalScrollbar();
    return horizontalScrollbar && horizontalScrollbar->enabled();
}

bool ScrollableArea::allowsVerticalScrolling() const
{
    auto* verticalScrollbar = this->verticalScrollbar();
    return verticalScrollbar && verticalScrollbar->enabled();
}

String ScrollableArea::horizontalScrollbarStateForTesting() const
{
    return scrollbarsController().horizontalScrollbarStateForTesting();
}

String ScrollableArea::verticalScrollbarStateForTesting() const
{
    return scrollbarsController().verticalScrollbarStateForTesting();
}

const LayoutScrollSnapOffsetsInfo* ScrollableArea::snapOffsetsInfo() const
{
    return existingScrollAnimator() ? existingScrollAnimator()->snapOffsetsInfo() : nullptr;
}

void ScrollableArea::setScrollSnapOffsetInfo(const LayoutScrollSnapOffsetsInfo& info)
{
    if (info.isEmpty()) {
        clearSnapOffsets();
        return;
    }

    // Consider having a non-empty set of snap offsets as a cue to initialize the ScrollAnimator.
    scrollAnimator().setSnapOffsetsInfo(info);
}

void ScrollableArea::clearSnapOffsets()
{
    if (auto* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->setSnapOffsetsInfo(LayoutScrollSnapOffsetsInfo());
}

std::optional<unsigned> ScrollableArea::currentHorizontalSnapPointIndex() const
{
    if (auto* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal);
    return std::nullopt;
}

std::optional<unsigned> ScrollableArea::currentVerticalSnapPointIndex() const
{
    if (auto* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical);
    return std::nullopt;
}

void ScrollableArea::setCurrentHorizontalSnapPointIndex(std::optional<unsigned> index)
{
    scrollAnimator().setActiveScrollSnapIndexForAxis(ScrollEventAxis::Horizontal, index);
}

void ScrollableArea::setCurrentVerticalSnapPointIndex(std::optional<unsigned> index)
{
    scrollAnimator().setActiveScrollSnapIndexForAxis(ScrollEventAxis::Vertical, index);
}

void ScrollableArea::resnapAfterLayout()
{
    LOG_WITH_STREAM(ScrollSnap, stream << *this << " updateScrollSnapState: isScrollSnapInProgress " << isScrollSnapInProgress() << " isUserScrollInProgress " << isUserScrollInProgress());

    auto* scrollAnimator = existingScrollAnimator();
    if (!scrollAnimator || isScrollSnapInProgress() || isUserScrollInProgress())
        return;

    scrollAnimator->resnapAfterLayout();

    const auto* info = snapOffsetsInfo();
    if (!info)
        return;

    auto currentOffset = scrollOffset();
    auto correctedOffset = currentOffset;

    if (!horizontalScrollbar() || horizontalScrollbar()->pressedPart() == ScrollbarPart::NoPart) {
        const auto& horizontal = info->horizontalSnapOffsets;
        auto activeHorizontalIndex = currentHorizontalSnapPointIndex();
        if (activeHorizontalIndex)
            correctedOffset.setX(horizontal[*activeHorizontalIndex].offset.toInt());
    }

    if (!verticalScrollbar() || verticalScrollbar()->pressedPart() == ScrollbarPart::NoPart) {
        const auto& vertical = info->verticalSnapOffsets;
        auto activeVerticalIndex = currentVerticalSnapPointIndex();
        if (activeVerticalIndex)
            correctedOffset.setY(vertical[*activeVerticalIndex].offset.toInt());
    }

    if (correctedOffset != currentOffset) {
        auto position = scrollPositionFromOffset(correctedOffset);
        if (scrollAnimationStatus() == ScrollAnimationStatus::NotAnimating)
            scrollToOffsetWithoutAnimation(correctedOffset);
        else
            scrollAnimator->retargetRunningAnimation(position);
    }
}

void ScrollableArea::doPostThumbMoveSnapping(ScrollbarOrientation orientation)
{
    auto* scrollAnimator = existingScrollAnimator();
    if (!scrollAnimator)
        return;

    auto currentOffset = scrollOffset();
    auto newOffset = currentOffset;
    if (orientation == ScrollbarOrientation::Horizontal)
        newOffset.setX(scrollAnimator->scrollOffsetAdjustedForSnapping(ScrollEventAxis::Horizontal, currentOffset, ScrollSnapPointSelectionMethod::Closest));
    else
        newOffset.setY(scrollAnimator->scrollOffsetAdjustedForSnapping(ScrollEventAxis::Vertical, currentOffset, ScrollSnapPointSelectionMethod::Closest));

    if (newOffset == currentOffset)
        return;

    auto position = scrollPositionFromOffset(newOffset);
    scrollAnimator->scrollToPositionWithAnimation(position);
}

bool ScrollableArea::isPinnedOnSide(BoxSide side) const
{
    switch (side) {
    case BoxSide::Top:
        if (!allowsVerticalScrolling())
            return true;
        return scrollPosition().y() <= minimumScrollPosition().y();
    case BoxSide::Bottom:
        if (!allowsVerticalScrolling())
            return true;
        return scrollPosition().y() >= maximumScrollPosition().y();
    case BoxSide::Left:
        if (!allowsHorizontalScrolling())
            return true;
        return scrollPosition().x() <= minimumScrollPosition().x();
    case BoxSide::Right:
        if (!allowsHorizontalScrolling())
            return true;
        return scrollPosition().x() >= maximumScrollPosition().x();
    }
    return false;
}

RectEdges<bool> ScrollableArea::edgePinnedState() const
{
    auto scrollPosition = this->scrollPosition();
    auto minScrollPosition = minimumScrollPosition();
    auto maxScrollPosition = maximumScrollPosition();

    bool horizontallyUnscrollable = !allowsHorizontalScrolling();
    bool verticallyUnscrollable = !allowsVerticalScrolling();

    // Top, right, bottom, left.
    return {
        verticallyUnscrollable || scrollPosition.y() <= minScrollPosition.y(),
        horizontallyUnscrollable || scrollPosition.x() >= maxScrollPosition.x(),
        verticallyUnscrollable || scrollPosition.y() >= maxScrollPosition.y(),
        horizontallyUnscrollable || scrollPosition.x() <= minScrollPosition.x()
    };
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

std::optional<BoxSide> ScrollableArea::targetSideForScrollDelta(FloatSize delta, ScrollEventAxis axis)
{
    switch (axis) {
    case ScrollEventAxis::Horizontal:
        if (delta.width() < 0)
            return BoxSide::Left;

        if (delta.width() > 0)
            return BoxSide::Right;
        break;

    case ScrollEventAxis::Vertical:
        if (delta.height() < 0)
            return BoxSide::Top;

        if (delta.height() > 0)
            return BoxSide::Bottom;
        break;
    }

    return { };
}

LayoutRect ScrollableArea::getRectToExposeForScrollIntoView(const LayoutRect& visibleBounds, const LayoutRect& exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY, const std::optional<LayoutRect> visibilityCheckRect) const
{
    static const int minIntersectForReveal = 32;

    ScrollAlignment::Behavior scrollX = alignX.getHiddenBehavior();
    
    bool intersectsInX = false;
    if (visibilityCheckRect)
        intersectsInX = visibilityCheckRect->maxX() >= visibleBounds.x() && visibilityCheckRect->x() <= visibleBounds.maxX();
    else
        intersectsInX = exposeRect.maxX() >= visibleBounds.x() && exposeRect.x() <= visibleBounds.maxX();
    
    // Determine the appropriate X behavior.
    if (intersectsInX) {
        LayoutUnit intersectWidth = std::max(LayoutUnit(), std::min(visibleBounds.maxX(), exposeRect.maxX()) - std::max(visibleBounds.x(), exposeRect.x()));

        if (intersectWidth == exposeRect.width() || (alignX.legacyHorizontalVisibilityThresholdEnabled() && intersectWidth >= minIntersectForReveal)) {
            // If the rectangle is fully visible, use the specified visible behavior.
            // If the rectangle is partially visible, but over a certain threshold,
            // then treat it as fully visible to avoid unnecessary horizontal scrolling
            scrollX = alignX.getVisibleBehavior();
        } else if (intersectWidth == visibleBounds.width()) {
            // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
            scrollX = alignX.getVisibleBehavior();
            if (scrollX == ScrollAlignment::Behavior::AlignCenter)
                scrollX = ScrollAlignment::Behavior::NoScroll;
        } else if (intersectWidth > 0)
            // If the rectangle is partially visible, but not above the minimum threshold, use the specified partial behavior
            scrollX = alignX.getPartialBehavior();
        else
            scrollX = alignX.getHiddenBehavior();
    }

    // If we're trying to align to the closest edge, and the exposeRect is further right
    // than the visibleBounds, and not bigger than the visible area, then align with the right.
    if (scrollX == ScrollAlignment::Behavior::AlignToClosestEdge && exposeRect.maxX() > visibleBounds.maxX() && exposeRect.width() < visibleBounds.width())
        scrollX = ScrollAlignment::Behavior::AlignRight;

    // Given the X behavior, compute the X coordinate.
    LayoutUnit x;
    if (scrollX == ScrollAlignment::Behavior::NoScroll)
        x = visibleBounds.x();
    else if (scrollX == ScrollAlignment::Behavior::AlignRight)
        x = exposeRect.maxX() - visibleBounds.width();
    else if (scrollX == ScrollAlignment::Behavior::AlignCenter)
        x = exposeRect.x() + (exposeRect.width() - visibleBounds.width()) / 2;
    else
        x = exposeRect.x();

    ScrollAlignment::Behavior scrollY = alignY.getHiddenBehavior();
    
    bool intersectsInY = false;
    if (visibilityCheckRect)
        intersectsInY = visibilityCheckRect->maxY() >= visibleBounds.y() && visibilityCheckRect->y() <= visibleBounds.maxY();
    else
        intersectsInY = exposeRect.maxY() >= visibleBounds.y() && exposeRect.y() <= visibleBounds.maxY();

    // Determine the appropriate Y behavior.
    if (intersectsInY) {
        LayoutUnit intersectHeight = std::max(LayoutUnit(), std::min(visibleBounds.maxY(), exposeRect.maxY()) - std::max(visibleBounds.y(), exposeRect.y()));
        if (intersectHeight == exposeRect.height()) {
            // If the rectangle is fully visible, use the specified visible behavior.
            scrollY = alignY.getVisibleBehavior();
        } else if (intersectHeight == visibleBounds.height()) {
            // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
            scrollY = alignY.getVisibleBehavior();
            if (scrollY == ScrollAlignment::Behavior::AlignCenter)
                scrollY = ScrollAlignment::Behavior::NoScroll;
        } else if (intersectHeight > 0)
            // If the rectangle is partially visible, use the specified partial behavior
            scrollY = alignY.getPartialBehavior();
        else
            scrollY = alignY.getHiddenBehavior();
    }

    // If we're trying to align to the closest edge, and the exposeRect is further down
    // than the visibleBounds, and not bigger than the visible area, then align with the bottom.
    if (scrollY == ScrollAlignment::Behavior::AlignToClosestEdge && exposeRect.maxY() > visibleBounds.maxY() && exposeRect.height() < visibleBounds.height())
        scrollY = ScrollAlignment::Behavior::AlignBottom;

    // Given the Y behavior, compute the Y coordinate.
    LayoutUnit y;
    if (scrollY == ScrollAlignment::Behavior::NoScroll)
        y = visibleBounds.y();
    else if (scrollY == ScrollAlignment::Behavior::AlignBottom)
        y = exposeRect.maxY() - visibleBounds.height();
    else if (scrollY == ScrollAlignment::Behavior::AlignCenter)
        y = exposeRect.y() + (exposeRect.height() - visibleBounds.height()) / 2;
    else
        y = exposeRect.y();

    return LayoutRect(LayoutPoint(x, y), visibleBounds.size());
}

TextStream& operator<<(TextStream& ts, const ScrollableArea& scrollableArea)
{
    ts << scrollableArea.debugDescription();
    return ts;
}

FloatSize ScrollableArea::deltaForPropagation(const FloatSize& biasedDelta) const
{
    auto filteredDelta = biasedDelta;
    if (horizontalOverscrollBehaviorPreventsPropagation())
        filteredDelta.setWidth(0);
    if (verticalOverscrollBehaviorPreventsPropagation())
        filteredDelta.setHeight(0);
    return filteredDelta;
}

bool ScrollableArea::shouldBlockScrollPropagation(const FloatSize& biasedDelta) const
{
    return ((horizontalOverscrollBehaviorPreventsPropagation() || verticalOverscrollBehaviorPreventsPropagation())
        && ((horizontalOverscrollBehaviorPreventsPropagation() && verticalOverscrollBehaviorPreventsPropagation())
        || (horizontalOverscrollBehaviorPreventsPropagation() && !biasedDelta.height()) || (verticalOverscrollBehaviorPreventsPropagation()
        && !biasedDelta.width())));
}

} // namespace WebCore
