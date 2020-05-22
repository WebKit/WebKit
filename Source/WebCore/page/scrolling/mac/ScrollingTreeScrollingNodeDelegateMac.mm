/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ScrollingTreeScrollingNodeDelegateMac.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#import "Logging.h"
#import "ScrollingStateScrollingNode.h"
#import "ScrollingTree.h"
#import "ScrollingTreeFrameScrollingNode.h"
#import "ScrollingTreeScrollingNode.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>

namespace WebCore {

ScrollingTreeScrollingNodeDelegateMac::ScrollingTreeScrollingNodeDelegateMac(ScrollingTreeScrollingNode& scrollingNode)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_scrollController(*this)
{
}

ScrollingTreeScrollingNodeDelegateMac::~ScrollingTreeScrollingNodeDelegateMac()
{
    releaseReferencesToScrollerImpsOnTheMainThread();
}

void ScrollingTreeScrollingNodeDelegateMac::nodeWillBeDestroyed()
{
    m_scrollController.stopAllTimers();
}

#if ENABLE(CSS_SCROLL_SNAP)
static inline Vector<LayoutUnit> convertToLayoutUnits(const Vector<float>& snapOffsetsAsFloat)
{
    Vector<LayoutUnit> snapOffsets;
    snapOffsets.reserveInitialCapacity(snapOffsetsAsFloat.size());
    for (auto offset : snapOffsetsAsFloat)
        snapOffsets.uncheckedAppend(offset);

    return snapOffsets;
}

static inline Vector<ScrollOffsetRange<LayoutUnit>> convertToLayoutUnits(const Vector<ScrollOffsetRange<float>>& snapOffsetRangesAsFloat)
{
    Vector<ScrollOffsetRange<LayoutUnit>> snapOffsetRanges;
    snapOffsetRanges.reserveInitialCapacity(snapOffsetRangesAsFloat.size());
    for (auto range : snapOffsetRangesAsFloat)
        snapOffsetRanges.uncheckedAppend({ LayoutUnit(range.start), LayoutUnit(range.end) });

    return snapOffsetRanges;
}
#endif

void ScrollingTreeScrollingNodeDelegateMac::updateFromStateNode(const ScrollingStateScrollingNode& scrollingStateNode)
{
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::PainterForScrollbar)) {
        releaseReferencesToScrollerImpsOnTheMainThread();
        m_verticalScrollerImp = scrollingStateNode.verticalScrollerImp();
        m_horizontalScrollerImp = scrollingStateNode.horizontalScrollerImp();
    }

#if ENABLE(CSS_SCROLL_SNAP)
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalSnapOffsets) || scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HorizontalSnapOffsetRanges))
        updateScrollSnapPoints(ScrollEventAxis::Horizontal, convertToLayoutUnits(scrollingStateNode.horizontalSnapOffsets()), convertToLayoutUnits(scrollingStateNode.horizontalSnapOffsetRanges()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalSnapOffsets) || scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::VerticalSnapOffsetRanges))
        updateScrollSnapPoints(ScrollEventAxis::Vertical, convertToLayoutUnits(scrollingStateNode.verticalSnapOffsets()), convertToLayoutUnits(scrollingStateNode.verticalSnapOffsetRanges()));

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CurrentHorizontalSnapOffsetIndex))
        setActiveScrollSnapIndexForAxis(ScrollEventAxis::Horizontal, scrollingStateNode.currentHorizontalSnapPointIndex());
    
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::CurrentVerticalSnapOffsetIndex))
        setActiveScrollSnapIndexForAxis(ScrollEventAxis::Vertical, scrollingStateNode.currentVerticalSnapPointIndex());
#endif
}

void ScrollingTreeScrollingNodeDelegateMac::updateScrollSnapPoints(ScrollEventAxis axis, const Vector<LayoutUnit>& snapOffsets, const Vector<ScrollOffsetRange<LayoutUnit>>& snapRanges)
{
    m_scrollController.updateScrollSnapPoints(axis, snapOffsets, snapRanges);
}

void ScrollingTreeScrollingNodeDelegateMac::setActiveScrollSnapIndexForAxis(ScrollEventAxis axis, unsigned index)
{
    m_scrollController.setActiveScrollSnapIndexForAxis(axis, index);
}

