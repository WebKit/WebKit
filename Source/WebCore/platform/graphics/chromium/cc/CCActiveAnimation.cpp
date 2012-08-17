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

#include "TraceEvent.h"
#include "cc/CCAnimationCurve.h"
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

#include <cmath>

namespace {

// This should match the RunState enum.
static const char* const s_runStateNames[] = {
    "WaitingForNextTick",
    "WaitingForTargetAvailability",
    "WaitingForStartTime",
    "WaitingForDeletion",
    "Running",
    "Paused",
    "Finished",
    "Aborted"
};

COMPILE_ASSERT(static_cast<int>(WebCore::CCActiveAnimation::RunStateEnumSize) == WTF_ARRAY_LENGTH(s_runStateNames), RunState_names_match_enum);

// This should match the TargetProperty enum.
static const char* const s_targetPropertyNames[] = {
    "Transform",
    "Opacity"
};

COMPILE_ASSERT(static_cast<int>(WebCore::CCActiveAnimation::TargetPropertyEnumSize) == WTF_ARRAY_LENGTH(s_targetPropertyNames), TargetProperty_names_match_enum);

} // namespace

namespace WebCore {

PassOwnPtr<CCActiveAnimation> CCActiveAnimation::create(PassOwnPtr<CCAnimationCurve> curve, int animationId, int groupId, TargetProperty targetProperty)
{
    return adoptPtr(new CCActiveAnimation(curve, animationId, groupId, targetProperty));
}

CCActiveAnimation::CCActiveAnimation(PassOwnPtr<CCAnimationCurve> curve, int animationId, int groupId, TargetProperty targetProperty)
    : m_curve(curve)
    , m_id(animationId)
    , m_group(groupId)
    , m_targetProperty(targetProperty)
    , m_runState(WaitingForTargetAvailability)
    , m_iterations(1)
    , m_startTime(0)
    , m_alternatesDirection(false)
    , m_timeOffset(0)
    , m_needsSynchronizedStartTime(false)
    , m_suspended(false)
    , m_pauseTime(0)
    , m_totalPausedTime(0)
    , m_isControllingInstance(false)
{
}

CCActiveAnimation::~CCActiveAnimation()
{
    if (m_runState == Running || m_runState == Paused)
        setRunState(Aborted, 0);
}

void CCActiveAnimation::setRunState(RunState runState, double monotonicTime)
{
    if (m_suspended)
        return;

    char nameBuffer[256];
    snprintf(nameBuffer, sizeof(nameBuffer), "%s-%d%s", s_targetPropertyNames[m_targetProperty], m_group, m_isControllingInstance ? "(impl)" : "");

    bool isWaitingToStart = m_runState == WaitingForNextTick
        || m_runState == WaitingForTargetAvailability
        || m_runState == WaitingForStartTime;

    if (isWaitingToStart && runState == Running)
        TRACE_EVENT_ASYNC_BEGIN1("cc", "CCActiveAnimation", this, "Name", TRACE_STR_COPY(nameBuffer));

    bool wasFinished = isFinished();

    const char* oldRunStateName = s_runStateNames[m_runState];

    if (runState == Running && m_runState == Paused)
        m_totalPausedTime += monotonicTime - m_pauseTime;
    else if (runState == Paused)
        m_pauseTime = monotonicTime;
    m_runState = runState;

    const char* newRunStateName = s_runStateNames[runState];

    if (!wasFinished && isFinished())
        TRACE_EVENT_ASYNC_END0("cc", "CCActiveAnimation", this);

    char stateBuffer[256];
    snprintf(stateBuffer, sizeof(stateBuffer), "%s->%s", oldRunStateName, newRunStateName);

    TRACE_EVENT_INSTANT2("cc", "CCLayerAnimationController::setRunState", "Name", TRACE_STR_COPY(nameBuffer), "State", TRACE_STR_COPY(stateBuffer));
}

void CCActiveAnimation::suspend(double monotonicTime)
{
    setRunState(Paused, monotonicTime);
    m_suspended = true;
}

void CCActiveAnimation::resume(double monotonicTime)
{
    m_suspended = false;
    setRunState(Running, monotonicTime);
}

bool CCActiveAnimation::isFinishedAt(double monotonicTime) const
{
    if (isFinished())
        return true;

    if (m_needsSynchronizedStartTime)
        return false;

    return m_runState == Running
        && m_iterations >= 0
        && m_iterations * m_curve->duration() <= monotonicTime - startTime() - m_totalPausedTime;
}

double CCActiveAnimation::trimTimeToCurrentIteration(double monotonicTime) const
{
    double trimmed = monotonicTime + m_timeOffset;

    // If we're paused, time is 'stuck' at the pause time.
    if (m_runState == Paused)
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
    if (trimmed < m_curve->duration())
        return trimmed;

    // If greater than or equal to the total duration, return iteration duration.
    if (m_iterations >= 0 && trimmed >= m_curve->duration() * m_iterations) {
        if (m_alternatesDirection && !(m_iterations % 2))
            return 0;
        return m_curve->duration();
    }

    // We need to know the current iteration if we're alternating.
    int iteration = static_cast<int>(trimmed / m_curve->duration());

    // Calculate x where trimmed = x + n * m_curve->duration() for some positive integer n.
    trimmed = fmod(trimmed, m_curve->duration());

    // If we're alternating and on an odd iteration, reverse the direction.
    if (m_alternatesDirection && iteration % 2 == 1)
        return m_curve->duration() - trimmed;

    return trimmed;
}

PassOwnPtr<CCActiveAnimation> CCActiveAnimation::clone(InstanceType instanceType) const
{
    return cloneAndInitialize(instanceType, m_runState, m_startTime);
}

PassOwnPtr<CCActiveAnimation> CCActiveAnimation::cloneAndInitialize(InstanceType instanceType, RunState initialRunState, double startTime) const
{
    OwnPtr<CCActiveAnimation> toReturn(adoptPtr(new CCActiveAnimation(m_curve->clone(), m_id, m_group, m_targetProperty)));
    toReturn->m_runState = initialRunState;
    toReturn->m_iterations = m_iterations;
    toReturn->m_startTime = startTime;
    toReturn->m_pauseTime = m_pauseTime;
    toReturn->m_totalPausedTime = m_totalPausedTime;
    toReturn->m_timeOffset = m_timeOffset;
    toReturn->m_alternatesDirection = m_alternatesDirection;
    toReturn->m_isControllingInstance = instanceType == ControllingInstance;
    return toReturn.release();
}

void CCActiveAnimation::pushPropertiesTo(CCActiveAnimation* other) const
{
    // Currently, we only push changes due to pausing and resuming animations on the main thread.
    if (m_runState == CCActiveAnimation::Paused || other->m_runState == CCActiveAnimation::Paused) {
        other->m_runState = m_runState;
        other->m_pauseTime = m_pauseTime;
        other->m_totalPausedTime = m_totalPausedTime;
    }
}

} // namespace WebCore
