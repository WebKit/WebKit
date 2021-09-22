/*
 * Copyright (C) 2010, 2011, 2015 Apple Inc. All rights reserved.
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
#import "ScrollAnimatorMac.h"

#if PLATFORM(MAC)

#import "Gradient.h"
#import "GraphicsLayer.h"
#import "Logging.h"
#import "PlatformWheelEvent.h"
#import "ScrollView.h"
#import "ScrollableArea.h"
#import "ScrollbarsController.h"
#import <wtf/text/TextStream.h>

namespace WebCore {

std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollAnimatorMac>(scrollableArea);
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollableArea& scrollableArea)
    : ScrollAnimator(scrollableArea)
{
}

ScrollAnimatorMac::~ScrollAnimatorMac() = default;

bool ScrollAnimatorMac::platformAllowsScrollAnimation() const
{
    static bool enabled = [[NSUserDefaults standardUserDefaults] boolForKey:@"NSScrollAnimationEnabled"];
    return enabled;
}

static bool rubberBandingEnabledForSystem()
{
    static bool initialized = false;
    static bool enabled = true;
    // Caches the result, which is consistent with other apps like the Finder, which all
    // require a restart after changing this default.
    if (!initialized) {
        // Uses -objectForKey: and not -boolForKey: in order to default to true if the value wasn't set.
        id value = [[NSUserDefaults standardUserDefaults] objectForKey:@"NSScrollViewRubberbanding"];
        if ([value isKindOfClass:[NSNumber class]])
            enabled = [value boolValue];
        initialized = true;
    }
    return enabled;
}

bool ScrollAnimatorMac::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier, OptionSet<ScrollBehavior> behavior)
{
    m_scrollableArea.scrollbarsController().setScrollbarAnimationsUnsuspendedByUserInteraction(true);

    return ScrollAnimator::scroll(orientation, granularity, step, multiplier, behavior);
}

FloatPoint ScrollAnimatorMac::adjustScrollPositionIfNecessary(const FloatPoint& position) const
{
    if (!m_scrollableArea.constrainsScrollingToContentEdge())
        return position;

    return m_scrollableArea.constrainScrollPosition(ScrollPosition(position));
}

void ScrollAnimatorMac::adjustScrollPositionToBoundsIfNecessary()
{
    bool currentlyConstrainsToContentEdge = m_scrollableArea.constrainsScrollingToContentEdge();
    m_scrollableArea.setConstrainsScrollingToContentEdge(true);

    ScrollPosition currentScrollPosition = m_scrollableArea.scrollPosition();
    ScrollPosition constrainedPosition = m_scrollableArea.constrainScrollPosition(currentScrollPosition);
    immediateScrollBy(constrainedPosition - currentScrollPosition);

    m_scrollableArea.setConstrainsScrollingToContentEdge(currentlyConstrainsToContentEdge);
}

bool ScrollAnimatorMac::isUserScrollInProgress() const
{
    return m_scrollController.isUserScrollInProgress();
}

bool ScrollAnimatorMac::isRubberBandInProgress() const
{
    return m_scrollController.isRubberBandInProgress();
}

bool ScrollAnimatorMac::isScrollSnapInProgress() const
{
    return m_scrollController.isScrollSnapInProgress();
}

void ScrollAnimatorMac::notifyPositionChanged(const FloatSize& delta)
{
    m_scrollableArea.scrollbarsController().notifyContentAreaScrolled(delta);
    ScrollAnimator::notifyPositionChanged(delta);
}

void ScrollAnimatorMac::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac " << this << " scrollableArea " << m_scrollableArea << " handleWheelEventPhase " << phase);

    // FIXME: Need to ensure we get PlatformWheelEventPhase::Ended.
    if (phase == PlatformWheelEventPhase::Began)
        m_scrollableArea.scrollbarsController().didBeginScrollGesture();
    else if (phase == PlatformWheelEventPhase::Ended || phase == PlatformWheelEventPhase::Cancelled)
        m_scrollableArea.scrollbarsController().didEndScrollGesture();
    else if (phase == PlatformWheelEventPhase::MayBegin)
        m_scrollableArea.scrollbarsController().mayBeginScrollGesture();
}

bool ScrollAnimatorMac::shouldForwardWheelEventsToParent(const PlatformWheelEvent& wheelEvent) const
{
    if (std::abs(wheelEvent.deltaY()) >= std::abs(wheelEvent.deltaX()))
        return !allowsVerticalStretching(wheelEvent);

    return !allowsHorizontalStretching(wheelEvent);
}
    
bool ScrollAnimatorMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    m_scrollableArea.scrollbarsController().setScrollbarAnimationsUnsuspendedByUserInteraction(true);

    if (!wheelEvent.hasPreciseScrollingDeltas() || !rubberBandingEnabledForSystem())
        return ScrollAnimator::handleWheelEvent(wheelEvent);

    m_scrollController.updateGestureInProgressState(wheelEvent);

    // FIXME: This is somewhat roundabout hack to allow forwarding wheel events
    // up to the parent scrollable area. It takes advantage of the fact that
    // the base class implementation of handleWheelEvent will not accept the
    // wheel event if there is nowhere to scroll.
    if (shouldForwardWheelEventsToParent(wheelEvent)) {
        bool didHandleEvent = ScrollAnimator::handleWheelEvent(wheelEvent);
        if (didHandleEvent || (!wheelEvent.deltaX() && !wheelEvent.deltaY()))
            handleWheelEventPhase(wheelEvent.phase());
        return didHandleEvent;
    }

    bool didHandleEvent = m_scrollController.handleWheelEvent(wheelEvent);

    if (didHandleEvent)
        handleWheelEventPhase(wheelEvent.phase());

    return didHandleEvent;
}

RectEdges<bool> ScrollAnimatorMac::edgePinnedState() const
{
    return m_scrollableArea.edgePinnedState();
}

bool ScrollAnimatorMac::isPinnedForScrollDelta(const FloatSize& delta) const
{
    if (fabsf(delta.height()) >= fabsf(delta.width()))
        return m_scrollableArea.isPinnedForScrollDeltaOnAxis(delta.height(), ScrollEventAxis::Vertical);

    if (delta.width())
        return m_scrollableArea.isPinnedForScrollDeltaOnAxis(delta.width(), ScrollEventAxis::Horizontal);

    return false;
}

static bool gestureShouldBeginSnap(const PlatformWheelEvent& wheelEvent, ScrollEventAxis axis, const LayoutScrollSnapOffsetsInfo* offsetInfo)
{
    if (!offsetInfo)
        return false;

    if (offsetInfo->offsetsForAxis(axis).isEmpty())
        return false;

    if (wheelEvent.phase() != PlatformWheelEventPhase::Ended && !wheelEvent.isEndOfMomentumScroll())
        return false;

    return true;
}

bool ScrollAnimatorMac::allowsVerticalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (m_scrollableArea.verticalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        Scrollbar* hScroller = m_scrollableArea.horizontalScrollbar();
        Scrollbar* vScroller = m_scrollableArea.verticalScrollbar();
        bool scrollbarsAllowStretching = ((vScroller && vScroller->enabled()) || (!hScroller || !hScroller->enabled()));
        bool eventPreventsStretching = m_scrollableArea.hasScrollableOrRubberbandableAncestor() && wheelEvent.isGestureStart() && m_scrollableArea.isPinnedForScrollDeltaOnAxis(-wheelEvent.deltaY(), ScrollEventAxis::Vertical);
        if (!eventPreventsStretching)
            eventPreventsStretching = gestureShouldBeginSnap(wheelEvent, ScrollEventAxis::Vertical, m_scrollableArea.snapOffsetsInfo());
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

bool ScrollAnimatorMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent) const
{
    switch (m_scrollableArea.horizontalScrollElasticity()) {
    case ScrollElasticityAutomatic: {
        Scrollbar* hScroller = m_scrollableArea.horizontalScrollbar();
        Scrollbar* vScroller = m_scrollableArea.verticalScrollbar();
        bool scrollbarsAllowStretching = ((hScroller && hScroller->enabled()) || (!vScroller || !vScroller->enabled()));
        bool eventPreventsStretching = m_scrollableArea.hasScrollableOrRubberbandableAncestor() && wheelEvent.isGestureStart() && m_scrollableArea.isPinnedForScrollDeltaOnAxis(-wheelEvent.deltaX(), ScrollEventAxis::Horizontal);
        if (!eventPreventsStretching)
            eventPreventsStretching = gestureShouldBeginSnap(wheelEvent, ScrollEventAxis::Horizontal, m_scrollableArea.snapOffsetsInfo());
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

IntSize ScrollAnimatorMac::stretchAmount() const
{
    return m_scrollableArea.overhangAmount();
}

bool ScrollAnimatorMac::allowsHorizontalScrolling() const
{
    return m_scrollableArea.allowsHorizontalScrolling();
}

bool ScrollAnimatorMac::allowsVerticalScrolling() const
{
    return m_scrollableArea.allowsVerticalScrolling();
}

bool ScrollAnimatorMac::shouldRubberBandInDirection(ScrollDirection) const
{
    return false;
}

void ScrollAnimatorMac::immediateScrollByWithoutContentEdgeConstraints(const FloatSize& delta)
{
    m_scrollableArea.setConstrainsScrollingToContentEdge(false);
    immediateScrollBy(delta);
    m_scrollableArea.setConstrainsScrollingToContentEdge(true);
}

void ScrollAnimatorMac::immediateScrollBy(const FloatSize& delta)
{
    FloatPoint currentPosition = this->currentPosition();
    FloatPoint newPosition = adjustScrollPositionIfNecessary(currentPosition + delta);
    if (newPosition == currentPosition)
        return;

    setCurrentPosition(newPosition, NotifyScrollableArea::Yes);
}

bool ScrollAnimatorMac::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    return m_scrollController.processWheelEventForScrollSnap(wheelEvent);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
