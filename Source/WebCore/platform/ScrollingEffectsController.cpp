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

#include "config.h"
#include "ScrollingEffectsController.h"

#include "KeyboardScrollingAnimator.h"
#include "LayoutSize.h"
#include "Logging.h"
#include "PlatformWheelEvent.h"
#include "ScrollAnimationKeyboard.h"
#include "ScrollAnimationMomentum.h"
#include "ScrollAnimationSmooth.h"
#include "ScrollExtents.h"
#include "ScrollableArea.h"
#include <wtf/text/TextStream.h>

#if ENABLE(KINETIC_SCROLLING) && !PLATFORM(MAC)
#include "ScrollAnimationKinetic.h"
#endif

#if HAVE(RUBBER_BANDING)
#include "ScrollAnimationRubberBand.h"
#endif

namespace WebCore {

ScrollingEffectsController::ScrollingEffectsController(ScrollingEffectsControllerClient& client)
    : m_client(client)
{
}

void ScrollingEffectsController::animationCallback(MonotonicTime currentTime)
{
    if (m_currentAnimation) {
        if (m_currentAnimation->isActive())
            m_currentAnimation->serviceAnimation(currentTime);

        if (m_currentAnimation && !m_currentAnimation->isActive())
            m_currentAnimation = nullptr;
    }

    updateRubberBandAnimatingState();
    
    startOrStopAnimationCallbacks();
}

void ScrollingEffectsController::startOrStopAnimationCallbacks()
{
    bool needsCallbacks = m_isAnimatingRubberBand || m_isAnimatingKeyboardScrolling || m_currentAnimation;
    if (needsCallbacks == m_isRunningAnimatingCallback)
        return;

    if (needsCallbacks) {
        m_client.startAnimationCallback(*this);
        m_isRunningAnimatingCallback = true;
        return;
    }

    m_client.stopAnimationCallback(*this);
    m_isRunningAnimatingCallback = false;
}

void ScrollingEffectsController::willBeginKeyboardScrolling()
{
    setIsAnimatingKeyboardScrolling(true);
}

void ScrollingEffectsController::didStopKeyboardScrolling()
{
    setIsAnimatingKeyboardScrolling(false);
}

bool ScrollingEffectsController::startKeyboardScroll(const KeyboardScroll& scrollData)
{
    if (m_currentAnimation)
        m_currentAnimation->stop();

    m_currentAnimation = makeUnique<ScrollAnimationKeyboard>(*this);
    bool started = downcast<ScrollAnimationKeyboard>(*m_currentAnimation).startKeyboardScroll(scrollData);
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " startAnimatedScrollToDestination " << *m_currentAnimation << " started " << started);
    return started;
}

void ScrollingEffectsController::finishKeyboardScroll(bool immediate)
{
    if (is<ScrollAnimationKeyboard>(m_currentAnimation))
        downcast<ScrollAnimationKeyboard>(*m_currentAnimation).finishKeyboardScroll(immediate);
}

bool ScrollingEffectsController::startAnimatedScrollToDestination(FloatPoint startOffset, FloatPoint destinationOffset)
{
    if (m_currentAnimation)
        m_currentAnimation->stop();

    // We always create and attempt to start the animation. If it turns out to not need animating, then the animation
    // remains inactive, and we'll remove it on the next animationCallback().
    m_currentAnimation = makeUnique<ScrollAnimationSmooth>(*this);
    bool started = downcast<ScrollAnimationSmooth>(*m_currentAnimation).startAnimatedScrollToDestination(startOffset, destinationOffset);
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " startAnimatedScrollToDestination " << *m_currentAnimation << " started " << started);
    return started;
}

bool ScrollingEffectsController::retargetAnimatedScroll(FloatPoint newDestinationOffset)
{
    if (!is<ScrollAnimationSmooth>(m_currentAnimation.get()))
        return false;
    
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " retargetAnimatedScroll to " << newDestinationOffset);

    ASSERT(m_currentAnimation->isActive());
    return downcast<ScrollAnimationSmooth>(*m_currentAnimation).retargetActiveAnimation(newDestinationOffset);
}

bool ScrollingEffectsController::retargetAnimatedScrollBy(FloatSize offset)
{
    if (!is<ScrollAnimationSmooth>(m_currentAnimation.get()) || !m_currentAnimation->isActive())
        return false;

    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " retargetAnimatedScrollBy " << offset);

    if (auto destinationOffset = m_currentAnimation->destinationOffset())
        return m_currentAnimation->retargetActiveAnimation(*destinationOffset + offset);

    return false;
}

