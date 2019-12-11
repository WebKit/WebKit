/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CSS_SCROLL_SNAP)

#include "AxisScrollSnapOffsets.h"
#include "PlatformWheelEvent.h"
#include "ScrollTypes.h"
#include <wtf/Optional.h>
#include <wtf/Seconds.h>

namespace WebCore {

class FloatPoint;
class FloatSize;

class ScrollingMomentumCalculator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScrollingMomentumCalculator(const FloatSize& viewportSize, const FloatSize& contentSize, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity);
    static std::unique_ptr<ScrollingMomentumCalculator> create(const FloatSize& viewportSize, const FloatSize& contentSize, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity);
    WEBCORE_EXPORT static void setPlatformMomentumScrollingPredictionEnabled(bool);
    virtual ~ScrollingMomentumCalculator() = default;

    virtual FloatPoint scrollOffsetAfterElapsedTime(Seconds) = 0;
    virtual Seconds animationDuration() = 0;
    virtual FloatSize predictedDestinationOffset();
    void setRetargetedScrollOffset(const FloatSize&);

protected:
    const FloatSize& retargetedScrollOffset() const { return m_retargetedScrollOffset.value(); }
    virtual void retargetedScrollOffsetDidChange() { }

    FloatSize m_initialDelta;
    FloatSize m_initialVelocity;
    FloatSize m_initialScrollOffset;
    FloatSize m_viewportSize;
    FloatSize m_contentSize;

private:
    Optional<FloatSize> m_retargetedScrollOffset;
};

class BasicScrollingMomentumCalculator final : public ScrollingMomentumCalculator {
public:
    BasicScrollingMomentumCalculator(const FloatSize& viewportSize, const FloatSize& contentSize, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity);

private:
    FloatPoint scrollOffsetAfterElapsedTime(Seconds) final;
    Seconds animationDuration() final;
    void initializeInterpolationCoefficientsIfNecessary();
    void initializeSnapProgressCurve();
    float animationProgressAfterElapsedTime(Seconds) const;
    FloatSize linearlyInterpolatedOffsetAtProgress(float progress);
    FloatSize cubicallyInterpolatedOffsetAtProgress(float progress) const;

    float m_snapAnimationCurveMagnitude { 0 };
    float m_snapAnimationDecayFactor { 0 };
    FloatSize m_snapAnimationCurveCoefficients[4] { };
    bool m_forceLinearAnimationCurve { false };
    bool m_momentumCalculatorRequiresInitialization { true };
    Optional<FloatSize> m_predictedDestinationOffset;
};

} // namespace WebCore

#endif // ENABLE(CSS_SCROLL_SNAP)
