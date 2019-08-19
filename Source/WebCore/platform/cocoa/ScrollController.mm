/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#import "ScrollController.h"

#import "LayoutSize.h"
#import "PlatformWheelEvent.h"
#import "WheelEventTestTrigger.h"
#import <sys/sysctl.h>
#import <sys/time.h>

#if ENABLE(CSS_SCROLL_SNAP)
#import "ScrollSnapAnimatorState.h"
#import "ScrollableArea.h"
#endif

#if PLATFORM(MAC)
#import <pal/spi/mac/NSScrollViewSPI.h>
#endif

#if ENABLE(RUBBER_BANDING) || ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

#if ENABLE(RUBBER_BANDING)
static const float scrollVelocityZeroingTimeout = 0.10f;
static const float rubberbandDirectionLockStretchRatio = 1;
static const float rubberbandMinimumRequiredDeltaBeforeStretch = 10;
#endif

#if PLATFORM(MAC)
static float elasticDeltaForTimeDelta(float initialPosition, float initialVelocity, float elapsedTime)
{
    return _NSElasticDeltaForTimeDelta(initialPosition, initialVelocity, elapsedTime);
}

static float elasticDeltaForReboundDelta(float delta)
{
    return _NSElasticDeltaForReboundDelta(delta);
}

static float reboundDeltaForElasticDelta(float delta)
{
    return _NSReboundDeltaForElasticDelta(delta);
}

static float scrollWheelMultiplier()
{
    static float multiplier = -1;
    if (multiplier < 0) {
        multiplier = [[NSUserDefaults standardUserDefaults] floatForKey:@"NSScrollWheelMultiplier"];
        if (multiplier <= 0)
            multiplier = 1;
    }
    return multiplier;
}
#endif

static ScrollEventAxis otherScrollEventAxis(ScrollEventAxis axis)
{
    return axis == ScrollEventAxis::Horizontal ? ScrollEventAxis::Vertical : ScrollEventAxis::Horizontal;
}

ScrollController::ScrollController(ScrollControllerClient& client)
    : m_client(client)
#if ENABLE(RUBBER_BANDING)
    , m_snapRubberbandTimer(RunLoop::current(), this, &ScrollController::snapRubberBandTimerFired)
#endif
#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
    , m_statelessSnapTransitionTimer(RunLoop::current(), this, &ScrollController::statelessSnapTransitionTimerFired)
    , m_scrollSnapTimer(RunLoop::current(), this, &ScrollController::scrollSnapTimerFired)
#endif
{
}

#if PLATFORM(MAC)
bool ScrollController::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
#if ENABLE(CSS_SCROLL_SNAP)
    if (!processWheelEventForScrollSnap(wheelEvent))
        return false;
