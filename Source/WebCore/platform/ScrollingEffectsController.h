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

#include "FloatPoint.h"
#include "FloatSize.h"

#include "RectEdges.h"
#include "ScrollAnimation.h"
#include "ScrollSnapAnimatorState.h"
#include "ScrollSnapOffsetsInfo.h"
#include "ScrollTypes.h"
#include "WheelEventTestMonitor.h"
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>

namespace WebCore {

class KeyboardScrollingAnimator;
class LayoutSize;
class PlatformWheelEvent;
class ScrollSnapAnimatorState;
class ScrollingEffectsController;
class ScrollableArea;
class WheelEventTestMonitor;

class ScrollingEffectsControllerTimer : public RunLoop::TimerBase {
public:
    ScrollingEffectsControllerTimer(RunLoop& runLoop, Function<void()>&& callback)
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

class ScrollingEffectsControllerClient {
protected:
    virtual ~ScrollingEffectsControllerClient() = default;

public:
    // Only used for non-animation timers.
    virtual std::unique_ptr<ScrollingEffectsControllerTimer> createTimer(Function<void()>&&) = 0;

    virtual void startAnimationCallback(ScrollingEffectsController&) = 0;
    virtual void stopAnimationCallback(ScrollingEffectsController&) = 0;

    virtual void updateKeyboardScrollPosition(MonotonicTime) { }
    virtual KeyboardScrollingAnimator *keyboardScrollingAnimator() const { return nullptr; }

    virtual bool allowsHorizontalScrolling() const = 0;
    virtual bool allowsVerticalScrolling() const = 0;

    // FIXME: Maybe ScrollBehaviorStatus should be stored on ScrollingEffectsController.
    virtual void setScrollBehaviorStatus(ScrollBehaviorStatus) = 0;
    virtual ScrollBehaviorStatus scrollBehaviorStatus() const = 0;

    virtual void immediateScrollBy(const FloatSize&) = 0;
    virtual void immediateScrollByWithoutContentEdgeConstraints(const FloatSize&) = 0;

    // If the current scroll position is within the overhang area, this function will cause
    // the page to scroll to the nearest boundary point.
    virtual void adjustScrollPositionToBoundsIfNecessary() = 0;

#if HAVE(RUBBER_BANDING)
    virtual bool allowsHorizontalStretching(const PlatformWheelEvent&) const = 0;
    virtual bool allowsVerticalStretching(const PlatformWheelEvent&) const = 0;
    virtual IntSize stretchAmount() const = 0;

    virtual bool isPinnedForScrollDelta(const FloatSize&) const = 0;

    virtual RectEdges<bool> edgePinnedState() const = 0;

    virtual bool shouldRubberBandInDirection(ScrollDirection) const = 0;

    // FIXME: use ScrollClamping to collapse these to one.
    virtual void willStartRubberBandSnapAnimation() { }
    virtual void didStopRubberbandSnapAnimation() { }

    virtual void rubberBandingStateChanged(bool) { }
#endif

    virtual void deferWheelEventTestCompletionForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason) const { /* Do nothing */ }
    virtual void removeWheelEventTestCompletionDeferralForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason) const { /* Do nothing */ }

    virtual FloatPoint scrollOffset() const = 0;
    virtual void willStartScrollSnapAnimation() { }
    virtual void didStopScrollSnapAnimation() { }
    virtual float pageScaleFactor() const = 0;
    virtual ScrollExtents scrollExtents() const = 0;
};

class ScrollingEffectsController : public ScrollAnimationClient {
    WTF_MAKE_NONCOPYABLE(ScrollingEffectsController);

public:
    explicit ScrollingEffectsController(ScrollingEffectsControllerClient&);
    virtual ~ScrollingEffectsController();

    bool usesScrollSnap() const;
    void stopAllTimers();
    void scrollPositionChanged();

    bool startAnimatedScrollToDestination(FloatPoint startOffset, FloatPoint destinationOffset);
    bool regargetAnimatedScroll(FloatPoint newDestinationOffset);
    void stopAnimatedScroll();

    // FIXME: Hack for ScrollAnimatorGeneric. Needs cleanup.
    bool processWheelEventForKineticScrolling(const PlatformWheelEvent&);

    bool startMomentumScrollWithInitialVelocity(const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta, const WTF::Function<FloatPoint(const FloatPoint&)>& destinationModifier);

    void beginKeyboardScrolling();
    void stopKeyboardScrolling();
    
    // Should be called periodically by the client. Started by startAnimationCallback(), stopped by stopAnimationCallback().
    void animationCallback(MonotonicTime);

    void updateGestureInProgressState(const PlatformWheelEvent&);
    
    void contentsSizeChanged();

