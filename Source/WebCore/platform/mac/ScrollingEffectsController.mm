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

static ScrollEventAxis dominantAxisFavoringVertical(FloatSize delta)
{
    if (fabsf(delta.height()) >= fabsf(delta.width()))
        return ScrollEventAxis::Vertical;

    return ScrollEventAxis::Horizontal;
}

static FloatSize deltaAlignedToAxis(FloatSize delta, ScrollEventAxis axis)
{
    switch (axis) {
    case ScrollEventAxis::Horizontal: return FloatSize { delta.width(), 0 };
    case ScrollEventAxis::Vertical: return FloatSize { 0, delta.height() };
    }

    return { };
}

static FloatSize deltaAlignedToDominantAxis(FloatSize delta)
{
    auto dominantAxis = dominantAxisFavoringVertical(delta);
    return deltaAlignedToAxis(delta, dominantAxis);
}

static std::optional<BoxSide> affectedSideOnDominantAxis(FloatSize delta)
{
    auto dominantAxis = dominantAxisFavoringVertical(delta);
    return ScrollableArea::targetSideForScrollDelta(delta, dominantAxis);
}

static bool isHorizontalSide(std::optional<BoxSide> side)
{
    return side && (*side == BoxSide::Left || *side == BoxSide::Right);
}

static bool isVerticalSide(std::optional<BoxSide> side)
{
    return side && (*side == BoxSide::Top || *side == BoxSide::Bottom);
}

