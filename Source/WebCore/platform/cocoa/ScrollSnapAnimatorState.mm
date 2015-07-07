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
#include <wtf/CurrentTime.h>

#if ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

ScrollSnapAnimatorState::ScrollSnapAnimatorState(ScrollEventAxis axis, const Vector<LayoutUnit>& snapOffsets)
    : m_snapOffsets(snapOffsets)
    , m_axis(axis)
    , m_currentState(ScrollSnapState::DestinationReached)
    , m_initialOffset(0)
    , m_targetOffset(0)
    , m_beginTrackingWheelDeltaOffset(0)
{
}

void ScrollSnapAnimatorState::pushInitialWheelDelta(float wheelDelta)
{
    if (m_numWheelDeltasTracked < wheelDeltaWindowSize)
        m_wheelDeltaWindow[m_numWheelDeltasTracked++] = wheelDelta;
}

float ScrollSnapAnimatorState::averageInitialWheelDelta() const
{
    if (!m_numWheelDeltasTracked)
        return 0;

    float sum = 0;
    int numZeroDeltas = 0;
    for (int i = 0; i < m_numWheelDeltasTracked; ++i) {
        sum += m_wheelDeltaWindow[i];
        if (!m_wheelDeltaWindow[i])
            numZeroDeltas++;
    }

    return m_numWheelDeltasTracked == numZeroDeltas ? 0 : sum / (m_numWheelDeltasTracked - numZeroDeltas);
}

void ScrollSnapAnimatorState::clearInitialWheelDeltaWindow()
{
    for (int i = 0; i < m_numWheelDeltasTracked; ++i)
        m_wheelDeltaWindow[i] = 0;

    m_numWheelDeltasTracked = 0;
}

bool ScrollSnapAnimatorState::isSnapping() const
{
    return m_currentState == ScrollSnapState::Gliding || m_currentState == ScrollSnapState::Snapping;
}
    
bool ScrollSnapAnimatorState::canReachTargetWithCurrentInitialScrollDelta() const
{
    if (m_initialOffset == m_targetOffset || !m_initialScrollDelta)
        return true;
    
    return m_initialOffset < m_targetOffset ? m_initialScrollDelta > 0 : m_initialScrollDelta < 0;
}
    
float ScrollSnapAnimatorState::interpolatedOffsetAtProgress(float progress) const
{
    progress = std::max(0.0f, std::min(1.0f, progress));
    return m_initialOffset + progress * (m_targetOffset - m_initialOffset);
}
    
static const int maxNumScrollSnapParameterEstimationIterations = 10;
static const float scrollSnapDecayFactorConvergenceThreshold = 0.001;
static const float initialScrollSnapCurveMagnitude = 1.1;
static const float minScrollSnapInitialProgress = 0.15;
static const float maxScrollSnapInitialProgress = 0.5;
static const double scrollSnapAnimationDuration = 0.5;

/**
 * Computes and sets parameters required for tracking the progress of a snap animation curve, interpolated
 * or linear. The progress curve s(t) maps time t to progress s; both variables are in the interval [0, 1].
 * The time input t is 0 when the current time is the start of the animation, t = m_startTime, and 1 when the
 * current time is at or after the end of the animation, t = m_startTime + m_scrollSnapAnimationDuration.
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
void ScrollSnapAnimationCurveState::initializeSnapProgressCurve(const FloatSize& initialVector, const FloatSize& targetVector, const FloatSize& initialDelta)
{
    float initialProgress = std::max(minScrollSnapInitialProgress, std::min(initialDelta.diagonalLength() / (targetVector - initialVector).diagonalLength(), maxScrollSnapInitialProgress));
    float previousDecayFactor = 1.0f;
    m_snapAnimationCurveMagnitude = initialScrollSnapCurveMagnitude;
    for (int i = 0; i < maxNumScrollSnapParameterEstimationIterations; ++i) {
        m_snapAnimationDecayFactor = m_snapAnimationCurveMagnitude / (m_snapAnimationCurveMagnitude - initialProgress);
        m_snapAnimationCurveMagnitude = 1.0f / (1.0f - std::pow(m_snapAnimationDecayFactor, -60.0f * scrollSnapAnimationDuration));
        if (std::abs(m_snapAnimationDecayFactor - previousDecayFactor) < scrollSnapDecayFactorConvergenceThreshold)
            break;
        
        previousDecayFactor = m_snapAnimationDecayFactor;
    }
    m_startTime = monotonicallyIncreasingTime();
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
 * snap point without interpolation.
 */
void ScrollSnapAnimationCurveState::initializeInterpolationCoefficientsIfNecessary(const FloatSize& initialVector, const FloatSize& targetVector, const FloatSize& initialDelta)
{
    FloatSize startToEndVector = targetVector - initialVector;
    float startToEndDistance = startToEndVector.diagonalLength();
    float initialDeltaMagnitude = initialDelta.diagonalLength();
    float cosTheta = initialDelta.isZero() ? 0 : (initialDelta.width() * startToEndVector.width() + initialDelta.height() * startToEndVector.height()) / (std::max(1.0f, initialDeltaMagnitude) * startToEndDistance);
    if (cosTheta <= 0)
        return;
    
    float sideLength = startToEndDistance / (2.0f * cosTheta + 1.0f);
    FloatSize controlVector1 = initialVector + sideLength * initialDelta / initialDeltaMagnitude;
    FloatSize controlVector2 = controlVector1 + (sideLength * startToEndVector / startToEndDistance);
    m_snapAnimationCurveCoefficients[0] = initialVector;
    m_snapAnimationCurveCoefficients[1] = 3 * (controlVector1 - initialVector);
    m_snapAnimationCurveCoefficients[2] = 3 * (initialVector - 2 * controlVector1 + controlVector2);
    m_snapAnimationCurveCoefficients[3] = 3 * (controlVector1 - controlVector2) - initialVector + targetVector;
    shouldAnimateDirectlyToSnapPoint = false;
}
    
FloatPoint ScrollSnapAnimationCurveState::interpolatedPositionAtProgress(float progress) const
{
    ASSERT(!shouldAnimateDirectlyToSnapPoint);
    progress = std::max(0.0f, std::min(1.0f, progress));
    FloatPoint interpolatedPoint(0.0f, 0.0f);
    for (int i = 0; i < 4; ++i)
        interpolatedPoint += std::pow(progress, i) * m_snapAnimationCurveCoefficients[i];
    
    return interpolatedPoint;
}
    
bool ScrollSnapAnimationCurveState::shouldCompleteSnapAnimationImmediatelyAtTime(double time) const
{
    return m_startTime + scrollSnapAnimationDuration < time;
}

float ScrollSnapAnimationCurveState::animationProgressAtTime(double time) const
{
    float timeProgress = std::max(0.0, std::min(1.0, (time - m_startTime) / scrollSnapAnimationDuration));
    return std::min(1.0, m_snapAnimationCurveMagnitude * (1.0 - std::pow(m_snapAnimationDecayFactor, -60.0f * scrollSnapAnimationDuration * timeProgress)));
}
    
} // namespace WebCore

#endif // CSS_SCROLL_SNAP
