/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorDOMAgent_h
#define InspectorDOMAgent_h

#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "Timer.h"

#include <wtf/Deque.h>
#include <wtf/ListHashSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {
class ContainerNode;
class CharacterData;
class Document;
class Element;
class Event;
class InspectorDOMAgent;
class InspectorFrontend;
class MatchJob;
class InstrumentingAgents;
class NameNodeMap;
class Node;
class Page;
class RevalidateStyleAttributeTask;
class ScriptValue;

typedef String ErrorString;

#if ENABLE(INSPECTOR)

struct EventListenerInfo {
    EventListenerInfo(Node* node, const AtomicString& eventType, const EventListenerVector& eventListenerVector)
        : node(node)
        , eventType(eventType)
        , eventListenerVector(eventListenerVector)
    {
    }

    Node* node;
    const AtomicString eventType;
    const EventListenerVector eventListenerVector;
};

class InspectorDOMAgent {
public:
    struct DOMListener {
        virtual ~DOMListener()
        {
        }
        virtual void didRemoveDocument(Document*) = 0;
        virtual void didRemoveDOMNode(Node*) = 0;
        virtual void didModifyDOMAttr(Element*) = 0;
    };

    static PassOwnPtr<InspectorDOMAgent> create(InstrumentingAgents* instrumentingAgents, InjectedScriptHost* injectedScriptHost)
    {
        return adoptPtr(new InspectorDOMAgent(instrumentingAgents, injectedScriptHost));
    }

    ~InspectorDOMAgent();

    void setFrontend(InspectorFrontend*);
    void clearFrontend();

    Vector<Document*> documents();
    void reset();

    void querySelector(ErrorString*, long nodeId, const String& selectors, bool documentWide, long* elementId);
    void querySelectorAll(ErrorString*, long nodeId, const String& selectors, bool documentWide, RefPtr<InspectorArray>* result);
    // Methods called from the frontend for DOM nodes inspection.
    void getChildNodes(ErrorString*, long nodeId);
    void setAttribute(ErrorString*, long elementId, const String& name, const String& value, bool* success);
    void removeAttribute(ErrorString*, long elementId, const String& name, bool* success);
    void removeNode(ErrorString*, long nodeId, long* outNodeId);
    void changeTagName(ErrorString*, long nodeId, const String& tagName, long* newId);
    void getOuterHTML(ErrorString*, long nodeId, WTF::String* outerHTML);
    void setOuterHTML(ErrorString*, long nodeId, const String& outerHTML, long* newId);
    void setTextNodeValue(ErrorString*, long nodeId, const String& value, bool* success);
    void getEventListenersForNode(ErrorString*, long nodeId, long* outNodeId, RefPtr<InspectorArray>* listenersArray);
    void addInspectedNode(ErrorString*, long nodeId);
    void performSearch(ErrorString*, const String& whitespaceTrimmedQuery, bool runSynchronously);
    void searchCanceled(ErrorString*);
    void resolveNode(ErrorString*, long nodeId, const String& objectGroup, RefPtr<InspectorValue>* result);
    void pushNodeToFrontend(ErrorString*, PassRefPtr<InspectorObject> objectId, long* nodeId);

    // Methods called from the InspectorInstrumentation.
    void setDocument(Document*);
    void releaseDanglingNodes();

    void mainFrameDOMContentLoaded();
    void loadEventFired(Document*);

    void didInsertDOMNode(Node*);
    void didRemoveDOMNode(Node*);
    void didModifyDOMAttr(Element*);
    void characterDataModified(CharacterData*);
    void didInvalidateStyleAttr(Node*);

    Node* nodeForId(long nodeId);
    long pushNodePathToFrontend(Node*);
    void pushChildNodesToFrontend(long nodeId);
    void pushNodeByPathToFrontend(ErrorString*, const String& path, long* nodeId);
    void copyNode(ErrorString*, long nodeId);
    void setDOMListener(DOMListener*);

    String documentURLString(Document*) const;

    // We represent embedded doms as a part of the same hierarchy. Hence we treat children of frame owners differently.
    // We also skip whitespace text nodes conditionally. Following methods encapsulate these specifics.
    static Node* innerFirstChild(Node*);
    static Node* innerNextSibling(Node*);
    static Node* innerPreviousSibling(Node*);
    static unsigned innerChildNodeCount(Node*);
    static Node* innerParentNode(Node*);
    static bool isWhitespace(Node*);

private:
    InspectorDOMAgent(InstrumentingAgents*, InjectedScriptHost*);

    // Node-related methods.
    typedef HashMap<RefPtr<Node>, long> NodeToIdMap;
    long bind(Node*, NodeToIdMap*);
    void unbind(Node*, NodeToIdMap*);

    Node* nodeToSelectOn(long nodeId, bool documentWide);

    bool pushDocumentToFrontend();

    bool hasBreakpoint(Node*, long type);
    void updateSubtreeBreakpoints(Node* root, uint32_t rootMask, bool value);
    void descriptionForDOMEvent(Node* target, long breakpointType, bool insertion, PassRefPtr<InspectorObject> description);

    PassRefPtr<InspectorObject> buildObjectForNode(Node*, int depth, NodeToIdMap*);
    PassRefPtr<InspectorArray> buildArrayForElementAttributes(Element*);
    PassRefPtr<InspectorArray> buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap);
    PassRefPtr<InspectorObject> buildObjectForEventListener(const RegisteredEventListener&, const AtomicString& eventType, Node*);

    void onMatchJobsTimer(Timer<InspectorDOMAgent>*);
    void reportNodesAsSearchResults(ListHashSet<Node*>& resultCollector);

    Node* nodeForPath(const String& path);
    PassRefPtr<InspectorArray> toArray(const Vector<String>& data);

    void discardBindings();

    InjectedScript injectedScriptForNode(ErrorString*, Node*);

    InstrumentingAgents* m_instrumentingAgents;
    InjectedScriptHost* m_injectedScriptHost;
    InspectorFrontend::DOM* m_frontend;
    DOMListener* m_domListener;
    NodeToIdMap m_documentNodeToIdMap;
    // Owns node mappings for dangling nodes.
    Vector<NodeToIdMap*> m_danglingNodeToIdMaps;
    HashMap<long, Node*> m_idToNode;
    HashMap<long, NodeToIdMap*> m_idToNodesMap;
    HashSet<long> m_childrenRequested;
    long m_lastNodeId;
    RefPtr<Document> m_document;
    Deque<MatchJob*> m_pendingMatchJobs;
    Timer<InspectorDOMAgent> m_matchJobsTimer;
    HashSet<RefPtr<Node> > m_searchResults;
    OwnPtr<RevalidateStyleAttributeTask> m_revalidateStyleAttrTask;
};

#endif // ENABLE(INSPECTOR)

} // namespace WebCore

#endif // !defined(InspectorDOMAgent_h)
