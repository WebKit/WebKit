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

#include "config.h"
#include "ScrollController.h"

#include "PlatformWheelEvent.h"
#include "WebCoreSystemInterface.h"
#include <sys/sysctl.h>
#include <sys/time.h>

#if ENABLE(CSS_SCROLL_SNAP)
#include "ScrollSnapAnimatorState.h"
#include "ScrollableArea.h"
#endif

#if ENABLE(RUBBER_BANDING)

static NSTimeInterval systemUptime()
{
    if ([[NSProcessInfo processInfo] respondsToSelector:@selector(systemUptime)])
        return [[NSProcessInfo processInfo] systemUptime];

    // Get how long system has been up. Found by looking getting "boottime" from the kernel.
    static struct timeval boottime = {0, 0};
    if (!boottime.tv_sec) {
        int mib[2] = {CTL_KERN, KERN_BOOTTIME};
        size_t size = sizeof(boottime);
        if (-1 == sysctl(mib, 2, &boottime, &size, 0, 0))
            boottime.tv_sec = 0;
    }
    struct timeval now;
    if (boottime.tv_sec && -1 != gettimeofday(&now, 0)) {
        struct timeval uptime;
        timersub(&now, &boottime, &uptime);
        NSTimeInterval result = uptime.tv_sec + (uptime.tv_usec / 1E+6);
        return result;
    }
    return 0;
}


