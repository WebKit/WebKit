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
#include "ScrollAnimationKinetic.h"
#include "ScrollAnimationMomentum.h"
#include "ScrollAnimationSmooth.h"
#include "ScrollableArea.h"
#include "WheelEventTestMonitor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingEffectsController::ScrollingEffectsController(ScrollingEffectsControllerClient& client)
    : m_client(client)
{
}

void ScrollingEffectsController::animationCallback(MonotonicTime currentTime)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingEffectsController " << this << " animationCallback: isAnimatingRubberBand " << m_isAnimatingRubberBand << " isAnimatingScrollSnap " << m_isAnimatingScrollSnap << "isAnimatingKeyboardScrolling" << m_isAnimatingKeyboardScrolling);

    if (m_currentAnimation) {
        if (m_currentAnimation->isActive())
            m_currentAnimation->serviceAnimation(currentTime);

        if (m_currentAnimation && !m_currentAnimation->isActive())
            m_currentAnimation = nullptr;
    }

    updateRubberBandAnimatingState(currentTime);
    updateKeyboardScrollingAnimatingState(currentTime);
    
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

void ScrollingEffectsController::beginKeyboardScrolling()
{
    setIsAnimatingKeyboardScrolling(true);
}

void ScrollingEffectsController::stopKeyboardScrolling()
{
    setIsAnimatingKeyboardScrolling(false);
}

bool ScrollingEffectsController::startAnimatedScrollToDestination(FloatPoint startOffset, FloatPoint destinationOffset)
{
    if (m_currentAnimation)
        m_currentAnimation->stop();

    // We always create and attempt to start the animation. If it turns out to not need animating, then the animation
    // remains inactive, and we'll remove it on the next animationCallback().
    m_currentAnimation = makeUnique<ScrollAnimationSmooth>(*this);
    return downcast<ScrollAnimationSmooth>(*m_currentAnimation).startAnimatedScrollToDestination(startOffset, destinationOffset);
}

bool ScrollingEffectsController::regargetAnimatedScroll(FloatPoint newDestinationOffset)
{
    if (!is<ScrollAnimationSmooth>(m_currentAnimation.get()))
        return false;
    
    ASSERT(m_currentAnimation->isActive());
    return downcast<ScrollAnimationSmooth>(*m_currentAnimation).retargetActiveAnimation(newDestinationOffset);
}

void ScrollingEffectsController::stopAnimatedScroll()
{
    if (m_currentAnimation)
        m_currentAnimation->stop();
}

bool ScrollingEffectsController::processWheelEventForKineticScrolling(const PlatformWheelEvent& event)
{
#if ENABLE(KINETIC_SCROLLING)
    if (m_currentAnimation && !is<ScrollAnimationKinetic>(m_currentAnimation.get())) {
        m_currentAnimation->stop();
        m_currentAnimation = nullptr;
    }

    if (!m_currentAnimation)
        m_currentAnimation = makeUnique<ScrollAnimationKinetic>(*this);

    auto& kineticAnimation = downcast<ScrollAnimationKinetic>(*m_currentAnimation);
    kineticAnimation.appendToScrollHistory(event);

    if (event.isEndOfNonMomentumScroll()) {
        kineticAnimation.startAnimatedScrollWithInitialVelocity(m_client.scrollOffset(), kineticAnimation.computeVelocity(), m_client.allowsHorizontalScrolling(), m_client.allowsVerticalScrolling());
        return true;
    }
    if (event.isTransitioningToMomentumScroll()) {
        kineticAnimation.clearScrollHistory();
        kineticAnimation.startAnimatedScrollWithInitialVelocity(m_client.scrollOffset(), event.swipeVelocity(), m_client.allowsHorizontalScrolling(), m_client.allowsVerticalScrolling());
        return true;
    }
#else
    UNUSED_PARAM(event);
#endif
    return false;
}

bool ScrollingEffectsController::startMomentumScrollWithInitialVelocity(const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta, const Function<FloatPoint(const FloatPoint&)>& destinationModifier)
{
    if (m_currentAnimation) {
        if (is<ScrollAnimationMomentum>(m_currentAnimation.get()))
            m_currentAnimation->stop();
        else
            m_currentAnimation = nullptr;
    }

    if (!m_currentAnimation)
        m_currentAnimation = makeUnique<ScrollAnimationMomentum>(*this);

    return downcast<ScrollAnimationMomentum>(*m_currentAnimation).startAnimatedScrollWithInitialVelocity(initialOffset, initialVelocity, initialDelta, destinationModifier);
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

#if PLATFORM(MAC)
    startDeferringWheelEventTestCompletionDueToScrollSnapping();
#endif
    m_client.willStartScrollSnapAnimation();
    setIsAnimatingScrollSnap(true);
}

void ScrollingEffectsController::stopScrollSnapAnimation()
{
    if (!m_isAnimatingScrollSnap)
        return;

    LOG_WITH_STREAM(ScrollSnap, stream << "ScrollingEffectsController " << this << " stopScrollSnapAnimation (main thread " << isMainThread() << ")");

#if PLATFORM(MAC)
    stopDeferringWheelEventTestCompletionDueToScrollSnapping();
#endif
    m_client.didStopScrollSnapAnimation();

    setIsAnimatingScrollSnap(false);
}

void ScrollingEffectsController::updateKeyboardScrollingAnimatingState(MonotonicTime currentTime)
{
    if (!m_isAnimatingKeyboardScrolling)
        return;

    m_client.keyboardScrollingAnimator()->updateKeyboardScrollPosition(currentTime);
}

void ScrollingEffectsController::scrollToOffsetForAnimation(const FloatPoint& scrollOffset)
{
    auto currentOffset = m_client.scrollOffset();
    auto scrollDelta = scrollOffset - currentOffset;
    m_client.immediateScrollBy(scrollDelta);
}

void ScrollingEffectsController::scrollAnimationDidUpdate(ScrollAnimation&, const FloatPoint& currentOffset)
{
    scrollToOffsetForAnimation(currentOffset);
}

void ScrollingEffectsController::scrollAnimationWillStart(ScrollAnimation&)
{
    startOrStopAnimationCallbacks();
}

void ScrollingEffectsController::scrollAnimationDidEnd(ScrollAnimation&)
{
    if (usesScrollSnap() && m_isAnimatingScrollSnap) {
        m_scrollSnapState->transitionToDestinationReachedState();
        stopScrollSnapAnimation();
    }

    m_client.setScrollBehaviorStatus(ScrollBehaviorStatus::NotInAnimation);
    startOrStopAnimationCallbacks();
}

ScrollExtents ScrollingEffectsController::scrollExtentsForAnimation(ScrollAnimation&)
{
    return m_client.scrollExtents();
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

void ScrollingEffectsController::updateRubberBandAnimatingState(MonotonicTime)
{
}

#endif // PLATFORM(MAC)

} // namespace WebCore
