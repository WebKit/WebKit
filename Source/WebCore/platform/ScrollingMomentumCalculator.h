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

#include "PlatformWheelEvent.h"
#include "ScrollExtents.h"
#include "ScrollTypes.h"
#include <wtf/Seconds.h>

namespace WebCore {

class FloatPoint;
class FloatSize;

class ScrollingMomentumCalculator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static void setPlatformMomentumScrollingPredictionEnabled(bool);

    static std::unique_ptr<ScrollingMomentumCalculator> create(const ScrollExtents&, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity);

    ScrollingMomentumCalculator(const ScrollExtents&, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity);
    virtual ~ScrollingMomentumCalculator() = default;

    virtual FloatPoint scrollOffsetAfterElapsedTime(Seconds) = 0;
    virtual Seconds animationDuration() = 0;

    FloatPoint destinationScrollOffset() const { return m_retargetedScrollOffset.value_or(m_initialDestinationOffset); }

    void setRetargetedScrollOffset(const FloatPoint&);

protected:
    virtual FloatPoint predictedDestinationOffset();

    virtual void destinationScrollOffsetDidChange() { }

    FloatSize m_initialDelta;
    FloatSize m_initialVelocity;
    FloatPoint m_initialScrollOffset;
    FloatPoint m_initialDestinationOffset;
    ScrollExtents m_scrollExtents;

private:
    std::optional<FloatPoint> m_retargetedScrollOffset;
};

class BasicScrollingMomentumCalculator final : public ScrollingMomentumCalculator {
public:
    BasicScrollingMomentumCalculator(const ScrollExtents&, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity);

private:
    FloatPoint scrollOffsetAfterElapsedTime(Seconds) final;
    Seconds animationDuration() final;

    void initializeInterpolationCoefficientsIfNecessary();
    void initializeSnapProgressCurve();
    float animationProgressAfterElapsedTime(Seconds) const;

    FloatPoint linearlyInterpolatedOffsetAtProgress(float progress);
    FloatPoint cubicallyInterpolatedOffsetAtProgress(float progress) const;

    float m_snapAnimationCurveMagnitude { 0 };
    float m_snapAnimationDecayFactor { 0 };
    FloatSize m_snapAnimationCurveCoefficients[4] { };
    bool m_forceLinearAnimationCurve { false };
    bool m_momentumCalculatorRequiresInitialization { true };
};

} // namespace WebCore
