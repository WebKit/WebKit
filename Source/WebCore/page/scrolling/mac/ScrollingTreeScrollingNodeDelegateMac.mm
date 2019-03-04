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

void ScrollingTreeScrollingNodeDelegateMac::updateFromStateNode(const ScrollingStateScrollingNode& scrollingStateNode)
{
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::PainterForScrollbar)) {
        releaseReferencesToScrollerImpsOnTheMainThread();
        m_verticalScrollerImp = scrollingStateNode.verticalScrollerImp();
        m_horizontalScrollerImp = scrollingStateNode.horizontalScrollerImp();
    }
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
    if (scrollingNode().expectsWheelEventTestTrigger()) {
        if (scrollingTree().shouldHandleWheelEventSynchronously(wheelEvent))
            removeTestDeferralForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(scrollingNode().scrollingNodeID()), WheelEventTestTrigger::ScrollingThreadSyncNeeded);
        else
            deferTestsForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(scrollingNode().scrollingNodeID()), WheelEventTestTrigger::ScrollingThreadSyncNeeded);
    }
#endif

    return m_scrollController.handleWheelEvent(wheelEvent);
}

bool ScrollingTreeScrollingNodeDelegateMac::isScrollSnapInProgress() const
{
    return m_scrollController.isScrollSnapInProgress();
}

// FIXME: We should find a way to share some of the code from newGestureIsStarting(), isAlreadyPinnedInDirectionOfGesture(),
// allowsVerticalStretching(), and allowsHorizontalStretching() with the implementation in ScrollAnimatorMac.
static bool newGestureIsStarting(const PlatformWheelEvent& wheelEvent)
{
    return wheelEvent.phase() == PlatformWheelEventPhaseMayBegin || wheelEvent.phase() == PlatformWheelEventPhaseBegan;
}

bool ScrollingTreeScrollingNodeDelegateMac::isAlreadyPinnedInDirectionOfGesture(const PlatformWheelEvent& wheelEvent, ScrollEventAxis axis)
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

bool ScrollingTreeScrollingNodeDelegateMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent)
{
    switch (horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        bool scrollbarsAllowStretching = hasEnabledHorizontalScrollbar() || !hasEnabledVerticalScrollbar();
        bool eventPreventsStretching = newGestureIsStarting(wheelEvent) && isAlreadyPinnedInDirectionOfGesture(wheelEvent, ScrollEventAxis::Horizontal);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsVerticalStretching(const PlatformWheelEvent& wheelEvent)
{
    switch (verticalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        bool scrollbarsAllowStretching = hasEnabledVerticalScrollbar() || !hasEnabledHorizontalScrollbar();
        bool eventPreventsStretching = newGestureIsStarting(wheelEvent) && isAlreadyPinnedInDirectionOfGesture(wheelEvent, ScrollEventAxis::Vertical);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

IntSize ScrollingTreeScrollingNodeDelegateMac::stretchAmount()
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

    if (scrollingNode().isRootNode()) {
        if (stretch.isZero())
            scrollingTree().setMainFrameIsRubberBanding(false);
        else
            scrollingTree().setMainFrameIsRubberBanding(true);
    }

    return stretch;
}

bool ScrollingTreeScrollingNodeDelegateMac::pinnedInDirection(const FloatSize& delta)
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

bool ScrollingTreeScrollingNodeDelegateMac::canScrollHorizontally()
{
    return hasEnabledHorizontalScrollbar();
}

bool ScrollingTreeScrollingNodeDelegateMac::canScrollVertically()
{
    return hasEnabledVerticalScrollbar();
}

bool ScrollingTreeScrollingNodeDelegateMac::shouldRubberBandInDirection(ScrollDirection)
{
    return true;
}

void ScrollingTreeScrollingNodeDelegateMac::immediateScrollBy(const FloatSize& delta)
{
    scrollingNode().scrollBy(delta);
}

void ScrollingTreeScrollingNodeDelegateMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& offset)
{
    scrollingNode().scrollBy(offset, ScrollPositionClamp::None);
}

void ScrollingTreeScrollingNodeDelegateMac::stopSnapRubberbandTimer()
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

void ScrollingTreeScrollingNodeDelegateMac::startScrollSnapTimer()
{
    scrollingTree().setMainFrameIsScrollSnapping(true);
}

void ScrollingTreeScrollingNodeDelegateMac::stopScrollSnapTimer()
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

void ScrollingTreeScrollingNodeDelegateMac::deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier identifier, WheelEventTestTrigger::DeferTestTriggerReason reason) const
{
    if (!scrollingNode().expectsWheelEventTestTrigger())
        return;

    LOG(WheelEventTestTriggers, "  ScrollingTreeScrollingNodeDelegateMac::deferTestsForReason: STARTING deferral for %p because of %d", identifier, reason);
    scrollingTree().deferTestsForReason(identifier, reason);
}
    
void ScrollingTreeScrollingNodeDelegateMac::removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier identifier, WheelEventTestTrigger::DeferTestTriggerReason reason) const
{
    if (!scrollingNode().expectsWheelEventTestTrigger())
        return;
    
    LOG(WheelEventTestTriggers, "   ScrollingTreeScrollingNodeDelegateMac::deferTestsForReason: ENDING deferral for %p because of %d", identifier, reason);
    scrollingTree().removeTestDeferralForReason(identifier, reason);
}

void ScrollingTreeScrollingNodeDelegateMac::updateScrollbarPainters()
{
    if (m_verticalScrollerImp || m_horizontalScrollerImp) {
        auto scrollPosition = currentScrollPosition();

        [CATransaction begin];
        [CATransaction lock];

        if ([m_verticalScrollerImp shouldUsePresentationValue]) {
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(scrollPosition.y(), totalContentsSize().height(), scrollableAreaSize().height(), presentationValue, overhangAmount);
            [m_verticalScrollerImp setPresentationValue:presentationValue];
        }

        if ([m_horizontalScrollerImp shouldUsePresentationValue]) {
            float presentationValue;
            float overhangAmount;
            ScrollableArea::computeScrollbarValueAndOverhang(scrollPosition.x(), totalContentsSize().width(), scrollableAreaSize().width(), presentationValue, overhangAmount);
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
