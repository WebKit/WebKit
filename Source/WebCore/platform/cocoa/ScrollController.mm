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
#import "Logging.h"
#import "PlatformWheelEvent.h"
#import "WheelEventTestMonitor.h"
#import <sys/sysctl.h>
#import <sys/time.h>
#import <wtf/text/TextStream.h>

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
static const Seconds scrollVelocityZeroingTimeout = 100_ms;
static const float rubberbandDirectionLockStretchRatio = 1;
static const float rubberbandMinimumRequiredDeltaBeforeStretch = 10;
#endif

#if PLATFORM(MAC)
static float elasticDeltaForTimeDelta(float initialPosition, float initialVelocity, Seconds elapsedTime)
{
    return _NSElasticDeltaForTimeDelta(initialPosition, initialVelocity, elapsedTime.seconds());
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
{
}

ScrollController::~ScrollController()
{
    ASSERT(m_timersWereStopped);
}

void ScrollController::stopAllTimers()
{
#if ENABLE(RUBBER_BANDING)
    if (m_snapRubberbandTimer)
        m_snapRubberbandTimer->stop();
#endif

#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
    if (m_statelessSnapTransitionTimer)
        m_statelessSnapTransitionTimer->stop();

    if (m_scrollSnapTimer)
        m_scrollSnapTimer->stop();
#endif

#if ASSERT_ENABLED
    m_timersWereStopped = true;
#endif
}

#if PLATFORM(MAC)
bool ScrollController::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
#if ENABLE(CSS_SCROLL_SNAP)
    if (processWheelEventForScrollSnap(wheelEvent))
        return false; // FIXME: Why don't we report that we handled it?
#endif
    if (wheelEvent.phase() == PlatformWheelEventPhaseMayBegin || wheelEvent.phase() == PlatformWheelEventPhaseCancelled)
        return false;

    if (wheelEvent.phase() == PlatformWheelEventPhaseBegan) {
        // FIXME: Trying to decide if a gesture is horizontal or vertical at the "began" phase is very error-prone.
        auto direction = directionFromEvent(wheelEvent, ScrollEventAxis::Horizontal);
        // FIXME: pinnedInDirection() needs cleanup.
        if (direction && m_client.pinnedInDirection(FloatSize(-wheelEvent.deltaX(), 0)) && !shouldRubberBandInDirection(direction.value()))
            return false;

        direction = directionFromEvent(wheelEvent, ScrollEventAxis::Vertical);
        if (direction && m_client.pinnedInDirection(FloatSize(0, -wheelEvent.deltaY())) && !shouldRubberBandInDirection(direction.value()))
            return false;

        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = { };
        m_momentumVelocity = { };

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
    if (m_ignoreMomentumScrolls && (isMomentumScrollEvent || m_snapRubberbandTimer)) {
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
    // FIXME: Use wheelDeltaBiasingTowardsVertical().
    if (fabsf(deltaY) >= fabsf(deltaX))
        deltaX = 0;
    else
        deltaY = 0;

    bool shouldStretch = false;

    PlatformWheelEventPhase momentumPhase = wheelEvent.momentumPhase();

    // If we are starting momentum scrolling then do some setup.
    if (!m_momentumScrollInProgress && (momentumPhase == PlatformWheelEventPhaseBegan || momentumPhase == PlatformWheelEventPhaseChanged))
        m_momentumScrollInProgress = true;

    auto timeDelta = wheelEvent.timestamp() - m_lastMomentumScrollTimestamp;
    if (m_inScrollGesture || m_momentumScrollInProgress) {
        if (m_lastMomentumScrollTimestamp && timeDelta > 0_s && timeDelta < scrollVelocityZeroingTimeout) {
            m_momentumVelocity.setWidth(eventCoalescedDeltaX / timeDelta.seconds());
            m_momentumVelocity.setHeight(eventCoalescedDeltaY / timeDelta.seconds());
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

    bool handled = true;

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
            if (deltaX) {
                if (!m_client.allowsHorizontalStretching(wheelEvent)) {
                    deltaX = 0;
                    eventCoalescedDeltaX = 0;
                    handled = false;
                } else if (!isHorizontallyStretched && !m_client.pinnedInDirection(FloatSize(deltaX, 0))) {
                    deltaX *= scrollWheelMultiplier();

                    m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(deltaX, 0));
                    deltaX = 0;
                }
            }

            if (deltaY) {
                if (!m_client.allowsVerticalStretching(wheelEvent)) {
                    deltaY = 0;
                    eventCoalescedDeltaY = 0;
                    handled = false;
                } else if (!isVerticallyStretched && !m_client.pinnedInDirection(FloatSize(0, deltaY))) {
                    deltaY *= scrollWheelMultiplier();

                    m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(0, deltaY));
                    deltaY = 0;
                }
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
        m_lastMomentumScrollTimestamp = { };
    }

    return handled;
}
#endif // PLATFORM(MAC)

FloatSize ScrollController::wheelDeltaBiasingTowardsVertical(const PlatformWheelEvent& wheelEvent)
{
    auto deltaX = wheelEvent.deltaX();
    auto deltaY = wheelEvent.deltaY();

    if (fabsf(deltaY) >= fabsf(deltaX))
        deltaX = 0;
    else
        deltaY = 0;

    return { deltaX, deltaY };
}

Optional<ScrollDirection> ScrollController::directionFromEvent(const PlatformWheelEvent& wheelEvent, Optional<ScrollEventAxis> axis, WheelAxisBias bias)
{
    // FIXME: It's impossible to infer direction from a single event, since the start of a gesture is either zero or
    // has small deltas on both axes.

    auto wheelDelta = FloatSize { wheelEvent.deltaX(), wheelEvent.deltaY() };
    if (bias == WheelAxisBias::Vertical)
        wheelDelta = wheelDeltaBiasingTowardsVertical(wheelEvent);

    if (axis) {
        switch (axis.value()) {
        case ScrollEventAxis::Vertical:
            if (wheelDelta.height() < 0)
                return ScrollDown;

            if (wheelDelta.height() > 0)
                return ScrollUp;
            break;

        case ScrollEventAxis::Horizontal:
            if (wheelDelta.width() > 0)
                return ScrollLeft;

            if (wheelDelta.width() < 0)
                return ScrollRight;
        }

        return WTF::nullopt;
    }

    // Check Y first because vertical scrolling dominates.
    if (wheelDelta.height() < 0)
        return ScrollDown;

    if (wheelDelta.height() > 0)
        return ScrollUp;

    if (wheelDelta.width() > 0)
        return ScrollLeft;

    if (wheelDelta.width() < 0)
        return ScrollRight;

    return WTF::nullopt;
}

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
        auto timeDelta = MonotonicTime::now() - m_startTime;

        if (m_startStretch.isZero()) {
            m_startStretch = m_client.stretchAmount();
            if (m_startStretch == FloatSize()) {
                stopSnapRubberbandTimer();

                m_stretchScrollForce = { };
                m_startTime = { };
                m_startStretch = { };
                m_origVelocity = { };
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

        FloatPoint delta(roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.width(), -m_origVelocity.width(), timeDelta)),
            roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.height(), -m_origVelocity.height(), timeDelta)));

        if (fabs(delta.x()) >= 1 || fabs(delta.y()) >= 1) {
            m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(delta.x(), delta.y()) - m_client.stretchAmount());

            FloatSize newStretch = m_client.stretchAmount();

            m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(newStretch.width()));
            m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(newStretch.height()));
        } else {
            m_client.adjustScrollPositionToBoundsIfNecessary();

            stopSnapRubberbandTimer();
            m_stretchScrollForce = { };
            m_startTime = { };
            m_startStretch = { };
            m_origVelocity = { };
        }
    } else {
        m_startTime = MonotonicTime::now();
        m_startStretch = { };
        if (!isRubberBandInProgress())
            stopSnapRubberbandTimer();
    }
}
#endif

