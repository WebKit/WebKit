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

#ifndef InspectorInstrumentation_h
#define InspectorInstrumentation_h

#include "Document.h"
#include "Page.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

class CharacterData;
class Element;
class InspectorController;
class Node;

class InspectorInstrumentation {
public:
    static void willInsertDOMNode(Document*, Node*, Node* parent);
    static void didInsertDOMNode(Document*, Node*);
    static void willRemoveDOMNode(Document*, Node*);
    static void willModifyDOMAttr(Document*, Element*);
    static void didModifyDOMAttr(Document*, Element*);
    static void characterDataModified(Document*, CharacterData*);

    static int instrumentWillDispatchEvent(Document*, const Event&, DOMWindow*, Node*, const Vector<RefPtr<ContainerNode> >& ancestors);
    static void instrumentDidDispatchEvent(Document*, int instrumentationCookie);

    static void instrumentWillSendXMLHttpRequest(ScriptExecutionContext*, const String&);

#if ENABLE(INSPECTOR)
    static void frontendCreated() { s_frontendCounter += 1; }
    static void frontendDeleted() { s_frontendCounter -= 1; }
    static bool hasFrontends() { return s_frontendCounter; }
#endif

private:
#if ENABLE(INSPECTOR)
    static void willInsertDOMNodeImpl(InspectorController*, Node* node, Node* parent);
    static void didInsertDOMNodeImpl(InspectorController*, Node*);
    static void willRemoveDOMNodeImpl(InspectorController*, Node*);
    static void didRemoveDOMNodeImpl(InspectorController*, Node*);
    static void willModifyDOMAttrImpl(InspectorController*, Element*);
    static void didModifyDOMAttrImpl(InspectorController*, Element*);
    static void characterDataModifiedImpl(InspectorController*, CharacterData*);

    static int instrumentWillDispatchEventImpl(InspectorController*, const Event&, DOMWindow*, Node*, const Vector<RefPtr<ContainerNode> >& ancestors);
    static void instrumentDidDispatchEventImpl(InspectorController*, int instrumentationCookie);

    static void instrumentWillSendXMLHttpRequestImpl(InspectorController*, const String&);

    static InspectorController* inspectorControllerForScriptExecutionContext(ScriptExecutionContext*);
    static InspectorController* inspectorControllerForDocument(Document*);
    static InspectorController* inspectorControllerForPage(Page*);

    static int s_frontendCounter;
#endif
};

inline void InspectorInstrumentation::willInsertDOMNode(Document* document, Node* node, Node* parent)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        willInsertDOMNodeImpl(inspectorController, node, parent);
#endif
}

inline void InspectorInstrumentation::didInsertDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        didInsertDOMNodeImpl(inspectorController, node);
#endif
}

inline void InspectorInstrumentation::willRemoveDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document)) {
        willRemoveDOMNodeImpl(inspectorController, node);
        didRemoveDOMNodeImpl(inspectorController, node);
    }
#endif
}

inline void InspectorInstrumentation::willModifyDOMAttr(Document* document, Element* element)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        willModifyDOMAttrImpl(inspectorController, element);
#endif
}

inline void InspectorInstrumentation::didModifyDOMAttr(Document* document, Element* element)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        didModifyDOMAttrImpl(inspectorController, element);
#endif
}

inline void InspectorInstrumentation::characterDataModified(Document* document, CharacterData* characterData)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        characterDataModifiedImpl(inspectorController, characterData);
#endif
}

inline int InspectorInstrumentation::instrumentWillDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const Vector<RefPtr<ContainerNode> >& ancestors)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        return instrumentWillDispatchEventImpl(inspectorController, event, window, node, ancestors);
#endif
    return 0;
}

inline void InspectorInstrumentation::instrumentDidDispatchEvent(Document* document, int instrumentationCookie)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        instrumentDidDispatchEventImpl(inspectorController, instrumentationCookie);
#endif
}

inline void InspectorInstrumentation::instrumentWillSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForScriptExecutionContext(context))
        instrumentWillSendXMLHttpRequestImpl(inspectorController, url);
#endif
}

#if ENABLE(INSPECTOR)
inline InspectorController* InspectorInstrumentation::inspectorControllerForScriptExecutionContext(ScriptExecutionContext* context)
{
    if (hasFrontends() && context && context->isDocument())
        return inspectorControllerForPage(static_cast<Document*>(context)->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerForDocument(Document* document)
{
    if (hasFrontends())
        return inspectorControllerForPage(document->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerForPage(Page* page)
{
    return page ? page->inspectorController() : 0;
}
#endif

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_h)
