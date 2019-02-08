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

#ifndef ScrollSnapAnimatorState_h
#define ScrollSnapAnimatorState_h

#if ENABLE(CSS_SCROLL_SNAP)

#include "AxisScrollSnapOffsets.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "LayoutPoint.h"
#include "PlatformWheelEvent.h"
#include "ScrollSnapOffsetsInfo.h"
#include "ScrollTypes.h"
#include "ScrollingMomentumCalculator.h"
#include <wtf/MonotonicTime.h>

namespace WebCore {

enum class ScrollSnapState {
    Snapping,
    Gliding,
    DestinationReached,
    UserInteraction
};

class ScrollSnapAnimatorState {
    WTF_MAKE_FAST_ALLOCATED;
public:
    const Vector<LayoutUnit>& snapOffsetsForAxis(ScrollEventAxis axis) const
    {
        return axis == ScrollEventAxis::Horizontal ? m_snapOffsetsX : m_snapOffsetsY;
    }

    const Vector<ScrollOffsetRange<LayoutUnit>>& snapOffsetRangesForAxis(ScrollEventAxis axis) const
    {
        return axis == ScrollEventAxis::Horizontal ? m_snapOffsetRangesX : m_snapOffsetRangesY;
    }

    void setSnapOffsetsAndPositionRangesForAxis(ScrollEventAxis axis, const Vector<LayoutUnit>& snapOffsets, const Vector<ScrollOffsetRange<LayoutUnit>>& snapOffsetRanges)
    {
        if (axis == ScrollEventAxis::Horizontal) {
            m_snapOffsetsX = snapOffsets;
            m_snapOffsetRangesX = snapOffsetRanges;
        } else {
            m_snapOffsetsY = snapOffsets;
            m_snapOffsetRangesY = snapOffsetRanges;
        }
    }

    ScrollSnapState currentState() const { return m_currentState; }

    unsigned activeSnapIndexForAxis(ScrollEventAxis axis) const
    {
        return axis == ScrollEventAxis::Horizontal ? m_activeSnapIndexX : m_activeSnapIndexY;
    }

    void setActiveSnapIndexForAxis(ScrollEventAxis axis, unsigned index)
    {
        if (axis == ScrollEventAxis::Horizontal)
            m_activeSnapIndexX = index;
        else
            m_activeSnapIndexY = index;
    }

    FloatPoint currentAnimatedScrollOffset(bool& isAnimationComplete) const;

    // State transition helpers.
    void transitionToSnapAnimationState(const FloatSize& contentSize, const FloatSize& viewportSize, float pageScale, const FloatPoint& initialOffset);
    void transitionToGlideAnimationState(const FloatSize& contentSize, const FloatSize& viewportSize, float pageScale, const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta);
    void transitionToUserInteractionState();
    void transitionToDestinationReachedState();

private:
    float targetOffsetForStartOffset(const Vector<LayoutUnit>& snapOffsets, const Vector<ScrollOffsetRange<LayoutUnit>>& snapOffsetRanges, float maxScrollOffset, float startOffset, float predictedOffset, float pageScale, float delta, unsigned& outActiveSnapIndex) const;
    void teardownAnimationForState(ScrollSnapState);
    void setupAnimationForState(ScrollSnapState, const FloatSize& contentSize, const FloatSize& viewportSize, float pageScale, const FloatPoint& initialOffset, const FloatSize& initialVelocity, const FloatSize& initialDelta);

    ScrollSnapState m_currentState { ScrollSnapState::UserInteraction };

    Vector<LayoutUnit> m_snapOffsetsX;
    Vector<ScrollOffsetRange<LayoutUnit>> m_snapOffsetRangesX;
    unsigned m_activeSnapIndexX { 0 };
    Vector<LayoutUnit> m_snapOffsetsY;
    Vector<ScrollOffsetRange<LayoutUnit>> m_snapOffsetRangesY;
    unsigned m_activeSnapIndexY { 0 };

    MonotonicTime m_startTime;
    std::unique_ptr<ScrollingMomentumCalculator> m_momentumCalculator;
};

} // namespace WebCore

#endif // ENABLE(CSS_SCROLL_SNAP)

#endif // ScrollSnapAnimatorState_h
