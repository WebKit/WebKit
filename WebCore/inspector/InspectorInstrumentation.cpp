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

#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorDebuggerAgent.h"

namespace WebCore {

int InspectorInstrumentation::s_frontendCounter = 0;

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

void InspectorInstrumentation::instrumentWillSendXMLHttpRequestImpl(InspectorController* inspectorController, const KURL& url)
{
    if (!inspectorController->hasFrontend())
        return;
    inspectorController->instrumentWillSendXMLHttpRequest(url);
}

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
