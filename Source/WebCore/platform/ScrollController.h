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

#pragma once

#if ENABLE(RUBBER_BANDING) || ENABLE(CSS_SCROLL_SNAP)

#include "FloatPoint.h"
#include "FloatSize.h"
#include "RectEdges.h"
#include "ScrollTypes.h"
#include "WheelEventTestMonitor.h"
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>

#if ENABLE(CSS_SCROLL_SNAP)
#include "ScrollSnapAnimatorState.h"
#include "ScrollSnapOffsetsInfo.h"
#endif

namespace WebCore {

class LayoutSize;
class PlatformWheelEvent;
class ScrollController;
class ScrollableArea;
class WheelEventTestMonitor;

class ScrollControllerTimer : public RunLoop::TimerBase {
public:
    ScrollControllerTimer(RunLoop& runLoop, Function<void()>&& callback)
        : RunLoop::TimerBase(runLoop)
        , m_callback(WTFMove(callback))
    {
    }

    void fired() final
    {
        m_callback();
    }

private:
    Function<void()> m_callback;
};

class ScrollControllerClient {
protected:
    virtual ~ScrollControllerClient() = default;

public:
    virtual std::unique_ptr<ScrollControllerTimer> createTimer(Function<void()>&&) = 0;

#if ENABLE(RUBBER_BANDING)
    virtual bool allowsHorizontalStretching(const PlatformWheelEvent&) const = 0;
    virtual bool allowsVerticalStretching(const PlatformWheelEvent&) const = 0;
    virtual IntSize stretchAmount() const = 0;

    virtual bool isPinnedForScrollDelta(const FloatSize&) const = 0;

    virtual RectEdges<bool> edgePinnedState() const = 0;

    virtual bool allowsHorizontalScrolling() const = 0;
    virtual bool allowsVerticalScrolling() const = 0;
    virtual bool shouldRubberBandInDirection(ScrollDirection) const = 0;

    // FIXME: use ScrollClamping to collapse these to one.
    virtual void immediateScrollBy(const FloatSize&) = 0;
    virtual void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) = 0;

    virtual void willStartRubberBandSnapAnimation() { }
    virtual void didStopRubberbandSnapAnimation() { }

    virtual void rubberBandingStateChanged(bool) { }

    // If the current scroll position is within the overhang area, this function will cause
    // the page to scroll to the nearest boundary point.
    virtual void adjustScrollPositionToBoundsIfNecessary() = 0;
#endif

    virtual void deferWheelEventTestCompletionForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason) const { /* Do nothing */ }
    virtual void removeWheelEventTestCompletionDeferralForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason) const { /* Do nothing */ }

#if ENABLE(CSS_SCROLL_SNAP)
    virtual FloatPoint scrollOffset() const = 0;
    virtual void immediateScrollOnAxis(ScrollEventAxis, float delta) = 0;
    virtual void willStartScrollSnapAnimation() { }
    virtual void didStopScrollSnapAnimation() { }

    virtual float pageScaleFactor() const = 0;

    virtual unsigned activeScrollOffsetIndex(ScrollEventAxis) const
    {
        return 0;
    }

    virtual LayoutSize scrollExtent() const = 0;
    virtual FloatSize viewportSize() const = 0;
#endif
};

class ScrollController {
    WTF_MAKE_NONCOPYABLE(ScrollController);

public:
    explicit ScrollController(ScrollControllerClient&);
    ~ScrollController();

