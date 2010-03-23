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
#include "CSSMutableStyleDeclaration.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "ContainerNode.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "CString.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentType.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHeadElement.h"
#include "InspectorFrontend.h"
#include "markup.h"
#include "MutationEvent.h"
#include "Node.h"
#include "NodeList.h"
#include "PlatformString.h"
#include "ScriptEventListener.h"
#include "ScriptObject.h"
#include "StyleSheetList.h"
#include "Text.h"

#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

InspectorDOMAgent::InspectorDOMAgent(InspectorFrontend* frontend)
    : EventListener(InspectorDOMAgentType)
    , m_frontend(frontend)
    , m_lastNodeId(1)
    , m_lastStyleId(1)
    , m_lastRuleId(1)
{
}

InspectorDOMAgent::~InspectorDOMAgent()
{
    reset();
}

void InspectorDOMAgent::reset()
{
    discardBindings();

    ListHashSet<RefPtr<Document> > copy = m_documents;
    for (ListHashSet<RefPtr<Document> >::iterator it = copy.begin(); it != copy.end(); ++it)
        stopListening((*it).get());

    ASSERT(!m_documents.size());
}

void InspectorDOMAgent::setDocument(Document* doc)
{
    if (doc == mainFrameDocument())
        return;

    reset();

    if (doc) {
        startListening(doc);
        if (doc->documentElement())
            pushDocumentToFrontend();
    } else
        m_frontend->setDocument(ScriptObject());
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
    doc->addEventListener(eventNames().loadEvent, this, true);
    m_documents.add(doc);
}

void InspectorDOMAgent::stopListening(Document* doc)
{
    if (!m_documents.contains(doc))
        return;

    doc->removeEventListener(eventNames().DOMContentLoadedEvent, this, false);
    doc->removeEventListener(eventNames().loadEvent, this, true);
    m_documents.remove(doc);
}