bool ScrollingEffectsController::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (processWheelEventForScrollSnap(wheelEvent))
        return true;

    if (wheelEvent.phase() == PlatformWheelEventPhase::MayBegin || wheelEvent.phase() == PlatformWheelEventPhase::Cancelled)
        return false;

    if (wheelEvent.phase() == PlatformWheelEventPhase::Began) {
        // FIXME: Trying to decide if a gesture is horizontal or vertical at the "began" phase is very error-prone.
        auto horizontalSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Horizontal);
        if (horizontalSide && m_client.isPinnedOnSide(*horizontalSide) && !shouldRubberBandOnSide(*horizontalSide))
            return false;

        auto verticalSide = ScrollableArea::targetSideForScrollDelta(-wheelEvent.delta(), ScrollEventAxis::Vertical);
        if (verticalSide && m_client.isPinnedOnSide(*verticalSide) && !shouldRubberBandOnSide(*verticalSide))
            return false;

        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = { };
        m_momentumVelocity = { };

        IntSize stretchAmount = m_client.stretchAmount();
        m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(stretchAmount.width()));
        m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(stretchAmount.height()));
        m_unappliedOverscrollDelta = { };

        stopRubberbandAnimation();
        updateRubberBandingState();
        return true;
    }

    if (wheelEvent.phase() == PlatformWheelEventPhase::Ended) {
        // FIXME: This triggers the rubberband timer even when we don't start rubberbanding.
        startRubberbandAnimationIfNecessary();
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

    // Reset unapplied overscroll because we may decide to remove delta at various points and put it into this value.
    auto delta = std::exchange(m_unappliedOverscrollDelta, { });

    IntSize stretchAmount = m_client.stretchAmount();
    bool isVerticallyStretched = stretchAmount.height();
    bool isHorizontallyStretched = stretchAmount.width();

    auto eventCoalescedDelta = (isVerticallyStretched || isHorizontallyStretched) ? -wheelEvent.unacceleratedScrollingDelta() : -wheelEvent.delta();
    delta += eventCoalescedDelta;

    // FIXME: All of the code below could be simplified since predominantAxis is known, and we zero out the delta on the other axis.
    auto affectedSide = affectedSideOnDominantAxis(delta);

    float deltaX = delta.width();
    float deltaY = delta.height();

    bool shouldStretch = false;

    auto momentumPhase = wheelEvent.momentumPhase();

    if (!m_momentumScrollInProgress && (momentumPhase == PlatformWheelEventPhase::Began || momentumPhase == PlatformWheelEventPhase::Changed))
        m_momentumScrollInProgress = true;

    auto timeDelta = wheelEvent.timestamp() - m_lastMomentumScrollTimestamp;
    if (m_inScrollGesture || m_momentumScrollInProgress) {
        if (m_lastMomentumScrollTimestamp && timeDelta > 0_s && timeDelta < scrollVelocityZeroingTimeout) {
            m_momentumVelocity = eventCoalescedDelta / timeDelta.seconds();
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
        } else {
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
            m_momentumVelocity = { };
        }

        if (isVerticallyStretched) {
            if (!isHorizontallyStretched && isHorizontalSide(affectedSide) && m_client.isPinnedOnSide(*affectedSide)) {
                // Stretching only in the vertical.
                if (deltaY && (fabsf(deltaX / deltaY) < rubberbandDirectionLockStretchRatio))
                    deltaX = 0;
                else if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                    m_unappliedOverscrollDelta.expand(deltaX, 0);
                    deltaX = 0;
                } else
                    m_unappliedOverscrollDelta.expand(deltaX, 0);
            }
        } else if (isHorizontallyStretched) {
            // Stretching only in the horizontal.
            if (isVerticalSide(affectedSide) && m_client.isPinnedOnSide(*affectedSide)) {
                if (deltaX && (fabsf(deltaY / deltaX) < rubberbandDirectionLockStretchRatio))
                    deltaY = 0;
                else if (fabsf(deltaY) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                    m_unappliedOverscrollDelta.expand(0, deltaY);
                    deltaY = 0;
                } else
                    m_unappliedOverscrollDelta.expand(0, deltaY);
            }
        } else {
            // Not stretching at all yet.
            if (affectedSide && m_client.isPinnedOnSide(*affectedSide)) {
                if (fabsf(deltaY) >= fabsf(deltaX)) {
                    if (fabsf(deltaX) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                        m_unappliedOverscrollDelta.expand(deltaX, 0);
                        deltaX = 0;
                    } else
                        m_unappliedOverscrollDelta.expand(deltaX, 0);
                }

                if (!m_client.allowsHorizontalStretching(wheelEvent))
                    deltaX = 0;

                if (!m_client.allowsVerticalStretching(wheelEvent))
                    deltaY = 0;

                shouldStretch = deltaX || deltaY;
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
            affectedSide = affectedSideOnDominantAxis({ deltaX, deltaY });
            if (deltaX) {
                if (!m_client.allowsHorizontalStretching(wheelEvent)) {
                    deltaX = 0;
                    eventCoalescedDelta.setWidth(0);
                    handled = false;
                } else if (!isHorizontallyStretched && !m_client.isPinnedOnSide(*affectedSide)) {
                    deltaX *= scrollWheelMultiplier();

                    m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(deltaX, 0));
                    deltaX = 0;
                }
            }

            if (deltaY) {
                if (!m_client.allowsVerticalStretching(wheelEvent)) {
                    deltaY = 0;
                    eventCoalescedDelta.setHeight(0);
                    handled = false;
                } else if (!isVerticallyStretched && !m_client.isPinnedOnSide(*affectedSide)) {
                    deltaY *= scrollWheelMultiplier();

                    m_client.immediateScrollByWithoutContentEdgeConstraints(FloatSize(0, deltaY));
                    deltaY = 0;
                }
            }

            IntSize stretchAmount = m_client.stretchAmount();

            if (m_momentumScrollInProgress) {
                auto sideAffectedByEventDelta = affectedSideOnDominantAxis(eventCoalescedDelta);
                if ((!sideAffectedByEventDelta || m_client.isPinnedOnSide(*sideAffectedByEventDelta)) && m_lastMomentumScrollTimestamp) {
                    m_ignoreMomentumScrolls = true;
                    m_momentumScrollInProgress = false;
                    startRubberbandAnimationIfNecessary();
                }
            }

            m_stretchScrollForce.setWidth(m_stretchScrollForce.width() + deltaX);
            m_stretchScrollForce.setHeight(m_stretchScrollForce.height() + deltaY);

            FloatSize dampedDelta(ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.width())), ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.height())));

            LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::handleWheelEvent() - stretchScrollForce " << m_stretchScrollForce << " move delta " << FloatSize(deltaX, deltaY) << " dampedDelta " << dampedDelta);

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
    return deltaAlignedToDominantAxis(wheelEvent.delta());
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
    
    if (!m_momentumScrollInProgress || m_ignoreMomentumScrolls) {
        auto timeDelta = currentTime - m_startTime;

        if (m_startStretch.isZero()) {
            m_startStretch = m_client.stretchAmount();
            if (m_startStretch.isZero()) {
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

            LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::updateRubberBandAnimatingState() - starting rubbberband with m_origVelocity" << m_origVelocity << " m_startStretch " << m_startStretch);
        }

        auto rubberBandDelta = FloatSize {
            roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.width(), -m_origVelocity.width(), timeDelta)),
            roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_startStretch.height(), -m_origVelocity.height(), timeDelta))
        };

        if (fabs(rubberBandDelta.width()) >= 1 || fabs(rubberBandDelta.height()) >= 1) {
            auto stretchDelta = rubberBandDelta - FloatSize(m_client.stretchAmount());

            LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::updateRubberBandAnimatingState() - rubberBandDelta " << rubberBandDelta << " stretched " << m_client.stretchAmount() << " moving by " << stretchDelta);

            m_client.immediateScrollByWithoutContentEdgeConstraints(stretchDelta);

            FloatSize newStretch = m_client.stretchAmount();

            m_stretchScrollForce.setWidth(reboundDeltaForElasticDelta(newStretch.width()));
            m_stretchScrollForce.setHeight(reboundDeltaForElasticDelta(newStretch.height()));
        } else {
            LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::updateRubberBandAnimatingState() - rubber band complete");
            m_client.adjustScrollPositionToBoundsIfNecessary();
            stopRubberbanding();
        }
    } else {
        LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::updateRubberBandAnimatingState() - not animating, momentumScrollInProgress " << m_momentumScrollInProgress << " ignoreMomentumScrolls " << m_ignoreMomentumScrolls);
        m_startTime = currentTime;
        m_startStretch = { };
        if (!isRubberBandInProgressInternal())
            stopRubberbandAnimation();
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
    stopRubberbandAnimation();
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

void ScrollingEffectsController::stopRubberbandAnimation()
{
    m_client.didStopRubberbandSnapAnimation();

    setIsAnimatingRubberBand(false);

    m_client.removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::RubberbandInProgress);
}

void ScrollingEffectsController::startRubberbandAnimationIfNecessary()
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

bool ScrollingEffectsController::shouldRubberBandOnSide(BoxSide side) const
{
    return m_client.shouldRubberBandOnSide(side);
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

    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " updateRubberBandingState - isRubberBanding " << isRubberBanding);

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
        if (m_scrollSnapState->transitionToGlideAnimationState(m_client.scrollExtents(), m_client.pageScaleFactor(), m_client.scrollOffset(), m_dragEndedScrollingVelocity, FloatSize(-wheelEvent.deltaX(), -wheelEvent.deltaY()))) {
            startScrollSnapAnimation();
            isMomentumScrolling = true;
        }
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

} // namespace WebCore

#endif // PLATFORM(MAC)
