/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

#if ENABLE(INSPECTOR)

#include "AtomicString.h"
#include "ContainerNode.h"
#include "Cookie.h"
#include "CookieJar.h"
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
#include "ScriptEventListener.h"
#include "ScriptObject.h"
#include "Text.h"

#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

InspectorDOMAgent::InspectorDOMAgent(InspectorFrontend* frontend)
    : EventListener(InspectorDOMAgentType)
    , m_frontend(frontend)
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
    discardBindings();

    ListHashSet<RefPtr<Document> > copy = m_documents;
    for (ListHashSet<RefPtr<Document> >::iterator it = copy.begin(); it != copy.end(); ++it)
        stopListening((*it).get());

    ASSERT(!m_documents.size());

    if (doc) {
        startListening(doc);
        if (doc->documentElement()) {
            pushDocumentToFrontend();
        }
    }
}

void InspectorDOMAgent::releaseDanglingNodes()
{
    deleteAllValues(m_danglingNodeToIdMaps);
    m_danglingNodeToIdMaps.clear();
}

void InspectorDOMAgent::startListening(Document* doc)
{
    if (m_documents.contains(doc))
        return;

    doc->addEventListener(eventNames().DOMContentLoadedEvent, this, false);
    doc->addEventListener(eventNames().DOMNodeInsertedEvent, this, false);
    doc->addEventListener(eventNames().DOMNodeRemovedEvent, this, false);
    doc->addEventListener(eventNames().DOMAttrModifiedEvent, this, false);
    doc->addEventListener(eventNames().loadEvent, this, true);
    m_documents.add(doc);
}

void InspectorDOMAgent::stopListening(Document* doc)
{
    if (!m_documents.contains(doc))
        return;

    doc->removeEventListener(eventNames().DOMContentLoadedEvent, this, false);
    doc->removeEventListener(eventNames().DOMNodeInsertedEvent, this, false);
    doc->removeEventListener(eventNames().DOMNodeRemovedEvent, this, false);
    doc->removeEventListener(eventNames().DOMAttrModifiedEvent, this, false);
    doc->removeEventListener(eventNames().loadEvent, this, true);
    m_documents.remove(doc);
}

void InspectorDOMAgent::handleEvent(ScriptExecutionContext*, Event* event)
{
    AtomicString type = event->type();
    Node* node = event->target()->toNode();

    if (type == eventNames().DOMAttrModifiedEvent) {
        long id = m_documentNodeToIdMap.get(node);
        // If node is not mapped yet -> ignore the event.
        if (!id)
            return;

        Element* element = static_cast<Element*>(node);
        m_frontend->attributesUpdated(id, buildArrayForElementAttributes(element));
    } else if (type == eventNames().DOMNodeInsertedEvent) {
        if (isWhitespace(node))
            return;

        // We could be attaching existing subtree. Forget the bindings.
        unbind(node, &m_documentNodeToIdMap);

        Node* parent = static_cast<MutationEvent*>(event)->relatedNode();
        long parentId = m_documentNodeToIdMap.get(parent);
        // Return if parent is not mapped yet.
        if (!parentId)
            return;

        if (!m_childrenRequested.contains(parentId)) {
            // No children are mapped yet -> only notify on changes of hasChildren.
            m_frontend->childNodeCountUpdated(parentId, innerChildNodeCount(parent));
        } else {
            // Children have been requested -> return value of a new child.
            Node* prevSibling = innerPreviousSibling(node);
            long prevId = prevSibling ? m_documentNodeToIdMap.get(prevSibling) : 0;
            ScriptObject value = buildObjectForNode(node, 0, &m_documentNodeToIdMap);
            m_frontend->childNodeInserted(parentId, prevId, value);
        }
    } else if (type == eventNames().DOMNodeRemovedEvent) {
        if (isWhitespace(node))
            return;

        Node* parent = static_cast<MutationEvent*>(event)->relatedNode();
        long parentId = m_documentNodeToIdMap.get(parent);
        // If parent is not mapped yet -> ignore the event.
        if (!parentId)
            return;

        if (!m_childrenRequested.contains(parentId)) {
            // No children are mapped yet -> only notify on changes of hasChildren.
            if (innerChildNodeCount(parent) == 1)
                m_frontend->childNodeCountUpdated(parentId, 0);
        } else {
            m_frontend->childNodeRemoved(parentId, m_documentNodeToIdMap.get(node));
        }
        unbind(node, &m_documentNodeToIdMap);
    } else if (type == eventNames().DOMContentLoadedEvent) {
        // Re-push document once it is loaded.
        discardBindings();
        pushDocumentToFrontend();
    } else if (type == eventNames().loadEvent) {
        long frameOwnerId = m_documentNodeToIdMap.get(node);
        if (!frameOwnerId)
            return;

        if (!m_childrenRequested.contains(frameOwnerId)) {
            // No children are mapped yet -> only notify on changes of hasChildren.
            m_frontend->childNodeCountUpdated(frameOwnerId, innerChildNodeCount(node));
        } else {
            // Re-add frame owner element together with its new children.
            long parentId = m_documentNodeToIdMap.get(innerParentNode(node));
            m_frontend->childNodeRemoved(parentId, frameOwnerId);
            long prevId = m_documentNodeToIdMap.get(innerPreviousSibling(node));
            ScriptObject value = buildObjectForNode(node, 0, &m_documentNodeToIdMap);
            m_frontend->childNodeInserted(parentId, prevId, value);
            // Invalidate children requested flag for the element.
            m_childrenRequested.remove(m_childrenRequested.find(frameOwnerId));
        }
    }
}

