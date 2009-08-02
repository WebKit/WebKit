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

#include "EventListener.h"
#include "ScriptArray.h"
#include "ScriptObject.h"
#include "ScriptState.h"

#include <wtf/ListHashSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
    class Element;
    class Event;
    class Document;
    class InspectorFrontend;
    class NameNodeMap;
    class Node;
    class Page;

    class InspectorDOMAgent : public EventListener {
    public:
        InspectorDOMAgent(InspectorFrontend* frontend);
        ~InspectorDOMAgent();

        // Methods called from the frontend.
        void getChildNodes(long callId, long elementId);
        void setAttribute(long callId, long elementId, const String& name, const String& value);
        void removeAttribute(long callId, long elementId, const String& name);
        void setTextNodeValue(long callId, long elementId, const String& value);

        // Methods called from the InspectorController.
        void setDocument(Document* document);

        Node* nodeForId(long nodeId);
        long idForNode(Node* node);
        long pushNodePathToFrontend(Node* node);

   private:
        void startListening(Document* document);
        void stopListening(Document* document);

        virtual void handleEvent(Event* event, bool isWindowEvent);

        long bind(Node* node);
        void unbind(Node* node);

        void pushDocumentElementToFrontend();
        void pushChildNodesToFrontend(long elementId);

        ScriptObject buildObjectForNode(Node* node, int depth);
        ScriptArray buildArrayForElementAttributes(Element* elemen);
        ScriptArray buildArrayForElementChildren(Element* element, int depth);

        // We represent embedded doms as a part of the same hierarchy. Hence we treat children of frame owners differently.
        // We also skip whitespace text nodes conditionally. Following methods encapsulate these specifics.
        Node* innerFirstChild(Node* node);
        Node* innerNextSibling(Node* node);
        Node* innerPreviousSibling(Node* node);
        int innerChildNodeCount(Node* node);
        Element* innerParentElement(Node* node);
        bool isWhitespace(Node* node);

        Document* mainFrameDocument();
        void discardBindings();

        InspectorFrontend* m_frontend;
        HashMap<Node*, long> m_nodeToId;
        HashMap<long, Node*> m_idToNode;
        HashSet<long> m_childrenRequested;
        long m_lastNodeId;
        ListHashSet<RefPtr<Document> > m_documents;
        RefPtr<EventListener> m_eventListener;
    };


} // namespace WebCore

#endif // !defined(InspectorDOMAgent_h)
