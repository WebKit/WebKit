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

#include "config.h"
#include "ThreadedScrollingTreeScrollingNodeDelegate.h"

#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)

#include "ScrollExtents.h"
#include "ScrollingStateScrollingNode.h"
#include "ScrollingTree.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include "ScrollingTreeScrollingNode.h"

namespace WebCore {

ThreadedScrollingTreeScrollingNodeDelegate::ThreadedScrollingTreeScrollingNodeDelegate(ScrollingTreeScrollingNode& scrollingNode)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_scrollController(*this)
{
}

ThreadedScrollingTreeScrollingNodeDelegate::~ThreadedScrollingTreeScrollingNodeDelegate() = default;

void ThreadedScrollingTreeScrollingNodeDelegate::updateFromStateNode(const ScrollingStateScrollingNode& scrollingStateNode)
{
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::SnapOffsetsInfo))
        m_scrollController.setSnapOffsetsInfo(scrollingStateNode.snapOffsetsInfo().convertUnits<LayoutScrollSnapOffsetsInfo>());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CurrentHorizontalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Horizontal, scrollingStateNode.currentHorizontalSnapPointIndex());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CurrentVerticalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Vertical, scrollingStateNode.currentVerticalSnapPointIndex());
}

void ThreadedScrollingTreeScrollingNodeDelegate::updateSnapScrollState()
{
    scrollingNode().setScrollSnapInProgress(m_scrollController.isScrollSnapInProgress());

    if (m_scrollController.activeScrollSnapIndexDidChange())
        scrollingTree().setActiveScrollSnapIndices(scrollingNode().scrollingNodeID(), m_scrollController.activeScrollSnapIndexForAxis(ScrollEventAxis::Horizontal), m_scrollController.activeScrollSnapIndexForAxis(ScrollEventAxis::Vertical));
}

void ThreadedScrollingTreeScrollingNodeDelegate::updateUserScrollInProgressForEvent(const PlatformWheelEvent& wheelEvent)
{
    bool wasInUserScroll = m_scrollController.isUserScrollInProgress();
    m_scrollController.updateGestureInProgressState(wheelEvent);
    bool isInUserScroll = m_scrollController.isUserScrollInProgress();
    if (isInUserScroll != wasInUserScroll)
        scrollingNode().setUserScrollInProgress(isInUserScroll);
}

bool ThreadedScrollingTreeScrollingNodeDelegate::startAnimatedScrollToPosition(FloatPoint destinationPosition)
{
    auto currentOffset = ScrollableArea::scrollOffsetFromPosition(currentScrollPosition(), scrollOrigin());
    auto destinationOffset = ScrollableArea::scrollOffsetFromPosition(destinationPosition, scrollOrigin());
    return m_scrollController.startAnimatedScrollToDestination(currentOffset, destinationOffset);
}

void ThreadedScrollingTreeScrollingNodeDelegate::stopAnimatedScroll()
{
    m_scrollController.stopAnimatedScroll();
}

void ThreadedScrollingTreeScrollingNodeDelegate::serviceScrollAnimation(MonotonicTime currentTime)
{
    m_scrollController.animationCallback(currentTime);
}

std::unique_ptr<ScrollingEffectsControllerTimer> ThreadedScrollingTreeScrollingNodeDelegate::createTimer(Function<void()>&& function)
{
    // This is only used for a scroll snap timer.
    return WTF::makeUnique<ScrollingEffectsControllerTimer>(RunLoop::current(), [function = WTFMove(function), protectedNode = Ref { scrollingNode() }] {
        Locker locker { protectedNode->scrollingTree().treeLock() };
        function();
    });
}

void ThreadedScrollingTreeScrollingNodeDelegate::startAnimationCallback(ScrollingEffectsController&)
{
    scrollingNode().setScrollAnimationInProgress(true);
}

void ThreadedScrollingTreeScrollingNodeDelegate::stopAnimationCallback(ScrollingEffectsController&)
{
    scrollingNode().setScrollAnimationInProgress(false);
}

bool ThreadedScrollingTreeScrollingNodeDelegate::allowsHorizontalScrolling() const
{
    return ScrollingTreeScrollingNodeDelegate::allowsHorizontalScrolling();
}

