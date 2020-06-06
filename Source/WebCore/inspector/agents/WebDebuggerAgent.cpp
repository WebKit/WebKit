/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "WebDebuggerAgent.h"

#include "EventListener.h"
#include "EventTarget.h"
#include "InstrumentingAgents.h"
#include "ScriptExecutionContext.h"
#include "Timer.h"

namespace WebCore {

using namespace Inspector;

WebDebuggerAgent::WebDebuggerAgent(WebAgentContext& context)
    : InspectorDebuggerAgent(context)
    , m_instrumentingAgents(context.instrumentingAgents)
{
}

WebDebuggerAgent::~WebDebuggerAgent() = default;

bool WebDebuggerAgent::enabled() const
{
    return m_instrumentingAgents.enabledWebDebuggerAgent() == this && InspectorDebuggerAgent::enabled();
}

void WebDebuggerAgent::enable()
{
    m_instrumentingAgents.setEnabledWebDebuggerAgent(this);

    InspectorDebuggerAgent::enable();
}

void WebDebuggerAgent::disable(bool isBeingDestroyed)
{
    m_instrumentingAgents.setEnabledWebDebuggerAgent(nullptr);

    InspectorDebuggerAgent::disable(isBeingDestroyed);
}

void WebDebuggerAgent::didAddEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    if (!breakpointsActive())
        return;

    auto& eventListeners = target.eventListeners(eventType);
    auto position = eventListeners.findMatching([&](auto& registeredListener) {
        return &registeredListener->callback() == &listener && registeredListener->useCapture() == capture;
    });
    if (position == notFound)
        return;

    auto& registeredListener = eventListeners.at(position);
    if (m_registeredEventListeners.contains(registeredListener.get()))
        return;

    JSC::JSGlobalObject* scriptState = target.scriptExecutionContext()->execState();
    if (!scriptState)
        return;

    int identifier = m_nextEventListenerIdentifier++;
    m_registeredEventListeners.set(registeredListener.get(), identifier);

    didScheduleAsyncCall(scriptState, InspectorDebuggerAgent::AsyncCallType::EventListener, identifier, registeredListener->isOnce());
}

void WebDebuggerAgent::willRemoveEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    auto& eventListeners = target.eventListeners(eventType);
    size_t listenerIndex = eventListeners.findMatching([&](auto& registeredListener) {
        return &registeredListener->callback() == &listener && registeredListener->useCapture() == capture;
    });

    if (listenerIndex == notFound)
        return;

    int identifier = m_registeredEventListeners.take(eventListeners[listenerIndex].get());
    didCancelAsyncCall(InspectorDebuggerAgent::AsyncCallType::EventListener, identifier);
}

void WebDebuggerAgent::willHandleEvent(const RegisteredEventListener& listener)
{
    auto it = m_registeredEventListeners.find(&listener);
    if (it == m_registeredEventListeners.end())
        return;

    willDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::EventListener, it->value);
}

int WebDebuggerAgent::willPostMessage()
{
    if (!breakpointsActive())
        return 0;

    auto postMessageIdentifier = m_nextPostMessageIdentifier++;
    m_postMessageTasks.add(postMessageIdentifier);
    return postMessageIdentifier;
}

void WebDebuggerAgent::didPostMessage(int postMessageIdentifier, JSC::JSGlobalObject& state)
{
    if (!breakpointsActive())
        return;

    if (!postMessageIdentifier || !m_postMessageTasks.contains(postMessageIdentifier))
        return;

    didScheduleAsyncCall(&state, InspectorDebuggerAgent::AsyncCallType::PostMessage, postMessageIdentifier, true);
}

void WebDebuggerAgent::didFailPostMessage(int postMessageIdentifier)
{
    if (!postMessageIdentifier)
        return;

    auto it = m_postMessageTasks.find(postMessageIdentifier);
    if (it == m_postMessageTasks.end())
        return;

    didCancelAsyncCall(InspectorDebuggerAgent::AsyncCallType::PostMessage, postMessageIdentifier);

    m_postMessageTasks.remove(it);
}

void WebDebuggerAgent::willDispatchPostMessage(int postMessageIdentifier)
{
    if (!postMessageIdentifier || !m_postMessageTasks.contains(postMessageIdentifier))
        return;

    willDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::PostMessage, postMessageIdentifier);
}

void WebDebuggerAgent::didDispatchPostMessage(int postMessageIdentifier)
{
    if (!postMessageIdentifier)
        return;

    auto it = m_postMessageTasks.find(postMessageIdentifier);
    if (it == m_postMessageTasks.end())
        return;

    didDispatchAsyncCall();

    m_postMessageTasks.remove(it);
}

void WebDebuggerAgent::didClearAsyncStackTraceData()
{
    m_registeredEventListeners.clear();
    m_postMessageTasks.clear();
    m_nextEventListenerIdentifier = 1;
    m_nextPostMessageIdentifier = 1;
}

} // namespace WebCore
