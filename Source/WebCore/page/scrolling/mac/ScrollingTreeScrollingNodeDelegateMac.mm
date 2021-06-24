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

void ScrollingTreeScrollingNodeDelegateMac::willDoProgrammaticScroll(const FloatPoint& targetPosition)
{
    if (scrollPositionIsNotRubberbandingEdge(targetPosition)) {
        LOG(Scrolling, "ScrollingTreeScrollingNodeDelegateMac::willDoProgrammaticScroll() - scrolling away from rubberbanding edge so stopping rubberbanding");
        m_scrollController.stopRubberbanding();
    }
}

bool ScrollingTreeScrollingNodeDelegateMac::scrollPositionIsNotRubberbandingEdge(const FloatPoint& targetPosition) const
{
#if ENABLE(RUBBER_BANDING)
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
#else
    UNUSED_PARAM(targetPosition);
#endif
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

bool ScrollingTreeScrollingNodeDelegateMac::isPinnedForScrollDeltaOnAxis(float scrollDelta, ScrollEventAxis axis, float scrollLimit) const
{
    auto scrollPosition = currentScrollPosition();
    switch (axis) {
    case ScrollEventAxis::Vertical:
        if (!allowsVerticalScrolling())
            return true;

        if (scrollDelta < 0) {
            auto topOffset = scrollPosition.y() - minimumScrollPosition().y();
            return topOffset <= scrollLimit;
        }

        if (scrollDelta > 0) {
            auto bottomOffset = maximumScrollPosition().y() - scrollPosition.y();
            return bottomOffset <= scrollLimit;
        }
        break;
    case ScrollEventAxis::Horizontal:
        if (!allowsHorizontalScrolling())
            return true;

        if (scrollDelta < 0) {
            auto leftOffset = scrollPosition.x() - minimumScrollPosition().x();
            return leftOffset <= scrollLimit;
        }

        if (scrollDelta > 0) {
            auto rightOffset = maximumScrollPosition().x() - scrollPosition.x();
            return rightOffset <= scrollLimit;
        }
        break;
    }

    return false;
}

std::unique_ptr<ScrollControllerTimer> ScrollingTreeScrollingNodeDelegateMac::createTimer(Function<void()>&& function)
{
    return WTF::makeUnique<ScrollControllerTimer>(RunLoop::current(), [function = WTFMove(function), protectedNode = makeRef(scrollingNode())] {
        Locker locker { protectedNode->scrollingTree().treeLock() };
        function();
    });
}

void ScrollingTreeScrollingNodeDelegateMac::startAnimationCallback(ScrollController&)
{
    if (!m_scrollControllerAnimationTimer)
        m_scrollControllerAnimationTimer = WTF::makeUnique<RunLoop::Timer<ScrollingTreeScrollingNodeDelegateMac>>(RunLoop::current(), this, &ScrollingTreeScrollingNodeDelegateMac::scrollControllerAnimationTimerFired);

    if (m_scrollControllerAnimationTimer->isActive())
        return;

    m_scrollControllerAnimationTimer->startRepeating(1_s / 60.);
}

void ScrollingTreeScrollingNodeDelegateMac::stopAnimationCallback(ScrollController&)
{
    if (m_scrollControllerAnimationTimer)
        m_scrollControllerAnimationTimer->stop();
}

void ScrollingTreeScrollingNodeDelegateMac::scrollControllerAnimationTimerFired()
{
    m_scrollController.animationCallback(MonotonicTime::now());
}

bool ScrollingTreeScrollingNodeDelegateMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        bool scrollbarsAllowStretching = allowsHorizontalScrolling() || !allowsVerticalScrolling();
        bool eventPreventsStretching = wheelEvent.isGestureStart() && isPinnedForScrollDeltaOnAxis(-wheelEvent.deltaX(), ScrollEventAxis::Horizontal);
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
        bool scrollbarsAllowStretching = allowsVerticalScrolling() || !allowsHorizontalScrolling();
        bool eventPreventsStretching = wheelEvent.isGestureStart() && isPinnedForScrollDeltaOnAxis(-wheelEvent.deltaY(), ScrollEventAxis::Vertical);
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

bool ScrollingTreeScrollingNodeDelegateMac::isPinnedForScrollDelta(const FloatSize& delta) const
{
    // This "offset < 1" logic was added in r107488. Unclear if it's needed.
    constexpr float scrollOffsetLimit = 1.0f - std::numeric_limits<float>::epsilon();

    if (fabsf(delta.height()) >= fabsf(delta.width()))
        return isPinnedForScrollDeltaOnAxis(delta.height(), ScrollEventAxis::Vertical, scrollOffsetLimit);

    if (delta.width())
        return isPinnedForScrollDeltaOnAxis(delta.width(), ScrollEventAxis::Horizontal, scrollOffsetLimit);

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

bool ScrollingTreeScrollingNodeDelegateMac::shouldRubberBandInDirection(ScrollDirection direction) const
{
    if (scrollingNode().isRootNode())
        return scrollingTree().mainFrameCanRubberBandInDirection(direction);

    switch (direction) {
    case ScrollDirection::ScrollUp:
    case ScrollDirection::ScrollDown:
        return allowsVerticalScrolling();
    case ScrollDirection::ScrollLeft:
    case ScrollDirection::ScrollRight:
        return allowsHorizontalScrolling();
    }
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
    scrollingNode().setScrollSnapInProgress(true);
}

void ScrollingTreeScrollingNodeDelegateMac::didStopScrollSnapAnimation()
{
    scrollingNode().setScrollSnapInProgress(false);
}
    
LayoutSize ScrollingTreeScrollingNodeDelegateMac::scrollExtent() const
{
    return LayoutSize(totalContentsSize());
}

FloatSize ScrollingTreeScrollingNodeDelegateMac::viewportSize() const
{
    return scrollableAreaSize();
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
