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

#include "LayoutSize.h"
#include "PlatformWheelEvent.h"
#include "WebCoreSystemInterface.h"
#include "WheelEventTestTrigger.h"
#include <sys/sysctl.h>
#include <sys/time.h>
#include <wtf/CurrentTime.h>

#if ENABLE(CSS_SCROLL_SNAP)
#include "ScrollSnapAnimatorState.h"
#include "ScrollableArea.h"
#endif

#if ENABLE(RUBBER_BANDING) || ENABLE(CSS_SCROLL_SNAP)

#if PLATFORM(MAC)
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
#endif

namespace WebCore {

#if ENABLE(RUBBER_BANDING)
static const float scrollVelocityZeroingTimeout = 0.10f;
static const float rubberbandDirectionLockStretchRatio = 1;
static const float rubberbandMinimumRequiredDeltaBeforeStretch = 10;
#endif

#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
static const float inertialScrollPredictionFactor = 16.7;
static const double statelessScrollSnapDelay = 0.5;
#endif

#if PLATFORM(MAC)
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
#endif

ScrollController::ScrollController(ScrollControllerClient& client)
    : m_client(client)
#if ENABLE(RUBBER_BANDING)
    , m_snapRubberbandTimer(RunLoop::current(), this, &ScrollController::snapRubberBandTimerFired)
#endif
#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
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
    m_snapRubberbandTimer.startRepeating(1.0 / 60.0);

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
    CFTimeInterval timeDelta = systemUptime() - m_lastMomentumScrollTimestamp;
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

void ScrollController::processWheelEventForScrollSnapOnAxis(ScrollEventAxis axis, const PlatformWheelEvent& event)
{
    ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);

    float wheelDelta = axis == ScrollEventAxis::Horizontal ? -event.deltaX() : -event.deltaY();
    WheelEventStatus wheelStatus = toWheelEventStatus(event.phase(), event.momentumPhase());
    
    switch (wheelStatus) {
    case WheelEventStatus::UserScrollBegin:
    case WheelEventStatus::UserScrolling:
        endScrollSnapAnimation(ScrollSnapState::UserInteraction);
        break;
            
    case WheelEventStatus::UserScrollEnd:
        beginScrollSnapAnimation(axis, ScrollSnapState::Snapping);
        break;
        
    case WheelEventStatus::InertialScrollBegin:
        // Begin tracking wheel deltas for glide prediction.
        endScrollSnapAnimation(ScrollSnapState::UserInteraction);
        snapState.pushInitialWheelDelta(wheelDelta);
        snapState.m_beginTrackingWheelDeltaOffset = m_client.scrollOffsetOnAxis(axis);
        break;
            
    case WheelEventStatus::InertialScrolling:
        // This check for DestinationReached ensures that we don't receive another set of momentum events after ending the last glide.
        if (snapState.m_currentState != ScrollSnapState::Gliding && snapState.m_currentState != ScrollSnapState::DestinationReached) {
            if (snapState.wheelDeltaTrackingIsInProgress() && wheelDelta)
                snapState.pushInitialWheelDelta(wheelDelta);
            
            if (snapState.hasFinishedTrackingWheelDeltas() && snapState.averageInitialWheelDelta())
                beginScrollSnapAnimation(axis, ScrollSnapState::Gliding);
        }
        break;
        
    case WheelEventStatus::InertialScrollEnd:
        if (snapState.wheelDeltaTrackingIsInProgress() && snapState.averageInitialWheelDelta())
            beginScrollSnapAnimation(axis, ScrollSnapState::Gliding);

        snapState.clearInitialWheelDeltaWindow();
        snapState.m_shouldOverrideWheelEvent = false;
        break;

    case WheelEventStatus::StatelessScrollEvent:
        endScrollSnapAnimation(ScrollSnapState::UserInteraction);
        snapState.clearInitialWheelDeltaWindow();
        snapState.m_shouldOverrideWheelEvent = false;
        m_scrollSnapTimer.startOneShot(statelessScrollSnapDelay);
        if (axis == ScrollEventAxis::Horizontal)
            m_expectingHorizontalStatelessScrollSnap = true;
        else
            m_expectingVerticalStatelessScrollSnap = true;
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
    bool shouldAllowWheelEventToPropagate = true;
    if (m_verticalScrollSnapState) {
        processWheelEventForScrollSnapOnAxis(ScrollEventAxis::Vertical, wheelEvent);
        shouldAllowWheelEventToPropagate &= !shouldOverrideWheelEvent(ScrollEventAxis::Vertical, wheelEvent);
    }
    if (m_horizontalScrollSnapState) {
        processWheelEventForScrollSnapOnAxis(ScrollEventAxis::Horizontal, wheelEvent);
        shouldAllowWheelEventToPropagate &= !shouldOverrideWheelEvent(ScrollEventAxis::Horizontal, wheelEvent);
    }
    return shouldAllowWheelEventToPropagate;
}
#endif

void ScrollController::updateScrollSnapState(const ScrollableArea& scrollableArea)
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

#if PLATFORM(MAC)
void ScrollController::startScrollSnapTimer()
{
    if (!m_scrollSnapTimer.isActive()) {
        m_client.startScrollSnapTimer();
        m_scrollSnapTimer.startRepeating(1.0 / 60.0);
    }

    if (!m_scrollSnapTimer.isActive())
        return;

    m_client.deferTestsForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(this), WheelEventTestTrigger::ScrollSnapInProgress);
}