long InspectorDOMAgent::bind(Node* node, NodeToIdMap* nodesMap)
{
    long id = nodesMap->get(node);
    if (id)
        return id;
    id = m_lastNodeId++;
    nodesMap->set(node, id);
    m_idToNode.set(id, node);
    m_idToNodesMap.set(id, nodesMap);
    return id;
}

void InspectorDOMAgent::unbind(Node* node, NodeToIdMap* nodesMap)
{
    if (node->isFrameOwnerElement()) {
        const HTMLFrameOwnerElement* frameOwner = static_cast<const HTMLFrameOwnerElement*>(node);
        stopListening(frameOwner->contentDocument());
    }

    int id = nodesMap->get(node);
    if (!id)
        return;
    m_idToNode.remove(id);
    nodesMap->remove(node);
    bool childrenRequested = m_childrenRequested.contains(id);
    if (childrenRequested) {
        // Unbind subtree known to client recursively.
        m_childrenRequested.remove(id);
        Node* child = innerFirstChild(node);
        while (child) {
            unbind(child, nodesMap);
            child = innerNextSibling(child);
        }
    }
}

void InspectorDOMAgent::pushDocumentToFrontend()
{
    Document* document = mainFrameDocument();
    if (!m_documentNodeToIdMap.contains(document))
        m_frontend->setDocument(buildObjectForNode(document, 2, &m_documentNodeToIdMap));
}

void InspectorDOMAgent::pushChildNodesToFrontend(long nodeId)
{
    Node* node = nodeForId(nodeId);
    if (!node || (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE))
        return;
    if (m_childrenRequested.contains(nodeId))
        return;

    NodeToIdMap* nodeMap = m_idToNodesMap.get(nodeId);
    ScriptArray children = buildArrayForContainerChildren(node, 1, nodeMap);
    m_childrenRequested.add(nodeId);
    m_frontend->setChildNodes(nodeId, children);
}

void InspectorDOMAgent::discardBindings()
{
    m_documentNodeToIdMap.clear();
    m_idToNode.clear();
    releaseDanglingNodes();
    m_childrenRequested.clear();
}

Node* InspectorDOMAgent::nodeForId(long id)
{
    if (!id)
        return 0;

    HashMap<long, Node*>::iterator it = m_idToNode.find(id);
    if (it != m_idToNode.end())
        return it->second;
    return 0;
}

void InspectorDOMAgent::getChildNodes(long callId, long nodeId)
{
    pushChildNodesToFrontend(nodeId);
    m_frontend->didGetChildNodes(callId);
}

