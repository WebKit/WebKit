/*
* Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "InspectorInstrumentation.h"

#if ENABLE(INSPECTOR)

#include "DOMWindow.h"
#include "Event.h"
#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorTimelineAgent.h"
#include "XMLHttpRequest.h"

namespace WebCore {

int InspectorInstrumentation::s_frontendCounter = 0;

static bool eventHasListeners(const AtomicString& eventType, DOMWindow* window, Node* node, const Vector<RefPtr<ContainerNode> >& ancestors)
{
    if (window && window->hasEventListeners(eventType))
        return true;

    if (node->hasEventListeners(eventType))
        return true;

    for (size_t i = 0; i < ancestors.size(); i++) {
        ContainerNode* ancestor = ancestors[i].get();
        if (ancestor->hasEventListeners(eventType))
            return true;
    }

    return false;
}

void InspectorInstrumentation::willInsertDOMNodeImpl(InspectorController* inspectorController, Node* node, Node* parent)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;
    InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get();
    if (!domAgent)
        return;
    PassRefPtr<InspectorValue> eventData;
    if (domAgent->shouldBreakOnNodeInsertion(node, parent, &eventData))
        debuggerAgent->breakProgram(DOMBreakpointDebuggerEventType, eventData);
#endif
}

void InspectorInstrumentation::didInsertDOMNodeImpl(InspectorController* inspectorController, Node* node)
{
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didInsertDOMNode(node);
}

void InspectorInstrumentation::willRemoveDOMNodeImpl(InspectorController* inspectorController, Node* node)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;
    InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get();
    if (!domAgent)
        return;
    PassRefPtr<InspectorValue> eventData;
    if (domAgent->shouldBreakOnNodeRemoval(node, &eventData))
        debuggerAgent->breakProgram(DOMBreakpointDebuggerEventType, eventData);
#endif
}

void InspectorInstrumentation::didRemoveDOMNodeImpl(InspectorController* inspectorController, Node* node)
{
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didRemoveDOMNode(node);
}

void InspectorInstrumentation::willModifyDOMAttrImpl(InspectorController* inspectorController, Element* element)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;
    InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get();
    if (!domAgent)
        return;
    PassRefPtr<InspectorValue> eventData;
    if (domAgent->shouldBreakOnAttributeModification(element, &eventData))
        debuggerAgent->breakProgram(DOMBreakpointDebuggerEventType, eventData);
#endif
}

void InspectorInstrumentation::didModifyDOMAttrImpl(InspectorController* inspectorController, Element* element)
{
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didModifyDOMAttr(element);
}

void InspectorInstrumentation::characterDataModifiedImpl(InspectorController* inspectorController, CharacterData* characterData)
{
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->characterDataModified(characterData);
}


void InspectorInstrumentation::willSendXMLHttpRequestImpl(InspectorController* inspectorController, const String& url)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;

    unsigned int breakpointId = inspectorController->findXHRBreakpoint(url);
    if (!breakpointId)
        return;

    RefPtr<InspectorObject> eventData = InspectorObject::create();
    eventData->setNumber("breakpointId", breakpointId);
    eventData->setString("type", "XHR");
    eventData->setString("url", url);
    debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData);
#endif
}

void InspectorInstrumentation::didScheduleResourceRequestImpl(InspectorController* inspectorController, const String& url)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController))
        timelineAgent->didScheduleResourceRequest(url);
}

void InspectorInstrumentation::didInstallTimerImpl(InspectorController* inspectorController, int timerId, int timeout, bool singleShot)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController))
        timelineAgent->didInstallTimer(timerId, timeout, singleShot);
}

void InspectorInstrumentation::didRemoveTimerImpl(InspectorController* inspectorController, int timerId)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController))
        timelineAgent->didRemoveTimer(timerId);
}


InspectorInstrumentationCookie InspectorInstrumentation::willCallFunctionImpl(InspectorController* inspectorController, const String& scriptName, int scriptLine)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willCallFunction(scriptName, scriptLine);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didCallFunctionImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didCallFunction();
}

InspectorInstrumentationCookie InspectorInstrumentation::willChangeXHRReadyStateImpl(InspectorController* inspectorController, XMLHttpRequest* request)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent && request->hasEventListeners(eventNames().readystatechangeEvent)) {
        timelineAgent->willChangeXHRReadyState(request->url().string(), request->readyState());
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didChangeXHRReadyStateImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didChangeXHRReadyState();
}

InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventImpl(InspectorController* inspectorController, const Event& event, DOMWindow* window, Node* node, const Vector<RefPtr<ContainerNode> >& ancestors)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get()) {
        unsigned int breakpointId = inspectorController->findEventListenerBreakpoint(event.type());
        if (breakpointId) {
            RefPtr<InspectorObject> eventData = InspectorObject::create();
            eventData->setNumber("breakpointId", breakpointId);
            eventData->setString("type", "EventListener");
            eventData->setString("eventName", event.type());
            debuggerAgent->schedulePauseOnNextStatement(NativeBreakpointDebuggerEventType, eventData);
        }
    }
#endif

    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent && eventHasListeners(event.type(), window, node, ancestors)) {
        timelineAgent->willDispatchEvent(event);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didDispatchEventImpl(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent* debuggerAgent = cookie.first->m_debuggerAgent.get())
        debuggerAgent->cancelPauseOnNextStatement();
#endif

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindowImpl(InspectorController* inspectorController, const Event& event, DOMWindow* window)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent && window->hasEventListeners(event.type())) {
        timelineAgent->willDispatchEvent(event);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScriptImpl(InspectorController* inspectorController, const String& url, int lineNumber)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willEvaluateScript(url, lineNumber);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didEvaluateScriptImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didEvaluateScript();
}

InspectorInstrumentationCookie InspectorInstrumentation::willFireTimerImpl(InspectorController* inspectorController, int timerId)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willFireTimer(timerId);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didFireTimerImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didFireTimer();
}

InspectorInstrumentationCookie InspectorInstrumentation::willLayoutImpl(InspectorController* inspectorController)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willLayout();
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didLayoutImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didLayout();
}

InspectorInstrumentationCookie InspectorInstrumentation::willLoadXHRImpl(InspectorController* inspectorController, XMLHttpRequest* request)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent && request->hasEventListeners(eventNames().loadEvent)) {
        timelineAgent->willLoadXHR(request->url());
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didLoadXHRImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didLoadXHR();
}

InspectorInstrumentationCookie InspectorInstrumentation::willPaintImpl(InspectorController* inspectorController, const IntRect& rect)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willPaint(rect);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didPaintImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didPaint();
}

InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyleImpl(InspectorController* inspectorController)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willRecalculateStyle();
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didRecalculateStyleImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didRecalculateStyle();
}

InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceDataImpl(InspectorController* inspectorController, unsigned long identifier)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willReceiveResourceData(identifier);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didReceiveResourceDataImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didReceiveResourceData();
}

InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponseImpl(InspectorController* inspectorController, unsigned long identifier, const ResourceResponse& response)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willReceiveResourceResponse(identifier, response);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didReceiveResourceResponseImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didReceiveResourceResponse();
}

InspectorInstrumentationCookie InspectorInstrumentation::willWriteHTMLImpl(InspectorController* inspectorController, unsigned int length, unsigned int startLine)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willWriteHTML(length, startLine);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didWriteHTMLImpl(const InspectorInstrumentationCookie& cookie, unsigned int endLine)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didWriteHTML(endLine);
}

bool InspectorInstrumentation::hasFrontend(InspectorController* inspectorController)
{
    return inspectorController->hasFrontend();
}

InspectorTimelineAgent* InspectorInstrumentation::retrieveTimelineAgent(InspectorController* inspectorController)
{
    return inspectorController->m_timelineAgent.get();
}

InspectorTimelineAgent* InspectorInstrumentation::retrieveTimelineAgent(const InspectorInstrumentationCookie& cookie)
{
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie.first);
    if (timelineAgent && timelineAgent->id() == cookie.second)
        return timelineAgent;
    return 0;
}

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
