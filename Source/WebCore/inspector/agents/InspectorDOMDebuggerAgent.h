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

class Element;
class Event;
class Frame;
class Node;
class RegisteredEventListener;

typedef String ErrorString;

class InspectorDOMDebuggerAgent final : public InspectorAgentBase, public Inspector::DOMDebuggerBackendDispatcherHandler, public Inspector::InspectorDebuggerAgent::Listener {
    WTF_MAKE_NONCOPYABLE(InspectorDOMDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorDOMDebuggerAgent(WebAgentContext&, Inspector::InspectorDebuggerAgent*);
    virtual ~InspectorDOMDebuggerAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);
    void discardAgent();

    // DOMDebuggerBackendDispatcherHandler
    void setURLBreakpoint(ErrorString&, const String& url, const bool* optionalIsRegex);
    void removeURLBreakpoint(ErrorString&, const String& url);
    void setEventBreakpoint(ErrorString&, const String& breakpointType, const String* eventName);
    void removeEventBreakpoint(ErrorString&, const String& breakpointType, const String* eventName);
    void setDOMBreakpoint(ErrorString&, int nodeId, const String& type);
    void removeDOMBreakpoint(ErrorString&, int nodeId, const String& type);

    // InspectorDebuggerAgent::Listener
    void debuggerWasEnabled();
    void debuggerWasDisabled();

    // InspectorInstrumentation
    void willInsertDOMNode(Node& parent);
    void willInvalidateStyleAttr(Element&);
    void didInsertDOMNode(Node&);
    void willRemoveDOMNode(Node&);
    void didRemoveDOMNode(Node&);
    void willModifyDOMAttr(Element&);
    void willSendXMLHttpRequest(const String& url);
    void willFetch(const String& url);
    void frameDocumentUpdated(Frame&);
    void willHandleEvent(Event&, const RegisteredEventListener&);
    void didHandleEvent();
    void willFireTimer(bool oneShot);
    void willFireAnimationFrame();
    void mainFrameDOMContentLoaded();

    void disable();

private:
    enum class URLBreakpointSource { Fetch, XHR };
    void breakOnURLIfNeeded(const String& url, URLBreakpointSource);

    void descriptionForDOMEvent(Node& target, int breakpointType, bool insertion, JSON::Object& description);
    void updateSubtreeBreakpoints(Node*, uint32_t rootMask, bool set);
    bool hasBreakpoint(Node*, int type);

    RefPtr<Inspector::DOMDebuggerBackendDispatcher> m_backendDispatcher;
    Inspector::InjectedScriptManager& m_injectedScriptManager;

    Inspector::InspectorDebuggerAgent* m_debuggerAgent { nullptr };

    HashMap<Node*, uint32_t> m_domBreakpoints;
    HashSet<String> m_listenerBreakpoints;

    enum class URLBreakpointType { RegularExpression, Text };
    HashMap<String, URLBreakpointType> m_urlBreakpoints;

    bool m_pauseOnAllAnimationFramesEnabled { false };
    bool m_pauseOnAllIntervalsEnabled { false };
    bool m_pauseOnAllListenersEnabled { false };
    bool m_pauseOnAllTimeoutsEnabled { false };
    bool m_pauseOnAllURLsEnabled { false };
};

} // namespace WebCore