unsigned ScrollingTreeScrollingNodeDelegateMac::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    return m_scrollController.activeScrollSnapIndexForAxis(axis);
}

bool ScrollingTreeScrollingNodeDelegateMac::activeScrollSnapIndexDidChange() const
{
    return m_scrollController.activeScrollSnapIndexDidChange();
}

bool ScrollingTreeScrollingNodeDelegateMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseBegan) {
        [m_verticalScrollerImp setUsePresentationValue:YES];
        [m_horizontalScrollerImp setUsePresentationValue:YES];
    }
    if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseEnded || wheelEvent.momentumPhase() == PlatformWheelEventPhaseCancelled) {
        [m_verticalScrollerImp setUsePresentationValue:NO];
        [m_horizontalScrollerImp setUsePresentationValue:NO];
    }

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    if (scrollingTree().isMonitoringWheelEvents())
        deferWheelEventTestCompletionForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(scrollingNode().scrollingNodeID()), WheelEventTestMonitor::HandlingWheelEvent);
#endif

    // PlatformWheelEventPhaseMayBegin fires when two fingers touch the trackpad, and is used to flash overlay scrollbars.
    // We know we're scrollable at this point, so handle the event.
    if (wheelEvent.phase() == PlatformWheelEventPhaseMayBegin)
        return true;

    auto handled = m_scrollController.handleWheelEvent(wheelEvent);

#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    if (scrollingTree().isMonitoringWheelEvents())
        removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(scrollingNode().scrollingNodeID()), WheelEventTestMonitor::HandlingWheelEvent);
#endif

    return handled;
}

bool ScrollingTreeScrollingNodeDelegateMac::isScrollSnapInProgress() const
{
    return m_scrollController.isScrollSnapInProgress();
}

// FIXME: We should find a way to share some of the code from newGestureIsStarting(), isAlreadyPinnedInDirectionOfGesture(),
// allowsVerticalStretching(), and allowsHorizontalStretching() with the implementation in ScrollAnimatorMac.
// This is also the same as PlatformWheelEvent::shouldConsiderLatching().
static bool newGestureIsStarting(const PlatformWheelEvent& wheelEvent)
{
    return wheelEvent.phase() == PlatformWheelEventPhaseMayBegin || wheelEvent.phase() == PlatformWheelEventPhaseBegan;
}

bool ScrollingTreeScrollingNodeDelegateMac::isAlreadyPinnedInDirectionOfGesture(const PlatformWheelEvent& wheelEvent, ScrollEventAxis axis) const
{
    auto scrollPosition = currentScrollPosition();
    switch (axis) {
    case ScrollEventAxis::Vertical:
        return (wheelEvent.deltaY() > 0 && scrollPosition.y() <= minimumScrollPosition().y()) || (wheelEvent.deltaY() < 0 && scrollPosition.y() >= maximumScrollPosition().y());
    case ScrollEventAxis::Horizontal:
        return (wheelEvent.deltaX() > 0 && scrollPosition.x() <= minimumScrollPosition().x()) || (wheelEvent.deltaX() < 0 && scrollPosition.x() >= maximumScrollPosition().x());
    }

    ASSERT_NOT_REACHED();
    return false;
}