long InspectorDOMAgent::pushNodePathToFrontend(Node* nodeToPush)
{
    ASSERT(nodeToPush);  // Invalid input

    // If we are sending information to the client that is currently being created. Send root node first.
    pushDocumentToFrontend();

    // Return id in case the node is known.
    long result = m_documentNodeToIdMap.get(nodeToPush);
    if (result)
        return result;

    Node* node = nodeToPush;
    Vector<Node*> path;
    NodeToIdMap* danglingMap = 0;
    while (true) {
        Node* parent = innerParentNode(node);
        if (!parent) {
            // Node being pushed is detached -> push subtree root.
            danglingMap = new NodeToIdMap();
            m_danglingNodeToIdMaps.append(danglingMap);
            m_frontend->setDetachedRoot(buildObjectForNode(node, 0, danglingMap));
            break;
        } else {
            path.append(parent);
            if (m_documentNodeToIdMap.get(parent))
                break;
            else
                node = parent;
        }
    }

    NodeToIdMap* map = danglingMap ? danglingMap : &m_documentNodeToIdMap;
    for (int i = path.size() - 1; i >= 0; --i) {
        long nodeId = map->get(path.at(i));
        ASSERT(nodeId);
        pushChildNodesToFrontend(nodeId);
    }
    return map->get(nodeToPush);
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

void InspectorDOMAgent::setTextNodeValue(long callId, long nodeId, const String& value)
{
    Node* node = nodeForId(nodeId);
    if (node && (node->nodeType() == Node::TEXT_NODE)) {
        Text* text_node = static_cast<Text*>(node);
        ExceptionCode ec = 0;
        text_node->replaceWholeText(value, ec);
        m_frontend->didApplyDomChange(callId, ec == 0);
    } else {
        m_frontend->didApplyDomChange(callId, false);
    }
}

void InspectorDOMAgent::getEventListenersForNode(long callId, long nodeId)
{
    Node* node = nodeForId(nodeId);
    ScriptArray listenersArray = m_frontend->newScriptArray();
    unsigned counter = 0;
    EventTargetData* d;

    // Quick break if a null node or no listeners at all
    if (!node || !(d = node->eventTargetData())) {
        m_frontend->didGetEventListenersForNode(callId, nodeId, listenersArray);
        return;
    }

    // Get the list of event types this Node is concerned with
    Vector<AtomicString> eventTypes;
    const EventListenerMap& listenerMap = d->eventListenerMap;
    HashMap<AtomicString, EventListenerVector>::const_iterator end = listenerMap.end();
    for (HashMap<AtomicString, EventListenerVector>::const_iterator iter = listenerMap.begin(); iter != end; ++iter)
        eventTypes.append(iter->first);

    // Quick break if no useful listeners
    size_t eventTypesLength = eventTypes.size();
    if (eventTypesLength == 0) {
        m_frontend->didGetEventListenersForNode(callId, nodeId, listenersArray);
        return;
    }

    // The Node's Event Ancestors (not including self)
    Vector<RefPtr<ContainerNode> > ancestors;
    node->eventAncestors(ancestors);

    // Nodes and their Listeners for the concerned event types (order is top to bottom)
    Vector<EventListenerInfo> eventInformation;
    for (size_t i = ancestors.size(); i; --i) {
        ContainerNode* ancestor = ancestors[i - 1].get();
        for (size_t j = 0; j < eventTypesLength; ++j) {
            AtomicString& type = eventTypes[j];
            if (ancestor->hasEventListeners(type))
                eventInformation.append(EventListenerInfo(static_cast<Node*>(ancestor), type, ancestor->getEventListeners(type)));
        }
    }

    // Insert the Current Node at the end of that list (last in capturing, first in bubbling)
    for (size_t i = 0; i < eventTypesLength; ++i) {
        const AtomicString& type = eventTypes[i];
        eventInformation.append(EventListenerInfo(node, type, node->getEventListeners(type)));
    }

    // Get Capturing Listeners (in this order)
    size_t eventInformationLength = eventInformation.size();
    for (size_t i = 0; i < eventInformationLength; ++i) {
        const EventListenerInfo& info = eventInformation[i];
        const EventListenerVector& vector = info.eventListenerVector;
        for (size_t j = 0; j < vector.size(); ++j) {
            const RegisteredEventListener& listener = vector[j];
            if (listener.useCapture)
                listenersArray.set(counter++, buildObjectForEventListener(listener, info.eventType, info.node));
        }
    }

    // Get Bubbling Listeners (reverse order)
    for (size_t i = eventInformationLength; i; --i) {
        const EventListenerInfo& info = eventInformation[i - 1];
        const EventListenerVector& vector = info.eventListenerVector;
        for (size_t j = 0; j < vector.size(); ++j) {
            const RegisteredEventListener& listener = vector[j];
            if (!listener.useCapture)
                listenersArray.set(counter++, buildObjectForEventListener(listener, info.eventType, info.node));
        }
    }

    m_frontend->didGetEventListenersForNode(callId, nodeId, listenersArray);
}

void InspectorDOMAgent::getCookies(long callId)
{
    Document* doc = mainFrameDocument();
    Vector<Cookie> cookiesList;
    bool isImplemented = getRawCookies(doc, doc->cookieURL(), cookiesList);

    if (!isImplemented)
        m_frontend->didGetCookies(callId, m_frontend->newScriptArray(), doc->cookie());
    else
        m_frontend->didGetCookies(callId, buildArrayForCookies(cookiesList), String());
}

ScriptObject InspectorDOMAgent::buildObjectForNode(Node* node, int depth, NodeToIdMap* nodesMap)
{
    ScriptObject value = m_frontend->newScriptObject();

    long id = bind(node, nodesMap);
    String nodeName;
    String localName;
    String nodeValue;

    switch (node->nodeType()) {
        case Node::TEXT_NODE:
        case Node::COMMENT_NODE:
            nodeValue = node->nodeValue();
            break;
        case Node::ATTRIBUTE_NODE:
            localName = node->localName();
            break;
        case Node::DOCUMENT_FRAGMENT_NODE:
            break;
        case Node::DOCUMENT_NODE:
        case Node::ELEMENT_NODE:
        default:
            nodeName = node->nodeName();
            localName = node->localName();
            break;
    }

    value.set("id", static_cast<int>(id));
    value.set("nodeType", node->nodeType());
    value.set("nodeName", nodeName);
    value.set("localName", localName);
    value.set("nodeValue", nodeValue);

    if (node->nodeType() == Node::ELEMENT_NODE) {
        Element* element = static_cast<Element*>(node);
        value.set("attributes", buildArrayForElementAttributes(element));
    }
    if (node->nodeType() == Node::ELEMENT_NODE || node->nodeType() == Node::DOCUMENT_NODE) {
        int nodeCount = innerChildNodeCount(node);
        value.set("childNodeCount", nodeCount);
        ScriptArray children = buildArrayForContainerChildren(node, depth, nodesMap);
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

ScriptArray InspectorDOMAgent::buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap)
{
    ScriptArray children = m_frontend->newScriptArray();
    if (depth == 0) {
        int index = 0;
        // Special case the_only text child.
        if (innerChildNodeCount(container) == 1) {
            Node *child = innerFirstChild(container);
            if (child->nodeType() == Node::TEXT_NODE)
                children.set(index++, buildObjectForNode(child, 0, nodesMap));
        }
        return children;
    } else if (depth > 0) {
        depth--;
    }

    int index = 0;
    for (Node *child = innerFirstChild(container); child; child = innerNextSibling(child))
        children.set(index++, buildObjectForNode(child, depth, nodesMap));
    return children;
}

ScriptObject InspectorDOMAgent::buildObjectForEventListener(const RegisteredEventListener& registeredEventListener, const AtomicString& eventType, Node* node)
{
    RefPtr<EventListener> eventListener = registeredEventListener.listener;
    ScriptObject value = m_frontend->newScriptObject();
    value.set("type", eventType);
    value.set("useCapture", registeredEventListener.useCapture);
    value.set("isAttribute", eventListener->isAttribute());
    value.set("nodeId", static_cast<long long>(pushNodePathToFrontend(node)));
    value.set("listener", getEventListenerHandlerBody(m_frontend->scriptState(), eventListener.get()));
    return value;
}

ScriptObject InspectorDOMAgent::buildObjectForCookie(const Cookie& cookie)
{
    ScriptObject value = m_frontend->newScriptObject();
    value.set("name", cookie.name);
    value.set("value", cookie.value);
    value.set("domain", cookie.domain);
    value.set("path", cookie.path);
    value.set("expires", cookie.expires);
    value.set("size", static_cast<int>(cookie.name.length() + cookie.value.length()));
    value.set("httpOnly", cookie.httpOnly);
    value.set("secure", cookie.secure);
    value.set("session", cookie.session);
    return value;
}

ScriptArray InspectorDOMAgent::buildArrayForCookies(const Vector<Cookie>& cookiesList)
{
    ScriptArray cookies = m_frontend->newScriptArray();
    unsigned length = cookiesList.size();
    for (unsigned i = 0; i < length; ++i) {
        const Cookie& cookie = cookiesList[i];
        cookies.set(i, buildObjectForCookie(cookie));
    }
    return cookies;
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

Node* InspectorDOMAgent::innerParentNode(Node* node)
{
    Node* parent = node->parentNode();
    if (parent && parent->nodeType() == Node::DOCUMENT_NODE)
        return static_cast<Document*>(parent)->ownerElement();
    return parent;
}

bool InspectorDOMAgent::isWhitespace(Node* node)
{
    //TODO: pull ignoreWhitespace setting from the frontend and use here.
    return node && node->nodeType() == Node::TEXT_NODE && node->nodeValue().stripWhiteSpace().length() == 0;
}

Document* InspectorDOMAgent::mainFrameDocument() const
{
    ListHashSet<RefPtr<Document> >::const_iterator it = m_documents.begin();
    if (it != m_documents.end())
        return it->get();
    return 0;
}

bool InspectorDOMAgent::operator==(const EventListener& listener)
{
    if (const InspectorDOMAgent* inspectorDOMAgentListener = InspectorDOMAgent::cast(&listener))
        return mainFrameDocument() == inspectorDOMAgentListener->mainFrameDocument();
    return false;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