bool ScrollController::usesScrollSnap() const
{
#if ENABLE(CSS_SCROLL_SNAP)
    return !!m_scrollSnapState;
#else
    return false;
#endif
}

bool ScrollController::isUserScrollInProgress() const
{
#if PLATFORM(MAC)
    return m_inScrollGesture;
#else
    return false;
#endif
}

bool ScrollController::isRubberBandInProgress() const
{
#if ENABLE(RUBBER_BANDING) && PLATFORM(MAC)
    if (!m_inScrollGesture && !m_momentumScrollInProgress && !m_snapRubberbandTimer)
        return false;

    return !m_client.stretchAmount().isZero();
#else
    return false;
#endif
}

bool ScrollController::isScrollSnapInProgress() const
{
    if (!usesScrollSnap())
        return false;

#if ENABLE(CSS_SCROLL_SNAP) && PLATFORM(MAC)
    if (m_inScrollGesture || m_momentumScrollInProgress || m_scrollSnapTimer)
        return true;
#endif
    return false;
}

#if ENABLE(RUBBER_BANDING)
void ScrollController::startSnapRubberbandTimer()
{
    m_client.willStartRubberBandSnapAnimation();

    // Make a new one each time to ensure it fires on the current RunLoop.
    m_snapRubberbandTimer = m_client.createTimer([this] {
        snapRubberBandTimerFired();
    });
    m_snapRubberbandTimer->startRepeating(1_s / 60.);

    m_client.deferWheelEventTestCompletionForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::RubberbandInProgress);
}

