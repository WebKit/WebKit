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

#include "config.h"
#include "InspectorDOMAgent.h"

#include "AtomicString.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorFrontend.h"
#include "markup.h"
#include "MutationEvent.h"
#include "Node.h"
#include "NodeList.h"
#include "PlatformString.h"
#include "ScriptObject.h"
#include "Text.h"

#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

InspectorDOMAgent::InspectorDOMAgent(InspectorFrontend* frontend)
    : m_frontend(frontend)
    , m_lastNodeId(1)
{
}

InspectorDOMAgent::~InspectorDOMAgent()
{
    setDocument(0);
}

void InspectorDOMAgent::setDocument(Document* doc)
{
    if (doc == mainFrameDocument())
        return;

    ListHashSet<RefPtr<Document> > copy = m_documents;
    for (ListHashSet<RefPtr<Document> >::iterator it = copy.begin(); it != copy.end(); ++it)
        stopListening((*it).get());

    ASSERT(!m_documents.size());

    if (doc) {
        startListening(doc);
        if (doc->documentElement()) {
            pushDocumentElementToFrontend();
        }
    } else {
        discardBindings();
    }
}

void InspectorDOMAgent::startListening(Document* doc)
{
    if (m_documents.contains(doc))
        return;

    doc->addEventListener(eventNames().DOMContentLoadedEvent, this, false);
    doc->addEventListener(eventNames().DOMNodeInsertedEvent, this, false);
    doc->addEventListener(eventNames().DOMNodeRemovedEvent, this, false);
    doc->addEventListener(eventNames().DOMNodeRemovedFromDocumentEvent, this, true);
    doc->addEventListener(eventNames().DOMAttrModifiedEvent, this, false);
    m_documents.add(doc);
}

void InspectorDOMAgent::stopListening(Document* doc)
{
    if (!m_documents.contains(doc))
        return;

    doc->removeEventListener(eventNames().DOMContentLoadedEvent, this, false);
    doc->removeEventListener(eventNames().DOMNodeInsertedEvent, this, false);
    doc->removeEventListener(eventNames().DOMNodeRemovedEvent, this, false);
    doc->removeEventListener(eventNames().DOMNodeRemovedFromDocumentEvent, this, true);
    doc->removeEventListener(eventNames().DOMAttrModifiedEvent, this, false);
    m_documents.remove(doc);
}

void InspectorDOMAgent::handleEvent(Event* event, bool)
{
    AtomicString type = event->type();
    Node* node = event->target()->toNode();

    // Remove mapping entry if necessary.
    if (type == eventNames().DOMNodeRemovedFromDocumentEvent) {
        unbind(node);
        return;
    }

    if (type == eventNames().DOMAttrModifiedEvent) {
        long id = idForNode(node);
        // If node is not mapped yet -> ignore the event.
        if (!id)
            return;

        Element* element = static_cast<Element*>(node);
        m_frontend->attributesUpdated(id, buildArrayForElementAttributes(element));
    } else if (type == eventNames().DOMNodeInsertedEvent) {
        if (isWhitespace(node))
            return;

        Node* parent = static_cast<MutationEvent*>(event)->relatedNode();
        long parentId = idForNode(parent);
        // Return if parent is not mapped yet.
        if (!parentId)
            return;

        if (!m_childrenRequested.contains(parentId)) {
            // No children are mapped yet -> only notify on changes of hasChildren.
            m_frontend->hasChildrenUpdated(parentId, true);
        } else {
            // Children have been requested -> return value of a new child.
            long prevId = idForNode(innerPreviousSibling(node));

            ScriptObject value = buildObjectForNode(node, 0);
            m_frontend->childNodeInserted(parentId, prevId, value);
        }
    } else if (type == eventNames().DOMNodeRemovedEvent) {
        if (isWhitespace(node))
            return;

        Node* parent = static_cast<MutationEvent*>(event)->relatedNode();
        long parentId = idForNode(parent);
        // If parent is not mapped yet -> ignore the event.
        if (!parentId)
            return;

        if (!m_childrenRequested.contains(parentId)) {
            // No children are mapped yet -> only notify on changes of hasChildren.
            if (innerChildNodeCount(parent) == 1)
                m_frontend->hasChildrenUpdated(parentId, false);
        } else {
            m_frontend->childNodeRemoved(parentId, idForNode(node));
        }
    } else if (type == eventNames().DOMContentLoadedEvent) {
        // Re-push document once it is loaded.
        discardBindings();
        pushDocumentElementToFrontend();
    }
}

