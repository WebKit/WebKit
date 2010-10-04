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
    if (!inspectorController->hasFrontend())
        return;
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
    if (!inspectorController->hasFrontend())
        return;
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didInsertDOMNode(node);
}

void InspectorInstrumentation::willRemoveDOMNodeImpl(InspectorController* inspectorController, Node* node)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (!inspectorController->hasFrontend())
        return;
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
    if (!inspectorController->hasFrontend())
        return;
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didRemoveDOMNode(node);
}

void InspectorInstrumentation::willModifyDOMAttrImpl(InspectorController* inspectorController, Element* element)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (!inspectorController->hasFrontend())
        return;
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
    if (!inspectorController->hasFrontend())
        return;
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didModifyDOMAttr(element);
}

void InspectorInstrumentation::characterDataModifiedImpl(InspectorController* inspectorController, CharacterData* characterData)
{
    if (!inspectorController->hasFrontend())
        return;
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->characterDataModified(characterData);
}

int InspectorInstrumentation::instrumentWillDispatchEventImpl(InspectorController* inspectorController, const Event& event, DOMWindow* window, Node* node, const Vector<RefPtr<ContainerNode> >& ancestors)
{
    int instrumentationCookie = 0;

    if (!inspectorController->hasFrontend())
        return instrumentationCookie;

#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get()) {
        if (inspectorController->shouldBreakOnEvent(event.type())) {
            RefPtr<InspectorObject> eventData = InspectorObject::create();
            eventData->setString("type", "EventListener");
            eventData->setString("eventName", event.type());
            debuggerAgent->schedulePauseOnNextStatement(NativeBreakpointDebuggerEventType, eventData);
        }
    }
#endif

    InspectorTimelineAgent* timelineAgent = inspectorController->m_timelineAgent.get();
    if (timelineAgent && eventHasListeners(event.type(), window, node, ancestors)) {
        timelineAgent->willDispatchEvent(event);
        instrumentationCookie = timelineAgent->id();
    }
    return instrumentationCookie;
}

void InspectorInstrumentation::instrumentDidDispatchEventImpl(InspectorController* inspectorController, int instrumentationCookie)
{
    if (!inspectorController->hasFrontend())
        return;

#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get())
        debuggerAgent->cancelPauseOnNextStatement();
#endif

    InspectorTimelineAgent* timelineAgent = inspectorController->m_timelineAgent.get();
    if (timelineAgent && timelineAgent->id() == instrumentationCookie)
        timelineAgent->didDispatchEvent();
}

void InspectorInstrumentation::instrumentWillSendXMLHttpRequestImpl(InspectorController* inspectorController, const String& url)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (!inspectorController->hasFrontend())
        return;

    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;

    if (!inspectorController->shouldBreakOnXMLHttpRequest(url))
        return;

    RefPtr<InspectorObject> eventData = InspectorObject::create();
    eventData->setString("type", "XHR");
    eventData->setString("url", url);
    debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData);
#endif
}

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