void ScrollController::stopScrollSnapTimer()
{
    m_client.stopScrollSnapTimer();
    m_scrollSnapTimer.stop();
    
    if (m_scrollSnapTimer.isActive())
        return;

    m_client.removeTestDeferralForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(this), WheelEventTestTrigger::ScrollSnapInProgress);
}

void ScrollController::scrollSnapTimerFired()
{
    if (m_expectingHorizontalStatelessScrollSnap || m_expectingVerticalStatelessScrollSnap) {
        if (m_expectingHorizontalStatelessScrollSnap)
            beginScrollSnapAnimation(ScrollEventAxis::Horizontal, ScrollSnapState::Snapping);
        if (m_expectingVerticalStatelessScrollSnap)
            beginScrollSnapAnimation(ScrollEventAxis::Vertical, ScrollSnapState::Snapping);
        return;
    }
        
    bool snapOnHorizontalAxis = isSnappingOnAxis(ScrollEventAxis::Horizontal);
    bool snapOnVerticalAxis = isSnappingOnAxis(ScrollEventAxis::Vertical);
    if (snapOnHorizontalAxis && !m_horizontalScrollSnapState->canReachTargetWithCurrentInitialScrollDelta()) {
        m_horizontalScrollSnapState->m_currentState = ScrollSnapState::DestinationReached;
        snapOnHorizontalAxis = false;
    }
    if (snapOnVerticalAxis && !m_verticalScrollSnapState->canReachTargetWithCurrentInitialScrollDelta()) {
        m_verticalScrollSnapState->m_currentState = ScrollSnapState::DestinationReached;
        snapOnVerticalAxis = false;
    }
    if (!snapOnHorizontalAxis && !snapOnVerticalAxis) {
        endScrollSnapAnimation(ScrollSnapState::DestinationReached);
        return;
    }
    
    double currentTime = monotonicallyIncreasingTime();
    if (m_scrollSnapCurveState->shouldCompleteSnapAnimationImmediatelyAtTime(currentTime)) {
        float finalHorizontalDelta = 0;
        float finalVerticalDelta = 0;
        if (snapOnHorizontalAxis)
            finalHorizontalDelta = m_horizontalScrollSnapState->m_targetOffset - m_client.scrollOffsetOnAxis(ScrollEventAxis::Horizontal);
        if (snapOnVerticalAxis)
            finalVerticalDelta = m_verticalScrollSnapState->m_targetOffset - m_client.scrollOffsetOnAxis(ScrollEventAxis::Vertical);

        if (finalHorizontalDelta || finalVerticalDelta)
            m_client.immediateScrollBy(FloatSize(finalHorizontalDelta, finalVerticalDelta));

        endScrollSnapAnimation(ScrollSnapState::DestinationReached);
        return;
    }
    
    float animationProgress = m_scrollSnapCurveState->animationProgressAtTime(currentTime);
    float horizontalDelta = 0;
    float verticalDelta = 0;
    if (m_scrollSnapCurveState->shouldAnimateDirectlyToSnapPoint) {
        if (snapOnHorizontalAxis)
            horizontalDelta = m_horizontalScrollSnapState->interpolatedOffsetAtProgress(animationProgress) - m_client.scrollOffsetOnAxis(ScrollEventAxis::Horizontal);
        if (snapOnVerticalAxis)
            verticalDelta = m_verticalScrollSnapState->interpolatedOffsetAtProgress(animationProgress) - m_client.scrollOffsetOnAxis(ScrollEventAxis::Vertical);

    } else {
        FloatPoint interpolatedPoint = m_scrollSnapCurveState->interpolatedPositionAtProgress(animationProgress);
        horizontalDelta = interpolatedPoint.x() - m_client.scrollOffsetOnAxis(ScrollEventAxis::Horizontal);
        verticalDelta = interpolatedPoint.y() - m_client.scrollOffsetOnAxis(ScrollEventAxis::Vertical);
    }
    
    if (horizontalDelta || verticalDelta)
        m_client.immediateScrollBy(FloatSize(horizontalDelta, verticalDelta));
}

