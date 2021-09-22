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
#import "ScrollingEffectsController.h"

#import "Logging.h"
#import "PlatformWheelEvent.h"
#import "WheelEventTestMonitor.h"
#import <pal/spi/mac/NSScrollViewSPI.h>
#import <sys/sysctl.h>
#import <sys/time.h>
#import <wtf/text/TextStream.h>

#if PLATFORM(MAC)

namespace WebCore {

static const Seconds scrollVelocityZeroingTimeout = 100_ms;
static const float rubberbandDirectionLockStretchRatio = 1;
static const float rubberbandMinimumRequiredDeltaBeforeStretch = 10;

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

ScrollingEffectsController::~ScrollingEffectsController()
{
    ASSERT(m_timersWereStopped);
}

void ScrollingEffectsController::stopAllTimers()
{
    if (m_statelessSnapTransitionTimer)
        m_statelessSnapTransitionTimer->stop();

#if ASSERT_ENABLED
    m_timersWereStopped = true;
#endif
}

bool ScrollingEffectsController::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (processWheelEventForScrollSnap(wheelEvent))
        return true;

    if (wheelEvent.phase() == PlatformWheelEventPhase::MayBegin || wheelEvent.phase() == PlatformWheelEventPhase::Cancelled)
        return false;

    if (wheelEvent.phase() == PlatformWheelEventPhase::Began) {
        // FIXME: Trying to decide if a gesture is horizontal or vertical at the "began" phase is very error-prone.
        auto direction = directionFromEvent(wheelEvent, ScrollEventAxis::Horizontal);
        if (direction && m_client.isPinnedForScrollDelta(FloatSize(-wheelEvent.deltaX(), 0)) && !shouldRubberBandInDirection(direction.value()))
            return false;

        direction = directionFromEvent(wheelEvent, ScrollEventAxis::Vertical);
        if (direction && m_client.isPinnedForScrollDelta(FloatSize(0, -wheelEvent.deltaY())) && !shouldRubberBandInDirection(direction.value()))
            return false;

        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = { };
        m_momentumVelocity = { };

        IntSize stretchAmount = m_client.stretchAmount();
        m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(stretchAmount.width()));
        m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(stretchAmount.height()));
        m_overflowScrollDelta = { };

        stopSnapRubberbandAnimation();
        updateRubberBandingState();
        return true;
    }

    if (wheelEvent.phase() == PlatformWheelEventPhase::Ended) {
        // FIXME: This triggers the rubberband timer even when we don't start rubberbanding.
        snapRubberBand();
        updateRubberBandingState();
        return true;
    }

    bool isMomentumScrollEvent = (wheelEvent.momentumPhase() != PlatformWheelEventPhase::None);
    if (m_ignoreMomentumScrolls && (isMomentumScrollEvent || m_isAnimatingRubberBand)) {
        if (wheelEvent.momentumPhase() == PlatformWheelEventPhase::Ended) {
            m_ignoreMomentumScrolls = false;
            return true;
        }

        if (isMomentumScrollEvent && m_ignoreMomentumScrolls)
            return true;

        return false;
    }

    float deltaX = m_overflowScrollDelta.width();
    float deltaY = m_overflowScrollDelta.height();

    // Reset overflow values because we may decide to remove delta at various points and put it into overflow.
    m_overflowScrollDelta = { };

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
    if (!m_momentumScrollInProgress && (momentumPhase == PlatformWheelEventPhase::Began || momentumPhase == PlatformWheelEventPhase::Changed))
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
            if (!isHorizontallyStretched && m_client.isPinnedForScrollDelta(FloatSize(deltaX, 0))) {
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
            if (m_client.isPinnedForScrollDelta(FloatSize(0, deltaY))) {
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
            if (m_client.isPinnedForScrollDelta(FloatSize(deltaX, deltaY))) {
                if (fabsf(deltaY) >= fabsf(deltaX)) {
                    if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                        m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
                        deltaX = 0;
                    } else
                        m_overflowScrollDelta.setWidth(m_overflowScrollDelta.width() + deltaX);
                }

                if (!m_client.allowsHorizontalStretching(wheelEvent))
                    deltaX = 0;

                if (!m_client.allowsVerticalStretching(wheelEvent))
                    deltaY = 0;

                shouldStretch = deltaX || deltaY;
            }
        }

        LOG_WITH_STREAM(Scrolling, stream << "ScrollingEffectsController::handleWheelEvent() - deltaX " << deltaX << " deltaY " << deltaY << " pinned " << m_client.isPinnedForScrollDelta(FloatSize(deltaX, deltaY)) << " shouldStretch " << shouldStretch);
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
                } else if (!isHorizontallyStretched && !m_client.isPinnedForScrollDelta(FloatSize(deltaX, 0))) {
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
                } else if (!isVerticallyStretched && !m_client.isPinnedForScrollDelta(FloatSize(0, deltaY))) {
                    deltaY *= scrollWheelMultiplier();

                    m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(0, deltaY));
                    deltaY = 0;
                }
            }

            IntSize stretchAmount = m_client.stretchAmount();

            if (m_momentumScrollInProgress) {
                if ((m_client.isPinnedForScrollDelta(FloatSize(eventCoalescedDeltaX, eventCoalescedDeltaY)) || (fabsf(eventCoalescedDeltaX) + fabsf(eventCoalescedDeltaY) <= 0)) && m_lastMomentumScrollTimestamp) {
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

    if (m_momentumScrollInProgress && momentumPhase == PlatformWheelEventPhase::Ended) {
        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = { };
    }

    updateRubberBandingState();

    return handled;
}

FloatSize ScrollingEffectsController::wheelDeltaBiasingTowardsVertical(const PlatformWheelEvent& wheelEvent)
{
    auto deltaX = wheelEvent.deltaX();
    auto deltaY = wheelEvent.deltaY();

    if (fabsf(deltaY) >= fabsf(deltaX))
        deltaX = 0;
    else
        deltaY = 0;

    return { deltaX, deltaY };
}

std::optional<ScrollDirection> ScrollingEffectsController::directionFromEvent(const PlatformWheelEvent& wheelEvent, std::optional<ScrollEventAxis> axis, WheelAxisBias bias)
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

        return std::nullopt;
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

    return std::nullopt;
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

void ScrollingEffectsController::updateRubberBandAnimatingState(MonotonicTime currentTime)
{
    if (!m_isAnimatingRubberBand)
        return;

    if (isScrollSnapInProgress())
        return;
    
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingEffectsController::updateRubberBandAnimatingState() - main thread " << isMainThread());

    if (!m_momentumScrollInProgress || m_ignoreMomentumScrolls) {
        auto timeDelta = currentTime - m_startTime;

        if (m_startStretch.isZero()) {
            m_startStretch = m_client.stretchAmount();
            if (m_startStretch == FloatSize()) {
                stopRubberbanding();
                return;
            }

            m_origVelocity = m_momentumVelocity;

            // Just like normal scrolling, prefer vertical rubberbanding
            if (fabsf(m_origVelocity.height()) >= fabsf(m_origVelocity.width()))
                m_origVelocity.setWidth(0);

            // Don't rubber-band horizontally if it's not possible to scroll horizontally
            if (!m_client.allowsHorizontalScrolling())
                m_origVelocity.setWidth(0);

            // Don't rubber-band vertically if it's not possible to scroll vertically
            if (!m_client.allowsVerticalScrolling())
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
            stopRubberbanding();
        }
    } else {
        m_startTime = currentTime;
        m_startStretch = { };
        if (!isRubberBandInProgressInternal())
            stopSnapRubberbandAnimation();
    }

    updateRubberBandingState();
}

void ScrollingEffectsController::scrollPositionChanged()
{
    updateRubberBandingState();
}

bool ScrollingEffectsController::isUserScrollInProgress() const
{
    return m_inScrollGesture;
}

bool ScrollingEffectsController::isRubberBandInProgress() const
{
    return m_isRubberBanding;
}

bool ScrollingEffectsController::isScrollSnapInProgress() const
{
    if (!usesScrollSnap())
        return false;

    if (m_inScrollGesture || m_momentumScrollInProgress || m_isAnimatingScrollSnap)
        return true;

    return false;
}

void ScrollingEffectsController::stopRubberbanding()
{
    stopSnapRubberbandAnimation();
    m_stretchScrollForce = { };
    m_startTime = { };
    m_startStretch = { };
    m_origVelocity = { };
    updateRubberBandingState();
}

void ScrollingEffectsController::startRubberbandAnimation()
{
    m_client.willStartRubberBandSnapAnimation();

    setIsAnimatingRubberBand(true);

    m_client.deferWheelEventTestCompletionForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::RubberbandInProgress);
}