long InspectorDOMAgent::bind(Node* node)
{
    HashMap<Node*, long>::iterator it = m_nodeToId.find(node);
    if (it != m_nodeToId.end())
        return it->second;
    long id = m_lastNodeId++;
    m_nodeToId.set(node, id);
    m_idToNode.set(id, node);
    return id;
}

void InspectorDOMAgent::unbind(Node* node)
{
    if (node->isFrameOwnerElement()) {
        const HTMLFrameOwnerElement* frameOwner = static_cast<const HTMLFrameOwnerElement*>(node);
        stopListening(frameOwner->contentDocument());
    }

    HashMap<Node*, long>::iterator it = m_nodeToId.find(node);
    if (it != m_nodeToId.end()) {
        m_idToNode.remove(m_idToNode.find(it->second));
        m_childrenRequested.remove(m_childrenRequested.find(it->second));
        m_nodeToId.remove(it);
    }
}

void InspectorDOMAgent::pushDocumentElementToFrontend()
{
    Element* docElem = mainFrameDocument()->documentElement();
    if (!m_nodeToId.contains(docElem))
        m_frontend->setDocumentElement(buildObjectForNode(docElem, 0));
}

void InspectorDOMAgent::pushChildNodesToFrontend(long elementId)
{
    Node* node = nodeForId(elementId);
    if (!node || (node->nodeType() != Node::ELEMENT_NODE))
        return;
    if (m_childrenRequested.contains(elementId))
        return;

    Element* element = static_cast<Element*>(node);
    ScriptArray children = buildArrayForElementChildren(element, 1);
    m_childrenRequested.add(elementId);
    m_frontend->setChildNodes(elementId, children);
}

void InspectorDOMAgent::discardBindings()
{
    m_nodeToId.clear();
    m_idToNode.clear();
    m_childrenRequested.clear();
}

Node* InspectorDOMAgent::nodeForId(long id)
{
    HashMap<long, Node*>::iterator it = m_idToNode.find(id);
    if (it != m_idToNode.end())
        return it->second;
    return 0;
}

long InspectorDOMAgent::idForNode(Node* node)
{
    if (!node)
        return 0;
    HashMap<Node*, long>::iterator it = m_nodeToId.find(node);
    if (it != m_nodeToId.end())
        return it->second;
    return 0;
}

void InspectorDOMAgent::getChildNodes(long callId, long elementId)
{
    pushChildNodesToFrontend(elementId);
    m_frontend->didGetChildNodes(callId);
}

long InspectorDOMAgent::pushNodePathToFrontend(Node* nodeToPush)
{
    ASSERT(nodeToPush);  // Invalid input

    // If we are sending information to the client that is currently being created. Send root node first.
    pushDocumentElementToFrontend();

    // Return id in case the node is known.
    long result = idForNode(nodeToPush);
    if (result)
        return result;

    Element* element = innerParentElement(nodeToPush);
    ASSERT(element);  // Node is detached or is a document itself

    Vector<Element*> path;
    while (element && !idForNode(element)) {
        path.append(element);
        element = innerParentElement(element);
    }

    // element is known to the client
    ASSERT(element);
    path.append(element);
    for (int i = path.size() - 1; i >= 0; --i) {
        long nodeId = idForNode(path.at(i));
        ASSERT(nodeId);
        pushChildNodesToFrontend(nodeId);
    }
    return idForNode(nodeToPush);
}

void InspectorDOMAgent::setAttribute(long callId, long elementId, const String& name, const String& value)
{
    Node* node = nodeForId(elementId);
    if (node && (node->nodeType() == Node::ELEMENT_NODE)) {
        Element* element = static_cast<Element*>(node);
        ExceptionCode ec = 0;
        element->setAttribute(name, value, ec);
        m_frontend->didApplyDomChange(callId, ec == 0);
    } else {
        m_frontend->didApplyDomChange(callId, false);
    }
}

void InspectorDOMAgent::removeAttribute(long callId, long elementId, const String& name)
{
    Node* node = nodeForId(elementId);
    if (node && (node->nodeType() == Node::ELEMENT_NODE)) {
        Element* element = static_cast<Element*>(node);
        ExceptionCode ec = 0;
        element->removeAttribute(name, ec);
        m_frontend->didApplyDomChange(callId, ec == 0);
    } else {
        m_frontend->didApplyDomChange(callId, false);
    }
}