namespace WebCore {

static const float scrollVelocityZeroingTimeout = 0.10f;
static const float rubberbandDirectionLockStretchRatio = 1;
static const float rubberbandMinimumRequiredDeltaBeforeStretch = 10;

#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
static const float snapMagnitudeMax = 25;
static const float snapMagnitudeMin = 5;
static const float snapThresholdHigh = 1000;
static const float snapThresholdLow = 50;

static const float inertialScrollPredictionFactor = 16.7;
static const float initialToFinalMomentumFactor = 1.0 / 40.0;

static const float glideBoostMultiplier = 3.5;

static const float maxTargetWheelDelta = 7;
static const float minTargetWheelDelta = 3.5;
#endif

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

static float elasticDeltaForTimeDelta(float initialPosition, float initialVelocity, float elapsedTime)
{
    return wkNSElasticDeltaForTimeDelta(initialPosition, initialVelocity, elapsedTime);
}

static float elasticDeltaForReboundDelta(float delta)
{
    return wkNSElasticDeltaForReboundDelta(delta);
}

static float reboundDeltaForElasticDelta(float delta)
{
    return wkNSReboundDeltaForElasticDelta(delta);
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

ScrollController::ScrollController(ScrollControllerClient* client)
    : m_client(client)
    , m_lastMomentumScrollTimestamp(0)
    , m_startTime(0)
    , m_snapRubberbandTimer(RunLoop::current(), this, &ScrollController::snapRubberBandTimerFired)
#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
    , m_horizontalScrollSnapTimer(RunLoop::current(), this, &ScrollController::horizontalScrollSnapTimerFired)
    , m_verticalScrollSnapTimer(RunLoop::current(), this, &ScrollController::verticalScrollSnapTimerFired)
#endif
    , m_inScrollGesture(false)
    , m_momentumScrollInProgress(false)
    , m_ignoreMomentumScrolls(false)
    , m_snapRubberbandTimerIsActive(false)
{
}

bool ScrollController::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
    if (!processWheelEventForScrollSnap(wheelEvent))
        return false;
#endif
    if (wheelEvent.phase() == PlatformWheelEventPhaseBegan) {
        // First, check if we should rubber-band at all.
        if (m_client->pinnedInDirection(FloatSize(-wheelEvent.deltaX(), 0))
            && !shouldRubberBandInHorizontalDirection(wheelEvent))
            return false;

        m_inScrollGesture = true;
        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = 0;
        m_momentumVelocity = FloatSize();

        IntSize stretchAmount = m_client->stretchAmount();
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

    IntSize stretchAmount = m_client->stretchAmount();
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

    CFTimeInterval timeDelta = wheelEvent.timestamp() - m_lastMomentumScrollTimestamp;
    if (m_inScrollGesture || m_momentumScrollInProgress) {
        if (m_lastMomentumScrollTimestamp && timeDelta > 0 && timeDelta < scrollVelocityZeroingTimeout) {
            m_momentumVelocity.setWidth(eventCoalescedDeltaX / (float)timeDelta);
            m_momentumVelocity.setHeight(eventCoalescedDeltaY / (float)timeDelta);
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
        } else {
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
            m_momentumVelocity = FloatSize();
        }

        if (isVerticallyStretched) {
            if (!isHorizontallyStretched && m_client->pinnedInDirection(FloatSize(deltaX, 0))) {
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
            if (m_client->pinnedInDirection(FloatSize(0, deltaY))) {
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
            if (m_client->pinnedInDirection(FloatSize(deltaX, deltaY))) {
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
                m_client->immediateScrollBy(FloatSize(0, deltaY));
            }
            if (deltaX) {
                deltaX *= scrollWheelMultiplier();
                m_client->immediateScrollBy(FloatSize(deltaX, 0));
            }
        } else {
            if (!m_client->allowsHorizontalStretching(wheelEvent)) {
                deltaX = 0;
                eventCoalescedDeltaX = 0;
            } else if (deltaX && !isHorizontallyStretched && !m_client->pinnedInDirection(FloatSize(deltaX, 0))) {
                deltaX *= scrollWheelMultiplier();

                m_client->immediateScrollByWithoutContentEdgeConstraints(FloatSize(deltaX, 0));
                deltaX = 0;
            }

            if (!m_client->allowsVerticalStretching(wheelEvent)) {
                deltaY = 0;
                eventCoalescedDeltaY = 0;
            } else if (deltaY && !isVerticallyStretched && !m_client->pinnedInDirection(FloatSize(0, deltaY))) {
                deltaY *= scrollWheelMultiplier();

                m_client->immediateScrollByWithoutContentEdgeConstraints(FloatSize(0, deltaY));
                deltaY = 0;
            }

            IntSize stretchAmount = m_client->stretchAmount();

            if (m_momentumScrollInProgress) {
                if ((m_client->pinnedInDirection(FloatSize(eventCoalescedDeltaX, eventCoalescedDeltaY)) || (fabsf(eventCoalescedDeltaX) + fabsf(eventCoalescedDeltaY) <= 0)) && m_lastMomentumScrollTimestamp) {
                    m_ignoreMomentumScrolls = true;
                    m_momentumScrollInProgress = false;
                    snapRubberBand();
                }
            }

            m_stretchScrollForce.setWidth(m_stretchScrollForce.width() + deltaX);
            m_stretchScrollForce.setHeight(m_stretchScrollForce.height() + deltaY);

            FloatSize dampedDelta(ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.width())), ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.height())));

            m_client->immediateScrollByWithoutContentEdgeConstraints(dampedDelta - stretchAmount);
        }
    }

    if (m_momentumScrollInProgress && momentumPhase == PlatformWheelEventPhaseEnded) {
        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = 0;
    }

