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
#import "ScrollingTree.h"
#import "ScrollingTreeFrameScrollingNode.h"
#import "ScrollingTreeScrollingNode.h"

namespace WebCore {

ScrollingTreeScrollingNodeDelegateMac::ScrollingTreeScrollingNodeDelegateMac(ScrollingTreeScrollingNode& scrollingNode)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_scrollController(*this)
{
}

ScrollingTreeScrollingNodeDelegateMac::~ScrollingTreeScrollingNodeDelegateMac() = default;

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
    switch (axis) {
    case ScrollEventAxis::Vertical:
        return (wheelEvent.deltaY() > 0 && scrollPosition().y() <= minimumScrollPosition().y()) || (wheelEvent.deltaY() < 0 && scrollPosition().y() >= maximumScrollPosition().y());
    case ScrollEventAxis::Horizontal:
        return (wheelEvent.deltaX() > 0 && scrollPosition().x() <= minimumScrollPosition().x()) || (wheelEvent.deltaX() < 0 && scrollPosition().x() >= maximumScrollPosition().x());
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

    if (scrollPosition().y() < minimumScrollPosition().y())
        stretch.setHeight(scrollPosition().y() - minimumScrollPosition().y());
    else if (scrollPosition().y() > maximumScrollPosition().y())
        stretch.setHeight(scrollPosition().y() - maximumScrollPosition().y());

    if (scrollPosition().x() < minimumScrollPosition().x())
        stretch.setWidth(scrollPosition().x() - minimumScrollPosition().x());
    else if (scrollPosition().x() > maximumScrollPosition().x())
        stretch.setWidth(scrollPosition().x() - maximumScrollPosition().x());

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

    if (fabsf(delta.height()) >= fabsf(delta.width())) {
        if (delta.height() < 0) {
            // We are trying to scroll up. Make sure we are not pinned to the top.
            limitDelta.setHeight(scrollPosition().y() - minimumScrollPosition().y());
        } else {
            // We are trying to scroll down. Make sure we are not pinned to the bottom.
            limitDelta.setHeight(maximumScrollPosition().y() - scrollPosition().y());
        }
    } else if (delta.width()) {
        if (delta.width() < 0) {
            // We are trying to scroll left. Make sure we are not pinned to the left.
            limitDelta.setWidth(scrollPosition().x() - minimumScrollPosition().x());
        } else {
            // We are trying to scroll right. Make sure we are not pinned to the right.
            limitDelta.setWidth(maximumScrollPosition().x() - scrollPosition().x());
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
    scrollingNode().scrollByWithoutContentEdgeConstraints(offset);
}

void ScrollingTreeScrollingNodeDelegateMac::stopSnapRubberbandTimer()
{
    scrollingTree().setMainFrameIsRubberBanding(false);

    // Since the rubberband timer has stopped, totalContentsSizeForRubberBand can be synchronized with totalContentsSize.
    scrollingNode().setTotalContentsSizeForRubberBand(totalContentsSize());
}

void ScrollingTreeScrollingNodeDelegateMac::adjustScrollPositionToBoundsIfNecessary()
{
    FloatPoint currentScrollPosition = scrollPosition();
    FloatPoint constrainedPosition = currentScrollPosition.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
    immediateScrollBy(constrainedPosition - currentScrollPosition);
}

#if ENABLE(CSS_SCROLL_SNAP)
FloatPoint ScrollingTreeScrollingNodeDelegateMac::scrollOffset() const
{
    return scrollPosition();
}

void ScrollingTreeScrollingNodeDelegateMac::immediateScrollOnAxis(ScrollEventAxis axis, float delta)
{
    const FloatPoint& currentPosition = scrollPosition();
    FloatPoint change;
    if (axis == ScrollEventAxis::Horizontal)
        change = FloatPoint(currentPosition.x() + delta, currentPosition.y());
    else
        change = FloatPoint(currentPosition.x(), currentPosition.y() + delta);

    immediateScrollBy(change - currentPosition);
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

} // namespace WebCore

#endif // PLATFORM(MAC) && ENABLE(ASYNC_SCROLLING)
