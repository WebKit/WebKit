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
#import "ScrollExtents.h"
#import "ScrollingStateScrollingNode.h"
#import "ScrollingTree.h"
#import "ScrollingTreeFrameScrollingNode.h"
#import "ScrollingTreeScrollingNode.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>

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

void ScrollingTreeScrollingNodeDelegateMac::updateFromStateNode(const ScrollingStateScrollingNode& scrollingStateNode)
{
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::PainterForScrollbar)) {
        releaseReferencesToScrollerImpsOnTheMainThread();
        m_verticalScrollerImp = scrollingStateNode.verticalScrollerImp();
        m_horizontalScrollerImp = scrollingStateNode.horizontalScrollerImp();
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::SnapOffsetsInfo))
        m_scrollController.setSnapOffsetsInfo(scrollingStateNode.snapOffsetsInfo().convertUnits<LayoutScrollSnapOffsetsInfo>());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CurrentHorizontalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Horizontal, scrollingStateNode.currentHorizontalSnapPointIndex());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::CurrentVerticalSnapOffsetIndex))
        m_scrollController.setActiveScrollSnapIndexForAxis(ScrollEventAxis::Vertical, scrollingStateNode.currentVerticalSnapPointIndex());
}

std::optional<unsigned> ScrollingTreeScrollingNodeDelegateMac::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    return m_scrollController.activeScrollSnapIndexForAxis(axis);
}

bool ScrollingTreeScrollingNodeDelegateMac::activeScrollSnapIndexDidChange() const
{
    return m_scrollController.activeScrollSnapIndexDidChange();
}

bool ScrollingTreeScrollingNodeDelegateMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    bool wasInMomentumPhase = m_inMomentumPhase;

    if (wheelEvent.momentumPhase() == PlatformWheelEventPhase::Began)
        m_inMomentumPhase = true;
    else if (wheelEvent.momentumPhase() == PlatformWheelEventPhase::Ended || wheelEvent.momentumPhase() == PlatformWheelEventPhase::Cancelled)
        m_inMomentumPhase = false;
    
    if (wasInMomentumPhase != m_inMomentumPhase) {
        [m_verticalScrollerImp setUsePresentationValue:m_inMomentumPhase];
        [m_horizontalScrollerImp setUsePresentationValue:m_inMomentumPhase];
    }

    auto deferrer = WheelEventTestMonitorCompletionDeferrer { scrollingTree().wheelEventTestMonitor(), reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(scrollingNode().scrollingNodeID()), WheelEventTestMonitor::HandlingWheelEvent };

    bool wasInUserScroll = m_scrollController.isUserScrollInProgress();
    m_scrollController.updateGestureInProgressState(wheelEvent);
    bool isInUserScroll = m_scrollController.isUserScrollInProgress();
    if (isInUserScroll != wasInUserScroll)
        scrollingNode().setUserScrollInProgress(isInUserScroll);

    // PlatformWheelEventPhase::MayBegin fires when two fingers touch the trackpad, and is used to flash overlay scrollbars.
    // We know we're scrollable at this point, so handle the event.
    if (wheelEvent.phase() == PlatformWheelEventPhase::MayBegin)
        return true;

    return m_scrollController.handleWheelEvent(wheelEvent);
}

bool ScrollingTreeScrollingNodeDelegateMac::startAnimatedScrollToPosition(FloatPoint destinationPosition)
{
    auto currentOffset = ScrollableArea::scrollOffsetFromPosition(currentScrollPosition(), scrollOrigin());
    auto destinationOffset = ScrollableArea::scrollOffsetFromPosition(destinationPosition, scrollOrigin());
    
    return m_scrollController.startAnimatedScrollToDestination(currentOffset, destinationOffset);
}

void ScrollingTreeScrollingNodeDelegateMac::stopAnimatedScroll()
{
    m_scrollController.stopAnimatedScroll();
}

