/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
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

#ifndef ScrollController_h
#define ScrollController_h

#if ENABLE(RUBBER_BANDING) || ENABLE(CSS_SCROLL_SNAP)

#include "FloatPoint.h"
#include "FloatSize.h"
#include "ScrollTypes.h"
#include "WheelEventTestTrigger.h"
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>

#if ENABLE(CSS_SCROLL_SNAP)
#include "ScrollSnapAnimatorState.h"
#include "ScrollSnapOffsetsInfo.h"
#endif

namespace WebCore {

class LayoutSize;
class PlatformWheelEvent;
class ScrollableArea;
class WheelEventTestTrigger;

class ScrollControllerClient {
protected:
    virtual ~ScrollControllerClient() = default;

public:
#if ENABLE(RUBBER_BANDING)
    virtual bool allowsHorizontalStretching(const PlatformWheelEvent&) = 0;
    virtual bool allowsVerticalStretching(const PlatformWheelEvent&) = 0;
    virtual IntSize stretchAmount() = 0;
    virtual bool pinnedInDirection(const FloatSize&) = 0;
    virtual bool canScrollHorizontally() = 0;
    virtual bool canScrollVertically() = 0;
    virtual bool shouldRubberBandInDirection(ScrollDirection) = 0;

    // FIXME: use ScrollClamping to collapse these to one.
    virtual void immediateScrollBy(const FloatSize&) = 0;
    virtual void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) = 0;
    virtual void startSnapRubberbandTimer()
    {
        // Override to perform client-specific snap start logic
    }

    virtual void stopSnapRubberbandTimer()
    {
        // Override to perform client-specific snap end logic
    }
    
    // If the current scroll position is within the overhang area, this function will cause
    // the page to scroll to the nearest boundary point.
    virtual void adjustScrollPositionToBoundsIfNecessary() = 0;
#endif

    virtual void deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) const { /* Do nothing */ }
    virtual void removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) const { /* Do nothing */ }

#if ENABLE(CSS_SCROLL_SNAP)
    virtual FloatPoint scrollOffset() const = 0;
    virtual void immediateScrollOnAxis(ScrollEventAxis, float delta) = 0;
    virtual void startScrollSnapTimer()
    {
        // Override to perform client-specific scroll snap point start logic
    }

    virtual void stopScrollSnapTimer()
    {
        // Override to perform client-specific scroll snap point end logic
    }

    virtual float pageScaleFactor() const
    {
        return 1.0f;
    }

    virtual unsigned activeScrollOffsetIndex(ScrollEventAxis) const
    {
        return 0;
    }

    virtual LayoutSize scrollExtent() const = 0;
    virtual FloatSize viewportSize() const = 0;
#endif
};

enum class WheelEventStatus {
    UserScrollBegin,
    UserScrolling,
    UserScrollEnd,
    InertialScrollBegin,
    InertialScrolling,
    InertialScrollEnd,
    StatelessScrollEvent,
    Unknown
};

class ScrollController {
    WTF_MAKE_NONCOPYABLE(ScrollController);

public:
    explicit ScrollController(ScrollControllerClient&);

#if PLATFORM(MAC)
    bool handleWheelEvent(const PlatformWheelEvent&);
#endif

    bool isRubberBandInProgress() const;
    bool isScrollSnapInProgress() const;

#if ENABLE(CSS_SCROLL_SNAP)
    void updateScrollSnapPoints(ScrollEventAxis, const Vector<LayoutUnit>&, const Vector<ScrollOffsetRange<LayoutUnit>>&);
    void setActiveScrollSnapIndexForAxis(ScrollEventAxis, unsigned);
    void setActiveScrollSnapIndicesForOffset(int x, int y);
    bool activeScrollSnapIndexDidChange() const { return m_activeScrollSnapIndexDidChange; }
    void setScrollSnapIndexDidChange(bool state) { m_activeScrollSnapIndexDidChange = state; }
    unsigned activeScrollSnapIndexForAxis(ScrollEventAxis) const;
    void updateScrollSnapState(const ScrollableArea&);
#if PLATFORM(MAC)
    bool processWheelEventForScrollSnap(const PlatformWheelEvent&);
#endif
#endif

private:
#if ENABLE(RUBBER_BANDING)
    void startSnapRubberbandTimer();
    void stopSnapRubberbandTimer();
    void snapRubberBand();
    void snapRubberBandTimerFired();

    bool shouldRubberBandInHorizontalDirection(const PlatformWheelEvent&);
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    void setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis, int);
#if PLATFORM(MAC)
    void scrollSnapTimerFired();
    void startScrollSnapTimer();
    void stopScrollSnapTimer();

    bool shouldOverrideInertialScrolling() const;
    void statelessSnapTransitionTimerFired();
    void startDeferringTestsDueToScrollSnapping();
    void stopDeferringTestsDueToScrollSnapping();
    void scheduleStatelessScrollSnap();
#endif
#endif

    ScrollControllerClient& m_client;

#if PLATFORM(MAC)
    CFTimeInterval m_lastMomentumScrollTimestamp { 0 };
#endif
    FloatSize m_overflowScrollDelta;
    FloatSize m_stretchScrollForce;
    FloatSize m_momentumVelocity;

#if ENABLE(RUBBER_BANDING)
    // Rubber band state.
    CFTimeInterval m_startTime { 0 };
    FloatSize m_startStretch;
    FloatSize m_origVelocity;
    RunLoop::Timer<ScrollController> m_snapRubberbandTimer;
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    std::unique_ptr<ScrollSnapAnimatorState> m_scrollSnapState;
#if PLATFORM(MAC)
    FloatSize m_dragEndedScrollingVelocity;
    RunLoop::Timer<ScrollController> m_statelessSnapTransitionTimer;
    RunLoop::Timer<ScrollController> m_scrollSnapTimer;
#endif
#endif

#if PLATFORM(MAC)
    bool m_inScrollGesture { false };
    bool m_momentumScrollInProgress { false };
    bool m_ignoreMomentumScrolls { false };
    bool m_snapRubberbandTimerIsActive { false };
#endif

    bool m_activeScrollSnapIndexDidChange { false };
};
    
} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING)

#endif // ScrollController_h
