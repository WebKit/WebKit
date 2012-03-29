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

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

namespace WebCore {

PassOwnPtr<ActivePlatformGestureAnimation> ActivePlatformGestureAnimation::create(PassOwnPtr<PlatformGestureCurve> curve, PlatformGestureCurveTarget* target)
{
    return adoptPtr(new ActivePlatformGestureAnimation(curve, target));
}

PassOwnPtr<ActivePlatformGestureAnimation> ActivePlatformGestureAnimation::create(PassOwnPtr<PlatformGestureCurve> curve, PlatformGestureCurveTarget* target, double startTime)
{
    return adoptPtr(new ActivePlatformGestureAnimation(curve, target, startTime));
}

ActivePlatformGestureAnimation::~ActivePlatformGestureAnimation()
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT_ASYNC_END0("input", "GestureAnimation", this);
#endif
}

ActivePlatformGestureAnimation::ActivePlatformGestureAnimation(PassOwnPtr<PlatformGestureCurve> curve, PlatformGestureCurveTarget* target)
    : m_startTime(0)
    , m_waitingForFirstTick(true)
    , m_curve(curve)
    , m_target(target)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT_ASYNC_BEGIN1("input", "GestureAnimation", this, "curve", m_curve->debugName());
#endif
}

ActivePlatformGestureAnimation::ActivePlatformGestureAnimation(PassOwnPtr<PlatformGestureCurve> curve, PlatformGestureCurveTarget* target, double startTime)
    : m_startTime(startTime)
    , m_waitingForFirstTick(false)
    , m_curve(curve)
    , m_target(target)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT_ASYNC_BEGIN1("input", "GestureAnimation", this, "curve", m_curve->debugName());
#endif
}

bool ActivePlatformGestureAnimation::animate(double time)
{
    if (m_waitingForFirstTick) {
        m_startTime = time;
        m_waitingForFirstTick = false;
    }
    // All PlatformGestureCurves assume zero-based time, so we subtract
    // the animation start time before passing to the curve.
    return m_curve->apply(time - m_startTime, m_target);
}

} // namespace WebCore