    return true;
}

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
    if (!m_momentumScrollInProgress || m_ignoreMomentumScrolls) {
        CFTimeInterval timeDelta = [NSDate timeIntervalSinceReferenceDate] - m_startTime;

        if (m_startStretch == FloatSize()) {
            m_startStretch = m_client->stretchAmount();
            if (m_startStretch == FloatSize()) {
                stopSnapRubberbandTimer();

                m_stretchScrollForce = FloatSize();
                m_startTime = 0;
                m_startStretch = FloatSize();
                m_origOrigin = FloatPoint();
                m_origVelocity = FloatSize();
                return;
            }

            m_origOrigin = m_client->absoluteScrollPosition() - m_startStretch;
            m_origVelocity = m_momentumVelocity;

            // Just like normal scrolling, prefer vertical rubberbanding
            if (fabsf(m_origVelocity.height()) >= fabsf(m_origVelocity.width()))
                m_origVelocity.setWidth(0);

            // Don't rubber-band horizontally if it's not possible to scroll horizontally
            if (!m_client->canScrollHorizontally())
                m_origVelocity.setWidth(0);

            // Don't rubber-band vertically if it's not possible to scroll vertically
            if (!m_client->canScrollVertically())
                m_origVelocity.setHeight(0);
        }

        FloatPoint delta(roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.width(), -m_origVelocity.width(), (float)timeDelta)),
            roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.height(), -m_origVelocity.height(), (float)timeDelta)));

        if (fabs(delta.x()) >= 1 || fabs(delta.y()) >= 1) {
            m_client->immediateScrollByWithoutContentEdgeConstraints(FloatSize(delta.x(), delta.y()) - m_client->stretchAmount());

            FloatSize newStretch = m_client->stretchAmount();

            m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(newStretch.width()));
            m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(newStretch.height()));
        } else {
            m_client->adjustScrollPositionToBoundsIfNecessary();

            stopSnapRubberbandTimer();
            m_stretchScrollForce = FloatSize();
            m_startTime = 0;
            m_startStretch = FloatSize();
            m_origOrigin = FloatPoint();
            m_origVelocity = FloatSize();
        }
    } else {
        m_startTime = [NSDate timeIntervalSinceReferenceDate];
        m_startStretch = FloatSize();
    }
}

bool ScrollController::isRubberBandInProgress() const
{
    if (!m_inScrollGesture && !m_momentumScrollInProgress && !m_snapRubberbandTimerIsActive)
        return false;

    return !m_client->stretchAmount().isZero();
}

void ScrollController::startSnapRubberbandTimer()
{
    m_client->startSnapRubberbandTimer();
    m_snapRubberbandTimer.startRepeating(1.0 / 60.0);
}

void ScrollController::stopSnapRubberbandTimer()
{
    m_client->stopSnapRubberbandTimer();
    m_snapRubberbandTimer.stop();
    m_snapRubberbandTimerIsActive = false;
}

void ScrollController::snapRubberBand()
{
    CFTimeInterval timeDelta = systemUptime() - m_lastMomentumScrollTimestamp;
    if (m_lastMomentumScrollTimestamp && timeDelta >= scrollVelocityZeroingTimeout)
        m_momentumVelocity = FloatSize();

    m_inScrollGesture = false;

    if (m_snapRubberbandTimerIsActive)
        return;

    m_startTime = [NSDate timeIntervalSinceReferenceDate];
    m_startStretch = FloatSize();
    m_origOrigin = FloatPoint();
    m_origVelocity = FloatSize();

    startSnapRubberbandTimer();
    m_snapRubberbandTimerIsActive = true;
}

bool ScrollController::shouldRubberBandInHorizontalDirection(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.deltaX() > 0)
        return m_client->shouldRubberBandInDirection(ScrollLeft);
    if (wheelEvent.deltaX() < 0)
        return m_client->shouldRubberBandInDirection(ScrollRight);

    return true;
}

#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
ScrollSnapAnimatorState& ScrollController::scrollSnapPointState(ScrollEventAxis axis)
{
    ASSERT(axis != ScrollEventAxis::Horizontal || m_horizontalScrollSnapState);
    ASSERT(axis != ScrollEventAxis::Vertical || m_verticalScrollSnapState);

    return (axis == ScrollEventAxis::Horizontal) ? *m_horizontalScrollSnapState : *m_verticalScrollSnapState;
}

const ScrollSnapAnimatorState& ScrollController::scrollSnapPointState(ScrollEventAxis axis) const
{
    ASSERT(axis != ScrollEventAxis::Horizontal || m_horizontalScrollSnapState);
    ASSERT(axis != ScrollEventAxis::Vertical || m_verticalScrollSnapState);
    
    return (axis == ScrollEventAxis::Horizontal) ? *m_horizontalScrollSnapState : *m_verticalScrollSnapState;
}

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

