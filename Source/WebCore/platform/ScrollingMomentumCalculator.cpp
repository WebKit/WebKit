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

#include "config.h"
#include "ScrollingMomentumCalculator.h"

#include "FloatPoint.h"
#include "FloatSize.h"

namespace WebCore {

static const Seconds scrollSnapAnimationDuration = 1_s;
static inline float projectedInertialScrollDistance(float initialWheelDelta)
{
    // On macOS 10.10 and earlier, we don't have a platform scrolling momentum calculator, so we instead approximate the scroll destination
    // by multiplying the initial wheel delta by a constant factor. By running a few experiments (i.e. logging scroll destination and initial
    // wheel delta for many scroll gestures) we determined that this is a reasonable way to approximate where scrolling will take us without
    // using _NSScrollingMomentumCalculator.
    static constexpr double inertialScrollPredictionFactor = 16.7;
    return inertialScrollPredictionFactor * initialWheelDelta;
}

ScrollingMomentumCalculator::ScrollingMomentumCalculator(const ScrollExtents& scrollExtents, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity)
    : m_initialDelta(initialDelta)
    , m_initialVelocity(initialVelocity)
    , m_initialScrollOffset(initialOffset)
    , m_scrollExtents(scrollExtents)
{
}

void ScrollingMomentumCalculator::setRetargetedScrollOffset(const FloatPoint& target)
{
    auto currentDestination = destinationScrollOffset();
    m_retargetedScrollOffset = target;
    
    if (currentDestination != destinationScrollOffset())
        destinationScrollOffsetDidChange();
}

FloatPoint ScrollingMomentumCalculator::predictedDestinationOffset()
{
    auto minScrollOffset = m_scrollExtents.minimumScrollOffset();
    auto maxScrollOffset = m_scrollExtents.maximumScrollOffset();

    float initialOffsetX = clampTo<float>(m_initialScrollOffset.x() + projectedInertialScrollDistance(m_initialDelta.width()), minScrollOffset.x(), maxScrollOffset.x());
    float initialOffsetY = clampTo<float>(m_initialScrollOffset.y() + projectedInertialScrollDistance(m_initialDelta.height()), minScrollOffset.y(), maxScrollOffset.y());
    return { initialOffsetX, initialOffsetY };
}

#if !PLATFORM(MAC)

std::unique_ptr<ScrollingMomentumCalculator> ScrollingMomentumCalculator::create(const ScrollExtents& scrollExtents, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity)
{
    return makeUnique<BasicScrollingMomentumCalculator>(scrollExtents, initialOffset, initialDelta, initialVelocity);
}

void ScrollingMomentumCalculator::setPlatformMomentumScrollingPredictionEnabled(bool)
{
}

#endif

BasicScrollingMomentumCalculator::BasicScrollingMomentumCalculator(const ScrollExtents& scrollExtents, const FloatPoint& initialOffset, const FloatSize& initialDelta, const FloatSize& initialVelocity)
    : ScrollingMomentumCalculator(scrollExtents, initialOffset, initialDelta, initialVelocity)
{
    m_initialDestinationOffset = predictedDestinationOffset();
}

FloatPoint BasicScrollingMomentumCalculator::linearlyInterpolatedOffsetAtProgress(float progress)
{
    return m_initialScrollOffset + progress * (destinationScrollOffset() - m_initialScrollOffset);
}

FloatPoint BasicScrollingMomentumCalculator::cubicallyInterpolatedOffsetAtProgress(float progress) const
{
    ASSERT(!m_forceLinearAnimationCurve);
    FloatPoint interpolatedPoint;
    for (int i = 0; i < 4; ++i)
        interpolatedPoint += std::pow(progress, i) * m_snapAnimationCurveCoefficients[i];

    return interpolatedPoint;
}

FloatPoint BasicScrollingMomentumCalculator::scrollOffsetAfterElapsedTime(Seconds elapsedTime)
{
    if (m_momentumCalculatorRequiresInitialization) {
        initializeSnapProgressCurve();
        initializeInterpolationCoefficientsIfNecessary();
        m_momentumCalculatorRequiresInitialization = false;
    }

    float progress = animationProgressAfterElapsedTime(elapsedTime);
    return m_forceLinearAnimationCurve ? linearlyInterpolatedOffsetAtProgress(progress) : cubicallyInterpolatedOffsetAtProgress(progress);
}

Seconds BasicScrollingMomentumCalculator::animationDuration()
{
    return scrollSnapAnimationDuration;
}

/**
 * Computes and sets coefficients required for interpolated snapping when scrolling in 2 dimensions, given
 * initial conditions (the initial and target vectors, along with the initial wheel delta as a vector). The
 * path is a cubic Bezier curve of the form p(s) = INITIAL + (C_1 * s) + (C_2 * s^2) + (C_3 * s^3) where each
 * C_i is a 2D vector and INITIAL is the vector representing the initial scroll offset. s is a real in the
 * interval [0, 1] indicating the "progress" of the curve (i.e. how much of the curve has been traveled).
 *
 * The curve has 4 control points, the first and last of which are the initial and target points, respectively.
 * The distances between adjacent control points are constrained to be the same, making the convex hull an
 * isosceles trapezoid with 3 sides of equal length. Additionally, the vector from the first control point to
 * the second points in the same direction as the initial scroll delta. These constraints ensure two properties:
 *     1. The direction of the snap animation at s=0 will be equal to the direction of the initial scroll delta.
 *     2. Points at regular intervals of s will be evenly spread out.
 *
 * If the initial scroll direction is orthogonal to or points in the opposite direction as the vector from the
 * initial point to the target point, initialization returns early and sets the curve to animate directly to the
 * snap point without cubic interpolation.
 *
 * FIXME: This should be refactored to use UnitBezier.
 */
void BasicScrollingMomentumCalculator::initializeInterpolationCoefficientsIfNecessary()
{
    m_forceLinearAnimationCurve = true;
    float initialDeltaMagnitude = m_initialDelta.diagonalLength();
    if (initialDeltaMagnitude < 1) {
        // The initial wheel delta is so insignificant that we're better off considering this to have the same effect as finishing a scroll gesture with no momentum.
        // Thus, cubic interpolation isn't needed here.
        return;
    }

    FloatSize startToEndVector = destinationScrollOffset() - m_initialScrollOffset;
    float startToEndDistance = startToEndVector.diagonalLength();
    if (!startToEndDistance) {
        // The start and end positions are the same, so we shouldn't try to interpolate a path.
        return;
    }

    float cosTheta = (m_initialDelta.width() * startToEndVector.width() + m_initialDelta.height() * startToEndVector.height()) / (initialDeltaMagnitude * startToEndDistance);
    if (cosTheta <= 0) {
        // It's possible that the user is not scrolling towards the target snap offset (for instance, scrolling against a corner when 2D scroll snapping).
        // In this case, just let the scroll offset animate to the target without computing a cubic curve.
        return;
    }

    float sideLength = startToEndDistance / (2.0f * cosTheta + 1.0f);
    auto initialOffsetAsSize = toFloatSize(m_initialScrollOffset);
    FloatSize controlVector1 = initialOffsetAsSize + sideLength * m_initialDelta / initialDeltaMagnitude;
    FloatSize controlVector2 = controlVector1 + (sideLength * startToEndVector / startToEndDistance);
    m_snapAnimationCurveCoefficients[0] = initialOffsetAsSize;
    m_snapAnimationCurveCoefficients[1] = 3 * (controlVector1 - initialOffsetAsSize);
    m_snapAnimationCurveCoefficients[2] = 3 * (initialOffsetAsSize - 2 * controlVector1 + controlVector2);
    m_snapAnimationCurveCoefficients[3] = 3 * (controlVector1 - controlVector2) - initialOffsetAsSize + toFloatSize(destinationScrollOffset());
    m_forceLinearAnimationCurve = false;
}

static const float framesPerSecond = 60.0f;

/**
 * Computes and sets parameters required for tracking the progress of a snap animation curve, interpolated
 * or linear. The progress curve s(t) maps time t to progress s; both variables are in the interval [0, 1].
 * The time input t is 0 when the current time is the start of the animation, t = 0, and 1 when the current
 * time is at or after the end of the animation, t = m_scrollSnapAnimationDuration.
 *
 * In this exponential progress model, s(t) = A - A * b^(-kt), where k = 60T is the number of frames in the
 * animation (assuming 60 FPS and an animation duration of T) and A, b are reals greater than or equal to 1.
 * Also note that we are given the initial progress, a value indicating the portion of the curve which our
 * initial scroll delta takes us. This is important when matching the initial speed of the animation to the
 * user's initial momentum scrolling speed. Let this initial progress amount equal v_0. I clamp this initial
 * progress amount to a minimum or maximum value.
 *
 * A is referred to as the curve magnitude, while b is referred to as the decay factor. We solve for A and b,
 * keeping the following constraints in mind:
 *     1. s(0) = 0
 *     2. s(1) = 1
 *     3. s(1/k) = v_0
 *
 * First, observe that s(0) = 0 holds for appropriate values of A, b. Solving for the remaining constraints
 * yields a nonlinear system of two equations. In lieu of a purely analytical solution, an alternating
 * optimization scheme is used to approximate A and b. This technique converges quickly (within 5 iterations
 * or so) for appropriate values of v_0. The optimization terminates early when the decay factor changes by
 * less than a threshold between one iteration and the next.
 */
void BasicScrollingMomentumCalculator::initializeSnapProgressCurve()
{
    static const int maxNumScrollSnapParameterEstimationIterations = 10;
    static const float scrollSnapDecayFactorConvergenceThreshold = 0.001;
    static const float initialScrollSnapCurveMagnitude = 1.1;
    static const float minScrollSnapInitialProgress = 0.1;
    static const float maxScrollSnapInitialProgress = 0.5;

    FloatSize alignmentVector = m_initialDelta * (destinationScrollOffset() - m_initialScrollOffset);
    float initialProgress;
    if (alignmentVector.width() + alignmentVector.height() > 0)
        initialProgress = clampTo(m_initialDelta.diagonalLength() / (destinationScrollOffset() - m_initialScrollOffset).diagonalLength(), minScrollSnapInitialProgress, maxScrollSnapInitialProgress);
    else
        initialProgress = minScrollSnapInitialProgress;

    float previousDecayFactor = 1.0f;
    m_snapAnimationCurveMagnitude = initialScrollSnapCurveMagnitude;
    for (int i = 0; i < maxNumScrollSnapParameterEstimationIterations; ++i) {
        m_snapAnimationDecayFactor = m_snapAnimationCurveMagnitude / (m_snapAnimationCurveMagnitude - initialProgress);
        m_snapAnimationCurveMagnitude = 1.0f / (1.0f - std::pow(m_snapAnimationDecayFactor, -framesPerSecond * scrollSnapAnimationDuration.value()));
        if (std::abs(m_snapAnimationDecayFactor - previousDecayFactor) < scrollSnapDecayFactorConvergenceThreshold)
            break;

        previousDecayFactor = m_snapAnimationDecayFactor;
    }
}

float BasicScrollingMomentumCalculator::animationProgressAfterElapsedTime(Seconds elapsedTime) const
{
    float timeProgress = clampTo<float>(elapsedTime / scrollSnapAnimationDuration, 0, 1);
    return std::min(1.0, m_snapAnimationCurveMagnitude * (1.0 - std::pow(m_snapAnimationDecayFactor, -framesPerSecond * scrollSnapAnimationDuration.value() * timeProgress)));
}

}; // namespace WebCore