std::unique_ptr<ScrollControllerTimer> ScrollingTreeScrollingNodeDelegateMac::createTimer(Function<void()>&& function)
{
    return WTF::makeUnique<ScrollControllerTimer>(RunLoop::current(), [function = WTFMove(function), protectedNode = makeRef(scrollingNode())] {
        LockHolder locker(protectedNode->scrollingTree().treeMutex());
        function();
    });
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        bool scrollbarsAllowStretching = hasEnabledHorizontalScrollbar() || !hasEnabledVerticalScrollbar();
        bool eventPreventsStretching = newGestureIsStarting(wheelEvent) && isAlreadyPinnedInDirectionOfGesture(wheelEvent, ScrollEventAxis::Horizontal);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed: {
        auto scrollDirection = ScrollController::directionFromEvent(wheelEvent, ScrollEventAxis::Horizontal);
        if (scrollDirection)
            return shouldRubberBandInDirection(scrollDirection.value());
        return true;
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsVerticalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (verticalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        bool scrollbarsAllowStretching = hasEnabledVerticalScrollbar() || !hasEnabledHorizontalScrollbar();
        bool eventPreventsStretching = newGestureIsStarting(wheelEvent) && isAlreadyPinnedInDirectionOfGesture(wheelEvent, ScrollEventAxis::Vertical);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed: {
        auto scrollDirection = ScrollController::directionFromEvent(wheelEvent, ScrollEventAxis::Vertical);
        if (scrollDirection)
            return shouldRubberBandInDirection(scrollDirection.value());
        return true;
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

IntSize ScrollingTreeScrollingNodeDelegateMac::stretchAmount() const
{
    IntSize stretch;
    auto scrollPosition = currentScrollPosition();

    if (scrollPosition.y() < minimumScrollPosition().y())
        stretch.setHeight(scrollPosition.y() - minimumScrollPosition().y());
    else if (scrollPosition.y() > maximumScrollPosition().y())
        stretch.setHeight(scrollPosition.y() - maximumScrollPosition().y());

    if (scrollPosition.x() < minimumScrollPosition().x())
        stretch.setWidth(scrollPosition.x() - minimumScrollPosition().x());
    else if (scrollPosition.x() > maximumScrollPosition().x())
        stretch.setWidth(scrollPosition.x() - maximumScrollPosition().x());

    return stretch;
}

// FIXME: Share more with ScrollingTreeScrollingNode::edgePinnedState().
bool ScrollingTreeScrollingNodeDelegateMac::pinnedInDirection(const FloatSize& delta) const
{
    FloatSize limitDelta;
    auto scrollPosition = currentScrollPosition();

    if (fabsf(delta.height()) >= fabsf(delta.width())) {
        if (delta.height() < 0) {
            // We are trying to scroll up. Make sure we are not pinned to the top.
            limitDelta.setHeight(scrollPosition.y() - minimumScrollPosition().y());
        } else {
            // We are trying to scroll down. Make sure we are not pinned to the bottom.
            limitDelta.setHeight(maximumScrollPosition().y() - scrollPosition.y());
        }
    } else if (delta.width()) {
        if (delta.width() < 0) {
            // We are trying to scroll left. Make sure we are not pinned to the left.
            limitDelta.setWidth(scrollPosition.x() - minimumScrollPosition().x());
        } else {
            // We are trying to scroll right. Make sure we are not pinned to the right.
            limitDelta.setWidth(maximumScrollPosition().x() - scrollPosition.x());
        }
    }

    if ((delta.width() || delta.height()) && (limitDelta.width() < 1 && limitDelta.height() < 1))
        return true;

    return false;
}

bool ScrollingTreeScrollingNodeDelegateMac::canScrollHorizontally() const
{
    return hasEnabledHorizontalScrollbar();
}

bool ScrollingTreeScrollingNodeDelegateMac::canScrollVertically() const
{
    return hasEnabledVerticalScrollbar();
}

bool ScrollingTreeScrollingNodeDelegateMac::shouldRubberBandInDirection(ScrollDirection direction) const
{
    if (scrollingNode().isRootNode())
        return scrollingTree().mainFrameCanRubberBandInDirection(direction);

    // FIXME: Consult the node.
    return true;
}

void ScrollingTreeScrollingNodeDelegateMac::immediateScrollBy(const FloatSize& delta)
{
    scrollingNode().scrollBy(delta);
}

void ScrollingTreeScrollingNodeDelegateMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& offset)
{
    scrollingNode().scrollBy(offset, ScrollClamping::Unclamped);
}

void ScrollingTreeScrollingNodeDelegateMac::didStopRubberbandSnapAnimation()
{
    scrollingTree().setMainFrameIsRubberBanding(false);

    // Since the rubberband timer has stopped, totalContentsSizeForRubberBand can be synchronized with totalContentsSize.
    scrollingNode().setTotalContentsSizeForRubberBand(totalContentsSize());
}

void ScrollingTreeScrollingNodeDelegateMac::adjustScrollPositionToBoundsIfNecessary()
{
    FloatPoint scrollPosition = currentScrollPosition();
    FloatPoint constrainedPosition = scrollPosition.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
    immediateScrollBy(constrainedPosition - scrollPosition);
}

#if ENABLE(CSS_SCROLL_SNAP)
FloatPoint ScrollingTreeScrollingNodeDelegateMac::scrollOffset() const
{
    return currentScrollPosition();
}

void ScrollingTreeScrollingNodeDelegateMac::immediateScrollOnAxis(ScrollEventAxis axis, float delta)
{
    const FloatPoint& scrollPosition = currentScrollPosition();
    FloatPoint change;
    if (axis == ScrollEventAxis::Horizontal)
        change = FloatPoint(scrollPosition.x() + delta, scrollPosition.y());
    else
        change = FloatPoint(scrollPosition.x(), scrollPosition.y() + delta);

    immediateScrollBy(change - scrollPosition);
}

float ScrollingTreeScrollingNodeDelegateMac::pageScaleFactor() const
{
    // FIXME: What should this return for non-root frames, and overflow?
    // Also, this should not have to access ScrollingTreeFrameScrollingNode.
    if (is<ScrollingTreeFrameScrollingNode>(scrollingNode()))
        return downcast<ScrollingTreeFrameScrollingNode>(scrollingNode()).frameScaleFactor();

    return 1;
}

void ScrollingTreeScrollingNodeDelegateMac::willStartScrollSnapAnimation()
{
    scrollingTree().setMainFrameIsScrollSnapping(true);
}

void ScrollingTreeScrollingNodeDelegateMac::didStopScrollSnapAnimation()
{
    scrollingTree().setMainFrameIsScrollSnapping(false);
}
    
LayoutSize ScrollingTreeScrollingNodeDelegateMac::scrollExtent() const
{
    return LayoutSize(totalContentsSize());
}

FloatSize ScrollingTreeScrollingNodeDelegateMac::viewportSize() const
{
    return scrollableAreaSize();
}
#endif

void ScrollingTreeScrollingNodeDelegateMac::deferWheelEventTestCompletionForReason(WheelEventTestMonitor::ScrollableAreaIdentifier identifier, WheelEventTestMonitor::DeferReason reason) const
{
    if (!scrollingTree().isMonitoringWheelEvents())
        return;

    scrollingTree().deferWheelEventTestCompletionForReason(identifier, reason);
}
    
void ScrollingTreeScrollingNodeDelegateMac::removeWheelEventTestCompletionDeferralForReason(WheelEventTestMonitor::ScrollableAreaIdentifier identifier, WheelEventTestMonitor::DeferReason reason) const
{
    if (!scrollingTree().isMonitoringWheelEvents())
        return;
    
    scrollingTree().removeWheelEventTestCompletionDeferralForReason(identifier, reason);
}

void ScrollingTreeScrollingNodeDelegateMac::updateScrollbarPainters()
{
    if (m_verticalScrollerImp || m_horizontalScrollerImp) {
        auto scrollOffset = scrollingNode().currentScrollOffset();

        [CATransaction begin];
        [CATransaction lock];

        if ([m_verticalScrollerImp shouldUsePresentationValue]) {
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(scrollOffset.y(), totalContentsSize().height(), scrollableAreaSize().height(), presentationValue, overhangAmount);
            [m_verticalScrollerImp setPresentationValue:presentationValue];
        }

        if ([m_horizontalScrollerImp shouldUsePresentationValue]) {
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(scrollOffset.x(), totalContentsSize().width(), scrollableAreaSize().width(), presentationValue, overhangAmount);
            [m_horizontalScrollerImp setPresentationValue:presentationValue];
        }

        [CATransaction unlock];
        [CATransaction commit];
    }
}

void ScrollingTreeScrollingNodeDelegateMac::releaseReferencesToScrollerImpsOnTheMainThread()
{
    if (m_verticalScrollerImp || m_horizontalScrollerImp) {
        // FIXME: This is a workaround in place for the time being since NSScrollerImps cannot be deallocated
        // on a non-main thread. rdar://problem/24535055
        WTF::callOnMainThread([verticalScrollerImp = WTFMove(m_verticalScrollerImp), horizontalScrollerImp = WTFMove(m_horizontalScrollerImp)] {
        });
    }
}

} // namespace WebCore

#endif // PLATFORM(MAC) && ENABLE(ASYNC_SCROLLING)
