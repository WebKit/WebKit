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
#endif

namespace WebCore {

class LayoutSize;
class PlatformWheelEvent;
class ScrollableArea;
class WheelEventTestTrigger;

class ScrollControllerClient {
protected:
    virtual ~ScrollControllerClient() { }

public:
#if ENABLE(RUBBER_BANDING)
    virtual bool allowsHorizontalStretching(const PlatformWheelEvent&) = 0;
    virtual bool allowsVerticalStretching(const PlatformWheelEvent&) = 0;
    virtual IntSize stretchAmount() = 0;
    virtual bool pinnedInDirection(const FloatSize&) = 0;
    virtual bool canScrollHorizontally() = 0;
    virtual bool canScrollVertically() = 0;
    virtual bool shouldRubberBandInDirection(ScrollDirection) = 0;

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
    virtual LayoutUnit scrollOffsetOnAxis(ScrollEventAxis) const = 0;
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
#endif
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
    void updateScrollSnapPoints(ScrollEventAxis, const Vector<LayoutUnit>&);
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
    LayoutUnit scrollOffsetOnAxis(ScrollEventAxis) const;
    void setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis, int);
    ScrollSnapAnimatorState& scrollSnapPointState(ScrollEventAxis);
    const ScrollSnapAnimatorState& scrollSnapPointState(ScrollEventAxis) const;
#if PLATFORM(MAC)
    void scrollSnapTimerFired();
    void startScrollSnapTimer();
    void stopScrollSnapTimer();

    void processWheelEventForScrollSnapOnAxis(ScrollEventAxis, const PlatformWheelEvent&);
    bool shouldOverrideWheelEvent(ScrollEventAxis, const PlatformWheelEvent&) const;

    void beginScrollSnapAnimation(ScrollEventAxis, ScrollSnapState);
    
    void endScrollSnapAnimation(ScrollSnapState);
    void initializeScrollSnapAnimationParameters();
    bool isSnappingOnAxis(ScrollEventAxis) const;
    
#endif
#endif

    ScrollControllerClient& m_client;
    
    CFTimeInterval m_lastMomentumScrollTimestamp { 0 };
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
    bool m_expectingHorizontalStatelessScrollSnap { false };
    bool m_expectingVerticalStatelessScrollSnap { false };
    std::unique_ptr<ScrollSnapAnimatorState> m_horizontalScrollSnapState;
    std::unique_ptr<ScrollSnapAnimatorState> m_verticalScrollSnapState;
    std::unique_ptr<ScrollSnapAnimationCurveState> m_scrollSnapCurveState;
#if PLATFORM(MAC)
    RunLoop::Timer<ScrollController> m_scrollSnapTimer;
#endif
#endif

    bool m_inScrollGesture { false };
    bool m_momentumScrollInProgress { false };
    bool m_ignoreMomentumScrolls { false };
    bool m_snapRubberbandTimerIsActive { false };
    bool m_activeScrollSnapIndexDidChange { false };
};
    
} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING)

#endif // ScrollController_h