    void setSnapOffsetsInfo(const LayoutScrollSnapOffsetsInfo&);
    const LayoutScrollSnapOffsetsInfo* snapOffsetsInfo() const;
    void setActiveScrollSnapIndexForAxis(ScrollEventAxis, std::optional<unsigned>);
    void updateActiveScrollSnapIndexForClientOffset();
    void resnapAfterLayout();

    std::optional<unsigned> activeScrollSnapIndexForAxis(ScrollEventAxis) const;
    float adjustedScrollDestination(ScrollEventAxis, FloatPoint destinationOffset, float velocity, std::optional<float> originalOffset) const;

    bool activeScrollSnapIndexDidChange() const { return m_activeScrollSnapIndexDidChange; }
    // FIXME: This is never called. We never set m_activeScrollSnapIndexDidChange back to false.
    void setScrollSnapIndexDidChange(bool state) { m_activeScrollSnapIndexDidChange = state; }

#if PLATFORM(MAC)
    // Returns true if handled.
    bool handleWheelEvent(const PlatformWheelEvent&);

    enum class WheelAxisBias { None, Vertical };
    static std::optional<ScrollDirection> directionFromEvent(const PlatformWheelEvent&, std::optional<ScrollEventAxis>, WheelAxisBias = WheelAxisBias::None);
    static FloatSize wheelDeltaBiasingTowardsVertical(const PlatformWheelEvent&);

    bool isScrollSnapInProgress() const;
    bool isUserScrollInProgress() const;

    // Returns true if handled.
    bool processWheelEventForScrollSnap(const PlatformWheelEvent&);

    void stopRubberbanding();
    bool isRubberBandInProgress() const;
    RectEdges<bool> rubberBandingEdges() const { return m_rubberBandingEdges; }
#endif

private:
    void updateRubberBandAnimatingState(MonotonicTime);
    void updateKeyboardScrollingAnimatingState(MonotonicTime);

    void setIsAnimatingRubberBand(bool);
    void setIsAnimatingScrollSnap(bool);
    void setIsAnimatingKeyboardScrolling(bool);

    void startScrollSnapAnimation();
    void stopScrollSnapAnimation();

#if PLATFORM(MAC)
    bool shouldOverrideMomentumScrolling() const;
    void statelessSnapTransitionTimerFired();
    void scheduleStatelessScrollSnap();
    void startDeferringWheelEventTestCompletionDueToScrollSnapping();
    void stopDeferringWheelEventTestCompletionDueToScrollSnapping();

    void startRubberbandAnimation();
    void stopSnapRubberbandAnimation();

    void snapRubberBand();
    bool shouldRubberBandInHorizontalDirection(const PlatformWheelEvent&) const;
    bool shouldRubberBandInDirection(ScrollDirection) const;
    bool isRubberBandInProgressInternal() const;
    void updateRubberBandingState();
    void updateRubberBandingEdges(IntSize clientStretch);
#endif

    void startOrStopAnimationCallbacks();
    void scrollToOffsetForAnimation(const FloatPoint& scrollOffset);

    // ScrollAnimationClient
    void scrollAnimationDidUpdate(ScrollAnimation&, const FloatPoint& /* currentOffset */) final;
    void scrollAnimationWillStart(ScrollAnimation&) final;
    void scrollAnimationDidEnd(ScrollAnimation&) final;
    ScrollExtents scrollExtentsForAnimation(ScrollAnimation&)  final;

    ScrollingEffectsControllerClient& m_client;

    std::unique_ptr<ScrollAnimation> m_currentAnimation;

    std::unique_ptr<ScrollSnapAnimatorState> m_scrollSnapState;
    bool m_activeScrollSnapIndexDidChange { false };

    bool m_isRunningAnimatingCallback { false };
    bool m_isAnimatingRubberBand { false };
    bool m_isAnimatingScrollSnap { false };
    bool m_isAnimatingKeyboardScrolling { false };

#if PLATFORM(MAC)
    WallTime m_lastMomentumScrollTimestamp;
    FloatSize m_unappliedOverscrollDelta;
    FloatSize m_stretchScrollForce;
    FloatSize m_momentumVelocity;

    bool m_inScrollGesture { false };
    bool m_momentumScrollInProgress { false };
    bool m_ignoreMomentumScrolls { false };
    bool m_isRubberBanding { false };

    FloatSize m_dragEndedScrollingVelocity;
    std::unique_ptr<ScrollingEffectsControllerTimer> m_statelessSnapTransitionTimer;

#if HAVE(RUBBER_BANDING)
    // Rubber band state.
    MonotonicTime m_startTime;
    FloatSize m_startStretch;
    FloatSize m_origVelocity;
    RectEdges<bool> m_rubberBandingEdges;
#endif

#if ASSERT_ENABLED
    bool m_timersWereStopped { false };
#endif
#endif
};

} // namespace WebCore
