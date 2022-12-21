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
#import "ScrollAnimationRubberBand.h"
#import "ScrollExtents.h"
#import "ScrollableArea.h"
#import <pal/spi/mac/NSScrollViewSPI.h>
#import <sys/sysctl.h>
#import <sys/time.h>
#import <wtf/SystemTracing.h>
#import <wtf/text/TextStream.h>

#if PLATFORM(MAC)

namespace WebCore {

static const Seconds scrollVelocityZeroingTimeout = 100_ms;
static const float rubberbandDirectionLockStretchRatio = 1;
static const float rubberbandMinimumRequiredDeltaBeforeStretch = 10;

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

bool ScrollingEffectsController::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (processWheelEventForScrollSnap(wheelEvent))
        return true;

    if (wheelEvent.phase() == PlatformWheelEventPhase::MayBegin || wheelEvent.phase() == PlatformWheelEventPhase::Cancelled)
        return false;

    if (wheelEvent.isEndOfNonMomentumScroll() || wheelEvent.isEndOfMomentumScroll())
        m_client.didStopWheelEventScroll();
    else if (wheelEvent.isGestureStart() || wheelEvent.isTransitioningToMomentumScroll())
        m_client.willStartWheelEventScroll();

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

        stopRubberBandAnimation();
        updateRubberBandingState();
    }

    if (wheelEvent.phase() == PlatformWheelEventPhase::Ended) {
        // FIXME: This triggers the rubberband timer even when we don't start rubberbanding.
        startRubberBandAnimationIfNecessary();
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

    IntSize stretchAmount = m_client.stretchAmount();
    bool isVerticallyStretched = stretchAmount.height();
    bool isHorizontallyStretched = stretchAmount.width();

    // Much of this code, including this use of unaccelerated deltas when stretched, is based on AppKit behavior.
    auto eventDelta = (isVerticallyStretched || isHorizontallyStretched) ? -wheelEvent.unacceleratedScrollingDelta() : -wheelEvent.delta();

    // Reset unapplied overscroll because we may decide to remove delta at various points and put it into this value.
    auto delta = std::exchange(m_unappliedOverscrollDelta, { });
    delta += eventDelta;

    if (wheelEvent.isGestureEvent()) {
        // FIXME: This replicates what WheelEventDeltaFilter does. We should apply that to events in all phases, and remove axis locking here (webkit.org/b/231207).
        delta = deltaAlignedToDominantAxis(delta);
    }

    auto momentumPhase = wheelEvent.momentumPhase();
    
    if (momentumPhase == PlatformWheelEventPhase::Began)
        WTFBeginAnimationSignpostAlways(nullptr, "Momentum scroll", "");
    
    if (!m_momentumScrollInProgress && (momentumPhase == PlatformWheelEventPhase::Began || momentumPhase == PlatformWheelEventPhase::Changed))
        m_momentumScrollInProgress = true;

    bool shouldStretch = false;
    auto timeDelta = wheelEvent.timestamp() - m_lastMomentumScrollTimestamp;
    if (m_inScrollGesture || m_momentumScrollInProgress) {
        if (m_lastMomentumScrollTimestamp && timeDelta > 0_s && timeDelta < scrollVelocityZeroingTimeout) {
            m_momentumVelocity = eventDelta / timeDelta.seconds();
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
        } else {
            m_lastMomentumScrollTimestamp = wheelEvent.timestamp();
            m_momentumVelocity = { };
        }

        shouldStretch = modifyScrollDeltaForStretching(wheelEvent, delta, isHorizontallyStretched, isVerticallyStretched);
    }

    bool handled = true;

    if (!delta.isZero()) {
        if (shouldStretch || isVerticallyStretched || isHorizontallyStretched) {
            if (delta.width() && !m_client.allowsHorizontalStretching(wheelEvent))
                handled = false;

            if (delta.height() && !m_client.allowsVerticalStretching(wheelEvent))
                handled = false;

            bool canStartAnimation = applyScrollDeltaWithStretching(wheelEvent, delta, isHorizontallyStretched, isVerticallyStretched);
            if (m_momentumScrollInProgress) {
                if (canStartAnimation && m_lastMomentumScrollTimestamp) {
                    m_ignoreMomentumScrolls = true;
                    m_momentumScrollInProgress = false;
                    startRubberBandAnimationIfNecessary();
                }
            }
        } else {
            delta.scale(scrollWheelMultiplier());
            m_client.immediateScrollBy(delta);
        }
    }

    if (m_momentumScrollInProgress && momentumPhase == PlatformWheelEventPhase::Ended) {
        WTFEndSignpostAlways(nullptr, "Momentum scroll");
        m_momentumScrollInProgress = false;
        m_ignoreMomentumScrolls = false;
        m_lastMomentumScrollTimestamp = { };
    }

    updateRubberBandingState();

    return handled;
}