void ScrollingEffectsController::stopSnapRubberbandAnimation()
{
    m_client.didStopRubberbandSnapAnimation();

    setIsAnimatingRubberBand(false);

    m_client.removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::RubberbandInProgress);
}

void ScrollingEffectsController::snapRubberBand()
{
    auto timeDelta = WallTime::now() - m_lastMomentumScrollTimestamp;
    if (m_lastMomentumScrollTimestamp && timeDelta >= scrollVelocityZeroingTimeout)
        m_momentumVelocity = { };

    if (m_isAnimatingRubberBand)
        return;

    m_startTime = MonotonicTime::now();
    m_startStretch = { };
    m_origVelocity = { };

    startRubberbandAnimation();
}

bool ScrollingEffectsController::shouldRubberBandInHorizontalDirection(const PlatformWheelEvent& wheelEvent) const
{
    auto direction = directionFromEvent(wheelEvent, ScrollEventAxis::Horizontal);
    if (direction)
        return shouldRubberBandInDirection(direction.value());

    return true;
}

bool ScrollingEffectsController::shouldRubberBandInDirection(ScrollDirection direction) const
{
    return m_client.shouldRubberBandInDirection(direction);
}

bool ScrollingEffectsController::isRubberBandInProgressInternal() const
{
    if (!m_inScrollGesture && !m_momentumScrollInProgress && !m_isAnimatingRubberBand)
        return false;

    return !m_client.stretchAmount().isZero();
}

