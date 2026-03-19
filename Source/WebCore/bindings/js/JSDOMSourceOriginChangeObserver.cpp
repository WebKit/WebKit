/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "JSDOMSourceOriginChangeObserver.h"

#if ENABLE(RESOURCE_ANALYTICS)

#include "EventListener.h"
#include "JSDOMGlobalObject.h"
#include "ScheduledAction.h"

namespace WebCore {

JSDOMSourceOriginChangeObserver::JSDOMSourceOriginChangeObserver(JSDOMGlobalObject& globalObject)
    : m_globalObject(globalObject)
{
}

void JSDOMSourceOriginChangeObserver::trackEventListenerOrigin(EventListener& listener, JSC::SourceOrigin&& origin)
{
    if (!origin.isNull())
        m_eventListenerOrigins.set(listener, WTF::move(origin));
}

JSC::SourceOrigin JSDOMSourceOriginChangeObserver::originForEventListener(const EventListener& listener) const
{
    auto it = m_eventListenerOrigins.find(listener);
    if (it != m_eventListenerOrigins.end())
        return it->value;
    return JSC::SourceOrigin { };
}

void JSDOMSourceOriginChangeObserver::trackScheduledActionOrigin(const ScheduledAction* action, JSC::SourceOrigin&& origin)
{
    if (!origin.isNull())
        m_scheduledActionOrigins.set(action, WTF::move(origin));
}

JSC::SourceOrigin JSDOMSourceOriginChangeObserver::takeScheduledActionOrigin(const ScheduledAction* action)
{
    return m_scheduledActionOrigins.take(action);
}

void JSDOMSourceOriginChangeObserver::willBeginProgramExecution(const JSC::SourceOrigin& origin)
{
    m_globalObject.willBeginProgramExecution(origin);
}

void JSDOMSourceOriginChangeObserver::didEndProgramExecution(const JSC::SourceOrigin& origin)
{
    m_globalObject.didEndProgramExecution(origin);
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_ANALYTICS)