#endif
    if (wheelEvent.phase() == PlatformWheelEventPhaseBegan) {
        // First, check if we should rubber-band at all.
        if (m_client.pinnedInDirection(FloatSize(-wheelEvent.deltaX(), 0))
            && !shouldRubberBandInHorizontalDirection(wheelEvent))
            return false;

        m_inScrollGesture = true;
        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = 0;
        m_momentumVelocity = FloatSize();

        IntSize stretchAmount = m_client.stretchAmount();
        m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(stretchAmount.width()));
        m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(stretchAmount.height()));
        m_overflowScrollDelta = FloatSize();

        stopSnapRubberbandTimer();

        return true;
    }

    if (wheelEvent.phase() == PlatformWheelEventPhaseEnded) {
        snapRubberBand();
        return true;
    }

    bool isMomentumScrollEvent = (wheelEvent.momentumPhase() != PlatformWheelEventPhaseNone);
    if (m_ignoreMomentumScrolls && (isMomentumScrollEvent || m_snapRubberbandTimerIsActive)) {
        if (wheelEvent.momentumPhase() == PlatformWheelEventPhaseEnded) {
            m_ignoreMomentumScrolls = false;
            return true;
        }
        return false;
    }

    float deltaX = m_overflowScrollDelta.width();
    float deltaY = m_overflowScrollDelta.height();

    // Reset overflow values because we may decide to remove delta at various points and put it into overflow.
    m_overflowScrollDelta = FloatSize();

    IntSize stretchAmount = m_client.stretchAmount();
    bool isVerticallyStretched = stretchAmount.height();
    bool isHorizontallyStretched = stretchAmount.width();

    float eventCoalescedDeltaX;
    float eventCoalescedDeltaY;

    if (isVerticallyStretched || isHorizontallyStretched) {
        eventCoalescedDeltaX = -wheelEvent.unacceleratedScrollingDeltaX();
        eventCoalescedDeltaY = -wheelEvent.unacceleratedScrollingDeltaY();
    } else {
        eventCoalescedDeltaX = -wheelEvent.deltaX();
        eventCoalescedDeltaY = -wheelEvent.deltaY();
    }

    deltaX += eventCoalescedDeltaX;
    deltaY += eventCoalescedDeltaY;

    // Slightly prefer scrolling vertically by applying the = case to deltaY
    if (fabsf(deltaY) >= fabsf(deltaX))
        deltaX = 0;
    else
        deltaY = 0;

    bool shouldStretch = false;

    PlatformWheelEventPhase momentumPhase = wheelEvent.momentumPhase();

    // If we are starting momentum scrolling then do some setup.
    if (!m_momentumScrollInProgress && (momentumPhase == PlatformWheelEventPhaseBegan || momentumPhase == PlatformWheelEventPhaseChanged))
        m_momentumScrollInProgress = true;

    CFTimeInterval timeDelta = wheelEvent.timestamp().secondsSinceEpoch().value() - m_lastMomentumScrollTimestamp;
    if (m_inScrollGesture || m_momentumScrollInProgress) {
        if (m_lastMomentumScrollTimestamp && timeDelta > 0 && timeDelta < scrollVelocityZeroingTimeout) {
            m_momentumVelocity.setWidth(eventCoalescedDeltaX / (float)timeDelta);
            m_momentumVelocity.setHeight(eventCoalescedDeltaY / (float)timeDelta);
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp().secondsSinceEpoch().value();
        } else {
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp().secondsSinceEpoch().value();
            m_momentumVelocity = FloatSize();
        }

        if (isVerticallyStretched) {
            if (!isHorizontallyStretched && m_client.pinnedInDirection(FloatSize(deltaX, 0))) {
                // Stretching only in the vertical.
                if (deltaY && (fabsf(deltaX / deltaY) < rubberbandDirectionLockStretchRatio))
                    deltaX = 0;
                else if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                    m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
                    deltaX = 0;
                } else
                    m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
            }
        } else if (isHorizontallyStretched) {
            // Stretching only in the horizontal.
            if (m_client.pinnedInDirection(FloatSize(0, deltaY))) {
                if (deltaX && (fabsf(deltaY / deltaX) < rubberbandDirectionLockStretchRatio))
                    deltaY = 0;
                else if (fabsf(deltaY) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                    m_overflowScrollDelta.setHeight(m_overflowScrollDelta.height() + deltaY);
                    deltaY = 0;
                } else
                    m_overflowScrollDelta.setHeight(m_overflowScrollDelta.height() + deltaY);
            }
        } else {
            // Not stretching at all yet.
            if (m_client.pinnedInDirection(FloatSize(deltaX, deltaY))) {
                if (fabsf(deltaY) >= fabsf(deltaX)) {
                    if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                        m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
                        deltaX = 0;
                    } else
                        m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
                }
                shouldStretch = true;
            }
        }
    }

    if (deltaX || deltaY) {
        if (!(shouldStretch || isVerticallyStretched || isHorizontallyStretched)) {
            if (deltaY) {
                deltaY *= scrollWheelMultiplier();
                m_client.immediateScrollBy(FloatSize(0, deltaY));
            }
            if (deltaX) {
                deltaX *= scrollWheelMultiplier();
                m_client.immediateScrollBy(FloatSize(deltaX, 0));
            }
        } else {
            if (!m_client.allowsHorizontalStretching(wheelEvent)) {
                deltaX = 0;
                eventCoalescedDeltaX = 0;
            } else if (deltaX && !isHorizontallyStretched && !m_client.pinnedInDirection(FloatSize(deltaX, 0))) {
                deltaX *= scrollWheelMultiplier();

                m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(deltaX, 0));
                deltaX = 0;
            }

            if (!m_client.allowsVerticalStretching(wheelEvent)) {
                deltaY = 0;
                eventCoalescedDeltaY = 0;
            } else if (deltaY && !isVerticallyStretched && !m_client.pinnedInDirection(FloatSize(0, deltaY))) {
                deltaY *= scrollWheelMultiplier();

                m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(0, deltaY));
                deltaY = 0;
            }

            IntSize stretchAmount = m_client.stretchAmount();

            if (m_momentumScrollInProgress) {
                if ((m_client.pinnedInDirection(FloatSize(eventCoalescedDeltaX, eventCoalescedDeltaY)) || (fabsf(eventCoalescedDeltaX) + fabsf(eventCoalescedDeltaY) <= 0)) && m_lastMomentumScrollTimestamp) {
                    m_ignoreMomentumScrolls = true;
                    m_momentumScrollInProgress = false;
                    snapRubberBand();
                }
            }

            m_stretchScrollForce.setWidth(m_stretchScrollForce.width() + deltaX);
            m_stretchScrollForce.setHeight(m_stretchScrollForce.height() + deltaY);

            FloatSize dampedDelta(ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.width())), ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.height())));

            m_client.immediateScrollByWithoutContentEdgeConstraints(dampedDelta - stretchAmount);
        }
    }

    if (m_momentumScrollInProgress && momentumPhase == PlatformWheelEventPhaseEnded) {
        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = 0;
    }

    return true;
}
#endif