void ScrollingEffectsController::updateRubberBandingState()
{
    bool isRubberBanding = isRubberBandInProgressInternal();
    if (isRubberBanding == m_isRubberBanding)
        return;

    m_isRubberBanding = isRubberBanding;
    if (m_isRubberBanding)
        updateRubberBandingEdges(m_client.stretchAmount());
    else
        m_rubberBandingEdges = { };

    m_client.rubberBandingStateChanged(m_isRubberBanding);
}

void ScrollingEffectsController::updateRubberBandingEdges(IntSize clientStretch)
{
    m_rubberBandingEdges.setLeft(clientStretch.width() < 0);
    m_rubberBandingEdges.setRight(clientStretch.width() > 0);

    m_rubberBandingEdges.setTop(clientStretch.height() < 0);
    m_rubberBandingEdges.setBottom(clientStretch.height() > 0);
}

enum class WheelEventStatus {
    UserScrollBegin,
    UserScrolling,
    UserScrollEnd,
    MomentumScrollBegin,
    MomentumScrolling,
    MomentumScrollEnd,
    StatelessScrollEvent,
    Unknown
};

static inline WheelEventStatus toWheelEventStatus(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    if (phase == PlatformWheelEventPhase::None) {
        switch (momentumPhase) {
        case PlatformWheelEventPhase::Began:
            return WheelEventStatus::MomentumScrollBegin;
                
        case PlatformWheelEventPhase::Changed:
            return WheelEventStatus::MomentumScrolling;
                
        case PlatformWheelEventPhase::Ended:
            return WheelEventStatus::MomentumScrollEnd;

        case PlatformWheelEventPhase::None:
            return WheelEventStatus::StatelessScrollEvent;

        default:
            return WheelEventStatus::Unknown;
        }
    }
    if (momentumPhase == PlatformWheelEventPhase::None) {
        switch (phase) {
        case PlatformWheelEventPhase::Began:
        case PlatformWheelEventPhase::MayBegin:
            return WheelEventStatus::UserScrollBegin;
                
        case PlatformWheelEventPhase::Changed:
            return WheelEventStatus::UserScrolling;
                
        case PlatformWheelEventPhase::Ended:
        case PlatformWheelEventPhase::Cancelled:
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

bool ScrollingEffectsController::shouldOverrideMomentumScrolling() const
{
    if (!usesScrollSnap())
        return false;

    ScrollSnapState scrollSnapState = m_scrollSnapState->currentState();
    return scrollSnapState == ScrollSnapState::Gliding || scrollSnapState == ScrollSnapState::DestinationReached;
}

void ScrollingEffectsController::scheduleStatelessScrollSnap()
{
    stopScrollSnapAnimation();
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

void ScrollingEffectsController::statelessSnapTransitionTimerFired()
{
    m_statelessSnapTransitionTimer = nullptr;

    if (!usesScrollSnap())
        return;

    if (m_scrollSnapState->transitionToSnapAnimationState(m_client.scrollExtents(), m_client.pageScaleFactor(), m_client.scrollOffset()))
        startScrollSnapAnimation();
}

void ScrollingEffectsController::startDeferringWheelEventTestCompletionDueToScrollSnapping()
{
    m_client.deferWheelEventTestCompletionForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::ScrollSnapInProgress);
}

void ScrollingEffectsController::stopDeferringWheelEventTestCompletionDueToScrollSnapping()
{
    m_client.removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::ScrollSnapInProgress);
}

bool ScrollingEffectsController::processWheelEventForScrollSnap(const PlatformWheelEvent& wheelEvent)
{
    if (!usesScrollSnap())
        return false;

    if (m_scrollSnapState->snapOffsetsForAxis(ScrollEventAxis::Horizontal).isEmpty() && m_scrollSnapState->snapOffsetsForAxis(ScrollEventAxis::Vertical).isEmpty())
        return false;

    auto status = toWheelEventStatus(wheelEvent.phase(), wheelEvent.momentumPhase());

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " processWheelEventForScrollSnap: status " << status);

    bool isMomentumScrolling = false;
    switch (status) {
    case WheelEventStatus::UserScrollBegin:
    case WheelEventStatus::UserScrolling:
        stopScrollSnapAnimation();
        m_scrollSnapState->transitionToUserInteractionState();
        m_dragEndedScrollingVelocity = -wheelEvent.scrollingVelocity();
        break;
    case WheelEventStatus::UserScrollEnd:
        if (m_scrollSnapState->transitionToSnapAnimationState(m_client.scrollExtents(), m_client.pageScaleFactor(), m_client.scrollOffset()))
            startScrollSnapAnimation();
        break;
    case WheelEventStatus::MomentumScrollBegin:
        if (m_scrollSnapState->transitionToGlideAnimationState(m_client.scrollExtents(), m_client.pageScaleFactor(), m_client.scrollOffset(), m_dragEndedScrollingVelocity, FloatSize(-wheelEvent.deltaX(), -wheelEvent.deltaY())))
            isMomentumScrolling = true;
        m_dragEndedScrollingVelocity = { };
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

void ScrollingEffectsController::updateGestureInProgressState(const PlatformWheelEvent& wheelEvent)
{
    if (wheelEvent.isGestureStart() || wheelEvent.isTransitioningToMomentumScroll())
        m_inScrollGesture = true;
    else if (wheelEvent.isEndOfNonMomentumScroll() || wheelEvent.isGestureCancel() || wheelEvent.isEndOfMomentumScroll())
        m_inScrollGesture = false;

    updateRubberBandingState();
}

void ScrollingEffectsController::startScrollSnapAnimation()
{
    if (m_isAnimatingScrollSnap)
        return;

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " startScrollSnapAnimation (main thread " << isMainThread() << ")");

    startDeferringWheelEventTestCompletionDueToScrollSnapping();
    m_client.willStartScrollSnapAnimation();
    setIsAnimatingScrollSnap(true);
}

void ScrollingEffectsController::stopScrollSnapAnimation()
{
    if (!m_isAnimatingScrollSnap)
        return;

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " stopScrollSnapAnimation (main thread " << isMainThread() << ")");

    stopDeferringWheelEventTestCompletionDueToScrollSnapping();
    m_client.didStopScrollSnapAnimation();

    setIsAnimatingScrollSnap(false);
}

void ScrollingEffectsController::updateScrollSnapAnimatingState(MonotonicTime currentTime)
{
    if (!m_isAnimatingScrollSnap)
        return;

    if (!usesScrollSnap()) {
        ASSERT_NOT_REACHED();
        return;
    }

    bool isAnimationComplete;
    auto animationOffset = m_scrollSnapState->currentAnimatedScrollOffset(currentTime, isAnimationComplete);
    auto currentOffset = m_client.scrollOffset();

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " updateScrollSnapAnimatingState - isAnimationComplete " << isAnimationComplete << " currentOffset " << currentOffset << " (main thread " << isMainThread() << ")");

    m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(animationOffset.x() - currentOffset.x(), animationOffset.y() - currentOffset.y()));
    if (isAnimationComplete) {
        m_scrollSnapState->transitionToDestinationReachedState();
        stopScrollSnapAnimation();
    }
}

} // namespace WebCore

#endif // PLATFORM(MAC)