void ScrollController::processWheelEventForScrollSnapOnAxis(ScrollEventAxis axis, const PlatformWheelEvent& event)
{
    ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);

    float wheelDelta = axis == ScrollEventAxis::Horizontal ? -event.deltaX() : -event.deltaY();
    WheelEventStatus wheelStatus = toWheelEventStatus(event.phase(), event.momentumPhase());
    
    switch (wheelStatus) {
    case WheelEventStatus::UserScrollBegin:
    case WheelEventStatus::UserScrolling:
        endScrollSnapAnimation(axis, ScrollSnapState::UserInteraction);
        break;
            
    case WheelEventStatus::UserScrollEnd:
        beginScrollSnapAnimation(axis, ScrollSnapState::Snapping);
        break;
        
    case WheelEventStatus::InertialScrollBegin:
        // Begin tracking wheel deltas for glide prediction.
        endScrollSnapAnimation(axis, ScrollSnapState::UserInteraction);
        snapState.pushInitialWheelDelta(wheelDelta);
        snapState.m_beginTrackingWheelDeltaOffset = m_client->scrollOffsetOnAxis(axis);
        break;
            
    case WheelEventStatus::InertialScrolling:
        // This check for DestinationReached ensures that we don't receive another set of momentum events after ending the last glide.
        if (snapState.m_currentState != ScrollSnapState::Gliding && snapState.m_currentState != ScrollSnapState::DestinationReached) {
            if (snapState.m_numWheelDeltasTracked < snapState.wheelDeltaWindowSize)
                snapState.pushInitialWheelDelta(wheelDelta);
            
            if (snapState.m_numWheelDeltasTracked == snapState.wheelDeltaWindowSize)
                beginScrollSnapAnimation(axis, ScrollSnapState::Gliding);
        }
        break;
        
    case WheelEventStatus::InertialScrollEnd:
        snapState.clearInitialWheelDeltaWindow();
        snapState.m_shouldOverrideWheelEvent = false;
        break;

    case WheelEventStatus::StatelessScrollEvent:
        endScrollSnapAnimation(axis, ScrollSnapState::UserInteraction);
        snapState.clearInitialWheelDeltaWindow();
        snapState.m_shouldOverrideWheelEvent = false;
        break;

    case WheelEventStatus::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool ScrollController::shouldOverrideWheelEvent(ScrollEventAxis axis, const PlatformWheelEvent& event) const
{
    const ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);

    return snapState.m_shouldOverrideWheelEvent && toWheelEventStatus(event.phase(), event.momentumPhase()) == WheelEventStatus::InertialScrolling;
}

bool ScrollController::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    if (m_verticalScrollSnapState) {
        processWheelEventForScrollSnapOnAxis(ScrollEventAxis::Vertical, wheelEvent);
        if (shouldOverrideWheelEvent(ScrollEventAxis::Vertical, wheelEvent))
            return false;
    }
    if (m_horizontalScrollSnapState) {
        processWheelEventForScrollSnapOnAxis(ScrollEventAxis::Horizontal, wheelEvent);
        if (shouldOverrideWheelEvent(ScrollEventAxis::Horizontal, wheelEvent))
            return false;
    }

    return true;
}

void ScrollController::updateScrollAnimatorsAndTimers(const ScrollableArea& scrollableArea)
{
    // FIXME: Currently, scroll snap animators are recreated even though the snap offsets alone can be updated.
    if (scrollableArea.horizontalSnapOffsets())
        m_horizontalScrollSnapState = std::make_unique<ScrollSnapAnimatorState>(ScrollEventAxis::Horizontal, *scrollableArea.horizontalSnapOffsets());
    else if (m_horizontalScrollSnapState)
        m_horizontalScrollSnapState = nullptr;

    if (scrollableArea.verticalSnapOffsets())
        m_verticalScrollSnapState = std::make_unique<ScrollSnapAnimatorState>(ScrollEventAxis::Vertical, *scrollableArea.verticalSnapOffsets());
    else if (m_verticalScrollSnapState)
        m_verticalScrollSnapState = nullptr;
}

void ScrollController::updateScrollSnapPoints(ScrollEventAxis axis, const Vector<LayoutUnit>& snapPoints)
{
    // FIXME: Currently, scroll snap animators are recreated even though the snap offsets alone can be updated.
    if (axis == ScrollEventAxis::Horizontal)
        m_horizontalScrollSnapState = !snapPoints.isEmpty() ? std::make_unique<ScrollSnapAnimatorState>(ScrollEventAxis::Horizontal, snapPoints) : nullptr;

    if (axis == ScrollEventAxis::Vertical)
        m_verticalScrollSnapState = !snapPoints.isEmpty() ? std::make_unique<ScrollSnapAnimatorState>(ScrollEventAxis::Vertical, snapPoints) : nullptr;
}

