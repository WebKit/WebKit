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

#include "Document.h"
#include "JSDOMWindow.h"
#include "ScheduledAction.h"
#include "ScriptExecutionContext.h"
#include <runtime/JSLock.h>

namespace WebCore {

int DOMTimer::m_timerNestingLevel = 0;

DOMTimer::DOMTimer(ScriptExecutionContext* context, ScheduledAction* action)
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
    
    m_nestingLevel = m_timerNestingLevel + 1;
}

DOMTimer::~DOMTimer()
{
    if (m_action) {
        JSC::JSLock lock(false);
        delete m_action;
    }
}

void DOMTimer::fired()
{
    ScriptExecutionContext* context = scriptExecutionContext();
    // FIXME: make it work with Workers SEC too.
    if (context->isDocument()) {
        Document* document = static_cast<Document*>(context);
        if (JSDOMWindow* window = toJSDOMWindow(document->frame())) {
            m_timerNestingLevel = m_nestingLevel;
            window->timerFired(this);
            m_timerNestingLevel = 0;
        }
    }
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
    JSC::JSLock lock(false);
    delete m_action;
    m_action = 0;
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