bool ScrollingEffectsController::modifyScrollDeltaForStretching(const PlatformWheelEvent& wheelEvent, FloatSize& delta, bool isHorizontallyStretched, bool isVerticallyStretched)
{
    auto affectedSide = affectedSideOnDominantAxis(delta);
    if (isVerticallyStretched) {
        if (!isHorizontallyStretched && affectedSide && m_client.isPinnedOnSide(*affectedSide)) {
            // Stretching only in the vertical.
            if (delta.height() && (fabsf(delta.width() / delta.height()) < rubberbandDirectionLockStretchRatio))
                delta.setWidth(0);
            else if (fabsf(delta.width()) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                m_unappliedOverscrollDelta.expand(delta.width(), 0);
                delta.setWidth(0);
            } else
                m_unappliedOverscrollDelta.expand(delta.width(), 0);
        }

        return false;
    }

    if (isHorizontallyStretched) {
        // Stretching only in the horizontal.
        if (affectedSide && m_client.isPinnedOnSide(*affectedSide)) {
            if (delta.width() && (fabsf(delta.height() / delta.width()) < rubberbandDirectionLockStretchRatio))
                delta.setHeight(0);
            else if (fabsf(delta.height()) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                m_unappliedOverscrollDelta.expand(0, delta.height());
                delta.setHeight(0);
            } else
                m_unappliedOverscrollDelta.expand(0, delta.height());
        }

        return false;
    }

    // Not stretching at all yet.
    if (affectedSide && m_client.isPinnedOnSide(*affectedSide)) {
        if (fabsf(delta.height()) >= fabsf(delta.width())) {
            if (fabsf(delta.width()) < rubberbandMinimumRequiredDeltaBeforeStretch) {
                m_unappliedOverscrollDelta.expand(delta.width(), 0);
                delta.setWidth(0);
            } else
                m_unappliedOverscrollDelta.expand(delta.width(), 0);
        }

        if (!m_client.allowsHorizontalStretching(wheelEvent))
            delta.setWidth(0);

        if (!m_client.allowsVerticalStretching(wheelEvent))
            delta.setHeight(0);

        return !delta.isZero();
    }
    
    return false;
}

bool ScrollingEffectsController::applyScrollDeltaWithStretching(const PlatformWheelEvent& wheelEvent, FloatSize delta, bool isHorizontallyStretched, bool isVerticallyStretched)
{
    auto eventDelta = (isVerticallyStretched || isHorizontallyStretched) ? -wheelEvent.unacceleratedScrollingDelta() : -wheelEvent.delta();
    auto affectedSide = affectedSideOnDominantAxis(delta);

    FloatSize deltaToScroll;

    if (delta.width()) {
        if (!m_client.allowsHorizontalStretching(wheelEvent)) {
            delta.setWidth(0);
            eventDelta.setWidth(0);
        } else if (!isHorizontallyStretched && !m_client.isPinnedOnSide(*affectedSide)) {
            delta.scale(scrollWheelMultiplier(), 1);
            deltaToScroll += FloatSize { delta.width(), 0 };
            delta.setWidth(0);
        }
    }

    if (delta.height()) {
        if (!m_client.allowsVerticalStretching(wheelEvent)) {
            delta.setHeight(0);
            eventDelta.setHeight(0);
        } else if (!isVerticallyStretched && !m_client.isPinnedOnSide(*affectedSide)) {
            delta.scale(1, scrollWheelMultiplier());
            deltaToScroll += FloatSize { 0, delta.height() };
            delta.setHeight(0);
        }
    }

    if (!deltaToScroll.isZero())
        m_client.immediateScrollBy(deltaToScroll, ScrollClamping::Unclamped);

    bool canStartAnimation = false;
    if (m_momentumScrollInProgress) {
        // Compute canStartAnimation, which looks at isPinnedOnSide(), before applying the stretch delta.
        auto sideAffectedByEventDelta = affectedSideOnDominantAxis(eventDelta);
        canStartAnimation = !sideAffectedByEventDelta || m_client.isPinnedOnSide(*sideAffectedByEventDelta);
    }

    m_stretchScrollForce += delta;
    auto dampedDelta = FloatSize {
        ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.width())),
        ceilf(elasticDeltaForReboundDelta(m_stretchScrollForce.height()))
    };

    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::applyScrollDeltaWithStretching() - stretchScrollForce " << m_stretchScrollForce << " move delta " << delta << " dampedDelta " << dampedDelta);

    auto stretchAmount = m_client.stretchAmount();
    m_client.immediateScrollBy(dampedDelta - stretchAmount, ScrollClamping::Unclamped);

    return canStartAnimation;
}

FloatSize ScrollingEffectsController::wheelDeltaBiasingTowardsVertical(const FloatSize& wheelDelta)
{
    return deltaAlignedToDominantAxis(wheelDelta);
}