void ScrollingTreeScrollingNodeDelegateMac::willDoProgrammaticScroll(const FloatPoint& targetPosition)
{
    if (scrollPositionIsNotRubberbandingEdge(targetPosition)) {
        LOG(Scrolling, "ScrollingTreeScrollingNodeDelegateMac::willDoProgrammaticScroll() - scrolling away from rubberbanding edge so stopping rubberbanding");
        m_scrollController.stopRubberBanding();
    }
}

bool ScrollingTreeScrollingNodeDelegateMac::scrollPositionIsNotRubberbandingEdge(const FloatPoint& targetPosition) const
{
    if (!m_scrollController.isRubberBandInProgress())
        return false;

    auto rubberbandingEdges = m_scrollController.rubberBandingEdges();

    auto minimumScrollPosition = this->minimumScrollPosition();
    auto maximumScrollPosition = this->maximumScrollPosition();

    for (auto side : allBoxSides) {
        if (!rubberbandingEdges[side])
            continue;

        switch (side) {
        case BoxSide::Top:
            if (targetPosition.y() != minimumScrollPosition.y())
                return true;
            break;
        case BoxSide::Right:
            if (targetPosition.x() != maximumScrollPosition.x())
                return true;
            break;
        case BoxSide::Bottom:
            if (targetPosition.y() != maximumScrollPosition.y())
                return true;
            break;
        case BoxSide::Left:
            if (targetPosition.x() != minimumScrollPosition.x())
                return true;
            break;
        }
    }
    return false;
}

void ScrollingTreeScrollingNodeDelegateMac::currentScrollPositionChanged()
{
    m_scrollController.scrollPositionChanged();
}

bool ScrollingTreeScrollingNodeDelegateMac::isRubberBandInProgress() const
{
    return m_scrollController.isRubberBandInProgress();
}

bool ScrollingTreeScrollingNodeDelegateMac::isScrollSnapInProgress() const
{
    return m_scrollController.isScrollSnapInProgress();
}

std::unique_ptr<ScrollingEffectsControllerTimer> ScrollingTreeScrollingNodeDelegateMac::createTimer(Function<void()>&& function)
{
    // This is only used for a scroll snap timer.
    return WTF::makeUnique<ScrollingEffectsControllerTimer>(RunLoop::current(), [function = WTFMove(function), protectedNode = Ref { scrollingNode() }] {
        Locker locker { protectedNode->scrollingTree().treeLock() };
        function();
    });
}

void ScrollingTreeScrollingNodeDelegateMac::startAnimationCallback(ScrollingEffectsController&)
{
    scrollingNode().setScrollAnimationInProgress(true);
}

void ScrollingTreeScrollingNodeDelegateMac::stopAnimationCallback(ScrollingEffectsController&)
{
    scrollingNode().setScrollAnimationInProgress(false);
}

void ScrollingTreeScrollingNodeDelegateMac::serviceScrollAnimation()
{
    m_scrollController.animationCallback(MonotonicTime::now());
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        bool scrollbarsAllowStretching = allowsHorizontalScrolling() || !allowsVerticalScrolling();
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Horizontal);
        bool eventPreventsStretching = wheelEvent.isGestureStart() && relevantSide && isPinnedOnSide(*relevantSide);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed: {
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Horizontal);
        if (relevantSide)
            return shouldRubberBandOnSide(*relevantSide);
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
        bool scrollbarsAllowStretching = allowsVerticalScrolling() || !allowsHorizontalScrolling();
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Vertical);
        bool eventPreventsStretching = wheelEvent.isGestureStart() && relevantSide && isPinnedOnSide(*relevantSide);
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticityNone:
        return false;
    case ScrollElasticityAllowed: {
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Vertical);
        if (relevantSide)
            return shouldRubberBandOnSide(*relevantSide);
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