void ScrollController::stopSnapRubberbandTimer()
{
    m_client.didStopRubberbandSnapAnimation();
    
    if (m_snapRubberbandTimer) {
        m_snapRubberbandTimer->stop();
        m_snapRubberbandTimer = nullptr;
    }
    
    m_client.removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::RubberbandInProgress);
}

void ScrollController::snapRubberBand()
{
    auto timeDelta = WallTime::now() - m_lastMomentumScrollTimestamp;
    if (m_lastMomentumScrollTimestamp && timeDelta >= scrollVelocityZeroingTimeout)
        m_momentumVelocity = { };

    if (m_snapRubberbandTimer)
        return;

    m_startTime = MonotonicTime::now();
    m_startStretch = { };
    m_origVelocity = { };

    startSnapRubberbandTimer();
}

bool ScrollController::shouldRubberBandInHorizontalDirection(const PlatformWheelEvent& wheelEvent) const
{
    auto direction = directionFromEvent(wheelEvent, ScrollEventAxis::Horizontal);
    if (direction)
        return shouldRubberBandInDirection(direction.value());

    return true;
}

bool ScrollController::shouldRubberBandInDirection(ScrollDirection direction) const
{
    return m_client.shouldRubberBandInDirection(direction);
}

#endif // ENABLE(RUBBER_BANDING)

#if ENABLE(CSS_SCROLL_SNAP)

