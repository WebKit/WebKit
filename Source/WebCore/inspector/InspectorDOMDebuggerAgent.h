/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorDOMDebuggerAgent_h
#define InspectorDOMDebuggerAgent_h

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)

#include "InspectorDebuggerAgent.h"
#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class InspectorObject;
}

namespace WebCore {

class Element;
class InspectorDOMAgent;
class InspectorDebuggerAgent;
class InstrumentingAgents;
class Node;

typedef String ErrorString;

class InspectorDOMDebuggerAgent : public InspectorAgentBase, public InspectorDebuggerAgent::Listener, public Inspector::InspectorDOMDebuggerBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDOMDebuggerAgent);
public:
    InspectorDOMDebuggerAgent(InstrumentingAgents*, InspectorDOMAgent*, InspectorDebuggerAgent*);
    virtual ~InspectorDOMDebuggerAgent();

    // DOMDebugger API
    virtual void setXHRBreakpoint(ErrorString*, const String& url) override;
    virtual void removeXHRBreakpoint(ErrorString*, const String& url) override;
    virtual void setEventListenerBreakpoint(ErrorString*, const String& eventName) override;
    virtual void removeEventListenerBreakpoint(ErrorString*, const String& eventName) override;
    virtual void setInstrumentationBreakpoint(ErrorString*, const String& eventName) override;
    virtual void removeInstrumentationBreakpoint(ErrorString*, const String& eventName) override;
    virtual void setDOMBreakpoint(ErrorString*, int nodeId, const String& type) override;
    virtual void removeDOMBreakpoint(ErrorString*, int nodeId, const String& type) override;

    // InspectorInstrumentation API
    void willInsertDOMNode(Node* parent);
    void didInvalidateStyleAttr(Node*);
    void didInsertDOMNode(Node*);
    void willRemoveDOMNode(Node*);
    void didRemoveDOMNode(Node*);
    void willModifyDOMAttr(Element*);
    void willSendXMLHttpRequest(const String& url);
    void pauseOnNativeEventIfNeeded(bool isDOMEvent, const String& eventName, bool synchronous);

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::InspectorDisconnectReason) override;
    virtual void discardAgent() override;

private:
    // InspectorDebuggerAgent::Listener implementation.
    virtual void debuggerWasEnabled() override;
    virtual void debuggerWasDisabled() override;
    virtual void stepInto() override;
    virtual void didPause() override;
    void disable();

    void descriptionForDOMEvent(Node* target, int breakpointType, bool insertion, Inspector::InspectorObject* description);
    void updateSubtreeBreakpoints(Node*, uint32_t rootMask, bool set);
    bool hasBreakpoint(Node*, int type);
    void discardBindings();
    void setBreakpoint(ErrorString*, const String& eventName);
    void removeBreakpoint(ErrorString*, const String& eventName);

    void clear();

    InspectorDOMAgent* m_domAgent;
    InspectorDebuggerAgent* m_debuggerAgent;
    RefPtr<Inspector::InspectorDOMDebuggerBackendDispatcher> m_backendDispatcher;
    HashMap<Node*, uint32_t> m_domBreakpoints;
    HashSet<String> m_eventListenerBreakpoints;
    HashSet<String> m_xhrBreakpoints;
    bool m_pauseInNextEventListener;
    bool m_pauseOnAllXHRsEnabled;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)

#endif // !defined(InspectorDOMDebuggerAgent_h)