void ScrollController::startScrollSnapTimer(ScrollEventAxis axis)
{
    RunLoop::Timer<ScrollController>& scrollSnapTimer = axis == ScrollEventAxis::Horizontal ? m_horizontalScrollSnapTimer : m_verticalScrollSnapTimer;
    if (!scrollSnapTimer.isActive()) {
        m_client->startScrollSnapTimer(axis);
        scrollSnapTimer.startRepeating(1.0 / 60.0);
    }
}

void ScrollController::stopScrollSnapTimer(ScrollEventAxis axis)
{
    m_client->stopScrollSnapTimer(axis);
    RunLoop::Timer<ScrollController>& scrollSnapTimer = axis == ScrollEventAxis::Horizontal ? m_horizontalScrollSnapTimer : m_verticalScrollSnapTimer;
    scrollSnapTimer.stop();
}

void ScrollController::horizontalScrollSnapTimerFired()
{
    scrollSnapAnimationUpdate(ScrollEventAxis::Horizontal);
}

void ScrollController::verticalScrollSnapTimerFired()
{
    scrollSnapAnimationUpdate(ScrollEventAxis::Vertical);
}

void ScrollController::scrollSnapAnimationUpdate(ScrollEventAxis axis)
{
    if (axis == ScrollEventAxis::Horizontal && !m_horizontalScrollSnapState)
        return;

    if (axis == ScrollEventAxis::Vertical && !m_verticalScrollSnapState)
        return;

    ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);
    if (snapState.m_currentState == ScrollSnapState::DestinationReached)
        return;
    
    ASSERT(snapState.m_currentState == ScrollSnapState::Gliding || snapState.m_currentState == ScrollSnapState::Snapping);
    float delta = snapState.m_currentState == ScrollSnapState::Snapping ? computeSnapDelta(axis) : computeGlideDelta(axis);
    if (delta)
        m_client->immediateScrollOnAxis(axis, delta);
    else
        endScrollSnapAnimation(axis, ScrollSnapState::DestinationReached);
}

static inline float projectedInertialScrollDistance(float initialWheelDelta)
{
    // FIXME: Experiments with inertial scrolling show a fairly consistent linear relationship between initial wheel delta and total distance scrolled.
    // In the future, we'll want to find a more accurate way of inertial scroll prediction.
    return inertialScrollPredictionFactor * initialWheelDelta;
}

void ScrollController::initializeGlideParameters(ScrollEventAxis axis, bool shouldIncreaseInitialWheelDelta)
{
    ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);
    
    // FIXME: Glide boost is a hacky way to speed up natural scrolling velocity. We should find a better way to accomplish this.
    if (shouldIncreaseInitialWheelDelta)
        snapState.m_glideInitialWheelDelta *= glideBoostMultiplier;
    
    // FIXME: There must be a better way to determine a good target delta than multiplying by a factor and clamping to min/max values.
    float targetFinalWheelDelta = initialToFinalMomentumFactor * (snapState.m_glideInitialWheelDelta < 0 ? -snapState.m_glideInitialWheelDelta : snapState.m_glideInitialWheelDelta);
    targetFinalWheelDelta = (snapState.m_glideInitialWheelDelta > 0 ? 1 : -1) * std::min(std::max(targetFinalWheelDelta, minTargetWheelDelta), maxTargetWheelDelta);
    snapState.m_glideMagnitude = (snapState.m_glideInitialWheelDelta + targetFinalWheelDelta) / 2;
    snapState.m_glidePhaseShift = acos((snapState.m_glideInitialWheelDelta - targetFinalWheelDelta) / (snapState.m_glideInitialWheelDelta + targetFinalWheelDelta));
}