void InspectorDOMAgent::handleEvent(ScriptExecutionContext*, Event* event)
{
    AtomicString type = event->type();
    Node* node = event->target()->toNode();

    if (type == eventNames().DOMContentLoadedEvent) {
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
            ScriptObject value = buildObjectForNode(node, 0, &m_documentNodeToIdMap);
            Node* previousSibling = innerPreviousSibling(node);
            long prevId = previousSibling ? m_documentNodeToIdMap.get(previousSibling) : 0;
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

bool InspectorDOMAgent::pushDocumentToFrontend()
{
    Document* document = mainFrameDocument();
    if (!document)
        return false;
    if (!m_documentNodeToIdMap.contains(document))
        m_frontend->setDocument(buildObjectForNode(document, 2, &m_documentNodeToIdMap));
    return true;
}

void InspectorDOMAgent::pushChildNodesToFrontend(long nodeId)
{
    Node* node = nodeForId(nodeId);
    if (!node || (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE && node->nodeType() != Node::DOCUMENT_FRAGMENT_NODE))
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

Node* InspectorDOMAgent::nodeForPath(const String& path)
{
    // The path is of form "1,HTML,2,BODY,1,DIV"
    Node* node = mainFrameDocument();
    if (!node)
        return 0;

    Vector<String> pathTokens;
    path.split(",", false, pathTokens);
    for (size_t i = 0; i < pathTokens.size() - 1; i += 2) {
        bool success = true;
        unsigned childNumber = pathTokens[i].toUInt(&success);
        if (!success)
            return 0;
        if (childNumber >= innerChildNodeCount(node))
            return 0;

        Node* child = innerFirstChild(node);
        String childName = pathTokens[i + 1];
        for (size_t j = 0; child && j < childNumber; ++j)
            child = innerNextSibling(child);

        if (!child || child->nodeName() != childName)
            return 0;
        node = child;
    }
    return node;
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
    if (!pushDocumentToFrontend())
        return 0;

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
    EventListenerMap::const_iterator end = listenerMap.end();
    for (EventListenerMap::const_iterator iter = listenerMap.begin(); iter != end; ++iter)
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

String InspectorDOMAgent::documentURLString(Document* document) const
{
    if (!document || document->url().isNull())
        return "";
    return document->url().string();
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

    value.set("id", id);
    value.set("nodeType", node->nodeType());
    value.set("nodeName", nodeName);
    value.set("localName", localName);
    value.set("nodeValue", nodeValue);

    if (node->nodeType() == Node::ELEMENT_NODE || node->nodeType() == Node::DOCUMENT_NODE || node->nodeType() == Node::DOCUMENT_FRAGMENT_NODE) {
        int nodeCount = innerChildNodeCount(node);
        value.set("childNodeCount", nodeCount);
        ScriptArray children = buildArrayForContainerChildren(node, depth, nodesMap);
        if (children.length() > 0)
            value.set("children", children);

        if (node->nodeType() == Node::ELEMENT_NODE) {
            Element* element = static_cast<Element*>(node);
            value.set("attributes", buildArrayForElementAttributes(element));
            if (node->isFrameOwnerElement()) {
                HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(node);
                value.set("documentURL", documentURLString(frameOwner->contentDocument()));
            }
        } else if (node->nodeType() == Node::DOCUMENT_NODE) {
            Document* document = static_cast<Document*>(node);
            value.set("documentURL", documentURLString(document));
        }
    } else if (node->nodeType() == Node::DOCUMENT_TYPE_NODE) {
        DocumentType* docType = static_cast<DocumentType*>(node);
        value.set("publicId", docType->publicId());
        value.set("systemId", docType->systemId());
        value.set("internalSubset", docType->internalSubset());
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
    value.set("nodeId", pushNodePathToFrontend(node));
    value.set("listener", getEventListenerHandlerBody(node->document(), m_frontend->scriptState(), eventListener.get()));
    return value;
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

unsigned InspectorDOMAgent::innerChildNodeCount(Node* node)
{
    unsigned count = 0;
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

void InspectorDOMAgent::didInsertDOMNode(Node* node)
{
    if (isWhitespace(node))
        return;

    // We could be attaching existing subtree. Forget the bindings.
    unbind(node, &m_documentNodeToIdMap);

    Node* parent = node->parentNode();
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
}

void InspectorDOMAgent::didRemoveDOMNode(Node* node)
{
    if (isWhitespace(node))
        return;

    Node* parent = node->parentNode();
    long parentId = m_documentNodeToIdMap.get(parent);
    // If parent is not mapped yet -> ignore the event.
    if (!parentId)
        return;

    if (!m_childrenRequested.contains(parentId)) {
        // No children are mapped yet -> only notify on changes of hasChildren.
        if (innerChildNodeCount(parent) == 1)
            m_frontend->childNodeCountUpdated(parentId, 0);
    } else
        m_frontend->childNodeRemoved(parentId, m_documentNodeToIdMap.get(node));
    unbind(node, &m_documentNodeToIdMap);    
}

void InspectorDOMAgent::didModifyDOMAttr(Element* element)
{
    long id = m_documentNodeToIdMap.get(element);
    // If node is not mapped yet -> ignore the event.
    if (!id)
        return;

    m_frontend->attributesUpdated(id, buildArrayForElementAttributes(element));
}

void InspectorDOMAgent::getStyles(long callId, long nodeId, bool authorOnly)    
{
    Node* node = nodeForId(nodeId);
    if (!node || node->nodeType() != Node::ELEMENT_NODE) {
        m_frontend->didGetStyles(callId, ScriptValue::undefined());
        return;
    }
    
    DOMWindow* defaultView = node->ownerDocument()->defaultView();
    if (!defaultView) {
        m_frontend->didGetStyles(callId, ScriptValue::undefined());
        return;
    }

    Element* element = static_cast<Element*>(node);
    RefPtr<CSSStyleDeclaration> computedStyle = defaultView->getComputedStyle(element, "");

    ScriptObject result = m_frontend->newScriptObject();
    if (element->style())
        result.set("inlineStyle", buildObjectForStyle(element->style(), true));
    result.set("computedStyle", buildObjectForStyle(computedStyle.get(), false));
    result.set("matchedCSSRules", getMatchedCSSRules(element, authorOnly));
    result.set("styleAttributes", getAttributeStyles(element));
    ScriptObject currentStyle = result;
    Element* parentElement = element->parentElement();
    while (parentElement) {
        ScriptObject parentStyle = m_frontend->newScriptObject();
        currentStyle.set("parent", parentStyle);
        if (parentElement->style() && parentElement->style()->length())
            parentStyle.set("inlineStyle", buildObjectForStyle(parentElement->style(), true));
        parentStyle.set("matchedCSSRules", getMatchedCSSRules(parentElement, authorOnly));
        parentElement = parentElement->parentElement();
        currentStyle = parentStyle;
    }
    m_frontend->didGetStyles(callId, result);
}

void InspectorDOMAgent::getAllStyles(long callId)
{
    ScriptArray result = m_frontend->newScriptArray();
    unsigned counter = 0;
    for (ListHashSet<RefPtr<Document> >::iterator it = m_documents.begin(); it != m_documents.end(); ++it) {
        StyleSheetList* list = (*it)->styleSheets();
        for (unsigned i = 0; i < list->length(); ++i) {
            StyleSheet* styleSheet = list->item(i);
            if (styleSheet->isCSSStyleSheet())
                result.set(counter++, buildObjectForStyleSheet(static_cast<CSSStyleSheet*>(styleSheet)));
        }
    }
    m_frontend->didGetAllStyles(callId, result);
}

void InspectorDOMAgent::getInlineStyle(long callId, long nodeId)
{
    Node* node = nodeForId(nodeId);
    if (!node || node->nodeType() != Node::ELEMENT_NODE) {
        m_frontend->didGetInlineStyle(callId, ScriptValue::undefined());
        return;
    }
    Element* element = static_cast<Element*>(node);
    m_frontend->didGetInlineStyle(callId, buildObjectForStyle(element->style(), true));
}

void InspectorDOMAgent::getComputedStyle(long callId, long nodeId)
{
    Node* node = nodeForId(nodeId);
    if (!node || node->nodeType() != Node::ELEMENT_NODE) {
        m_frontend->didGetComputedStyle(callId, ScriptValue::undefined());
        return;
    }

    DOMWindow* defaultView = node->ownerDocument()->defaultView();
    if (!defaultView) {
        m_frontend->didGetComputedStyle(callId, ScriptValue::undefined());
        return;
    }

    Element* element = static_cast<Element*>(node);
    RefPtr<CSSStyleDeclaration> computedStyle = defaultView->getComputedStyle(element, "");
    m_frontend->didGetComputedStyle(callId, buildObjectForStyle(computedStyle.get(), false));
}

ScriptArray InspectorDOMAgent::getMatchedCSSRules(Element* element, bool authorOnly)
{
    DOMWindow* defaultView = element->ownerDocument()->defaultView();
    if (!defaultView)
        return m_frontend->newScriptArray();

    RefPtr<CSSRuleList> matchedRules = defaultView->getMatchedCSSRules(element, "", authorOnly);
    ScriptArray matchedCSSRules = m_frontend->newScriptArray();
    unsigned counter = 0;
    for (unsigned i = 0; matchedRules.get() && i < matchedRules->length(); ++i) {
        CSSRule* rule = matchedRules->item(i);
        if (rule->type() == CSSRule::STYLE_RULE)
            matchedCSSRules.set(counter++, buildObjectForRule(static_cast<CSSStyleRule*>(rule)));
    }
    return matchedCSSRules;
}

ScriptObject InspectorDOMAgent::getAttributeStyles(Element* element)
{
    ScriptObject styleAttributes = m_frontend->newScriptObject();
    NamedNodeMap* attributes = element->attributes();
    for (unsigned i = 0; attributes && i < attributes->length(); ++i) {
        Attribute* attribute = attributes->attributeItem(i);
        if (attribute->style()) {
            String attributeName = attribute->localName();
            styleAttributes.set(attributeName.utf8().data(), buildObjectForStyle(attribute->style(), true));
        }
    }
    return styleAttributes;
}

void InspectorDOMAgent::applyStyleText(long callId, long styleId, const String& styleText, const String& propertyName)
{
    IdToStyleMap::iterator it = m_idToStyle.find(styleId);
    if (it == m_idToStyle.end()) {
        m_frontend->didApplyStyleText(callId, false, ScriptValue::undefined(), m_frontend->newScriptArray());
        return;
    }

    CSSStyleDeclaration* style = it->second.get();
    int styleTextLength = styleText.length();

    RefPtr<CSSMutableStyleDeclaration> tempMutableStyle = CSSMutableStyleDeclaration::create();
    tempMutableStyle->parseDeclaration(styleText);
    CSSStyleDeclaration* tempStyle = static_cast<CSSStyleDeclaration*>(tempMutableStyle.get());

    if (tempStyle->length() || !styleTextLength) {
        ExceptionCode ec = 0;
        // The input was parsable or the user deleted everything, so remove the
        // original property from the real style declaration. If this represents
        // a shorthand remove all the longhand properties.
        if (style->getPropertyShorthand(propertyName).isEmpty()) {
            Vector<String> longhandProps = longhandProperties(style, propertyName);
            for (unsigned i = 0; !ec && i < longhandProps.size(); ++i)
                style->removeProperty(longhandProps[i], ec);
        } else
            style->removeProperty(propertyName, ec);
        if (ec) {
            m_frontend->didApplyStyleText(callId, false, ScriptValue::undefined(), m_frontend->newScriptArray());
            return;
        }
    }

    // Notify caller that the property was successfully deleted.
    if (!styleTextLength) {
        ScriptArray changedProperties = m_frontend->newScriptArray();
        changedProperties.set(0, propertyName);
        m_frontend->didApplyStyleText(callId, true, ScriptValue::undefined(), changedProperties);
        return;
    }

    if (!tempStyle->length()) {
        m_frontend->didApplyStyleText(callId, false, ScriptValue::undefined(), m_frontend->newScriptArray());
        return;
    }

    // Iterate of the properties on the test element's style declaration and
    // add them to the real style declaration. We take care to move shorthands.
    HashSet<String> foundShorthands;
    Vector<String> changedProperties;

    for (unsigned i = 0; i < tempStyle->length(); ++i) {
        String name = tempStyle->item(i);
        String shorthand = tempStyle->getPropertyShorthand(name);

        if (!shorthand.isEmpty() && foundShorthands.contains(shorthand))
            continue;

        String value;
        String priority;
        if (!shorthand.isEmpty()) {
            value = shorthandValue(tempStyle, shorthand);
            priority = shorthandPriority(tempStyle, shorthand);
            foundShorthands.add(shorthand);
            name = shorthand;
        } else {
            value = tempStyle->getPropertyValue(name);
            priority = tempStyle->getPropertyPriority(name);
        }

        // Set the property on the real style declaration.
        ExceptionCode ec = 0;
        style->setProperty(name, value, priority, ec);
        changedProperties.append(name);
    }
    m_frontend->didApplyStyleText(callId, true, buildObjectForStyle(style, true), toArray(changedProperties));
}

void InspectorDOMAgent::setStyleText(long callId, long styleId, const String& cssText)
{
    IdToStyleMap::iterator it = m_idToStyle.find(styleId);
    if (it == m_idToStyle.end()) {
        m_frontend->didSetStyleText(callId, false);
        return;
    }
    CSSStyleDeclaration* style = it->second.get();
    ExceptionCode ec = 0;
    style->setCssText(cssText, ec);
    m_frontend->didSetStyleText(callId, !ec);
}

void InspectorDOMAgent::setStyleProperty(long callId, long styleId, const String& name, const String& value)
{
    IdToStyleMap::iterator it = m_idToStyle.find(styleId);
    if (it == m_idToStyle.end()) {
        m_frontend->didSetStyleProperty(callId, false);
        return;
    }

    CSSStyleDeclaration* style = it->second.get();
    ExceptionCode ec = 0;
    style->setProperty(name, value, ec);
    m_frontend->didSetStyleProperty(callId, !ec);
}

void InspectorDOMAgent::toggleStyleEnabled(long callId, long styleId, const String& propertyName, bool disabled)
{
    IdToStyleMap::iterator it = m_idToStyle.find(styleId);
    if (it == m_idToStyle.end()) {
        m_frontend->didToggleStyleEnabled(callId, ScriptValue::undefined());
        return;
    }
    CSSStyleDeclaration* style = it->second.get();

    IdToStyleMap::iterator disabledIt = m_idToDisabledStyle.find(styleId);
    if (disabledIt == m_idToDisabledStyle.end())
        disabledIt = m_idToDisabledStyle.set(styleId, CSSMutableStyleDeclaration::create()).first;
    CSSStyleDeclaration* disabledStyle = disabledIt->second.get();

    // TODO: make sure this works with shorthands right.
    ExceptionCode ec = 0;
    if (disabled) {
        disabledStyle->setProperty(propertyName, style->getPropertyValue(propertyName), style->getPropertyPriority(propertyName), ec);
        if (!ec)
            style->removeProperty(propertyName, ec);
    } else {
        style->setProperty(propertyName, disabledStyle->getPropertyValue(propertyName), disabledStyle->getPropertyPriority(propertyName), ec);
        if (!ec)
            disabledStyle->removeProperty(propertyName, ec);
    }
    if (ec) {
        m_frontend->didToggleStyleEnabled(callId, ScriptValue::undefined());
        return;
    }
    m_frontend->didToggleStyleEnabled(callId, buildObjectForStyle(style, true));
}

void InspectorDOMAgent::setRuleSelector(long callId, long ruleId, const String& selector, long selectedNodeId)
{
    IdToRuleMap::iterator it = m_idToRule.find(ruleId);
    if (it == m_idToRule.end()) {
        m_frontend->didSetRuleSelector(callId, ScriptValue::undefined(), false);
        return;
    }

    CSSStyleRule* rule = it->second.get();
    Node* node = nodeForId(selectedNodeId);

    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    ExceptionCode ec = 0;
    styleSheet->addRule(selector, rule->style()->cssText(), ec);
    if (ec) {
        m_frontend->didSetRuleSelector(callId, ScriptValue::undefined(), false);
        return;
    }

    CSSStyleRule* newRule = static_cast<CSSStyleRule*>(styleSheet->item(styleSheet->length() - 1));
    for (unsigned i = 0; i < styleSheet->length(); ++i) {
        if (styleSheet->item(i) == rule) {
            styleSheet->deleteRule(i, ec);
            break;
        }
    }

    if (ec) {
        m_frontend->didSetRuleSelector(callId, ScriptValue::undefined(), false);
        return;
    }

    m_frontend->didSetRuleSelector(callId, buildObjectForRule(newRule), ruleAffectsNode(newRule, node));
}

void InspectorDOMAgent::addRule(long callId, const String& selector, long selectedNodeId)
{
    Node* node = nodeForId(selectedNodeId);
    if (!node) {
        m_frontend->didAddRule(callId, ScriptValue::undefined(), false);
        return;
    }

    if (!m_inspectorStyleSheet.get()) {
        Document* ownerDocument = node->ownerDocument();
        ExceptionCode ec = 0;
        RefPtr<Element> styleElement = ownerDocument->createElement("style", ec);
        if (!ec)
            styleElement->setAttribute("type", "text/css", ec);
        if (!ec)
            ownerDocument->head()->appendChild(styleElement, ec);
        if (ec) {
            m_frontend->didAddRule(callId, ScriptValue::undefined(), false);
            return;
        }
        StyleSheetList* styleSheets = ownerDocument->styleSheets();
        StyleSheet* styleSheet = styleSheets->item(styleSheets->length() - 1);
        if (!styleSheet->isCSSStyleSheet()) {
            m_frontend->didAddRule(callId, ScriptValue::undefined(), false);
            return;
        }
        m_inspectorStyleSheet = static_cast<CSSStyleSheet*>(styleSheet);
    }

    ExceptionCode ec = 0;
    m_inspectorStyleSheet->addRule(selector, "", ec);
    if (ec) {
        m_frontend->didAddRule(callId, ScriptValue::undefined(), false);
        return;
    }

    CSSStyleRule* newRule = static_cast<CSSStyleRule*>(m_inspectorStyleSheet->item(m_inspectorStyleSheet->length() - 1));
    m_frontend->didAddRule(callId, buildObjectForRule(newRule), ruleAffectsNode(newRule, node));
}

long InspectorDOMAgent::bindStyle(CSSStyleDeclaration* style)
{
    long id = m_styleToId.get(style);
    if (!id) {
        id = m_lastStyleId++;
        m_idToStyle.set(id, style);
        m_styleToId.set(style, id);
    }
    return id;
}

long InspectorDOMAgent::bindRule(CSSStyleRule* rule)
{
    long id = m_ruleToId.get(rule);
    if (!id) {
        id = m_lastRuleId++;
        m_idToRule.set(id, rule);
        m_ruleToId.set(rule, id);
    }
    return id;
}

ScriptObject InspectorDOMAgent::buildObjectForStyle(CSSStyleDeclaration* style, bool bind)
{
    ScriptObject result = m_frontend->newScriptObject();
    if (bind) {
        long styleId = bindStyle(style);
        result.set("id", styleId);

        IdToStyleMap::iterator disabledIt = m_idToDisabledStyle.find(styleId);
        if (disabledIt != m_idToDisabledStyle.end()) {
            ScriptObject disabledStyle = m_frontend->newScriptObject();
            populateObjectWithStyleProperties(disabledIt->second.get(), disabledStyle);
            result.set("disabled", disabledStyle);
        }
    }
    result.set("width", style->getPropertyValue("width"));
    result.set("height", style->getPropertyValue("height"));
    populateObjectWithStyleProperties(style, result);
    return result;
}

void InspectorDOMAgent::populateObjectWithStyleProperties(CSSStyleDeclaration* style, ScriptObject& result)
{
    ScriptArray properties = m_frontend->newScriptArray();
    ScriptObject shorthandValues = m_frontend->newScriptObject();
    result.set("properties", properties);
    result.set("shorthandValues", shorthandValues);

    HashSet<String> foundShorthands;
    for (unsigned i = 0; i < style->length(); ++i) {
        ScriptObject property = m_frontend->newScriptObject();
        String name = style->item(i);
        property.set("name", name);
        property.set("priority", style->getPropertyPriority(name));
        property.set("implicit", style->isPropertyImplicit(name));
        String shorthand =  style->getPropertyShorthand(name);
        property.set("shorthand", shorthand);
        if (!shorthand.isEmpty() && !foundShorthands.contains(shorthand)) {
            foundShorthands.add(shorthand);
            shorthandValues.set(shorthand, shorthandValue(style, shorthand));
        }
        property.set("value", style->getPropertyValue(name));
        properties.set(i, property);
    }
}

ScriptObject InspectorDOMAgent::buildObjectForStyleSheet(CSSStyleSheet* styleSheet)
{
    ScriptObject result = m_frontend->newScriptObject();
    result.set("disabled", styleSheet->disabled());
    result.set("href", styleSheet->href());
    result.set("title", styleSheet->title());
    result.set("documentElementId", m_documentNodeToIdMap.get(styleSheet->doc()));
    ScriptArray cssRules = m_frontend->newScriptArray();
    result.set("cssRules", cssRules);
    PassRefPtr<CSSRuleList> cssRuleList = CSSRuleList::create(styleSheet, true);
    if (!cssRuleList)
        return result;
    unsigned counter = 0;
    for (unsigned i = 0; i < cssRuleList->length(); ++i) {
        CSSRule* rule = cssRuleList->item(i);
        if (rule->isStyleRule())
            cssRules.set(counter++, buildObjectForRule(static_cast<CSSStyleRule*>(rule)));
    }
    return result;
}

ScriptObject InspectorDOMAgent::buildObjectForRule(CSSStyleRule* rule)
{
    CSSStyleSheet* parentStyleSheet = rule->parentStyleSheet();

    ScriptObject result = m_frontend->newScriptObject();
    result.set("selectorText", rule->selectorText());
    result.set("cssText", rule->cssText());
    result.set("sourceLine", rule->sourceLine());
    if (parentStyleSheet) {
        ScriptObject parentStyleSheetValue = m_frontend->newScriptObject();
        result.set("parentStyleSheet", parentStyleSheetValue);
        parentStyleSheetValue.set("href", parentStyleSheet->href());
    }
    bool isUserAgent = parentStyleSheet && !parentStyleSheet->ownerNode() && parentStyleSheet->href().isEmpty();
    bool isUser = parentStyleSheet && parentStyleSheet->ownerNode() && parentStyleSheet->ownerNode()->nodeName() == "#document";
    result.set("isUserAgent", isUserAgent);
    result.set("isUser", isUser);
    result.set("isViaInspector", rule->parentStyleSheet() == m_inspectorStyleSheet.get());

    // Bind editable scripts only.
    bool bind = !isUserAgent && !isUser;
    result.set("style", buildObjectForStyle(rule->style(), bind));

    if (bind)
        result.set("id", bindRule(rule));
    return result;
}

Vector<String> InspectorDOMAgent::longhandProperties(CSSStyleDeclaration* style, const String& shorthandProperty)
{
    Vector<String> properties;
    HashSet<String> foundProperties;

    for (unsigned i = 0; i < style->length(); ++i) {
        String individualProperty = style->item(i);
        if (foundProperties.contains(individualProperty) || style->getPropertyShorthand(individualProperty) != shorthandProperty)
            continue;
        foundProperties.add(individualProperty);
        properties.append(individualProperty);
    }

    return properties;
}

String InspectorDOMAgent::shorthandValue(CSSStyleDeclaration* style, const String& shorthandProperty)
{
    String value = style->getPropertyValue(shorthandProperty);
    if (value.isEmpty()) {
        // Some shorthands (like border) return a null value, so compute a shorthand value.
        // FIXME: remove this when http://bugs.webkit.org/show_bug.cgi?id=15823 is fixed.
        for (unsigned i = 0; i < style->length(); ++i) {
            String individualProperty = style->item(i);
            if (style->getPropertyShorthand(individualProperty) != shorthandProperty)
                continue;
            if (style->isPropertyImplicit(individualProperty))
                continue;
            String individualValue = style->getPropertyValue(individualProperty);
            if (individualValue == "initial")
                continue;
            if (value.length())
                value.append(" ");
            value.append(individualValue);
        }
    }
    return value;
}

String InspectorDOMAgent::shorthandPriority(CSSStyleDeclaration* style, const String& shorthandProperty)
{
    String priority = style->getPropertyPriority(shorthandProperty);
    if (priority.isEmpty()) {
        for (unsigned i = 0; i < style->length(); ++i) {
            String individualProperty = style->item(i);
            if (style->getPropertyShorthand(individualProperty) != shorthandProperty)
                continue;
            priority = style->getPropertyPriority(individualProperty);
            break;
        }
    }
    return priority;
}

bool InspectorDOMAgent::ruleAffectsNode(CSSStyleRule* rule, Node* node)
{
    if (!node)
        return false;
    ExceptionCode ec = 0;
    RefPtr<NodeList> nodes = node->ownerDocument()->querySelectorAll(rule->selectorText(), ec);
    if (ec)
        return false;
    for (unsigned i = 0; i < nodes->length(); ++i) {
        if (nodes->item(i) == node)
            return true;
    }
    return false;
}

ScriptArray InspectorDOMAgent::toArray(const Vector<String>& data)
{
    ScriptArray result = m_frontend->newScriptArray();
    for (unsigned i = 0; i < data.size(); ++i)
        result.set(i, data[i]);
    return result;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