static inline float projectedInertialScrollDistance(float initialWheelDelta)
{
    // FIXME: Experiments with inertial scrolling show a fairly consistent linear relationship between initial wheel delta and total distance scrolled.
    // In the future, we'll want to find a more accurate way of inertial scroll prediction.
    return inertialScrollPredictionFactor * initialWheelDelta;
}
#endif

unsigned ScrollController::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    if ((axis == ScrollEventAxis::Horizontal) && !m_horizontalScrollSnapState)
        return 0;
    if ((axis == ScrollEventAxis::Vertical) && !m_verticalScrollSnapState)
        return 0;
    
    const ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);
    return snapState.m_activeSnapIndex;
}

void ScrollController::setActiveScrollSnapIndexForAxis(ScrollEventAxis axis, unsigned index)
{
    auto* snapState = (axis == ScrollEventAxis::Horizontal) ? m_horizontalScrollSnapState.get() : m_verticalScrollSnapState.get();
    if (!snapState)
        return;

    snapState->m_activeSnapIndex = index;
}

void ScrollController::setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis axis, int offset)
{
    float scaleFactor = m_client.pageScaleFactor();
    ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);
    
    LayoutUnit clampedOffset = std::min(std::max(LayoutUnit(offset / scaleFactor), snapState.m_snapOffsets.first()), snapState.m_snapOffsets.last());

    unsigned activeIndex = 0;
    (void)closestSnapOffset<LayoutUnit, float>(snapState.m_snapOffsets, clampedOffset, 0, activeIndex);

    if (activeIndex == snapState.m_activeSnapIndex)
        return;

    m_activeScrollSnapIndexDidChange = true;
    snapState.m_activeSnapIndex = activeIndex;
}

void ScrollController::setActiveScrollSnapIndicesForOffset(int x, int y)
{
    if (m_horizontalScrollSnapState)
        setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Horizontal, x);
    if (m_verticalScrollSnapState)
        setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Vertical, y);
}

#if PLATFORM(MAC)
void ScrollController::beginScrollSnapAnimation(ScrollEventAxis axis, ScrollSnapState newState)
{
    ASSERT(newState == ScrollSnapState::Gliding || newState == ScrollSnapState::Snapping);
    if (m_expectingHorizontalStatelessScrollSnap || m_expectingVerticalStatelessScrollSnap) {
        m_expectingHorizontalStatelessScrollSnap = false;
        m_expectingVerticalStatelessScrollSnap = false;
        stopScrollSnapTimer();
    }
    ScrollSnapAnimatorState& snapState = scrollSnapPointState(axis);

    LayoutUnit offset = m_client.scrollOffsetOnAxis(axis);
    float initialWheelDelta = newState == ScrollSnapState::Gliding ? snapState.averageInitialWheelDelta() : 0;
    LayoutUnit scaledProjectedScrollDestination = newState == ScrollSnapState::Gliding ? snapState.m_beginTrackingWheelDeltaOffset + LayoutUnit(projectedInertialScrollDistance(initialWheelDelta)) : offset;
    if (snapState.m_snapOffsets.isEmpty())
        return;

    float scaleFactor = m_client.pageScaleFactor();
    LayoutUnit originalProjectedScrollDestination = scaledProjectedScrollDestination / scaleFactor;
    
    LayoutUnit clampedScrollDestination = std::min(std::max(originalProjectedScrollDestination, snapState.m_snapOffsets.first()), snapState.m_snapOffsets.last());
    snapState.m_initialOffset = offset;
    m_activeScrollSnapIndexDidChange = false;
    snapState.m_targetOffset = scaleFactor * closestSnapOffset<LayoutUnit, float>(snapState.m_snapOffsets, clampedScrollDestination, initialWheelDelta, snapState.m_activeSnapIndex);
    if (snapState.m_initialOffset == snapState.m_targetOffset)
        return;

    LayoutUnit scrollExtent = (axis == ScrollEventAxis::Horizontal) ? m_client.scrollExtent().width() : m_client.scrollExtent().height();
    LayoutUnit projectedScrollDestination = clampedScrollDestination;
    if (originalProjectedScrollDestination < 0 || originalProjectedScrollDestination > scrollExtent)
        projectedScrollDestination = originalProjectedScrollDestination;
    
    m_activeScrollSnapIndexDidChange = true;
    snapState.m_currentState = newState;
    if (newState == ScrollSnapState::Gliding) {
        // Check if the other scroll axis needs to animate to the nearest snap point.
        snapState.m_initialScrollDelta = initialWheelDelta;
        snapState.m_shouldOverrideWheelEvent = true;
        snapState.clearInitialWheelDeltaWindow();
        ScrollEventAxis otherAxis = axis == ScrollEventAxis::Horizontal ? ScrollEventAxis::Vertical : ScrollEventAxis::Horizontal;
        if ((otherAxis == ScrollEventAxis::Horizontal && m_horizontalScrollSnapState && m_horizontalScrollSnapState->m_currentState == ScrollSnapState::UserInteraction)
            || (otherAxis == ScrollEventAxis::Vertical && m_verticalScrollSnapState && m_verticalScrollSnapState->m_currentState == ScrollSnapState::UserInteraction)) {
            
            ScrollSnapAnimatorState& otherState = scrollSnapPointState(otherAxis);
            if (!otherState.averageInitialWheelDelta()) {
                float offsetOnOtherAxis = m_client.scrollOffsetOnAxis(otherAxis);
                float snapOffsetForOtherAxis = scaleFactor * closestSnapOffset<LayoutUnit, float>(otherState.m_snapOffsets, offsetOnOtherAxis, 0, otherState.m_activeSnapIndex);
                if (offsetOnOtherAxis != snapOffsetForOtherAxis) {
                    otherState.m_initialOffset = offsetOnOtherAxis;
                    otherState.m_targetOffset = snapOffsetForOtherAxis;
                    otherState.m_initialScrollDelta = 0;
                    otherState.m_currentState = ScrollSnapState::Gliding;
                }
            }
        }
        
    } else {
        snapState.m_initialScrollDelta = initialWheelDelta;
    }
    initializeScrollSnapAnimationParameters();
    startScrollSnapTimer();
}