#if ENABLE(RUBBER_BANDING)
static inline float roundTowardZero(float num)
{
    return num > 0 ? ceilf(num - 0.5f) : floorf(num + 0.5f);
}

static inline float roundToDevicePixelTowardZero(float num)
{
    float roundedNum = roundf(num);
    if (fabs(num - roundedNum) < 0.125)
        num = roundedNum;

    return roundTowardZero(num);
}

void ScrollController::snapRubberBandTimerFired()
{
    if (isScrollSnapInProgress())
        return;
    
    if (!m_momentumScrollInProgress || m_ignoreMomentumScrolls) {
        CFTimeInterval timeDelta = [NSDate timeIntervalSinceReferenceDate] - m_startTime;

        if (m_startStretch == FloatSize()) {
            m_startStretch = m_client.stretchAmount();
            if (m_startStretch == FloatSize()) {
                stopSnapRubberbandTimer();

                m_stretchScrollForce = FloatSize();
                m_startTime = 0;
                m_startStretch = FloatSize();
                m_origVelocity = FloatSize();
                return;
            }

            m_origVelocity = m_momentumVelocity;

            // Just like normal scrolling, prefer vertical rubberbanding
            if (fabsf(m_origVelocity.height()) >= fabsf(m_origVelocity.width()))
                m_origVelocity.setWidth(0);

            // Don't rubber-band horizontally if it's not possible to scroll horizontally
            if (!m_client.canScrollHorizontally())
                m_origVelocity.setWidth(0);

            // Don't rubber-band vertically if it's not possible to scroll vertically
            if (!m_client.canScrollVertically())
                m_origVelocity.setHeight(0);
        }

        FloatPoint delta(roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.width(), -m_origVelocity.width(), (float)timeDelta)),
            roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.height(), -m_origVelocity.height(), (float)timeDelta)));

        if (fabs(delta.x()) >= 1 || fabs(delta.y()) >= 1) {
            m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(delta.x(), delta.y()) - m_client.stretchAmount());

            FloatSize newStretch = m_client.stretchAmount();

            m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(newStretch.width()));
            m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(newStretch.height()));
        } else {
            m_client.adjustScrollPositionToBoundsIfNecessary();

            stopSnapRubberbandTimer();
            m_stretchScrollForce = FloatSize();
            m_startTime = 0;
            m_startStretch = FloatSize();
            m_origVelocity = FloatSize();
        }
    } else {
        m_startTime = [NSDate timeIntervalSinceReferenceDate];
        m_startStretch = FloatSize();
        if (!isRubberBandInProgress())
            stopSnapRubberbandTimer();
    }
}
#endif

