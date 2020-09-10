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

#pragma once

#include "InspectorWebAgentBase.h"
#include <JavaScriptCore/InspectorDebuggerAgent.h>

namespace WebCore {

class EventListener;
class EventTarget;
class InstrumentingAgents;
class RegisteredEventListener;
class TimerBase;

class WebDebuggerAgent : public Inspector::InspectorDebuggerAgent {
    WTF_MAKE_NONCOPYABLE(WebDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~WebDebuggerAgent() override;
    bool enabled() const override;

    // InspectorInstrumentation
    void didAddEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    void willRemoveEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    void willHandleEvent(const RegisteredEventListener&);
    int willPostMessage();
    void didPostMessage(int postMessageIdentifier, JSC::JSGlobalObject&);
    void didFailPostMessage(int postMessageIdentifier);
    void willDispatchPostMessage(int postMessageIdentifier);
    void didDispatchPostMessage(int postMessageIdentifier);

protected:
    WebDebuggerAgent(WebAgentContext&);
    void internalEnable() override;
    void internalDisable(bool isBeingDestroyed) override;

    void didClearAsyncStackTraceData() final;

    InstrumentingAgents& m_instrumentingAgents;

private:
    HashMap<const RegisteredEventListener*, int> m_registeredEventListeners;
    HashSet<int> m_postMessageTasks;
    int m_nextEventListenerIdentifier { 1 };
    int m_nextPostMessageIdentifier { 1 };
};

} // namespace WebCore