void ScrollingEffectsController::stopAnimatedNonRubberbandingScroll()
{
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " stopAnimatedNonRubberbandingScroll");

    if (!m_currentAnimation)
        return;

#if HAVE(RUBBER_BANDING)
    if (is<ScrollAnimationRubberBand>(m_currentAnimation))
        return;
#endif

    if (is<ScrollAnimationMomentum>(m_currentAnimation)) {
        // If the animation is currently triggering rubberbanding, let it run. Ideally we'd check if the animation will cause rubberbanding at any time in the future.
        auto currentOffset = m_currentAnimation->currentOffset();
        auto extents = m_client.scrollExtents();
        auto constrainedOffset = currentOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
        if (currentOffset != constrainedOffset)
            return;
    }

    m_currentAnimation->stop();
}

void ScrollingEffectsController::stopAnimatedScroll()
{
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " stopAnimatedScroll");

    if (m_currentAnimation)
        m_currentAnimation->stop();
}

bool ScrollingEffectsController::startMomentumScrollWithInitialVelocity(const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta, const Function<FloatPoint(const FloatPoint&)>& destinationModifier)
{
    if (m_currentAnimation) {
        m_currentAnimation->stop();
        if (!is<ScrollAnimationMomentum>(m_currentAnimation.get()))
            m_currentAnimation = nullptr;
    }

    if (!m_currentAnimation)
        m_currentAnimation = makeUnique<ScrollAnimationMomentum>(*this);

    bool started = downcast<ScrollAnimationMomentum>(*m_currentAnimation).startAnimatedScrollWithInitialVelocity(initialOffset, initialVelocity, initialDelta, destinationModifier);
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController::startMomentumScrollWithInitialVelocity() - animation " << *m_currentAnimation << " initialVelocity " << initialVelocity << " initialDelta " << initialDelta << " started " << started);
    return started;
}

void ScrollingEffectsController::setIsAnimatingRubberBand(bool isAnimatingRubberBand)
{
    if (isAnimatingRubberBand == m_isAnimatingRubberBand)
        return;

    m_isAnimatingRubberBand = isAnimatingRubberBand;
    startOrStopAnimationCallbacks();
}

void ScrollingEffectsController::setIsAnimatingScrollSnap(bool isAnimatingScrollSnap)
{
    if (isAnimatingScrollSnap == m_isAnimatingScrollSnap)
        return;

    m_isAnimatingScrollSnap = isAnimatingScrollSnap;
    startOrStopAnimationCallbacks();
}

void ScrollingEffectsController::setIsAnimatingKeyboardScrolling(bool isAnimatingKeyboardScrolling)
{
    if (isAnimatingKeyboardScrolling == m_isAnimatingKeyboardScrolling)
        return;

    m_isAnimatingKeyboardScrolling = isAnimatingKeyboardScrolling;
    startOrStopAnimationCallbacks();
}

void ScrollingEffectsController::stopKeyboardScrolling()
{
    m_client.keyboardScrollingAnimator()->handleKeyUpEvent();
}

void ScrollingEffectsController::contentsSizeChanged()
{
    if (m_currentAnimation)
        m_currentAnimation->updateScrollExtents();
}

bool ScrollingEffectsController::usesScrollSnap() const
{
    return !!m_scrollSnapState;
}

void ScrollingEffectsController::setSnapOffsetsInfo(const LayoutScrollSnapOffsetsInfo& snapOffsetInfo)
{
    if (snapOffsetInfo.isEmpty()) {
        m_scrollSnapState = nullptr;
        return;
    }

    bool shouldComputeCurrentSnapIndices = !m_scrollSnapState;
    if (!m_scrollSnapState)
        m_scrollSnapState = makeUnique<ScrollSnapAnimatorState>(*this);

    m_scrollSnapState->setSnapOffsetInfo(snapOffsetInfo);

    if (shouldComputeCurrentSnapIndices)
        updateActiveScrollSnapIndexForClientOffset();

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " setSnapOffsetsInfo new state: " << ValueOrNull(m_scrollSnapState.get()));
}

const LayoutScrollSnapOffsetsInfo* ScrollingEffectsController::snapOffsetsInfo() const
{
    return m_scrollSnapState ? &m_scrollSnapState->snapOffsetInfo() : nullptr;
}