bool ScrollController::isRubberBandInProgress() const
{
#if ENABLE(RUBBER_BANDING) && PLATFORM(MAC)
    if (!m_inScrollGesture && !m_momentumScrollInProgress && !m_snapRubberbandTimerIsActive)
        return false;

    return !m_client.stretchAmount().isZero();
#else
    return false;
#endif
}

bool ScrollController::isScrollSnapInProgress() const
{
#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
    if (m_inScrollGesture || m_momentumScrollInProgress || m_scrollSnapTimer.isActive())
        return true;
#endif
    return false;
}

#if ENABLE(RUBBER_BANDING)
void ScrollController::startSnapRubberbandTimer()
{
    m_client.startSnapRubberbandTimer();
    m_snapRubberbandTimer.startRepeating(1_s / 60.);

    m_client.deferTestsForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(this), WheelEventTestTrigger::RubberbandInProgress);
}

void ScrollController::stopSnapRubberbandTimer()
{
    m_client.stopSnapRubberbandTimer();
    m_snapRubberbandTimer.stop();
    m_snapRubberbandTimerIsActive = false;
    
    m_client.removeTestDeferralForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(this), WheelEventTestTrigger::RubberbandInProgress);
}

void ScrollController::snapRubberBand()
{
    CFTimeInterval timeDelta = [NSProcessInfo processInfo].systemUptime - m_lastMomentumScrollTimestamp;
    if (m_lastMomentumScrollTimestamp && timeDelta >= scrollVelocityZeroingTimeout)
        m_momentumVelocity = FloatSize();

    m_inScrollGesture = false;

    if (m_snapRubberbandTimerIsActive)
        return;

    m_startTime = [NSDate timeIntervalSinceReferenceDate];
    m_startStretch = FloatSize();
    m_origVelocity = FloatSize();

    startSnapRubberbandTimer();
    m_snapRubberbandTimerIsActive = true;
}

bool ScrollController::shouldRubberBandInHorizontalDirection(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.deltaX() > 0)
        return m_client.shouldRubberBandInDirection(ScrollLeft);
    if (wheelEvent.deltaX() < 0)
        return m_client.shouldRubberBandInDirection(ScrollRight);

    return true;
}
#endif

#if ENABLE(CSS_SCROLL_SNAP)

#if PLATFORM(MAC)
static inline WheelEventStatus toWheelEventStatus(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    if (phase == PlatformWheelEventPhaseNone) {
        switch (momentumPhase) {
        case PlatformWheelEventPhaseBegan:
            return WheelEventStatus::InertialScrollBegin;
                
        case PlatformWheelEventPhaseChanged:
            return WheelEventStatus::InertialScrolling;
                
        case PlatformWheelEventPhaseEnded:
            return WheelEventStatus::InertialScrollEnd;

        case PlatformWheelEventPhaseNone:
            return WheelEventStatus::StatelessScrollEvent;

        default:
            return WheelEventStatus::Unknown;
        }
    }
    if (momentumPhase == PlatformWheelEventPhaseNone) {
        switch (phase) {
        case PlatformWheelEventPhaseBegan:
        case PlatformWheelEventPhaseMayBegin:
            return WheelEventStatus::UserScrollBegin;
                
        case PlatformWheelEventPhaseChanged:
            return WheelEventStatus::UserScrolling;
                
        case PlatformWheelEventPhaseEnded:
        case PlatformWheelEventPhaseCancelled:
            return WheelEventStatus::UserScrollEnd;
                
        default:
            return WheelEventStatus::Unknown;
        }
    }
    return WheelEventStatus::Unknown;
}

