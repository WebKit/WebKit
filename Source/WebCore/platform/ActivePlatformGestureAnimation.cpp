/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ActivePlatformGestureAnimation.h"

#include "PlatformGestureCurve.h"
#include "PlatformGestureCurveTarget.h"

namespace WebCore {

PassOwnPtr<ActivePlatformGestureAnimation> ActivePlatformGestureAnimation::create(double startTime, PassOwnPtr<PlatformGestureCurve> curve, PlatformGestureCurveTarget* target)
{
    return adoptPtr(new ActivePlatformGestureAnimation(startTime, curve, target));
}

ActivePlatformGestureAnimation::~ActivePlatformGestureAnimation()
{
}

ActivePlatformGestureAnimation::ActivePlatformGestureAnimation(double startTime, PassOwnPtr<PlatformGestureCurve> curve, PlatformGestureCurveTarget* target)
    : m_startTime(startTime)
    , m_curve(curve)
    , m_target(target)
{
}

bool ActivePlatformGestureAnimation::animate(double time)
{
    // All PlatformGestureCurves assume zero-based time, so we subtract
    // the animation start time before passing to the curve.
    return m_curve->apply(time - m_startTime, m_target);
}

} // namespace WebCore