std::optional<unsigned> ScrollingEffectsController::activeScrollSnapIndexForAxis(ScrollEventAxis axis) const
{
    if (!usesScrollSnap())
        return std::nullopt;

    return m_scrollSnapState->activeSnapIndexForAxis(axis);
}

void ScrollingEffectsController::setActiveScrollSnapIndexForAxis(ScrollEventAxis axis, std::optional<unsigned> index)
{
    if (!usesScrollSnap())
        return;

    m_scrollSnapState->setActiveSnapIndexForAxis(axis, index);
}

float ScrollingEffectsController::adjustedScrollDestination(ScrollEventAxis axis, FloatPoint destinationOffset, float velocity, std::optional<float> originalOffset) const
{
    if (!usesScrollSnap())
        return axis == ScrollEventAxis::Horizontal ? destinationOffset.x() : destinationOffset.y();

    return m_scrollSnapState->adjustedScrollDestination(axis, destinationOffset, velocity, originalOffset, m_client.scrollExtents(), m_client.pageScaleFactor());
}

#if !PLATFORM(MAC)
#if ENABLE(KINETIC_SCROLLING)
bool ScrollingEffectsController::processWheelEventForKineticScrolling(const PlatformWheelEvent& event)
{
    if (is<ScrollAnimationKinetic>(m_currentAnimation.get())) {
        auto& kineticAnimation = downcast<ScrollAnimationKinetic>(*m_currentAnimation);

        m_previousKineticAnimationInfo.startTime = kineticAnimation.startTime();
        m_previousKineticAnimationInfo.initialOffset = kineticAnimation.initialOffset();
        m_previousKineticAnimationInfo.initialVelocity = kineticAnimation.initialVelocity();

        m_currentAnimation->stop();
    }

    if (!event.hasPreciseScrollingDeltas()) {
        m_scrollHistory.clear();
        m_previousKineticAnimationInfo.initialVelocity = FloatSize();
        return false;
    }

    m_scrollHistory.append(event);

    if (!event.isEndOfNonMomentumScroll() && !event.isTransitioningToMomentumScroll())
        return false;

    if (m_currentAnimation && !is<ScrollAnimationKinetic>(m_currentAnimation.get())) {
        m_currentAnimation->stop();
        m_currentAnimation = nullptr;
        m_previousKineticAnimationInfo.initialVelocity = FloatSize();
    }

    if (usesScrollSnap())
        return false;

    if (!m_currentAnimation)
        m_currentAnimation = makeUnique<ScrollAnimationKinetic>(*this);

    auto& kineticAnimation = downcast<ScrollAnimationKinetic>(*m_currentAnimation);
    while (!m_scrollHistory.isEmpty())
        kineticAnimation.appendToScrollHistory(m_scrollHistory.takeFirst());

    FloatSize previousVelocity;
    if (!m_previousKineticAnimationInfo.initialVelocity.isZero()) {
        previousVelocity = kineticAnimation.accumulateVelocityFromPreviousGesture(m_previousKineticAnimationInfo.startTime,
            m_previousKineticAnimationInfo.initialOffset, m_previousKineticAnimationInfo.initialVelocity);
        m_previousKineticAnimationInfo.initialVelocity = FloatSize();
    }

    if (event.isEndOfNonMomentumScroll()) {
        kineticAnimation.startAnimatedScrollWithInitialVelocity(m_client.scrollOffset(), kineticAnimation.computeVelocity(), previousVelocity, m_client.allowsHorizontalScrolling(), m_client.allowsVerticalScrolling());
        return true;
    }
    if (event.isTransitioningToMomentumScroll()) {
        kineticAnimation.clearScrollHistory();
        kineticAnimation.startAnimatedScrollWithInitialVelocity(m_client.scrollOffset(), event.swipeVelocity(), previousVelocity, m_client.allowsHorizontalScrolling(), m_client.allowsVerticalScrolling());
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}
#endif

void ScrollingEffectsController::adjustDeltaForSnappingIfNeeded(float& deltaX, float& deltaY)
{
    if (snapOffsetsInfo() && !snapOffsetsInfo()->isEmpty()) {
        float scale = m_client.pageScaleFactor();
        auto scrollOffset = m_client.scrollOffset();
        auto extents = m_client.scrollExtents();

        auto originalOffset = LayoutPoint(scrollOffset.x() / scale, scrollOffset.y() / scale);
        auto newOffset = LayoutPoint((scrollOffset.x() + deltaX) / scale, (scrollOffset.y() + deltaY) / scale);

        auto offsetX = snapOffsetsInfo()->closestSnapOffset(ScrollEventAxis::Horizontal, LayoutSize(extents.contentsSize), newOffset, deltaX, originalOffset.x()).first;
        auto offsetY = snapOffsetsInfo()->closestSnapOffset(ScrollEventAxis::Vertical, LayoutSize(extents.contentsSize), newOffset, deltaY, originalOffset.y()).first;

        deltaX = (offsetX - originalOffset.x()) * scale;
        deltaY = (offsetY - originalOffset.y()) * scale;
    }
}

void ScrollingEffectsController::updateGestureInProgressState(const PlatformWheelEvent& wheelEvent)
{
#if ENABLE(KINETIC_SCROLLING)
    m_inScrollGesture = wheelEvent.hasPreciseScrollingDeltas() && !wheelEvent.isEndOfNonMomentumScroll() && !wheelEvent.isTransitioningToMomentumScroll();
#else
    UNUSED_PARAM(wheelEvent);
#endif
}

bool ScrollingEffectsController::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
#if ENABLE(KINETIC_SCROLLING)
    if (processWheelEventForKineticScrolling(wheelEvent))
        return true;
#endif

    auto scrollOffset = m_client.scrollOffset();
    float deltaX = m_client.allowsHorizontalScrolling() ? wheelEvent.deltaX() : 0;
    float deltaY = m_client.allowsVerticalScrolling() ? wheelEvent.deltaY() : 0;
    auto extents = m_client.scrollExtents();
    auto minPosition = extents.minimumScrollOffset();
    auto maxPosition = extents.maximumScrollOffset();

    if ((deltaX < 0 && scrollOffset.x() >= maxPosition.x())
        || (deltaX > 0 && scrollOffset.x() <= minPosition.x()))
        deltaX = 0;
    if ((deltaY < 0 && scrollOffset.y() >= maxPosition.y())
        || (deltaY > 0 && scrollOffset.y() <= minPosition.y()))
        deltaY = 0;

    if (wheelEvent.granularity() == ScrollByPageWheelEvent) {
        if (deltaX) {
            bool negative = deltaX < 0;
            deltaX = Scrollbar::pageStepDelta(extents.contentsSize.width());
            if (negative)
                deltaX = -deltaX;
        }
        if (deltaY) {
            bool negative = deltaY < 0;
            deltaY = Scrollbar::pageStepDelta(extents.contentsSize.height());
            if (negative)
                deltaY = -deltaY;
        }
    }

    deltaX = -deltaX;
    deltaY = -deltaY;

    if (!m_inScrollGesture)
        adjustDeltaForSnappingIfNeeded(deltaX, deltaY);

    if (!deltaX && !deltaY)
        return false;

#if ENABLE(SMOOTH_SCROLLING)
    if (m_client.scrollAnimationEnabled() && !m_inScrollGesture) {
        if (!retargetAnimatedScrollBy({ deltaX, deltaY }))
            startAnimatedScrollToDestination(scrollOffset, scrollOffset + FloatSize { deltaX, deltaY });
        return true;
    }
#endif

    m_client.immediateScrollBy({ deltaX, deltaY });

    return true;
}

