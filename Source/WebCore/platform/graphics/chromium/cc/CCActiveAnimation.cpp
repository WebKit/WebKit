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

#include "cc/CCActiveAnimation.h"

#include "cc/CCAnimationCurve.h"

#include <cmath>

namespace WebCore {

CCActiveAnimation::CCActiveAnimation(PassOwnPtr<CCAnimationCurve> curve, GroupID group, TargetProperty targetProperty)
    : m_animationCurve(curve)
    , m_group(group)
    , m_targetProperty(targetProperty)
    , m_runState(WaitingForTargetAvailability)
    , m_iterations(1)
    , m_startTime(0)
    , m_pauseTime(0)
    , m_totalPausedTime(0)
{
}

void CCActiveAnimation::setRunState(RunState runState, double now)
{
    if (runState == Running && m_runState == Paused)
        m_totalPausedTime += now - m_pauseTime;
    else if (runState == Paused)
        m_pauseTime = now;
    m_runState = runState;
}

bool CCActiveAnimation::isFinishedAt(double time) const
{
    if (m_runState == Finished || m_runState == Aborted)
        return true;

    return m_runState == Running
        && m_iterations >= 0
        && m_iterations * m_animationCurve->duration() <= time - startTime() - m_totalPausedTime;
}

double CCActiveAnimation::trimTimeToCurrentIteration(double now) const
{
    double trimmed = now;

    // If we're paused, time is 'stuck' at the pause time.
    if (m_runState == Paused && trimmed > m_pauseTime)
        trimmed = m_pauseTime;

    // Returned time should always be relative to the start time and should subtract
    // all time spent paused.
    trimmed -= m_startTime + m_totalPausedTime;

    // Zero is always the start of the animation.
    if (trimmed <= 0)
        return 0;

    // Always return zero if we have no iterations.
    if (!m_iterations)
        return 0;

    // If less than an iteration duration, just return trimmed.
    if (trimmed < m_animationCurve->duration())
        return trimmed;

    // If greater than or equal to the total duration, return iteration duration.
    if (m_iterations >= 0 && trimmed >= m_animationCurve->duration() * m_iterations)
        return m_animationCurve->duration();

    // Finally, return x where trimmed = x + n * m_animation->duration() for some positive integer n.
    return fmod(trimmed, m_animationCurve->duration());
}

} // namespace WebCore
