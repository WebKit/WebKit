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

#include "TouchFlingPlatformGestureCurve.h"

#include "PlatformGestureCurveTarget.h"
#include <math.h>

namespace WebCore {

using namespace std;

PassOwnPtr<PlatformGestureCurve> TouchFlingPlatformGestureCurve::create(const FloatPoint& velocity)
{
    return adoptPtr(new TouchFlingPlatformGestureCurve(velocity));
}

TouchFlingPlatformGestureCurve::TouchFlingPlatformGestureCurve(const FloatPoint& velocity)
    : m_velocity(velocity)
    , m_timeScaleFactor(1000 / max(1.f, max(fabs(velocity.x()), fabs(velocity.y()))))
{
    ASSERT(velocity != FloatPoint::zero());
}

TouchFlingPlatformGestureCurve::~TouchFlingPlatformGestureCurve()
{
}

bool TouchFlingPlatformGestureCurve::apply(double time, PlatformGestureCurveTarget* target)
{
    // Here we implement a cubic bezier curve with parameters [1 1 0 0] which gives
    // v(0) = 1, a(0) = 0, v(1) = 0, a(1) = 0. The curve is scaled by the initial
    // velocity, and the time parameter is re-scaled so that larger initial velocities
    // lead to longer initial animations. This should allow the animation to smoothly
    // continue the velocity at the end of the GestureScroll, and smoothly come to a rest
    // at the end. Finally, we have integrated the curve so we can deal with displacement
    // as a function of time, and not velocity.
    // Finally, we place a constant velocity from ts = 0 .. 0.5, and run the Bezier curve
    // from ts = 0.5 .. 1.5 to give a longer run-out.
    time *= m_timeScaleFactor;

    float displacement;
    if (time <= 0.5)
        displacement = time;
    else if (time <= 1.5) {
        double t = time - 0.5;
        displacement = t * (1 + t * t * (0.5 * t - 1)) + 0.5;
    } else
        displacement = 1.0;

    // Keep track of integer portion of scroll thus far, and prepare increment.
    IntPoint scroll(displacement * m_velocity.x(), displacement * m_velocity.y());
    IntPoint scrollIncrement(scroll - m_cumulativeScroll);
    m_cumulativeScroll = scroll;

    if (time < 1.5 || scrollIncrement != IntPoint::zero()) {
        target->scrollBy(scrollIncrement);
        return true;
    }

    return false;
}

} // namespace WebCore