bool ScrollController::shouldOverrideInertialScrolling() const
{
    if (!m_scrollSnapState)
        return false;

    ScrollSnapState scrollSnapState = m_scrollSnapState->currentState();
    return scrollSnapState == ScrollSnapState::Gliding || scrollSnapState == ScrollSnapState::DestinationReached;
}

void ScrollController::scheduleStatelessScrollSnap()
{
    stopScrollSnapTimer();
    m_statelessSnapTransitionTimer.stop();
    if (!m_scrollSnapState)
        return;

    static const Seconds statelessScrollSnapDelay = 750_ms;
    m_statelessSnapTransitionTimer.startOneShot(statelessScrollSnapDelay);
    startDeferringTestsDueToScrollSnapping();
}

void ScrollController::statelessSnapTransitionTimerFired()
{
    if (!m_scrollSnapState)
        return;

    m_scrollSnapState->transitionToSnapAnimationState(m_client.scrollExtent(), m_client.viewportSize(), m_client.pageScaleFactor(), m_client.scrollOffset());
    startScrollSnapTimer();
}

void ScrollController::startDeferringTestsDueToScrollSnapping()
{
    m_client.deferTestsForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(this), WheelEventTestTrigger::ScrollSnapInProgress);
}

void ScrollController::stopDeferringTestsDueToScrollSnapping()
{
    m_client.removeTestDeferralForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(this), WheelEventTestTrigger::ScrollSnapInProgress);
}

bool ScrollController::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    if (!m_scrollSnapState)
        return true;

    if (m_scrollSnapState->snapOffsetsForAxis(ScrollEventAxis::Horizontal).isEmpty() && m_scrollSnapState->snapOffsetsForAxis(ScrollEventAxis::Vertical).isEmpty())
        return true;

    WheelEventStatus status = toWheelEventStatus(wheelEvent.phase(), wheelEvent.momentumPhase());
    bool isInertialScrolling = false;
    switch (status) {
    case WheelEventStatus::UserScrollBegin:
    case WheelEventStatus::UserScrolling:
        stopScrollSnapTimer();
        m_scrollSnapState->transitionToUserInteractionState();
        m_dragEndedScrollingVelocity = -wheelEvent.scrollingVelocity();
        break;
    case WheelEventStatus::UserScrollEnd:
        m_scrollSnapState->transitionToSnapAnimationState(m_client.scrollExtent(), m_client.viewportSize(), m_client.pageScaleFactor(), m_client.scrollOffset());
        startScrollSnapTimer();
        break;
    case WheelEventStatus::InertialScrollBegin:
        m_scrollSnapState->transitionToGlideAnimationState(m_client.scrollExtent(), m_client.viewportSize(), m_client.pageScaleFactor(), m_client.scrollOffset(), m_dragEndedScrollingVelocity, FloatSize(-wheelEvent.deltaX(), -wheelEvent.deltaY()));
        m_dragEndedScrollingVelocity = { };
        isInertialScrolling = true;
        break;
    case WheelEventStatus::InertialScrolling:
    case WheelEventStatus::InertialScrollEnd:
        isInertialScrolling = true;
        break;
    case WheelEventStatus::StatelessScrollEvent:
        m_scrollSnapState->transitionToUserInteractionState();
        scheduleStatelessScrollSnap();
        break;
    case WheelEventStatus::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return !(isInertialScrolling && shouldOverrideInertialScrolling());
}

void ScrollController::startScrollSnapTimer()
{
    if (m_scrollSnapTimer.isActive())
        return;

    startDeferringTestsDueToScrollSnapping();
    m_client.startScrollSnapTimer();
    m_scrollSnapTimer.startRepeating(1_s / 60.);
}

void ScrollController::stopScrollSnapTimer()
{
    if (!m_scrollSnapTimer.isActive())
        return;

    stopDeferringTestsDueToScrollSnapping();
    m_client.stopScrollSnapTimer();
    m_scrollSnapTimer.stop();
}

