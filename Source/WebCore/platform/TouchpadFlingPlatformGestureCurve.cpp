/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TouchpadFlingPlatformGestureCurve.h"

#include "PlatformGestureCurveTarget.h"
#include <math.h>

namespace WebCore {

using namespace std;

// This curve implementation is based on the notion of a single, absolute curve, which starts at
// a large velocity and smoothly decreases to zero. For a given input velocity, we find where on
// the curve this velocity occurs, and start the animation at this point---denoted by (m_timeOffset,
// m_positionOffset).
//
// This has the effect of automatically determining an animation duration that scales with input
// velocity, as faster initial velocities start earlier on the curve and thus take longer to reach the end.
// No complicated time scaling is required.
//
// Since the starting velocity is implicitly determined by our starting point, we only store the
// relative magnitude and direction of both initial x- and y-velocities, and use this to scale the
// computed displacement at any point in time. This guarantees that fling trajectories are straight
// lines when viewed in x-y space. Initial velocities that lie outside the max velocity are constrained
// to start at zero (and thus are implicitly scaled).
//
// The curve is modelled as a 4th order polynomial, starting at t = 0, and ending at t = m_curveDuration.
// Attempts to generate position/velocity estimates outside this range are undefined.

const int TouchpadFlingPlatformGestureCurve::m_maxSearchIterations = 20;

PassOwnPtr<PlatformGestureCurve> TouchpadFlingPlatformGestureCurve::create(const FloatPoint& velocity, IntPoint cumulativeScroll)
{
    // The default parameters listed below are a matched set, and should not be changed independently of one another.
    return create(velocity, -5.70762e+03, 1.72e+02, 3.7e+00, 0, 0, 1.3, cumulativeScroll);
}

// FIXME: need to remove p3, p4 here and below as they are not used in the exponential curve, but leave in for now to facilitate
// the in-flight patch for https://bugs.webkit.org/show_bug.cgi?id=81663 .
PassOwnPtr<PlatformGestureCurve> TouchpadFlingPlatformGestureCurve::create(const FloatPoint& velocity, float p0, float p1, float p2, float p3, float p4, float curveDuration, IntPoint cumulativeScroll)
{
    return adoptPtr(new TouchpadFlingPlatformGestureCurve(velocity, p0, p1, p2, p3, p4, curveDuration, cumulativeScroll));
}

inline double position(double t, float* p)
{
    return p[0] * exp(-p[2] * t) - p[1] * t - p[0];
}

inline double velocity(double t, float* p)
{
    return -p[0] * p[2] * exp(-p[2] * t) - p[1];
}

TouchpadFlingPlatformGestureCurve::TouchpadFlingPlatformGestureCurve(const FloatPoint& initialVelocity, float p0, float p1, float p2, float p3, float p4, float curveDuration, const IntPoint& cumulativeScroll)
    : m_cumulativeScroll(cumulativeScroll)
    , m_curveDuration(curveDuration)
{
    ASSERT(initialVelocity != FloatPoint::zero());

    m_coeffs[0] = p0; // alpha
    m_coeffs[1] = p1; // beta
    m_coeffs[2] = p2; // gamma
    m_coeffs[3] = p3; // not used
    m_coeffs[4] = p4; // not used

    float maxInitialVelocity = max(fabs(initialVelocity.x()), fabs(initialVelocity.y()));

    // Force maxInitialVelocity to lie in the range v(0) to v(curveDuration), and assume that
    // the curve parameters define a monotonically decreasing velocity, or else bisection search may
    // fail.
    if (maxInitialVelocity > velocity(0, m_coeffs))
        maxInitialVelocity = velocity(0, m_coeffs);

    if (maxInitialVelocity < velocity(m_curveDuration, m_coeffs))
        maxInitialVelocity = velocity(m_curveDuration, m_coeffs);

    // We keep track of relative magnitudes and directions of the velocity/displacement components here.
    m_displacementRatio = FloatPoint(initialVelocity.x() / maxInitialVelocity, initialVelocity.y() / maxInitialVelocity);

    // Use basic bisection to estimate where we should start on the curve.
    // FIXME: Would Newton's method be better?
    const double epsilon = 1; // It is probably good enough to get the start point to within 1 pixel/sec.
    double t0 = 0;
    double t1 = curveDuration;
    int numIterations = 0;
    while (t0 < t1 && numIterations < m_maxSearchIterations) {
        numIterations++;
        m_timeOffset = (t0 + t1) * 0.5;
        double vOffset = velocity(m_timeOffset, m_coeffs);
        if (fabs(maxInitialVelocity - vOffset) < epsilon)
            break;

        if (vOffset > maxInitialVelocity)
            t0 = m_timeOffset;
        else
            t1 = m_timeOffset;
    }

    // Compute curve position at offset time
    m_positionOffset = position(m_timeOffset, m_coeffs);
}

TouchpadFlingPlatformGestureCurve::~TouchpadFlingPlatformGestureCurve()
{
}

bool TouchpadFlingPlatformGestureCurve::apply(double time, PlatformGestureCurveTarget* target)
{
    float displacement;
    if (time < 0)
        displacement = 0;
    else if (time + m_timeOffset < m_curveDuration)
        displacement = position(time + m_timeOffset, m_coeffs) - m_positionOffset;
    else
        displacement = position(m_curveDuration, m_coeffs) - m_positionOffset;

    // Keep track of integer portion of scroll thus far, and prepare increment.
    IntPoint scroll(displacement * m_displacementRatio.x(), displacement * m_displacementRatio.y());
    IntPoint scrollIncrement(scroll - m_cumulativeScroll);
    m_cumulativeScroll = scroll;

    if (time + m_timeOffset < m_curveDuration || scrollIncrement != IntPoint::zero()) {
        target->scrollBy(scrollIncrement);
        return true;
    }

    return false;
}

} // namespace WebCore
