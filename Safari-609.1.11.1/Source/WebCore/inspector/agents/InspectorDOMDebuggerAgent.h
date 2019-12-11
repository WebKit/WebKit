/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InspectorWebAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorDebuggerAgent.h>
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class Event;
class RegisteredEventListener;

typedef String ErrorString;

class InspectorDOMDebuggerAgent : public InspectorAgentBase, public Inspector::DOMDebuggerBackendDispatcherHandler, public Inspector::InspectorDebuggerAgent::Listener {
    WTF_MAKE_NONCOPYABLE(InspectorDOMDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~InspectorDOMDebuggerAgent() override;

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;
    void discardAgent() override;
    virtual bool enabled() const;

    // DOMDebuggerBackendDispatcherHandler
    void setURLBreakpoint(ErrorString&, const String& url, const bool* optionalIsRegex) final;
    void removeURLBreakpoint(ErrorString&, const String& url) final;
    void setEventBreakpoint(ErrorString&, const String& breakpointType, const String* eventName) final;
    void removeEventBreakpoint(ErrorString&, const String& breakpointType, const String* eventName) final;

    // InspectorDebuggerAgent::Listener
    void debuggerWasEnabled() override;
    void debuggerWasDisabled() override;

    // InspectorInstrumentation
    void willSendXMLHttpRequest(const String& url);
    void willFetch(const String& url);
    void willHandleEvent(Event&, const RegisteredEventListener&);
    void didHandleEvent();
    void willFireTimer(bool oneShot);

protected:
    InspectorDOMDebuggerAgent(WebAgentContext&, Inspector::InspectorDebuggerAgent*);
    virtual void enable();
    virtual void disable();

    virtual void setAnimationFrameBreakpoint(ErrorString&, bool enabled) = 0;

    Inspector::InspectorDebuggerAgent* m_debuggerAgent { nullptr };

private:
    enum class URLBreakpointSource { Fetch, XHR };
    void breakOnURLIfNeeded(const String& url, URLBreakpointSource);

    RefPtr<Inspector::DOMDebuggerBackendDispatcher> m_backendDispatcher;
    Inspector::InjectedScriptManager& m_injectedScriptManager;

    HashSet<String> m_listenerBreakpoints;

    enum class URLBreakpointType { RegularExpression, Text };
    HashMap<String, URLBreakpointType> m_urlBreakpoints;

    bool m_pauseOnAllIntervalsEnabled { false };
    bool m_pauseOnAllListenersEnabled { false };
    bool m_pauseOnAllTimeoutsEnabled { false };
    bool m_pauseOnAllURLsEnabled { false };
};

} // namespace WebCore
