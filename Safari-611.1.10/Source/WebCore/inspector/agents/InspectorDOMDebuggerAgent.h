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
#include <JavaScriptCore/Breakpoint.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorDebuggerAgent.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {

class Event;
class RegisteredEventListener;

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
    Inspector::Protocol::ErrorStringOr<void> setURLBreakpoint(const String& url, Optional<bool>&& isRegex, RefPtr<JSON::Object>&& options) final;
    Inspector::Protocol::ErrorStringOr<void> removeURLBreakpoint(const String& url, Optional<bool>&& isRegex) final;
    Inspector::Protocol::ErrorStringOr<void> setEventBreakpoint(Inspector::Protocol::DOMDebugger::EventBreakpointType, const String& eventName, RefPtr<JSON::Object>&& options) final;
    Inspector::Protocol::ErrorStringOr<void> removeEventBreakpoint(Inspector::Protocol::DOMDebugger::EventBreakpointType, const String& eventName) final;

    // InspectorDebuggerAgent::Listener
    void debuggerWasEnabled() override;
    void debuggerWasDisabled() override;

    // InspectorInstrumentation
    virtual void mainFrameNavigated();
    void willSendXMLHttpRequest(const String& url);
    void willFetch(const String& url);
    void willHandleEvent(Event&, const RegisteredEventListener&);
    void didHandleEvent(Event&, const RegisteredEventListener&);
    void willFireTimer(bool oneShot);
    void didFireTimer(bool oneShot);

protected:
    InspectorDOMDebuggerAgent(WebAgentContext&, Inspector::InspectorDebuggerAgent*);
    virtual void enable();
    virtual void disable();

    virtual bool setAnimationFrameBreakpoint(Inspector::Protocol::ErrorString&, RefPtr<JSC::Breakpoint>&&) = 0;

    Inspector::InspectorDebuggerAgent* m_debuggerAgent { nullptr };

private:
    enum class URLBreakpointSource { Fetch, XHR };
    void breakOnURLIfNeeded(const String& url, URLBreakpointSource);

    RefPtr<Inspector::DOMDebuggerBackendDispatcher> m_backendDispatcher;
    Inspector::InjectedScriptManager& m_injectedScriptManager;

    HashMap<String, Ref<JSC::Breakpoint>> m_listenerBreakpoints;
    RefPtr<JSC::Breakpoint> m_pauseOnAllIntervalsBreakpoint;
    RefPtr<JSC::Breakpoint> m_pauseOnAllListenersBreakpoint;
    RefPtr<JSC::Breakpoint> m_pauseOnAllTimeoutsBreakpoint;

    HashMap<String, Ref<JSC::Breakpoint>> m_urlTextBreakpoints;
    HashMap<String, Ref<JSC::Breakpoint>> m_urlRegexBreakpoints;
    RefPtr<JSC::Breakpoint> m_pauseOnAllURLsBreakpoint;
};

} // namespace WebCore