void ScrollingEffectsController::updateRubberBandAnimatingState()
{
    if (!m_isAnimatingRubberBand)
        return;

    if (isScrollSnapInProgress())
        return;
    
    if (m_momentumScrollInProgress && !m_ignoreMomentumScrolls) {
        LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::updateRubberBandAnimatingState() - not animating, momentumScrollInProgress " << m_momentumScrollInProgress << " ignoreMomentumScrolls " << m_ignoreMomentumScrolls);
        if (!isRubberBandInProgressInternal())
            stopRubberBandAnimation();
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

void ScrollingEffectsController::stopRubberBanding()
{
    stopRubberBandAnimation();
    updateRubberBandingState();
}

bool ScrollingEffectsController::startRubberBandAnimation(const FloatSize& initialVelocity, const FloatSize& initialOverscroll)
{
    if (m_currentAnimation)
        m_currentAnimation->stop();

    m_currentAnimation = makeUnique<ScrollAnimationRubberBand>(*this);
    bool started = downcast<ScrollAnimationRubberBand>(*m_currentAnimation).startRubberBandAnimation(initialVelocity, initialOverscroll);
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::startRubberBandAnimation() - animation " << *m_currentAnimation << " started " << started);
    return started;
}

void ScrollingEffectsController::stopRubberBandAnimation()
{
    if (m_isAnimatingRubberBand)
        stopAnimatedScroll();

    m_stretchScrollForce = { };
}

void ScrollingEffectsController::willStartRubberBandAnimation()
{
    m_isAnimatingRubberBand = true;
    m_client.willStartRubberBandAnimation();
    m_client.deferWheelEventTestCompletionForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::RubberbandInProgress);
}

void ScrollingEffectsController::didStopRubberBandAnimation()
{
    m_isAnimatingRubberBand = false;
    m_client.didStopRubberBandAnimation();
    m_client.removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), WheelEventTestMonitor::RubberbandInProgress);
}

void ScrollingEffectsController::startRubberBandAnimationIfNecessary()
{
    auto timeDelta = WallTime::now() - m_lastMomentumScrollTimestamp;
    if (m_lastMomentumScrollTimestamp && timeDelta >= scrollVelocityZeroingTimeout)
        m_momentumVelocity = { };

    if (m_isAnimatingRubberBand)
        return;

    auto extents = m_client.scrollExtents();
    auto targetOffset = m_client.scrollOffset();
    auto contrainedOffset = targetOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
    bool willOverscroll = targetOffset != contrainedOffset;

    auto stretchAmount = m_client.stretchAmount();
    if (stretchAmount.isZero() && !willOverscroll && m_stretchScrollForce.isZero())
        return;

    auto initialVelocity = m_momentumVelocity;

    // Just like normal scrolling, prefer vertical rubberbanding
    if (fabsf(initialVelocity.height()) >= fabsf(initialVelocity.width()))
        initialVelocity.setWidth(0);

    // Don't rubber-band horizontally if it's not possible to scroll horizontally
    if (!m_client.allowsHorizontalScrolling())
        initialVelocity.setWidth(0);

    // Don't rubber-band vertically if it's not possible to scroll vertically
    if (!m_client.allowsVerticalScrolling())
        initialVelocity.setHeight(0);

    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::startRubberBandAnimationIfNecessary() - rubberBandAnimationRunning " << m_isAnimatingRubberBand << " stretchAmount " << stretchAmount << " initialVelocity " << initialVelocity);
    startRubberBandAnimation(initialVelocity, stretchAmount);
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
    startDeferringWheelEventTestCompletion(WheelEventTestMonitor::ScrollSnapInProgress);
}

void ScrollingEffectsController::statelessSnapTransitionTimerFired()
{
    m_statelessSnapTransitionTimer = nullptr;

    if (!usesScrollSnap())
        return;

    if (m_scrollSnapState->transitionToSnapAnimationState(m_client.scrollExtents(), m_client.pageScaleFactor(), m_client.scrollOffset()))
        startScrollSnapAnimation();
    else
        stopDeferringWheelEventTestCompletion(WheelEventTestMonitor::ScrollSnapInProgress);
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
        m_scrollingVelocityForScrollSnap = -wheelEvent.scrollingVelocity();
        break;
    case WheelEventStatus::UserScrollEnd:
        if (m_scrollSnapState->transitionToSnapAnimationState(m_client.scrollExtents(), m_client.pageScaleFactor(), m_client.scrollOffset()))
            startScrollSnapAnimation();
        break;
    case WheelEventStatus::MomentumScrollBegin:
        if (m_scrollSnapState->transitionToGlideAnimationState(m_client.scrollExtents(), m_client.pageScaleFactor(), m_client.scrollOffset(), m_scrollingVelocityForScrollSnap, -wheelEvent.delta())) {
            startScrollSnapAnimation();
            isMomentumScrolling = true;
        }
        m_scrollingVelocityForScrollSnap = { };
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
