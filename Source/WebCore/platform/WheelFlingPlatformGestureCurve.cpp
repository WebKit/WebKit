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

#include "WheelFlingPlatformGestureCurve.h"

#include "PlatformGestureCurveTarget.h"
#include <math.h>

namespace WebCore {

PassOwnPtr<PlatformGestureCurve> WheelFlingPlatformGestureCurve::create(const FloatPoint& velocity)
{
    return adoptPtr(new WheelFlingPlatformGestureCurve(velocity));
}

WheelFlingPlatformGestureCurve::WheelFlingPlatformGestureCurve(const FloatPoint& velocity)
    : m_velocity(velocity)
{
    ASSERT(velocity != FloatPoint::zero());
}

WheelFlingPlatformGestureCurve::~WheelFlingPlatformGestureCurve()
{
}

bool WheelFlingPlatformGestureCurve::apply(double time, PlatformGestureCurveTarget* target)
{
    // Use a Rayleigh distribution for the curve. This simulates a velocity profile
    // that starts at 0, increases to a maximum, then decreases again smoothly. By
    // using the cumulative distribution function (CDF) instead of the point-mass function,
    // we can isolate timing jitter by remembering the CDF value at the last tick. Since
    // the CDF maxes out at 1, scale it by the input velocity.
    //
    // CDF -> F(x; sigma) = 1 - exp{-x^2/2\sigma^2}
    // ref: http://en.wikipedia.org/wiki/Rayleigh_distribution
    // FIXME: consider making the value of sigma settable in the constructor.
    static double twoSigmaSquaredInverse = 16; // sigma = 0.25
    float cdf = 1 - exp(-time * time * twoSigmaSquaredInverse);
    IntPoint scroll(cdf * m_velocity.x(), cdf * m_velocity.y());
    IntPoint scrollIncrement(scroll - m_cumulativeScroll);
    m_cumulativeScroll = scroll;

    if (cdf < 0.5 || scrollIncrement != IntPoint::zero()) {
        target->scrollBy(scrollIncrement);
        return true;
    }

    return false;
}

} // namespace WebCore