void ScrollController::endScrollSnapAnimation(ScrollSnapState newState)
{
    ASSERT(newState == ScrollSnapState::DestinationReached || newState == ScrollSnapState::UserInteraction);
    if (m_horizontalScrollSnapState)
        m_horizontalScrollSnapState->m_currentState = newState;

    if (m_verticalScrollSnapState)
        m_verticalScrollSnapState->m_currentState = newState;

    stopScrollSnapTimer();
}

void ScrollController::initializeScrollSnapAnimationParameters()
{
    if (!m_scrollSnapCurveState)
        m_scrollSnapCurveState = std::make_unique<ScrollSnapAnimationCurveState>();
    
    bool isSnappingOnHorizontalAxis = isSnappingOnAxis(ScrollEventAxis::Horizontal);
    bool isSnappingOnVerticalAxis = isSnappingOnAxis(ScrollEventAxis::Vertical);
    FloatSize initialVector(isSnappingOnHorizontalAxis ? m_horizontalScrollSnapState->m_initialOffset : m_client.scrollOffsetOnAxis(ScrollEventAxis::Horizontal),
        isSnappingOnVerticalAxis ? m_verticalScrollSnapState->m_initialOffset : m_client.scrollOffsetOnAxis(ScrollEventAxis::Vertical));
    FloatSize targetVector(isSnappingOnHorizontalAxis ? m_horizontalScrollSnapState->m_targetOffset : m_client.scrollOffsetOnAxis(ScrollEventAxis::Horizontal),
        isSnappingOnVerticalAxis ? m_verticalScrollSnapState->m_targetOffset : m_client.scrollOffsetOnAxis(ScrollEventAxis::Vertical));
    FloatSize initialDelta(isSnappingOnHorizontalAxis ? m_horizontalScrollSnapState->m_initialScrollDelta : 0,
        isSnappingOnVerticalAxis ? m_verticalScrollSnapState->m_initialScrollDelta : 0);

    // Animate directly by default. This flag will be changed as necessary if interpolation is possible.
    m_scrollSnapCurveState->shouldAnimateDirectlyToSnapPoint = true;
    m_scrollSnapCurveState->initializeSnapProgressCurve(initialVector, targetVector, initialDelta);
    if (isSnappingOnHorizontalAxis && isSnappingOnVerticalAxis)
        m_scrollSnapCurveState->initializeInterpolationCoefficientsIfNecessary(initialVector, targetVector, initialDelta);
}
    
bool ScrollController::isSnappingOnAxis(ScrollEventAxis axis) const
{
    if (axis == ScrollEventAxis::Horizontal)
        return m_horizontalScrollSnapState && m_horizontalScrollSnapState->isSnapping();

    return m_verticalScrollSnapState && m_verticalScrollSnapState->isSnapping();
}
    
#endif
#endif

} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING)