void ScrollController::scrollSnapTimerFired()
{
    if (!m_scrollSnapState) {
        ASSERT_NOT_REACHED();
        return;
    }

    bool isAnimationComplete;
    auto animationOffset = m_scrollSnapState->currentAnimatedScrollOffset(isAnimationComplete);
    auto currentOffset = m_client.scrollOffset();
    m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(animationOffset.x() - currentOffset.x(), animationOffset.y() - currentOffset.y()));
    if (isAnimationComplete) {
        m_scrollSnapState->transitionToDestinationReachedState();
        stopScrollSnapTimer();
    }
}
#endif // PLATFORM(MAC)

void ScrollController::updateScrollSnapState(const ScrollableArea& scrollableArea)
{
    if (auto* snapOffsets = scrollableArea.horizontalSnapOffsets()) {
        if (auto* snapOffsetRanges = scrollableArea.horizontalSnapOffsetRanges())
            updateScrollSnapPoints(ScrollEventAxis::Horizontal, *snapOffsets, *snapOffsetRanges);
        else
            updateScrollSnapPoints(ScrollEventAxis::Horizontal, *snapOffsets, { });
    } else
        updateScrollSnapPoints(ScrollEventAxis::Horizontal, { }, { });

    if (auto* snapOffsets = scrollableArea.verticalSnapOffsets()) {
        if (auto* snapOffsetRanges = scrollableArea.verticalSnapOffsetRanges())
            updateScrollSnapPoints(ScrollEventAxis::Vertical, *snapOffsets, *snapOffsetRanges);
        else
            updateScrollSnapPoints(ScrollEventAxis::Vertical, *snapOffsets, { });
    } else
        updateScrollSnapPoints(ScrollEventAxis::Vertical, { }, { });
}

void ScrollController::updateScrollSnapPoints(ScrollEventAxis axis, const Vector<LayoutUnit>& snapPoints, const Vector<ScrollOffsetRange<LayoutUnit>>& snapRanges)
{
    if (!m_scrollSnapState) {
        if (snapPoints.isEmpty())
            return;

        m_scrollSnapState = makeUnique<ScrollSnapAnimatorState>();
    }

    if (snapPoints.isEmpty() && m_scrollSnapState->snapOffsetsForAxis(otherScrollEventAxis(axis)).isEmpty())
        m_scrollSnapState = nullptr;
    else
        m_scrollSnapState->setSnapOffsetsAndPositionRangesForAxis(axis, snapPoints, snapRanges);
}

unsigned ScrollController::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    if (!m_scrollSnapState)
        return 0;

    return m_scrollSnapState->activeSnapIndexForAxis(axis);
}

void ScrollController::setActiveScrollSnapIndexForAxis(ScrollEventAxis axis, unsigned index)
{
    if (!m_scrollSnapState)
        return;

    m_scrollSnapState->setActiveSnapIndexForAxis(axis, index);
}

void ScrollController::setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis axis, int offset)
{
    if (!m_scrollSnapState)
        return;

    float scaleFactor = m_client.pageScaleFactor();
    ScrollSnapAnimatorState& snapState = *m_scrollSnapState;

    auto snapOffsets = snapState.snapOffsetsForAxis(axis);
    if (!snapOffsets.size())
        return;

    LayoutUnit clampedOffset = std::min(std::max(LayoutUnit(offset / scaleFactor), snapOffsets.first()), snapOffsets.last());

    unsigned activeIndex = 0;
    closestSnapOffset(snapState.snapOffsetsForAxis(axis), snapState.snapOffsetRangesForAxis(axis), clampedOffset, 0, activeIndex);

    if (activeIndex == activeScrollSnapIndexForAxis(axis))
        return;

    m_activeScrollSnapIndexDidChange = true;
    setActiveScrollSnapIndexForAxis(axis, activeIndex);
}

void ScrollController::setActiveScrollSnapIndicesForOffset(int x, int y)
{
    if (!m_scrollSnapState)
        return;

    setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Horizontal, x);
    setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Vertical, y);
}
#endif

} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING)