bool ScrollingTreeScrollingNodeDelegateMac::isPinnedOnSide(BoxSide side) const
{
    switch (side) {
    case BoxSide::Top:
        if (!allowsVerticalScrolling())
            return true;
        return currentScrollPosition().y() <= minimumScrollPosition().y();
    case BoxSide::Bottom:
        if (!allowsVerticalScrolling())
            return true;
        return currentScrollPosition().y() >= maximumScrollPosition().y();
    case BoxSide::Left:
        if (!allowsHorizontalScrolling())
            return true;
        return currentScrollPosition().x() <= minimumScrollPosition().x();
    case BoxSide::Right:
        if (!allowsHorizontalScrolling())
            return true;
        return currentScrollPosition().x() >= maximumScrollPosition().x();
    }
    return false;
}

RectEdges<bool> ScrollingTreeScrollingNodeDelegateMac::edgePinnedState() const
{
    return scrollingNode().edgePinnedState();
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsHorizontalScrolling() const
{
    return ScrollingTreeScrollingNodeDelegate::allowsHorizontalScrolling();
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsVerticalScrolling() const
{
    return ScrollingTreeScrollingNodeDelegate::allowsVerticalScrolling();
}

bool ScrollingTreeScrollingNodeDelegateMac::shouldRubberBandOnSide(BoxSide side) const
{
    if (scrollingNode().isRootNode())
        return scrollingTree().mainFrameCanRubberBandOnSide(side);

    switch (side) {
    case BoxSide::Top:
    case BoxSide::Bottom:
        return allowsVerticalScrolling();
    case BoxSide::Left:
    case BoxSide::Right:
        return allowsHorizontalScrolling();
    }
    return true;
}

void ScrollingTreeScrollingNodeDelegateMac::immediateScrollBy(const FloatSize& delta, ScrollClamping clamping)
{
    scrollingNode().scrollBy(delta, clamping);
}

void ScrollingTreeScrollingNodeDelegateMac::didStopRubberBandAnimation()
{
    // Since the rubberband timer has stopped, totalContentsSizeForRubberBand can be synchronized with totalContentsSize.
    scrollingNode().setTotalContentsSizeForRubberBand(totalContentsSize());
}

void ScrollingTreeScrollingNodeDelegateMac::rubberBandingStateChanged(bool inRubberBand)
{
    scrollingTree().setRubberBandingInProgressForNode(scrollingNode().scrollingNodeID(), inRubberBand);
}

void ScrollingTreeScrollingNodeDelegateMac::adjustScrollPositionToBoundsIfNecessary()
{
    FloatPoint scrollPosition = currentScrollPosition();
    FloatPoint constrainedPosition = scrollPosition.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
    immediateScrollBy(constrainedPosition - scrollPosition);
}

FloatPoint ScrollingTreeScrollingNodeDelegateMac::scrollOffset() const
{
    return ScrollableArea::scrollOffsetFromPosition(currentScrollPosition(), scrollOrigin());
}

float ScrollingTreeScrollingNodeDelegateMac::pageScaleFactor() const
{
    // FIXME: What should this return for non-root frames, and overflow?
    // Also, this should not have to access ScrollingTreeFrameScrollingNode.
    if (is<ScrollingTreeFrameScrollingNode>(scrollingNode()))
        return downcast<ScrollingTreeFrameScrollingNode>(scrollingNode()).frameScaleFactor();

    return 1;
}

void ScrollingTreeScrollingNodeDelegateMac::didStopAnimatedScroll()
{
    scrollingNode().didStopAnimatedScroll();
}

void ScrollingTreeScrollingNodeDelegateMac::willStartScrollSnapAnimation()
{
    scrollingNode().setScrollSnapInProgress(true);
}

void ScrollingTreeScrollingNodeDelegateMac::didStopScrollSnapAnimation()
{
    scrollingNode().setScrollSnapInProgress(false);
}
    
ScrollExtents ScrollingTreeScrollingNodeDelegateMac::scrollExtents() const
{
    return {
        scrollingNode().totalContentsSize(),
        scrollingNode().scrollableAreaSize()
    };
}

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
    if (m_inMomentumPhase && (m_verticalScrollerImp || m_horizontalScrollerImp)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        auto scrollOffset = scrollingNode().currentScrollOffset();

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
        END_BLOCK_OBJC_EXCEPTIONS
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