void ScrollController::beginScrollSnapAnimation(ScrollEventAxis axis, ScrollSnapState newState)
{
    ASSERT(newState == ScrollSnapState::Gliding || newState == ScrollSnapState::Snapping);
    
    ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);

    LayoutUnit offset = m_client->scrollOffsetOnAxis(axis);
    float initialWheelDelta = newState == ScrollSnapState::Gliding ? snapState.averageInitialWheelDelta() : 0;
    LayoutUnit projectedScrollDestination = newState == ScrollSnapState::Gliding ? snapState.m_beginTrackingWheelDeltaOffset + LayoutUnit(projectedInertialScrollDistance(initialWheelDelta)) : offset;
    if (snapState.m_snapOffsets.isEmpty())
        return;

    projectedScrollDestination = std::min(std::max(projectedScrollDestination, snapState.m_snapOffsets.first()), snapState.m_snapOffsets.last());
    snapState.m_initialOffset = offset;
    snapState.m_targetOffset = closestSnapOffset<LayoutUnit, float>(snapState.m_snapOffsets, projectedScrollDestination, initialWheelDelta);
    if (snapState.m_initialOffset == snapState.m_targetOffset)
        return;
    
    snapState.m_currentState = newState;
    if (newState == ScrollSnapState::Gliding) {
        snapState.m_shouldOverrideWheelEvent = true;
        snapState.m_glideInitialWheelDelta = initialWheelDelta;
        bool glideRequiresBoost;
        if (initialWheelDelta > 0)
            glideRequiresBoost = projectedScrollDestination - offset < snapState.m_targetOffset - projectedScrollDestination;
        else
            glideRequiresBoost = offset - projectedScrollDestination < projectedScrollDestination - snapState.m_targetOffset;
        
        initializeGlideParameters(axis, glideRequiresBoost);
        snapState.clearInitialWheelDeltaWindow();
    }
    startScrollSnapTimer(axis);
}

void ScrollController::endScrollSnapAnimation(ScrollEventAxis axis, ScrollSnapState newState)
{
    ASSERT(newState == ScrollSnapState::DestinationReached || newState == ScrollSnapState::UserInteraction);

    ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);

    if (snapState.m_currentState == ScrollSnapState::Gliding)
        snapState.clearInitialWheelDeltaWindow();
    
    snapState.m_currentState = newState;
    stopScrollSnapTimer(axis);
}

static inline float snapProgress(const LayoutUnit& offset, const ScrollSnapAnimatorState& snapState)
{
    const float distanceTraveled = static_cast<float>(offset - snapState.m_initialOffset);
    const float totalDistance = static_cast<float>(snapState.m_targetOffset - snapState.m_initialOffset);

    return distanceTraveled / totalDistance;
}

static inline float clampedSnapMagnitude(float thresholdedDistance)
{
    return snapMagnitudeMin + (snapMagnitudeMax - snapMagnitudeMin) * (thresholdedDistance - snapThresholdLow) / (snapThresholdHigh - snapThresholdLow);
}

// Computes the amount to scroll by when performing a "snap" operation, i.e. when a user releases the trackpad without flicking. The snap delta
// is a function of progress t, where t is equal to DISTANCE_TRAVELED / TOTAL_DISTANCE, DISTANCE_TRAVELED is the distance from the initialOffset
// to the current offset, and TOTAL_DISTANCE is the distance from initialOffset to targetOffset. The snapping equation is as follows:
// delta(t) = MAGNITUDE * sin(PI * t). MAGNITUDE indicates the top speed reached near the middle of the animation (t = 0.5), and is a linear
// relationship of the distance traveled, clamped by arbitrary min and max values.
float ScrollController::computeSnapDelta(ScrollEventAxis axis) const
{
    const ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);

    LayoutUnit offset = m_client->scrollOffsetOnAxis(axis);
    bool canComputeSnap =  (snapState.m_initialOffset <= offset && offset < snapState.m_targetOffset) || (snapState.m_targetOffset < offset && offset <= snapState.m_initialOffset);
    if (snapState.m_currentState != ScrollSnapState::Snapping || !canComputeSnap)
        return 0;
    
    float progress = snapProgress(offset, snapState);

    // Threshold the distance before computing magnitude, so only distances within a certain range are considered.
    int sign = snapState.m_initialOffset < snapState.m_targetOffset ? 1 : -1;
    float thresholdedDistance = std::min(std::max<float>((snapState.m_targetOffset - snapState.m_initialOffset) * sign, snapThresholdLow), snapThresholdHigh);

    float magnitude = clampedSnapMagnitude(thresholdedDistance);

    float rawSnapDelta = std::max<float>(1, magnitude * std::sin(piFloat * progress));
    if ((snapState.m_targetOffset < offset && offset - rawSnapDelta < snapState.m_targetOffset) || (snapState.m_targetOffset > offset && offset + rawSnapDelta > snapState.m_targetOffset))
        return snapState.m_targetOffset - offset;
    
    return sign * rawSnapDelta;
}