bool ScrollingEffectsController::isUserScrollInProgress() const
{
    return m_inScrollGesture;
}

bool ScrollingEffectsController::isScrollSnapInProgress() const
{
    if (!usesScrollSnap())
        return false;

    if (m_inScrollGesture || (m_currentAnimation && m_currentAnimation->isActive()))
        return true;

    return false;
}
#endif

void ScrollingEffectsController::updateActiveScrollSnapIndexForClientOffset()
{
    if (!usesScrollSnap())
        return;

    ScrollOffset offset = roundedIntPoint(m_client.scrollOffset());
    if (m_scrollSnapState->setNearestScrollSnapIndexForOffset(offset, m_client.scrollExtents(), m_client.pageScaleFactor()))
        m_activeScrollSnapIndexDidChange = true;
}

void ScrollingEffectsController::resnapAfterLayout()
{
    if (!usesScrollSnap())
        return;

    // If we are already snapped in a particular axis, maintain that. Otherwise, snap to the nearest eligible snap point.
    ScrollOffset offset = roundedIntPoint(m_client.scrollOffset());
    if (m_scrollSnapState->resnapAfterLayout(offset, m_client.scrollExtents(), m_client.pageScaleFactor()))
        m_activeScrollSnapIndexDidChange = true;
}

void ScrollingEffectsController::startScrollSnapAnimation()
{
    if (m_isAnimatingScrollSnap)
        return;

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " startScrollSnapAnimation (main thread " << isMainThread() << ")");

    startDeferringWheelEventTestCompletion(WheelEventTestMonitor::ScrollSnapInProgress);
    m_client.willStartScrollSnapAnimation();
    setIsAnimatingScrollSnap(true);
}

