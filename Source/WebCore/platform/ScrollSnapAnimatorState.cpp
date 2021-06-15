/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "ScrollSnapAnimatorState.h"

#if ENABLE(CSS_SCROLL_SNAP)

#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

void ScrollSnapAnimatorState::transitionToSnapAnimationState(const FloatSize& contentSize, const FloatSize& viewportSize, float pageScale, const FloatPoint& initialOffset)
{
    setupAnimationForState(ScrollSnapState::Snapping, contentSize, viewportSize, pageScale, initialOffset, { }, { });
}

void ScrollSnapAnimatorState::transitionToGlideAnimationState(const FloatSize& contentSize, const FloatSize& viewportSize, float pageScale, const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta)
{
    setupAnimationForState(ScrollSnapState::Gliding, contentSize, viewportSize, pageScale, initialOffset, initialVelocity, initialDelta);
}

void ScrollSnapAnimatorState::setupAnimationForState(ScrollSnapState state, const FloatSize& contentSize, const FloatSize& viewportSize, float pageScale, const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta)
{
    ASSERT(state == ScrollSnapState::Snapping || state == ScrollSnapState::Gliding);
    if (m_currentState == state)
        return;

    m_momentumCalculator = ScrollingMomentumCalculator::create(viewportSize, contentSize, initialOffset, initialDelta, initialVelocity);
    auto predictedScrollTarget = m_momentumCalculator->predictedDestinationOffset();
    float targetOffsetX = targetOffsetForStartOffset(ScrollEventAxis::Horizontal, viewportSize, contentSize.width() - viewportSize.width(), initialOffset.x(), predictedScrollTarget.width(), pageScale, initialDelta.width(), m_activeSnapIndexX);
    float targetOffsetY = targetOffsetForStartOffset(ScrollEventAxis::Vertical, viewportSize, contentSize.height() - viewportSize.height(), initialOffset.y(), predictedScrollTarget.height(), pageScale, initialDelta.height(), m_activeSnapIndexY);
    m_momentumCalculator->setRetargetedScrollOffset({ targetOffsetX, targetOffsetY });
    m_startTime = MonotonicTime::now();
    m_currentState = state;
}

void ScrollSnapAnimatorState::transitionToUserInteractionState()
{
    teardownAnimationForState(ScrollSnapState::UserInteraction);
}

void ScrollSnapAnimatorState::transitionToDestinationReachedState()
{
    teardownAnimationForState(ScrollSnapState::DestinationReached);
}

void ScrollSnapAnimatorState::teardownAnimationForState(ScrollSnapState state)
{
    ASSERT(state == ScrollSnapState::UserInteraction || state == ScrollSnapState::DestinationReached);
    if (m_currentState == state)
        return;

    m_momentumCalculator = nullptr;
    m_startTime = MonotonicTime();
    m_currentState = state;
}

FloatPoint ScrollSnapAnimatorState::currentAnimatedScrollOffset(MonotonicTime currentTime, bool& isAnimationComplete) const
{
    if (!m_momentumCalculator) {
        isAnimationComplete = true;
        return { };
    }

    Seconds elapsedTime = currentTime - m_startTime;
    isAnimationComplete = elapsedTime >= m_momentumCalculator->animationDuration();
    return m_momentumCalculator->scrollOffsetAfterElapsedTime(elapsedTime);
}

float ScrollSnapAnimatorState::targetOffsetForStartOffset(ScrollEventAxis axis, const FloatSize& viewportSize, float maxScrollOffset, float startOffset, float predictedOffset, float pageScale, float initialDelta, unsigned& outActiveSnapIndex) const
{
    const auto& snapOffsets = m_snapOffsetsInfo.offsetsForAxis(axis);
    if (snapOffsets.isEmpty()) {
        outActiveSnapIndex = invalidSnapOffsetIndex;
        return clampTo<float>(predictedOffset, 0, maxScrollOffset);
    }

    float targetOffset = m_snapOffsetsInfo.closestSnapOffset(axis, LayoutSize { viewportSize }, LayoutUnit(predictedOffset / pageScale), initialDelta, LayoutUnit(startOffset / pageScale)).first;
    return pageScale * clampTo<float>(targetOffset, 0, maxScrollOffset);
}

TextStream& operator<<(TextStream& ts, const ScrollSnapAnimatorState& state)
{
    ts << "ScrollSnapAnimatorState";
    ts.dumpProperty("snap offsets x", state.snapOffsetsForAxis(ScrollEventAxis::Horizontal));
    ts.dumpProperty("snap offsets y", state.snapOffsetsForAxis(ScrollEventAxis::Vertical));

    ts.dumpProperty("active snap index x", state.activeSnapIndexForAxis(ScrollEventAxis::Horizontal));
    ts.dumpProperty("active snap index y", state.activeSnapIndexForAxis(ScrollEventAxis::Vertical));

    return ts;
}

} // namespace WebCore

#endif // CSS_SCROLL_SNAP
