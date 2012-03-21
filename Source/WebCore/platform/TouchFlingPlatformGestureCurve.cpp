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
#include "UnitBezier.h"
#include <math.h>

namespace WebCore {

using namespace std;

PassOwnPtr<PlatformGestureCurve> TouchFlingPlatformGestureCurve::create(const FloatPoint& velocity)
{
    return adoptPtr(new TouchFlingPlatformGestureCurve(velocity));
}

TouchFlingPlatformGestureCurve::TouchFlingPlatformGestureCurve(const FloatPoint& velocity)
    : m_velocity(velocity)
    , m_timeScaleFactor(3 / log10(max(10.f, max(fabs(velocity.x()), fabs(velocity.y())))))
{
    ASSERT(velocity != FloatPoint::zero());
}

TouchFlingPlatformGestureCurve::~TouchFlingPlatformGestureCurve()
{
}

bool TouchFlingPlatformGestureCurve::apply(double time, PlatformGestureCurveTarget* target)
{
    // Use 2-D Bezier curve with a "stretched-italic-s" curve.
    // We scale time logarithmically as this (subjectively) feels better.
    time *= m_timeScaleFactor;

    static UnitBezier flingBezier(0.3333, 0.6666, 0.6666, 1);

    float displacement;
    if (time < 0)
        displacement = 0;
    else if (time < 1) {
        // Below, s is the curve parameter for the 2-D Bezier curve (time(s), displacement(s)).
        double s = flingBezier.solveCurveX(time, 1.e-3);
        displacement = flingBezier.sampleCurveY(s);
    } else
        displacement = 1;

    // Keep track of integer portion of scroll thus far, and prepare increment.
    IntPoint scroll(displacement * m_velocity.x(), displacement * m_velocity.y());
    IntPoint scrollIncrement(scroll - m_cumulativeScroll);
    m_cumulativeScroll = scroll;

    if (time < 1 || scrollIncrement != IntPoint::zero()) {
        target->scrollBy(scrollIncrement);
        return true;
    }

    return false;
}

} // namespace WebCore