#if PLATFORM(MAC)
static inline WheelEventStatus toWheelEventStatus(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    if (phase == PlatformWheelEventPhaseNone) {
        switch (momentumPhase) {
        case PlatformWheelEventPhaseBegan:
            return WheelEventStatus::MomentumScrollBegin;
                
        case PlatformWheelEventPhaseChanged:
            return WheelEventStatus::MomentumScrolling;
                
        case PlatformWheelEventPhaseEnded:
            return WheelEventStatus::MomentumScrollEnd;

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

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, WheelEventStatus status)
{
    switch (status) {
    case WheelEventStatus::UserScrollBegin: ts << "UserScrollBegin"; break;
    case WheelEventStatus::UserScrolling: ts << "UserScrolling"; break;
    case WheelEventStatus::UserScrollEnd: ts << "UserScrollEnd"; break;
    case WheelEventStatus::MomentumScrollBegin: ts << "MomentumScrollBegin"; break;
    case WheelEventStatus::MomentumScrolling: ts << "MomentumScrolling"; break;
    case WheelEventStatus::MomentumScrollEnd: ts << "MomentumScrollEnd"; break;
    case WheelEventStatus::StatelessScrollEvent: ts << "StatelessScrollEvent"; break;
    case WheelEventStatus::Unknown: ts << "Unknown"; break;
    }
    return ts;
}
#endif

bool ScrollController::shouldOverrideMomentumScrolling() const
{
    if (!usesScrollSnap())
        return false;

    ScrollSnapState scrollSnapState = m_scrollSnapState->currentState();
    return scrollSnapState == ScrollSnapState::Gliding || scrollSnapState == ScrollSnapState::DestinationReached;
}

void ScrollController::scheduleStatelessScrollSnap()
{
    stopScrollSnapTimer();
    if (m_statelessSnapTransitionTimer) {
        m_statelessSnapTransitionTimer->stop();
        m_statelessSnapTransitionTimer = nullptr;
    }
    if (!usesScrollSnap())
        return;

    static const Seconds statelessScrollSnapDelay = 750_ms;
    m_statelessSnapTransitionTimer = m_client.createTimer([this] {
        statelessSnapTransitionTimerFired();
    });
    m_statelessSnapTransitionTimer->startOneShot(statelessScrollSnapDelay);
    startDeferringWheelEventTestCompletionDueToScrollSnapping();
}

void ScrollController::statelessSnapTransitionTimerFired()
{
    m_statelessSnapTransitionTimer = nullptr;

    if (!usesScrollSnap())
        return;

    m_scrollSnapState->transitionToSnapAnimationState(m_client.scrollExtent(), m_client.viewportSize(), m_client.pageScaleFactor(), m_client.scrollOffset());
    startScrollSnapTimer();
}

void ScrollController::startDeferringWheelEventTestCompletionDueToScrollSnapping()
{
    m_client.deferWheelEventTestCompletionForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::ScrollSnapInProgress);
}

void ScrollController::stopDeferringWheelEventTestCompletionDueToScrollSnapping()
{
    m_client.removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::ScrollSnapInProgress);
}

bool ScrollController::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    if (!usesScrollSnap())
        return false;

    if (m_scrollSnapState->snapOffsetsForAxis(ScrollEventAxis::Horizontal).isEmpty() && m_scrollSnapState->snapOffsetsForAxis(ScrollEventAxis::Vertical).isEmpty())
        return false;

    auto status = toWheelEventStatus(wheelEvent.phase(), wheelEvent.momentumPhase());

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollController " << this << " processWheelEventForScrollSnap: status " << status);

    bool isMomentumScrolling = false;
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
    case WheelEventStatus::MomentumScrollBegin:
        m_scrollSnapState->transitionToGlideAnimationState(m_client.scrollExtent(), m_client.viewportSize(), m_client.pageScaleFactor(), m_client.scrollOffset(), m_dragEndedScrollingVelocity, FloatSize(-wheelEvent.deltaX(), -wheelEvent.deltaY()));
        m_dragEndedScrollingVelocity = { };
        isMomentumScrolling = true;
        break;
    case WheelEventStatus::MomentumScrolling:
    case WheelEventStatus::MomentumScrollEnd:
        isMomentumScrolling = true;
        break;
    case WheelEventStatus::StatelessScrollEvent:
        m_scrollSnapState->transitionToUserInteractionState();
        scheduleStatelessScrollSnap();
        break;
    case WheelEventStatus::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return isMomentumScrolling && shouldOverrideMomentumScrolling();
}

void ScrollController::updateGestureInProgressState(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.isGestureBegin() || wheelEvent.isTransitioningToMomentumScroll())
        m_inScrollGesture = true;
    else if (wheelEvent.isEndOfNonMomentumScroll() || wheelEvent.isGestureCancel() || wheelEvent.isEndOfMomentumScroll())
        m_inScrollGesture = false;
}

