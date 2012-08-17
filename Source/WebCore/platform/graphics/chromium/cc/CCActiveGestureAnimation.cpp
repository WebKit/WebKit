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

#include "CCActiveGestureAnimation.h"

#include "CCGestureCurve.h"
#include "TraceEvent.h"

namespace WebCore {

PassOwnPtr<CCActiveGestureAnimation> CCActiveGestureAnimation::create(PassOwnPtr<CCGestureCurve> curve, CCGestureCurveTarget* target)
{
    return adoptPtr(new CCActiveGestureAnimation(curve, target));
}

CCActiveGestureAnimation::CCActiveGestureAnimation(PassOwnPtr<CCGestureCurve> curve, CCGestureCurveTarget* target)
    : m_startTime(0)
    , m_waitingForFirstTick(true)
    , m_gestureCurve(curve)
    , m_gestureCurveTarget(target)
{
    TRACE_EVENT_ASYNC_BEGIN1("input", "GestureAnimation", this, "curve", m_gestureCurve->debugName());
}

CCActiveGestureAnimation::~CCActiveGestureAnimation()
{
    TRACE_EVENT_ASYNC_END0("input", "GestureAnimation", this);
}

bool CCActiveGestureAnimation::animate(double monotonicTime)
{
    if (m_waitingForFirstTick) {
        m_startTime = monotonicTime;
        m_waitingForFirstTick = false;
    }

    // CCGestureCurves used zero-based time, so subtract start-time.
    return m_gestureCurve->apply(monotonicTime - m_startTime, m_gestureCurveTarget);
}

} // namespace WebCore
