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
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollAnimatorMac);

std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollAnimatorMac>(scrollableArea);
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollableArea& scrollableArea)
    : ScrollAnimator(scrollableArea)
{
}

ScrollAnimatorMac::~ScrollAnimatorMac() = default;

bool ScrollAnimatorMac::isRubberBandInProgress() const
{
    return m_scrollController.isRubberBandInProgress();
}

void ScrollAnimatorMac::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollAnimatorMac " << this << " scrollableArea " << m_scrollableArea << " handleWheelEventPhase " << phase);

    // FIXME: Need to ensure we get PlatformWheelEventPhase::Ended.
    if (phase == PlatformWheelEventPhase::Began)
        checkedScrollableArea()->scrollbarsController().didBeginScrollGesture();
    else if (phase == PlatformWheelEventPhase::Ended || phase == PlatformWheelEventPhase::Cancelled)
        checkedScrollableArea()->scrollbarsController().didEndScrollGesture();
    else if (phase == PlatformWheelEventPhase::MayBegin)
        checkedScrollableArea()->scrollbarsController().mayBeginScrollGesture();
}

bool ScrollAnimatorMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    checkedScrollableArea()->scrollbarsController().setScrollbarAnimationsUnsuspendedByUserInteraction(true);
    m_scrollController.updateGestureInProgressState(wheelEvent);

    // Events in the PlatformWheelEventPhase::MayBegin phase have no deltas, and therefore never passes through the scroll handling logic below.
    // This causes us to return with an 'unhandled' return state, even though this event was successfully processed.
    //
    // We receive at least one PlatformWheelEventPhase::MayBegin when starting main-thread scrolling (see LocalFrameView::wheelEvent), which can
    // fool the scrolling thread into attempting to handle the scroll, unless we treat the event as handled here.
    if (wheelEvent.phase() == PlatformWheelEventPhase::MayBegin)
        return true;

    bool didHandleEvent = ScrollAnimator::handleWheelEvent(wheelEvent);
    if (didHandleEvent || wheelEvent.delta().isZero())
        handleWheelEventPhase(wheelEvent.phase());

    return didHandleEvent;
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
    CheckedRef scrollableArea = m_scrollableArea.get();
    if (scrollableArea->verticalOverscrollBehavior() == OverscrollBehavior::None)
        return false;
    
    switch (scrollableArea->verticalScrollElasticity()) {
    case ScrollElasticity::Automatic: {
        RefPtr hScroller = scrollableArea->horizontalScrollbar();
        RefPtr vScroller = scrollableArea->verticalScrollbar();
        bool scrollbarsAllowStretching = ((vScroller && vScroller->enabled()) || (!hScroller || !hScroller->enabled()));
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Vertical);
        bool eventPreventsStretching = scrollableArea->hasScrollableOrRubberbandableAncestor() && wheelEvent.isGestureStart() && relevantSide && scrollableArea->isPinnedOnSide(*relevantSide);
        if (!eventPreventsStretching)
            eventPreventsStretching = gestureShouldBeginSnap(wheelEvent, ScrollEventAxis::Vertical, scrollableArea->snapOffsetsInfo());
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticity::None:
        return false;
    case ScrollElasticity::Allowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollAnimatorMac::allowsHorizontalStretching(const PlatformWheelEvent& wheelEvent) const
{
    CheckedRef scrollableArea = m_scrollableArea.get();
    if (scrollableArea->horizontalOverscrollBehavior() == OverscrollBehavior::None)
        return false;
    
    switch (scrollableArea->horizontalScrollElasticity()) {
    case ScrollElasticity::Automatic: {
        RefPtr hScroller = scrollableArea->horizontalScrollbar();
        RefPtr vScroller = scrollableArea->verticalScrollbar();
        bool scrollbarsAllowStretching = ((hScroller && hScroller->enabled()) || (!vScroller || !vScroller->enabled()));
        auto relevantSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Horizontal);
        bool eventPreventsStretching = scrollableArea->hasScrollableOrRubberbandableAncestor() && wheelEvent.isGestureStart() && relevantSide && scrollableArea->isPinnedOnSide(*relevantSide);
        if (!eventPreventsStretching)
            eventPreventsStretching = gestureShouldBeginSnap(wheelEvent, ScrollEventAxis::Horizontal, scrollableArea->snapOffsetsInfo());
        return scrollbarsAllowStretching && !eventPreventsStretching;
    }
    case ScrollElasticity::None:
        return false;
    case ScrollElasticity::Allowed:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ScrollAnimatorMac::isScrollDeltaOpposingStretch(ScrollEventAxis axis, float delta) const
{
    return ScrollingEffectsController::isScrollDeltaOpposingStretch(stretchAmount(), axis, delta);
}

bool ScrollAnimatorMac::shouldRubberBandOnSide(BoxSide, FloatSize) const
{
    return false;
}

bool ScrollAnimatorMac::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    return m_scrollController.processWheelEventForScrollSnap(wheelEvent);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