    bool usesScrollSnap() const;
    void stopAllTimers();
    void scrollPositionChanged();

#if ENABLE(CSS_SCROLL_SNAP)
    void updateScrollSnapPoints(const ScrollSnapOffsetsInfo<LayoutUnit>&);
    void setActiveScrollSnapIndexForAxis(ScrollEventAxis, unsigned);
    void setActiveScrollSnapIndicesForOffset(ScrollOffset);
    bool activeScrollSnapIndexDidChange() const { return m_activeScrollSnapIndexDidChange; }
    void setScrollSnapIndexDidChange(bool state) { m_activeScrollSnapIndexDidChange = state; }
    unsigned activeScrollSnapIndexForAxis(ScrollEventAxis) const;
    void updateScrollSnapState(const ScrollableArea&);
    void updateGestureInProgressState(const PlatformWheelEvent&);
    float adjustScrollDestination(ScrollEventAxis, float destinationOffset, float velocity, Optional<float> originalOffset);
#endif

#if PLATFORM(MAC)
    // Returns true if handled.
    bool handleWheelEvent(const PlatformWheelEvent&);

    enum class WheelAxisBias { None, Vertical };
    static Optional<ScrollDirection> directionFromEvent(const PlatformWheelEvent&, Optional<ScrollEventAxis>, WheelAxisBias = WheelAxisBias::None);
    static FloatSize wheelDeltaBiasingTowardsVertical(const PlatformWheelEvent&);

    bool isScrollSnapInProgress() const;
    bool isUserScrollInProgress() const;

#if ENABLE(CSS_SCROLL_SNAP)
    // Returns true if handled.
    bool processWheelEventForScrollSnap(const PlatformWheelEvent&);
#endif

#if ENABLE(RUBBER_BANDING)
    void stopRubberbanding();
    bool isRubberBandInProgress() const;
    RectEdges<bool> rubberBandingEdges() const { return m_rubberBandingEdges; }
#endif
#endif

private:
#if ENABLE(CSS_SCROLL_SNAP)
    void setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis, int);
#endif

#if PLATFORM(MAC)
#if ENABLE(CSS_SCROLL_SNAP)
    void scrollSnapTimerFired();
    void startScrollSnapTimer();
    void stopScrollSnapTimer();
    bool shouldOverrideMomentumScrolling() const;
    void statelessSnapTransitionTimerFired();
    void scheduleStatelessScrollSnap();
    void startDeferringWheelEventTestCompletionDueToScrollSnapping();
    void stopDeferringWheelEventTestCompletionDueToScrollSnapping();
#endif

#if ENABLE(RUBBER_BANDING)
    void startSnapRubberbandTimer();
    void stopSnapRubberbandTimer();
    void snapRubberBand();
    void snapRubberBandTimerFired();
    bool shouldRubberBandInHorizontalDirection(const PlatformWheelEvent&) const;
    bool shouldRubberBandInDirection(ScrollDirection) const;
    bool isRubberBandInProgressInternal() const;
    void updateRubberBandingState();
    void updateRubberBandingEdges(IntSize clientStretch);
#endif
#endif

    ScrollControllerClient& m_client;
#if ENABLE(CSS_SCROLL_SNAP)
    std::unique_ptr<ScrollSnapAnimatorState> m_scrollSnapState;
    bool m_activeScrollSnapIndexDidChange { false };
#endif

#if PLATFORM(MAC)
    WallTime m_lastMomentumScrollTimestamp;
    FloatSize m_overflowScrollDelta;
    FloatSize m_stretchScrollForce;
    FloatSize m_momentumVelocity;

    bool m_inScrollGesture { false };
    bool m_momentumScrollInProgress { false };
    bool m_ignoreMomentumScrolls { false };
    bool m_isRubberBanding { false };

#if ENABLE(CSS_SCROLL_SNAP)
    FloatSize m_dragEndedScrollingVelocity;
    std::unique_ptr<ScrollControllerTimer> m_statelessSnapTransitionTimer;
    std::unique_ptr<ScrollControllerTimer> m_scrollSnapTimer;
#endif

#if ENABLE(RUBBER_BANDING)
    // Rubber band state.
    MonotonicTime m_startTime;
    FloatSize m_startStretch;
    FloatSize m_origVelocity;
    RectEdges<bool> m_rubberBandingEdges;
    std::unique_ptr<ScrollControllerTimer> m_snapRubberbandTimer;
#endif

#if ASSERT_ENABLED
    bool m_timersWereStopped { false };
#endif
#endif
};

} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING)