static inline float snapGlide(float progress, const ScrollSnapAnimatorState& snapState)
{
    // FIXME: We might want to investigate why -m_glidePhaseShift results in the behavior we want.
    return ceil(snapState.m_glideMagnitude * (1.0f + std::cos(piFloat * progress - snapState.m_glidePhaseShift)));
}

// Computes the amount to scroll by when performing a "glide" operation, i.e. when a user releases the trackpad with an initial velocity. Here,
// we want the scroll offset to animate directly to the snap point.
//
// The snap delta is a function of progress t, where: (1) t is equal to DISTANCE_TRAVELED / TOTAL_DISTANCE, (2) DISTANCE_TRAVELED is the distance
// from the initialOffset to the current offset, and (3) TOTAL_DISTANCE is the distance from initialOffset to targetOffset.
//
// The general model of our gliding equation is delta(t) = MAGNITUDE * (1 + cos(PI * t + PHASE_SHIFT)). This was determined after examining the
// momentum velocity curve as a function of progress. To compute MAGNITUDE and PHASE_SHIFT, we use initial velocity V0 and the final velocity VF,
// both as wheel deltas (pixels per timestep). VF should be a small value (< 10) chosen based on the initial velocity and TOTAL_DISTANCE.
// We also enforce the following constraints for the gliding equation:
//   1. delta(0) = V0, since we want the initial velocity of the gliding animation to match the user's scroll velocity. The exception to this is
//      when the glide velocity is not enough to naturally reach the next snap point, and thus requires a boost (see initializeGlideParameters)
//   2. delta(1) = VF, since at t=1, the animation has completed and we want the last wheel delta to match the final velocity VF. Note that this
//      doesn't guarantee that the final velocity will be exactly VF. However, assuming that the initial velocity is much less than TOTAL_DISTANCE,
//      the last wheel delta will be very close, if not the same, as VF.
// For MAGNITUDE = (V0 + VF) / 2 and PHASE_SHIFT = arccos((V0 - VF) / (V0 + VF)), observe that delta(0) and delta(1) evaluate respectively to V0
// and VF. Thus, we can express our gliding equation all in terms of V0, VF and t.
float ScrollController::computeGlideDelta(ScrollEventAxis axis) const
{
    const ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);

    LayoutUnit offset = m_client->scrollOffsetOnAxis(axis);
    bool canComputeGlide = (snapState.m_initialOffset <= offset && offset < snapState.m_targetOffset) || (snapState.m_targetOffset < offset && offset <= snapState.m_initialOffset);
    if (snapState.m_currentState != ScrollSnapState::Gliding || !canComputeGlide)
        return 0;

    const float progress = snapProgress(offset, snapState);
    const float rawGlideDelta = snapGlide(progress, snapState);

    float glideDelta = snapState.m_initialOffset < snapState.m_targetOffset ? std::max<float>(rawGlideDelta, 1) : std::min<float>(rawGlideDelta, -1);
    if ((snapState.m_initialOffset < snapState.m_targetOffset && offset + glideDelta > snapState.m_targetOffset) || (snapState.m_initialOffset > snapState.m_targetOffset && offset + glideDelta < snapState.m_targetOffset))
        return snapState.m_targetOffset - offset;
    
    return glideDelta;
}
#endif

} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING)
