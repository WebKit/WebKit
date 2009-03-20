/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "DOMTimer.h"

#include "ScheduledAction.h"
#include "ScriptExecutionContext.h"
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

static const int maxTimerNestingLevel = 5;
static const double oneMillisecond = 0.001;
double DOMTimer::s_minTimerInterval = 0.010; // 10 milliseconds

static int timerNestingLevel = 0;

DOMTimer::DOMTimer(ScriptExecutionContext* context, ScheduledAction* action, int timeout, bool singleShot)
    : ActiveDOMObject(context, this)
    , m_action(action)
    , m_nextFireInterval(0)
    , m_repeatInterval(0)
{
    static int lastUsedTimeoutId = 0;
    ++lastUsedTimeoutId;
    // Avoid wraparound going negative on us.
    if (lastUsedTimeoutId <= 0)
        lastUsedTimeoutId = 1;
    m_timeoutId = lastUsedTimeoutId;
    
    m_nestingLevel = timerNestingLevel + 1;

    scriptExecutionContext()->addTimeout(m_timeoutId, this);

    double intervalMilliseconds = max(oneMillisecond, timeout * oneMillisecond);

    // Use a minimum interval of 10 ms to match other browsers, but only once we've
    // nested enough to notice that we're repeating.
    // Faster timers might be "better", but they're incompatible.
    if (intervalMilliseconds < s_minTimerInterval && m_nestingLevel >= maxTimerNestingLevel)
        intervalMilliseconds = s_minTimerInterval;
    if (singleShot)
        startOneShot(intervalMilliseconds);
    else
        startRepeating(intervalMilliseconds);
}

DOMTimer::~DOMTimer()
{
    if (scriptExecutionContext()) {
        scriptExecutionContext()->removeTimeout(m_timeoutId);
    }
}
    
int DOMTimer::install(ScriptExecutionContext* context, ScheduledAction* action, int timeout, bool singleShot)
{
    // DOMTimer constructor links the new timer into a list of ActiveDOMObjects held by the 'context'.
    // The timer is deleted when context is deleted (DOMTimer::contextDestroyed) or explicitly via DOMTimer::removeById(),
    // or if it is a one-time timer and it has fired (DOMTimer::fired).
    DOMTimer* timer = new DOMTimer(context, action, timeout, singleShot);
    return timer->m_timeoutId;
}

void DOMTimer::removeById(ScriptExecutionContext* context, int timeoutId)
{
    // timeout IDs have to be positive, and 0 and -1 are unsafe to
    // even look up since they are the empty and deleted value
    // respectively
    if (timeoutId <= 0)
        return;
    delete context->findTimeout(timeoutId);
}

void DOMTimer::fired()
{
    ScriptExecutionContext* context = scriptExecutionContext();
    timerNestingLevel = m_nestingLevel;

    // Simple case for non-one-shot timers.
    if (isActive()) {
        if (repeatInterval() && repeatInterval() < s_minTimerInterval) {
            m_nestingLevel++;
            if (m_nestingLevel >= maxTimerNestingLevel)
                augmentRepeatInterval(s_minTimerInterval - repeatInterval());
        }
        
        // No access to member variables after this point, it can delete the timer.
        m_action->execute(context);
        return;
    }

    // Delete timer before executing the action for one-shot timers.
    ScheduledAction* action = m_action.release();

    // No access to member variables after this point.
    delete this;
    
    action->execute(context);
    delete action;
    timerNestingLevel = 0;
}

bool DOMTimer::hasPendingActivity() const
{
    return isActive();
}

void DOMTimer::contextDestroyed()
{
    ActiveDOMObject::contextDestroyed();
    delete this;
}

void DOMTimer::stop()
{
    TimerBase::stop();
    // Need to release JS objects potentially protected by ScheduledAction
    // because they can form circular references back to the ScriptExecutionContext
    // which will cause a memory leak.
    m_action.clear();
}

void DOMTimer::suspend() 
{ 
    ASSERT(m_nextFireInterval == 0 && m_repeatInterval == 0); 
    m_nextFireInterval = nextFireInterval();
    m_repeatInterval = repeatInterval();
    TimerBase::stop();
} 
 
void DOMTimer::resume() 
{ 
    start(m_nextFireInterval, m_repeatInterval);
    m_nextFireInterval = 0;
    m_repeatInterval = 0;
} 
 
 
bool DOMTimer::canSuspend() const 
{ 
    return true;
}

} // namespace WebCore