bool ThreadedScrollingTreeScrollingNodeDelegate::allowsVerticalScrolling() const
{
    return ScrollingTreeScrollingNodeDelegate::allowsVerticalScrolling();
}

void ThreadedScrollingTreeScrollingNodeDelegate::immediateScrollBy(const FloatSize& delta, ScrollClamping clamping)
{
    scrollingNode().scrollBy(delta, clamping);
}

void ThreadedScrollingTreeScrollingNodeDelegate::adjustScrollPositionToBoundsIfNecessary()
{
    FloatPoint scrollPosition = currentScrollPosition();
    FloatPoint constrainedPosition = scrollPosition.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
    immediateScrollBy(constrainedPosition - scrollPosition);
}

FloatPoint ThreadedScrollingTreeScrollingNodeDelegate::scrollOffset() const
{
    return ScrollableArea::scrollOffsetFromPosition(currentScrollPosition(), scrollOrigin());
}

float ThreadedScrollingTreeScrollingNodeDelegate::pageScaleFactor() const
{
    // FIXME: What should this return for non-root frames, and overflow?
    // Also, this should not have to access ScrollingTreeFrameScrollingNode.
    if (is<ScrollingTreeFrameScrollingNode>(scrollingNode()))
        return downcast<ScrollingTreeFrameScrollingNode>(scrollingNode()).frameScaleFactor();

    return 1;
}

void ThreadedScrollingTreeScrollingNodeDelegate::willStartAnimatedScroll()
{
    scrollingNode().willStartAnimatedScroll();
}

void ThreadedScrollingTreeScrollingNodeDelegate::didStopAnimatedScroll()
{
    scrollingNode().didStopAnimatedScroll();
}

void ThreadedScrollingTreeScrollingNodeDelegate::willStartWheelEventScroll()
{
    scrollingNode().willStartWheelEventScroll();
}

void ThreadedScrollingTreeScrollingNodeDelegate::didStopWheelEventScroll()
{
    scrollingNode().didStopWheelEventScroll();
}

void ThreadedScrollingTreeScrollingNodeDelegate::willStartScrollSnapAnimation()
{
    scrollingNode().setScrollSnapInProgress(true);
}

void ThreadedScrollingTreeScrollingNodeDelegate::didStopScrollSnapAnimation()
{
    scrollingNode().setScrollSnapInProgress(false);
}

ScrollExtents ThreadedScrollingTreeScrollingNodeDelegate::scrollExtents() const
{
    return {
        scrollingNode().totalContentsSize(),
        scrollingNode().scrollableAreaSize()
    };
}

void ThreadedScrollingTreeScrollingNodeDelegate::deferWheelEventTestCompletionForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason reason) const
{
    if (!scrollingTree().isMonitoringWheelEvents())
        return;

    // Just use the scrolling node ID as the identifier, since we know this is coming from a ScrollingEffectsController associated with this node.
    scrollingTree().deferWheelEventTestCompletionForReason(scrollingNode().scrollingNodeID(), reason);
}

void ThreadedScrollingTreeScrollingNodeDelegate::removeWheelEventTestCompletionDeferralForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason reason) const
{
    if (!scrollingTree().isMonitoringWheelEvents())
        return;

    // Just use the scrolling node ID as the identifier, since we know this is coming from a ScrollingEffectsController associated with this node.
    scrollingTree().removeWheelEventTestCompletionDeferralForReason(scrollingNode().scrollingNodeID(), reason);
}

FloatPoint ThreadedScrollingTreeScrollingNodeDelegate::adjustedScrollPosition(const FloatPoint& position) const
{
    return { roundf(position.x()), roundf(position.y()) };
}

void ThreadedScrollingTreeScrollingNodeDelegate::handleKeyboardScrollRequest(const RequestedKeyboardScrollData& scrollData)
{
    switch (scrollData.action) {
    case KeyboardScrollAction::StartAnimation:
        m_scrollController.startKeyboardScroll(*scrollData.keyboardScroll);
        break;
    case KeyboardScrollAction::StopWithAnimation:
        m_scrollController.finishKeyboardScroll(false);
        break;
    case KeyboardScrollAction::StopImmediately:
        m_scrollController.finishKeyboardScroll(true);
        break;
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