void ScrollController::startScrollSnapTimer()
{
    if (m_scrollSnapTimer)
        return;

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollController " << this << " startScrollSnapTimer (main thread " << isMainThread() << ")");

    startDeferringWheelEventTestCompletionDueToScrollSnapping();
    m_client.willStartScrollSnapAnimation();
    m_scrollSnapTimer = m_client.createTimer([this] {
        scrollSnapTimerFired();
    });
    m_scrollSnapTimer->startRepeating(1_s / 60.);
}

void ScrollController::stopScrollSnapTimer()
{
    if (!m_scrollSnapTimer)
        return;

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollController " << this << " stopScrollSnapTimer (main thread " << isMainThread() << ")");

    stopDeferringWheelEventTestCompletionDueToScrollSnapping();
    m_client.didStopScrollSnapAnimation();

    m_scrollSnapTimer->stop();
    m_scrollSnapTimer = nullptr;
}

void ScrollController::scrollSnapTimerFired()
{
    if (!usesScrollSnap()) {
        ASSERT_NOT_REACHED();
        return;
    }

    bool isAnimationComplete;
    auto animationOffset = m_scrollSnapState->currentAnimatedScrollOffset(isAnimationComplete);
    auto currentOffset = m_client.scrollOffset();

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollController " << this << " scrollSnapTimerFired - isAnimationComplete " << isAnimationComplete << " currentOffset " << currentOffset << " (main thread " << isMainThread() << ")");

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

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollController " << this << " updateScrollSnapState for " << scrollableArea << " new state " << ValueOrNull(m_scrollSnapState.get()));
}

void ScrollController::updateScrollSnapPoints(ScrollEventAxis axis, const Vector<LayoutUnit>& snapPoints, const Vector<ScrollOffsetRange<LayoutUnit>>& snapRanges)
{
    bool shouldComputeCurrentSnapIndices = false;
    if (!m_scrollSnapState) {
        if (snapPoints.isEmpty())
            return;

        m_scrollSnapState = makeUnique<ScrollSnapAnimatorState>();
        shouldComputeCurrentSnapIndices = true;
    }

    if (snapPoints.isEmpty() && m_scrollSnapState->snapOffsetsForAxis(otherScrollEventAxis(axis)).isEmpty())
        m_scrollSnapState = nullptr;
    else {
        m_scrollSnapState->setSnapOffsetsAndPositionRangesForAxis(axis, snapPoints, snapRanges);
        if (shouldComputeCurrentSnapIndices) {
            setActiveScrollSnapIndicesForOffset(roundedIntPoint(m_client.scrollOffset()));
            LOG_WITH_STREAM(ScrollSnap, stream << "ScrollController::updateScrollSnapPoints - computed initial snap indices: x " << m_scrollSnapState->activeSnapIndexForAxis(ScrollEventAxis::Horizontal) << ", y " << m_scrollSnapState->activeSnapIndexForAxis(ScrollEventAxis::Vertical));
        }
    }
}

unsigned ScrollController::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    if (!usesScrollSnap())
        return 0;

    return m_scrollSnapState->activeSnapIndexForAxis(axis);
}

void ScrollController::setActiveScrollSnapIndexForAxis(ScrollEventAxis axis, unsigned index)
{
    if (!usesScrollSnap())
        return;

    m_scrollSnapState->setActiveSnapIndexForAxis(axis, index);
}

void ScrollController::setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis axis, int offset)
{
    if (!usesScrollSnap())
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

void ScrollController::setActiveScrollSnapIndicesForOffset(ScrollOffset offset)
{
    if (!usesScrollSnap())
        return;

    setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Horizontal, offset.x());
    setNearestScrollSnapIndexForAxisAndOffset(ScrollEventAxis::Vertical, offset.y());
}
#endif

} // namespace WebCore

#endif // ENABLE(RUBBER_BANDING)