void InspectorDOMAgent::setTextNodeValue(long callId, long elementId, const String& value)
{
    Node* node = nodeForId(elementId);
    if (node && (node->nodeType() == Node::TEXT_NODE)) {
        Text* text_node = static_cast<Text*>(node);
        ExceptionCode ec = 0;
        text_node->replaceWholeText(value, ec);
        m_frontend->didApplyDomChange(callId, ec == 0);
    } else {
        m_frontend->didApplyDomChange(callId, false);
    }
}

ScriptObject InspectorDOMAgent::buildObjectForNode(Node* node, int depth)
{
    ScriptObject value = m_frontend->newScriptObject();

    long id = bind(node);
    String nodeName;
    String nodeValue;

    switch (node->nodeType()) {
        case Node::TEXT_NODE:
        case Node::COMMENT_NODE:
            nodeValue = node->nodeValue();
            break;
        case Node::ATTRIBUTE_NODE:
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
            break;
        case Node::ELEMENT_NODE:
        default:
            nodeName = node->nodeName();
            break;
    }

    value.set("id", static_cast<int>(id));
    value.set("nodeType", node->nodeType());
    value.set("nodeName", nodeName);
    value.set("nodeValue", nodeValue);

    if (node->nodeType() == Node::ELEMENT_NODE) {
        Element* element = static_cast<Element*>(node);
        value.set("attributes", buildArrayForElementAttributes(element));
        int nodeCount = innerChildNodeCount(element);
        value.set("childNodeCount", nodeCount);

        ScriptArray children = buildArrayForElementChildren(element, depth);
        if (children.length() > 0)
            value.set("children", children);
    }
    return value;
}

ScriptArray InspectorDOMAgent::buildArrayForElementAttributes(Element* element)
{
    ScriptArray attributesValue = m_frontend->newScriptArray();
    // Go through all attributes and serialize them.
    const NamedNodeMap* attrMap = element->attributes(true);
    if (!attrMap)
        return attributesValue;
    unsigned numAttrs = attrMap->length();
    int index = 0;
    for (unsigned i = 0; i < numAttrs; ++i) {
        // Add attribute pair
        const Attribute *attribute = attrMap->attributeItem(i);
        attributesValue.set(index++, attribute->name().toString());
        attributesValue.set(index++, attribute->value());
    }
    return attributesValue;
}

ScriptArray InspectorDOMAgent::buildArrayForElementChildren(Element* element, int depth)
{
    ScriptArray children = m_frontend->newScriptArray();
    if (depth == 0) {
        int index = 0;
        // Special case the_only text child.
        if (innerChildNodeCount(element) == 1) {
            Node *child = innerFirstChild(element);
            if (child->nodeType() == Node::TEXT_NODE)
                children.set(index++, buildObjectForNode(child, 0));
        }
        return children;
    } else if (depth > 0) {
        depth--;
    }

    int index = 0;
    for (Node *child = innerFirstChild(element); child; child = innerNextSibling(child))
        children.set(index++, buildObjectForNode(child, depth));
    return children;
}

Node* InspectorDOMAgent::innerFirstChild(Node* node)
{
    if (node->isFrameOwnerElement()) {
        HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(node);
        Document* doc = frameOwner->contentDocument();
        if (doc) {
            startListening(doc);
            return doc->firstChild();
        }
    }
    node = node->firstChild();
    while (isWhitespace(node))
        node = node->nextSibling();
    return node;
}

Node* InspectorDOMAgent::innerNextSibling(Node* node)
{
    do {
        node = node->nextSibling();
    } while (isWhitespace(node));
    return node;
}

Node* InspectorDOMAgent::innerPreviousSibling(Node* node)
{
    do {
        node = node->previousSibling();
    } while (isWhitespace(node));
    return node;
}

int InspectorDOMAgent::innerChildNodeCount(Node* node)
{
    int count = 0;
    Node* child = innerFirstChild(node);
    while (child) {
        count++;
        child = innerNextSibling(child);
    }
    return count;
}

Element* InspectorDOMAgent::innerParentElement(Node* node)
{
    Element* element = node->parentElement();
    if (!element)
        return node->ownerDocument()->ownerElement();
    return element;
}

bool InspectorDOMAgent::isWhitespace(Node* node)
{
    //TODO: pull ignoreWhitespace setting from the frontend and use here.
    return node && node->nodeType() == Node::TEXT_NODE && node->nodeValue().stripWhiteSpace().length() == 0;
}

Document* InspectorDOMAgent::mainFrameDocument()
{
    ListHashSet<RefPtr<Document> >::iterator it = m_documents.begin();
    if (it != m_documents.end())
        return it->get();
    return 0;
}

} // namespace WebCore