void ScrollingEffectsController::stopScrollSnapAnimation()
{
    if (!m_isAnimatingScrollSnap)
        return;

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " stopScrollSnapAnimation (main thread " << isMainThread() << ")");

    stopDeferringWheelEventTestCompletion(WheelEventTestMonitor::ScrollSnapInProgress);
    m_client.didStopScrollSnapAnimation();

    setIsAnimatingScrollSnap(false);
}

void ScrollingEffectsController::scrollAnimationDidUpdate(ScrollAnimation& animation, const FloatPoint& scrollOffset)
{
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " scrollAnimationDidUpdate " << animation << " (main thread " << isMainThread() << ") scrolling to " << scrollOffset);

    auto currentOffset = m_client.scrollOffset();
    auto scrollDelta = scrollOffset - currentOffset;

    m_client.immediateScrollBy(scrollDelta, animation.clamping());
}

void ScrollingEffectsController::scrollAnimationWillStart(ScrollAnimation& animation)
{
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " scrollAnimationWillStart " << animation);

#if HAVE(RUBBER_BANDING)
    if (is<ScrollAnimationRubberBand>(animation))
        willStartRubberBandAnimation();
#else
    UNUSED_PARAM(animation);
#endif

    if (is<ScrollAnimationKeyboard>(animation)) {
        willBeginKeyboardScrolling();
        startDeferringWheelEventTestCompletion(WheelEventTestMonitor::ScrollAnimationInProgress);
        return;
    }

    startDeferringWheelEventTestCompletion(WheelEventTestMonitor::ScrollAnimationInProgress);
    startOrStopAnimationCallbacks();
}

void ScrollingEffectsController::scrollAnimationDidEnd(ScrollAnimation& animation)
{
    LOG_WITH_STREAM(ScrollAnimations, stream << "ScrollingEffectsController " << this << " scrollAnimationDidEnd " << animation);

    if (usesScrollSnap() && m_isAnimatingScrollSnap) {
        m_scrollSnapState->transitionToDestinationReachedState();
        stopScrollSnapAnimation();
    }

#if HAVE(RUBBER_BANDING)
    if (is<ScrollAnimationRubberBand>(animation))
        didStopRubberBandAnimation();
#else
    UNUSED_PARAM(animation);
#endif

    // FIXME: Need to track state better and only call this when the running animation is for CSS smooth scrolling. Calling should be harmless, though.
    m_client.didStopAnimatedScroll();
    startOrStopAnimationCallbacks();
    stopDeferringWheelEventTestCompletion(WheelEventTestMonitor::ScrollAnimationInProgress);
}

ScrollExtents ScrollingEffectsController::scrollExtentsForAnimation(ScrollAnimation&)
{
    return m_client.scrollExtents();
}

FloatSize ScrollingEffectsController::overscrollAmount(ScrollAnimation&)
{
#if HAVE(RUBBER_BANDING)
    return m_client.stretchAmount();
#else
    return { };
#endif
}

FloatPoint ScrollingEffectsController::scrollOffset(ScrollAnimation&)
{
    return m_client.scrollOffset();
}

void ScrollingEffectsController::startDeferringWheelEventTestCompletion(WheelEventTestMonitor::DeferReason reason)
{
    m_client.deferWheelEventTestCompletionForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), reason);
}

void ScrollingEffectsController::stopDeferringWheelEventTestCompletion(WheelEventTestMonitor::DeferReason reason)
{
    m_client.removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(this), reason);
}

// Currently, only Mac supports momentum srolling-based scrollsnapping and rubber banding
// so all of these methods are a noop on non-Mac platforms.
#if !PLATFORM(MAC)
ScrollingEffectsController::~ScrollingEffectsController()
{
}

void ScrollingEffectsController::stopAllTimers()
{
}

void ScrollingEffectsController::scrollPositionChanged()
{
}

void ScrollingEffectsController::updateRubberBandAnimatingState()
{
}

#endif // PLATFORM(MAC)

} // namespace WebCore
