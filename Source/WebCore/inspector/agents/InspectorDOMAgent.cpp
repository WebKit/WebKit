/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "AXObjectCache.h"
#include "AccessibilityNodeObject.h"
#include "Attr.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CharacterData.h"
#include "CommandLineAPIHost.h"
#include "ComposedTreeIterator.h"
#include "ContainerNode.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DOMEditor.h"
#include "DOMException.h"
#include "DOMPatchSupport.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentType.h"
#include "Editing.h"
#include "Element.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "FullscreenManager.h"
#include "HTMLElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLScriptElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTemplateElement.h"
#include "HTMLVideoElement.h"
#include "HitTestResult.h"
#include "InspectorCSSAgent.h"
#include "InspectorClient.h"
#include "InspectorController.h"
#include "InspectorHistory.h"
#include "InspectorNodeFinder.h"
#include "InspectorOverlay.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "JSDOMBindingSecurity.h"
#include "JSEventListener.h"
#include "JSNode.h"
#include "MutationEvent.h"
#include "Node.h"
#include "NodeList.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PseudoElement.h"
#include "RenderStyle.h"
#include "RenderStyleConstants.h"
#include "ScriptState.h"
#include "SelectorChecker.h"
#include "ShadowRoot.h"
#include "StaticNodeList.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleSheetList.h"
#include "Text.h"
#include "TextNodeTraversal.h"
#include "Timer.h"
#include "VideoPlaybackQuality.h"
#include "WebInjectedScriptManager.h"
#include "XPathResult.h"
#include "markup.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorDebuggerAgent.h>
#include <JavaScriptCore/JSCInlines.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/Function.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

using namespace HTMLNames;

static const size_t maxTextSize = 10000;
static const UChar ellipsisUChar[] = { 0x2026, 0 };

static Color parseColor(RefPtr<JSON::Object>&& colorObject)
{
    if (!colorObject)
        return Color::transparentBlack;

    auto r = colorObject->getInteger(Protocol::DOM::RGBAColor::rKey);
    auto g = colorObject->getInteger(Protocol::DOM::RGBAColor::gKey);
    auto b = colorObject->getInteger(Protocol::DOM::RGBAColor::bKey);
    if (!r || !g || !b)
        return Color::transparentBlack;

    auto a = colorObject->getDouble(Protocol::DOM::RGBAColor::aKey);
    if (!a)
        return clampToComponentBytes<SRGBA>(*r, *g, *b);
    return clampToComponentBytes<SRGBA>(*r, *g, *b, convertToComponentByte(*a));
}

static Color parseConfigColor(const String& fieldName, JSON::Object& configObject)
{
    return parseColor(configObject.getObject(fieldName));
}

static bool parseQuad(Ref<JSON::Array>&& quadArray, FloatQuad* quad)
{
    const size_t coordinatesInQuad = 8;
    double coordinates[coordinatesInQuad];
    if (quadArray->length() != coordinatesInQuad)
        return false;
    for (size_t i = 0; i < coordinatesInQuad; ++i) {
        auto coordinate = quadArray->get(i)->asDouble();
        if (!coordinate)
            return false;
        coordinates[i] = *coordinate;
    }
    quad->setP1(FloatPoint(coordinates[0], coordinates[1]));
    quad->setP2(FloatPoint(coordinates[2], coordinates[3]));
    quad->setP3(FloatPoint(coordinates[4], coordinates[5]));
    quad->setP4(FloatPoint(coordinates[6], coordinates[7]));

    return true;
}

class RevalidateStyleAttributeTask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RevalidateStyleAttributeTask(InspectorDOMAgent*);
    void scheduleFor(Element*);
    void reset() { m_timer.stop(); }
    void timerFired();

private:
    InspectorDOMAgent* m_domAgent;
    Timer m_timer;
    HashSet<RefPtr<Element>> m_elements;
};

RevalidateStyleAttributeTask::RevalidateStyleAttributeTask(InspectorDOMAgent* domAgent)
    : m_domAgent(domAgent)
    , m_timer(*this, &RevalidateStyleAttributeTask::timerFired)
{
}

void RevalidateStyleAttributeTask::scheduleFor(Element* element)
{
    m_elements.add(element);
    if (!m_timer.isActive())
        m_timer.startOneShot(0_s);
}

void RevalidateStyleAttributeTask::timerFired()
{
    // The timer is stopped on m_domAgent destruction, so this method will never be called after m_domAgent has been destroyed.
    Vector<Element*> elements;
    for (auto& element : m_elements)
        elements.append(element.get());
    m_domAgent->styleAttributeInvalidated(elements);

    m_elements.clear();
}

class InspectableNode final : public CommandLineAPIHost::InspectableObject {
public:
    explicit InspectableNode(Node* node)
        : m_node(node)
    {
    }

    JSC::JSValue get(JSC::JSGlobalObject& state) final
    {
        return InspectorDOMAgent::nodeAsScriptValue(state, m_node.get());
    }
private:
    RefPtr<Node> m_node;
};

class EventFiredCallback final : public EventListener {
public:
    static Ref<EventFiredCallback> create(InspectorDOMAgent& domAgent)
    {
        return adoptRef(*new EventFiredCallback(domAgent));
    }

    bool operator==(const EventListener& other) const final
    {
        return this == &other;
    }

    void handleEvent(ScriptExecutionContext&, Event& event) final
    {
        if (!is<Node>(event.target()) || m_domAgent.m_dispatchedEvents.contains(&event))
            return;

        auto* node = downcast<Node>(event.target());
        auto nodeId = m_domAgent.pushNodePathToFrontend(node);
        if (!nodeId)
            return;

        m_domAgent.m_dispatchedEvents.add(&event);

        RefPtr<JSON::Object> data = JSON::Object::create();

#if ENABLE(FULLSCREEN_API)
        if (event.type() == eventNames().webkitfullscreenchangeEvent)
            data->setBoolean("enabled"_s, !!node->document().fullscreenManager().fullscreenElement());
#endif // ENABLE(FULLSCREEN_API)

        auto timestamp = m_domAgent.m_environment.executionStopwatch().elapsedTime().seconds();
        m_domAgent.m_frontendDispatcher->didFireEvent(nodeId, event.type(), timestamp, data->size() ? WTFMove(data) : nullptr);
    }

private:
    EventFiredCallback(InspectorDOMAgent& domAgent)
        : EventListener(EventListener::CPPEventListenerType)
        , m_domAgent(domAgent)
    {
    }

    InspectorDOMAgent& m_domAgent;
};

String InspectorDOMAgent::toErrorString(ExceptionCode ec)
{
    return ec ? String(DOMException::name(ec)) : emptyString();
}

String InspectorDOMAgent::toErrorString(Exception&& exception)
{
    return DOMException::name(exception.code());
}

InspectorDOMAgent::InspectorDOMAgent(PageAgentContext& context, InspectorOverlay* overlay)
    : InspectorAgentBase("DOM"_s, context)
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_frontendDispatcher(makeUnique<Inspector::DOMFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::DOMBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
    , m_overlay(overlay)
#if ENABLE(VIDEO)
    , m_mediaMetricsTimer(*this, &InspectorDOMAgent::mediaMetricsTimerFired)
#endif
{
}

InspectorDOMAgent::~InspectorDOMAgent() = default;

void InspectorDOMAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
    m_history = makeUnique<InspectorHistory>();
    m_domEditor = makeUnique<DOMEditor>(*m_history);

    m_instrumentingAgents.setPersistentDOMAgent(this);
    m_document = m_inspectedPage.mainFrame().document();

#if ENABLE(VIDEO)
    if (m_document)
        addEventListenersToNode(*m_document);

    for (auto* mediaElement : HTMLMediaElement::allMediaElements())
        addEventListenersToNode(*mediaElement);
#endif
}

void InspectorDOMAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_history.reset();
    m_domEditor.reset();
    m_nodeToFocus = nullptr;
    m_mousedOverNode = nullptr;
    m_inspectedNode = nullptr;

    Protocol::ErrorString ignored;
    setSearchingForNode(ignored, false, nullptr, false);
    hideHighlight();

    m_instrumentingAgents.setPersistentDOMAgent(nullptr);
    m_documentRequested = false;
    reset();
}

Vector<Document*> InspectorDOMAgent::documents()
{
    Vector<Document*> result;
    for (Frame* frame = m_document->frame(); frame; frame = frame->tree().traverseNext()) {
        Document* document = frame->document();
        if (!document)
            continue;
        result.append(document);
    }
    return result;
}

void InspectorDOMAgent::reset()
{
    if (m_history)
        m_history->reset();
    m_searchResults.clear();
    discardBindings();
    if (m_revalidateStyleAttrTask)
        m_revalidateStyleAttrTask->reset();
    m_document = nullptr;
}

void InspectorDOMAgent::setDocument(Document* document)
{
    if (document == m_document.get())
        return;

    reset();

    m_document = document;

    if (!m_documentRequested)
        return;

    // Immediately communicate null document or document that has finished loading.
    if (!document || !document->parsing())
        m_frontendDispatcher->documentUpdated();
}

void InspectorDOMAgent::releaseDanglingNodes()
{
    m_danglingNodeToIdMaps.clear();
}

Protocol::DOM::NodeId InspectorDOMAgent::bind(Node* node, NodeToIdMap* nodesMap)
{
    auto id = nodesMap->get(node);
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
    auto id = nodesMap->get(node);
    if (!id)
        return;

    m_idToNode.remove(id);

    if (node->isFrameOwnerElement()) {
        const HTMLFrameOwnerElement* frameOwner = static_cast<const HTMLFrameOwnerElement*>(node);
        if (Document* contentDocument = frameOwner->contentDocument())
            unbind(contentDocument, nodesMap);
    }

    if (is<Element>(*node)) {
        Element& element = downcast<Element>(*node);
        if (ShadowRoot* root = element.shadowRoot())
            unbind(root, nodesMap);
        if (PseudoElement* beforeElement = element.beforePseudoElement())
            unbind(beforeElement, nodesMap);
        if (PseudoElement* afterElement = element.afterPseudoElement())
            unbind(afterElement, nodesMap);
    }

    nodesMap->remove(node);

    if (auto* cssAgent = m_instrumentingAgents.enabledCSSAgent())
        cssAgent->didRemoveDOMNode(*node, id);

    if (m_childrenRequested.remove(id)) {
        // FIXME: Would be better to do this iteratively rather than recursively.
        for (Node* child = innerFirstChild(node); child; child = innerNextSibling(child))
            unbind(child, nodesMap);
    }
}

Node* InspectorDOMAgent::assertNode(Protocol::ErrorString& errorString, Protocol::DOM::NodeId nodeId)
{
    Node* node = nodeForId(nodeId);
    if (!node) {
        errorString = "Missing node for given nodeId"_s;
        return nullptr;
    }
    return node;
}

Document* InspectorDOMAgent::assertDocument(Protocol::ErrorString& errorString, Protocol::DOM::NodeId nodeId)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return nullptr;
    if (!is<Document>(node)) {
        errorString = "Node for given nodeId is not a document"_s;
        return nullptr;
    }
    return downcast<Document>(node);
}

Element* InspectorDOMAgent::assertElement(Protocol::ErrorString& errorString, Protocol::DOM::NodeId nodeId)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return nullptr;
    if (!is<Element>(node)) {
        errorString = "Node for given nodeId is not an element"_s;
        return nullptr;
    }
    return downcast<Element>(node);
}

Node* InspectorDOMAgent::assertEditableNode(Protocol::ErrorString& errorString, Protocol::DOM::NodeId nodeId)
{
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return nullptr;
    if (node->isInUserAgentShadowTree() && !m_allowEditingUserAgentShadowTrees) {
        errorString = "Node for given nodeId is in a shadow tree"_s;
        return nullptr;
    }
    if (node->isPseudoElement()) {
        errorString = "Node for given nodeId is a pseudo-element"_s;
        return nullptr;
    }
    return node;
}

Element* InspectorDOMAgent::assertEditableElement(Protocol::ErrorString& errorString, Protocol::DOM::NodeId nodeId)
{
    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return nullptr;
    if (!is<Element>(node)) {
        errorString = "Node for given nodeId is not an element"_s;
        return nullptr;
    }
    return downcast<Element>(node);
}

Protocol::ErrorStringOr<Ref<Protocol::DOM::Node>> InspectorDOMAgent::getDocument()
{
    m_documentRequested = true;

    if (!m_document)
        return makeUnexpected("Internal error: missing document"_s);

    // Reset backend state.
    RefPtr<Document> document = m_document;
    reset();
    m_document = document;

    auto root = buildObjectForNode(m_document.get(), 2, &m_documentNodeToIdMap);

    if (m_nodeToFocus)
        focusNode();

    return root;
}

void InspectorDOMAgent::pushChildNodesToFrontend(Protocol::DOM::NodeId nodeId, int depth)
{
    Node* node = nodeForId(nodeId);
    if (!node || (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE && node->nodeType() != Node::DOCUMENT_FRAGMENT_NODE))
        return;

    NodeToIdMap* nodeMap = m_idToNodesMap.get(nodeId);

    if (m_childrenRequested.contains(nodeId)) {
        if (depth <= 1)
            return;

        depth--;

        for (node = innerFirstChild(node); node; node = innerNextSibling(node)) {
            auto childNodeId = nodeMap->get(node);
            ASSERT(childNodeId);
            pushChildNodesToFrontend(childNodeId, depth);
        }

        return;
    }

    auto children = buildArrayForContainerChildren(node, depth, nodeMap);
    m_frontendDispatcher->setChildNodes(nodeId, WTFMove(children));
}

void InspectorDOMAgent::discardBindings()
{
    m_documentNodeToIdMap.clear();
    m_idToNode.clear();
    m_dispatchedEvents.clear();
    m_eventListenerEntries.clear();
    releaseDanglingNodes();
    m_childrenRequested.clear();
}

Protocol::DOM::NodeId InspectorDOMAgent::pushNodeToFrontend(Node* nodeToPush)
{
    if (!nodeToPush)
        return 0;

    // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented

    Protocol::ErrorString ignored;
    return pushNodeToFrontend(ignored, boundNodeId(&nodeToPush->document()), nodeToPush);
}

Protocol::DOM::NodeId InspectorDOMAgent::pushNodeToFrontend(Protocol::ErrorString& errorString, Protocol::DOM::NodeId documentNodeId, Node* nodeToPush)
{
    Document* document = assertDocument(errorString, documentNodeId);
    if (!document)
        return 0;
    if (&nodeToPush->document() != document) {
        errorString = "nodeToPush is not part of the document with given documentNodeId"_s;
        return 0;
    }

    return pushNodePathToFrontend(errorString, nodeToPush);
}

Node* InspectorDOMAgent::nodeForId(Protocol::DOM::NodeId id)
{
    if (!m_idToNode.isValidKey(id))
        return nullptr;

    return m_idToNode.get(id);
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::requestChildNodes(Protocol::DOM::NodeId nodeId, Optional<int>&& depth)
{
    int sanitizedDepth;

    if (!depth)
        sanitizedDepth = 1;
    else if (*depth == -1)
        sanitizedDepth = INT_MAX;
    else if (*depth > 0)
        sanitizedDepth = *depth;
    else
        return makeUnexpected("Unexpected value below -1 for given depth"_s);

    pushChildNodesToFrontend(nodeId, sanitizedDepth);

    return { };
}

Protocol::ErrorStringOr<Protocol::DOM::NodeId> InspectorDOMAgent::querySelector(Protocol::DOM::NodeId nodeId, const String& selector)
{
    Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);
    if (!is<ContainerNode>(*node))
        return makeUnexpected("Node for given nodeId is not a container node"_s);

    auto queryResult = downcast<ContainerNode>(*node).querySelector(selector);
    if (queryResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(queryResult.releaseException()));

    auto resultNodeId = pushNodePathToFrontend(errorString, queryResult.releaseReturnValue());
    if (!resultNodeId)
        return makeUnexpected(errorString);

    return resultNodeId;
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::DOM::NodeId>>> InspectorDOMAgent::querySelectorAll(Protocol::DOM::NodeId nodeId, const String& selector)
{
    Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);
    if (!is<ContainerNode>(*node))
        return makeUnexpected("Node for given nodeId is not a container node"_s);

    auto queryResult = downcast<ContainerNode>(*node).querySelectorAll(selector);
    if (queryResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(queryResult.releaseException()));

    auto nodes = queryResult.releaseReturnValue();

    auto nodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
    for (unsigned i = 0; i < nodes->length(); ++i)
        nodeIds->addItem(pushNodePathToFrontend(nodes->item(i)));
    return nodeIds;
}

Protocol::DOM::NodeId InspectorDOMAgent::pushNodePathToFrontend(Node* nodeToPush)
{
    Protocol::ErrorString ignored;
    return pushNodePathToFrontend(ignored, nodeToPush);
}

Protocol::DOM::NodeId InspectorDOMAgent::pushNodePathToFrontend(Protocol::ErrorString errorString, Node* nodeToPush)
{
    ASSERT(nodeToPush);  // Invalid input

    if (!m_document) {
        errorString = "Missing document"_s;
        return 0;
    }

    // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
    if (!m_documentNodeToIdMap.contains(m_document)) {
        errorString = "Document must have been requested"_s;
        return 0;
    }

    // Return id in case the node is known.
    if (auto result = m_documentNodeToIdMap.get(nodeToPush))
        return result;

    Node* node = nodeToPush;
    Vector<Node*> path;
    NodeToIdMap* danglingMap = 0;

    while (true) {
        Node* parent = innerParentNode(node);
        if (!parent) {
            // Node being pushed is detached -> push subtree root.
            auto newMap = makeUnique<NodeToIdMap>();
            danglingMap = newMap.get();
            m_danglingNodeToIdMaps.append(newMap.release());
            auto children = JSON::ArrayOf<Protocol::DOM::Node>::create();
            children->addItem(buildObjectForNode(node, 0, danglingMap));
            m_frontendDispatcher->setChildNodes(0, WTFMove(children));
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
        auto nodeId = map->get(path.at(i));
        ASSERT(nodeId);
        pushChildNodesToFrontend(nodeId);
    }
    return map->get(nodeToPush);
}

Protocol::DOM::NodeId InspectorDOMAgent::boundNodeId(const Node* node)
{
    return m_documentNodeToIdMap.get(const_cast<Node*>(node));
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::setAttributeValue(Protocol::DOM::NodeId nodeId, const String& name, const String& value)
{
    Protocol::ErrorString errorString;

    Element* element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!m_domEditor->setAttribute(*element, name, value, errorString))
        return makeUnexpected(errorString);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::setAttributesAsText(Protocol::DOM::NodeId nodeId, const String& text, const String& name)
{
    Protocol::ErrorString errorString;

    Element* element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    auto parsedElement = createHTMLElement(element->document(), spanTag);
    auto result = parsedElement.get().setInnerHTML("<span " + text + "></span>");
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    Node* child = parsedElement->firstChild();
    if (!child)
        return makeUnexpected("Could not parse given text"_s);

    Element* childElement = downcast<Element>(child);
    if (!childElement->hasAttributes() && !!name) {
        if (!m_domEditor->removeAttribute(*element, name, errorString))
            return makeUnexpected(errorString);
        return { };
    }

    bool foundOriginalAttribute = false;
    for (const Attribute& attribute : childElement->attributesIterator()) {
        // Add attribute pair
        foundOriginalAttribute = foundOriginalAttribute || attribute.name().toString() == name;
        if (!m_domEditor->setAttribute(*element, attribute.name().toString(), attribute.value(), errorString))
            return makeUnexpected(errorString);
    }

    if (!foundOriginalAttribute && !name.stripWhiteSpace().isEmpty()) {
        if (!m_domEditor->removeAttribute(*element, name, errorString))
            return makeUnexpected(errorString);
    }

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::removeAttribute(Protocol::DOM::NodeId nodeId, const String& name)
{
    Protocol::ErrorString errorString;

    Element* element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!m_domEditor->removeAttribute(*element, name, errorString))
        return makeUnexpected(errorString);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::removeNode(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    ContainerNode* parentNode = node->parentNode();
    if (!parentNode)
        return makeUnexpected("Cannot remove detached node"_s);

    if (!m_domEditor->removeChild(*parentNode, *node, errorString))
        return makeUnexpected(errorString);

    return { };
}

Protocol::ErrorStringOr<Protocol::DOM::NodeId> InspectorDOMAgent::setNodeName(Protocol::DOM::NodeId nodeId, const String& tagName)
{
    Protocol::ErrorString errorString;

    auto oldNode = assertElement(errorString, nodeId);
    if (!oldNode)
        return makeUnexpected(errorString);

    auto createElementResult = oldNode->document().createElementForBindings(tagName);
    if (createElementResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(createElementResult.releaseException()));

    auto newElement = createElementResult.releaseReturnValue();

    // Copy over the original node's attributes.
    newElement->cloneAttributesFromElement(*oldNode);

    // Copy over the original node's children.
    RefPtr<Node> child;
    while ((child = oldNode->firstChild())) {
        if (!m_domEditor->insertBefore(newElement, *child, 0, errorString))
            return makeUnexpected(errorString);
    }

    // Replace the old node with the new node
    RefPtr<ContainerNode> parent = oldNode->parentNode();
    if (!m_domEditor->insertBefore(*parent, newElement.copyRef(), oldNode->nextSibling(), errorString))
        return makeUnexpected(errorString);
    if (!m_domEditor->removeChild(*parent, *oldNode, errorString))
        return makeUnexpected(errorString);

    auto resultNodeId = pushNodePathToFrontend(errorString, newElement.ptr());
    if (m_childrenRequested.contains(nodeId))
        pushChildNodesToFrontend(resultNodeId);

    return resultNodeId;
}

Protocol::ErrorStringOr<String> InspectorDOMAgent::getOuterHTML(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    return serializeFragment(*node, SerializedNodes::SubtreeIncludingNode);
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::setOuterHTML(Protocol::DOM::NodeId nodeId, const String& outerHTML)
{
    Protocol::ErrorString errorString;

    if (!nodeId) {
        DOMPatchSupport { *m_domEditor, *m_document }.patchDocument(outerHTML);
        return { };
    }

    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    Document& document = node->document();
    if (!document.isHTMLDocument() && !document.isXMLDocument())
        return makeUnexpected("Document of node for given nodeId is not HTML/XML"_s);

    Node* newNode = nullptr;
    if (!m_domEditor->setOuterHTML(*node, outerHTML, newNode, errorString))
        return makeUnexpected(errorString);

    if (!newNode) {
        // The only child node has been deleted.
        return { };
    }

    auto newId = pushNodePathToFrontend(errorString, newNode);

    bool childrenRequested = m_childrenRequested.contains(nodeId);
    if (childrenRequested)
        pushChildNodesToFrontend(newId);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::insertAdjacentHTML(Protocol::DOM::NodeId nodeId, const String& position, const String& html)
{
    Protocol::ErrorString errorString;

    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    if (!is<Element>(node))
        return makeUnexpected("Node for given nodeId is not an element"_s);

    if (!m_domEditor->insertAdjacentHTML(downcast<Element>(*node), position, html, errorString))
        return makeUnexpected(errorString);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::setNodeValue(Protocol::DOM::NodeId nodeId, const String& value)
{
    Protocol::ErrorString errorString;

    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    if (!is<Text>(*node))
        return makeUnexpected("Node for given nodeId is not text"_s);

    if (!m_domEditor->replaceWholeText(downcast<Text>(*node), value, errorString))
        return makeUnexpected(errorString);

    return { };
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<String>>> InspectorDOMAgent::getSupportedEventNames()
{
    auto eventNames = JSON::ArrayOf<String>::create();

#define DOM_EVENT_NAMES_ADD(name) eventNames->addItem(#name);
    DOM_EVENT_NAMES_FOR_EACH(DOM_EVENT_NAMES_ADD)
#undef DOM_EVENT_NAMES_ADD

    return eventNames;
}

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::DOM::DataBinding>>> InspectorDOMAgent::getDataBindingsForNode(Protocol::DOM::NodeId)
{
    return makeUnexpected("Not supported"_s);
}

Protocol::ErrorStringOr<String> InspectorDOMAgent::getAssociatedDataForNode(Protocol::DOM::NodeId)
{
    return makeUnexpected("Not supported"_s);
}
#endif

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::DOM::EventListener>>> InspectorDOMAgent::getEventListenersForNode(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    auto* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    Vector<RefPtr<EventTarget>> ancestors;
    ancestors.append(node);
    for (auto* ancestor = node->parentOrShadowHostNode(); ancestor; ancestor = ancestor->parentOrShadowHostNode())
        ancestors.append(ancestor);
    if (auto* window = node->document().domWindow())
        ancestors.append(window);

    struct EventListenerInfo {
        RefPtr<EventTarget> eventTarget;
        const AtomString eventType;
        const EventListenerVector eventListeners;
    };

    Vector<EventListenerInfo> eventInformation;
    for (size_t i = ancestors.size(); i; --i) {
        auto& ancestor = ancestors[i - 1];
        for (auto& eventType : ancestor->eventTypes()) {
            EventListenerVector filteredListeners;
            for (auto& listener : ancestor->eventListeners(eventType)) {
                if (listener->callback().type() == EventListener::JSEventListenerType)
                    filteredListeners.append(listener);
            }
            if (!filteredListeners.isEmpty())
                eventInformation.append({ ancestor, eventType, WTFMove(filteredListeners) });
        }
    }

    auto listeners = JSON::ArrayOf<Protocol::DOM::EventListener>::create();

    auto addListener = [&] (RegisteredEventListener& listener, const EventListenerInfo& info) {
        Protocol::DOM::EventListenerId identifier = 0;
        bool disabled = false;
        RefPtr<JSC::Breakpoint> breakpoint;

        for (auto& inspectorEventListener : m_eventListenerEntries.values()) {
            if (inspectorEventListener.matches(*info.eventTarget, info.eventType, listener.callback(), listener.useCapture())) {
                identifier = inspectorEventListener.identifier;
                disabled = inspectorEventListener.disabled;
                breakpoint = inspectorEventListener.breakpoint;
                break;
            }
        }

        if (!identifier) {
            InspectorEventListener inspectorEventListener(m_lastEventListenerId++, *info.eventTarget, info.eventType, listener.callback(), listener.useCapture());

            identifier = inspectorEventListener.identifier;
            disabled = inspectorEventListener.disabled;
            breakpoint = inspectorEventListener.breakpoint;

            m_eventListenerEntries.add(identifier, inspectorEventListener);
        }

        listeners->addItem(buildObjectForEventListener(listener, identifier, *info.eventTarget, info.eventType, disabled, breakpoint));
    };

    // Get Capturing Listeners (in this order)
    size_t eventInformationLength = eventInformation.size();
    for (auto& info : eventInformation) {
        for (auto& listener : info.eventListeners) {
            if (listener->useCapture())
                addListener(*listener, info);
        }
    }

    // Get Bubbling Listeners (reverse order)
    for (size_t i = eventInformationLength; i; --i) {
        const EventListenerInfo& info = eventInformation[i - 1];
        for (auto& listener : info.eventListeners) {
            if (!listener->useCapture())
                addListener(*listener, info);
        }
    }

    if (m_inspectedNode == node)
        m_suppressEventListenerChangedEvent = false;

    return listeners;
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::setEventListenerDisabled(Protocol::DOM::EventListenerId eventListenerId, bool disabled)
{
    auto it = m_eventListenerEntries.find(eventListenerId);
    if (it == m_eventListenerEntries.end())
        return makeUnexpected("Missing event listener for given eventListenerId"_s);

    it->value.disabled = disabled;

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::setBreakpointForEventListener(Protocol::DOM::EventListenerId eventListenerId, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    auto it = m_eventListenerEntries.find(eventListenerId);
    if (it == m_eventListenerEntries.end())
        return makeUnexpected("Missing event listener for given eventListenerId"_s);

    if (it->value.breakpoint)
        return makeUnexpected("Breakpoint for given eventListenerId already exists"_s);

    it->value.breakpoint = InspectorDebuggerAgent::debuggerBreakpointFromPayload(errorString, WTFMove(options));
    if (!it->value.breakpoint)
        return makeUnexpected(errorString);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::removeBreakpointForEventListener(Protocol::DOM::EventListenerId eventListenerId)
{
    auto it = m_eventListenerEntries.find(eventListenerId);
    if (it == m_eventListenerEntries.end())
        return makeUnexpected("Missing event listener for given eventListenerId"_s);

    if (!it->value.breakpoint)
        return makeUnexpected("Breakpoint for given eventListenerId missing"_s);

    it->value.breakpoint = nullptr;

    return { };
}

Protocol::ErrorStringOr<Ref<Protocol::DOM::AccessibilityProperties>> InspectorDOMAgent::getAccessibilityPropertiesForNode(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    return buildObjectForAccessibilityProperties(*node);
}

Protocol::ErrorStringOr<std::tuple<String /* searchId */, int /* resultCount */>> InspectorDOMAgent::performSearch(const String& query, RefPtr<JSON::Array>&& nodeIds, Optional<bool>&& caseSensitive)
{
    Protocol::ErrorString errorString;

    // FIXME: Search works with node granularity - number of matches within node is not calculated.
    InspectorNodeFinder finder(query, caseSensitive && *caseSensitive);

    if (nodeIds) {
        for (auto& nodeValue : *nodeIds) {
            auto nodeId = nodeValue->asInteger();
            if (!nodeId)
                return makeUnexpected("Unexpected non-integer item in given nodeIds"_s);

            Node* node = assertNode(errorString, *nodeId);
            if (!node)
                return makeUnexpected(errorString);

            finder.performSearch(node);
        }
    } else {
        // There's no need to iterate the frames tree because
        // the search helper will go inside the frame owner elements.
        finder.performSearch(m_document.get());
    }

    auto searchId = IdentifiersFactory::createIdentifier();

    auto& resultsVector = m_searchResults.add(searchId, Vector<RefPtr<Node>>()).iterator->value;
    for (auto& result : finder.results())
        resultsVector.append(result);

    return { { searchId, resultsVector.size() } };
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::DOM::NodeId>>> InspectorDOMAgent::getSearchResults(const String& searchId, int fromIndex, int toIndex)
{
    SearchResults::iterator it = m_searchResults.find(searchId);
    if (it == m_searchResults.end())
        return makeUnexpected("Missing search result for given searchId"_s);

    int size = it->value.size();
    if (fromIndex < 0 || toIndex > size || fromIndex >= toIndex)
        return makeUnexpected("Invalid search result range for given fromIndex and toIndex"_s);

    auto nodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
    for (int i = fromIndex; i < toIndex; ++i)
        nodeIds->addItem(pushNodePathToFrontend((it->value)[i].get()));
    return nodeIds;
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::discardSearchResults(const String& searchId)
{
    m_searchResults.remove(searchId);

    return { };
}

bool InspectorDOMAgent::handleMousePress()
{
    if (!m_searchingForNode)
        return false;

    if (Node* node = m_overlay->highlightedNode()) {
        inspect(node);
        return true;
    }
    return false;
}

bool InspectorDOMAgent::handleTouchEvent(Node& node)
{
    if (!m_searchingForNode)
        return false;
    if (m_inspectModeHighlightConfig) {
        m_overlay->highlightNode(&node, *m_inspectModeHighlightConfig);
        inspect(&node);
        return true;
    }
    return false;
}

void InspectorDOMAgent::inspect(Node* inspectedNode)
{
    Protocol::ErrorString ignored;
    RefPtr<Node> node = inspectedNode;
    setSearchingForNode(ignored, false, nullptr, false);

    if (!node->isElementNode() && !node->isDocumentNode())
        node = node->parentNode();
    m_nodeToFocus = node;

    if (!m_nodeToFocus)
        return;

    focusNode();
}

void InspectorDOMAgent::focusNode()
{
    // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
    if (!m_documentRequested)
        return;

    ASSERT(m_nodeToFocus);

    RefPtr<Node> node = m_nodeToFocus.get();
    m_nodeToFocus = nullptr;

    Frame* frame = node->document().frame();
    if (!frame)
        return;

    JSC::JSGlobalObject* scriptState = mainWorldExecState(frame);
    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptFor(scriptState);
    if (injectedScript.hasNoValue())
        return;

    injectedScript.inspectObject(nodeAsScriptValue(*scriptState, node.get()));
}

void InspectorDOMAgent::mouseDidMoveOverElement(const HitTestResult& result, unsigned)
{
    m_mousedOverNode = result.innerNode();

    if (!m_searchingForNode)
        return;

    highlightMousedOverNode();
}

void InspectorDOMAgent::highlightMousedOverNode()
{
    Node* node = m_mousedOverNode.get();
    if (node && node->isTextNode())
        node = node->parentNode();
    if (node && m_inspectModeHighlightConfig)
        m_overlay->highlightNode(node, *m_inspectModeHighlightConfig);
}

void InspectorDOMAgent::setSearchingForNode(Protocol::ErrorString& errorString, bool enabled, RefPtr<JSON::Object>&& highlightInspectorObject, bool showRulers)
{
    if (m_searchingForNode == enabled)
        return;

    m_searchingForNode = enabled;

    m_overlay->setShowRulersDuringElementSelection(m_searchingForNode && showRulers);

    if (m_searchingForNode) {
        m_inspectModeHighlightConfig = highlightConfigFromInspectorObject(errorString, WTFMove(highlightInspectorObject));
        if (!m_inspectModeHighlightConfig)
            return;
        highlightMousedOverNode();
    } else
        hideHighlight();

    m_overlay->didSetSearchingForNode(m_searchingForNode);

    if (InspectorClient* client = m_inspectedPage.inspectorController().inspectorClient())
        client->elementSelectionChanged(m_searchingForNode);
}

std::unique_ptr<HighlightConfig> InspectorDOMAgent::highlightConfigFromInspectorObject(Protocol::ErrorString& errorString, RefPtr<JSON::Object>&& highlightInspectorObject)
{
    if (!highlightInspectorObject) {
        errorString = "Internal error: highlight configuration parameter is missing"_s;
        return nullptr;
    }

    auto highlightConfig = makeUnique<HighlightConfig>();
    highlightConfig->showInfo = highlightInspectorObject->getBoolean(Protocol::DOM::HighlightConfig::showInfoKey).valueOr(false);
    highlightConfig->content = parseConfigColor(Protocol::DOM::HighlightConfig::contentColorKey, *highlightInspectorObject);
    highlightConfig->padding = parseConfigColor(Protocol::DOM::HighlightConfig::paddingColorKey, *highlightInspectorObject);
    highlightConfig->border = parseConfigColor(Protocol::DOM::HighlightConfig::borderColorKey, *highlightInspectorObject);
    highlightConfig->margin = parseConfigColor(Protocol::DOM::HighlightConfig::marginColorKey, *highlightInspectorObject);
    return highlightConfig;
}

#if PLATFORM(IOS_FAMILY)
Protocol::ErrorStringOr<void> InspectorDOMAgent::setInspectModeEnabled(bool enabled, RefPtr<JSON::Object>&& highlightConfig)
{
    Protocol::ErrorString errorString;

    setSearchingForNode(errorString, enabled, WTFMove(highlightConfig), false);

    if (!!errorString)
        return makeUnexpected(errorString);

    return { };
}
#else
Protocol::ErrorStringOr<void> InspectorDOMAgent::setInspectModeEnabled(bool enabled, RefPtr<JSON::Object>&& highlightConfig, Optional<bool>&& showRulers)
{
    Protocol::ErrorString errorString;

    setSearchingForNode(errorString, enabled, WTFMove(highlightConfig), showRulers && *showRulers);

    if (!!errorString)
        return makeUnexpected(errorString);

    return { };
}
#endif

Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightRect(int x, int y, int width, int height, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, Optional<bool>&& usePageCoordinates)
{
    auto quad = makeUnique<FloatQuad>(FloatRect(x, y, width, height));
    innerHighlightQuad(WTFMove(quad), WTFMove(color), WTFMove(outlineColor), WTFMove(usePageCoordinates));

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightQuad(Ref<JSON::Array>&& quadObject, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, Optional<bool>&& usePageCoordinates)
{
    auto quad = makeUnique<FloatQuad>();
    if (!parseQuad(WTFMove(quadObject), quad.get()))
        return makeUnexpected("Unexpected invalid quad"_s);

    innerHighlightQuad(WTFMove(quad), WTFMove(color), WTFMove(outlineColor), WTFMove(usePageCoordinates));

    return { };
}

void InspectorDOMAgent::innerHighlightQuad(std::unique_ptr<FloatQuad> quad, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, Optional<bool>&& usePageCoordinates)
{
    auto highlightConfig = makeUnique<HighlightConfig>();
    highlightConfig->content = parseColor(WTFMove(color));
    highlightConfig->contentOutline = parseColor(WTFMove(outlineColor));
    highlightConfig->usePageCoordinates = usePageCoordinates ? *usePageCoordinates : false;
    m_overlay->highlightQuad(WTFMove(quad), *highlightConfig);
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightSelector(Ref<JSON::Object>&& highlightInspectorObject, const String& selectorString, const Protocol::Network::FrameId& frameId)
{
    Protocol::ErrorString errorString;

    auto highlightConfig = highlightConfigFromInspectorObject(errorString, WTFMove(highlightInspectorObject));
    if (!highlightConfig)
        return makeUnexpected(errorString);

    RefPtr<Document> document;

    if (!!frameId) {
        auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
        if (!pageAgent)
            return makeUnexpected("Page domain must be enabled"_s);

        auto* frame = pageAgent->assertFrame(errorString, frameId);
        if (!frame)
            return makeUnexpected(errorString);

        document = frame->document();
    } else
        document = m_document;

    if (!document)
        return makeUnexpected("Missing document of frame for given frameId"_s);

    CSSParser parser(*document);
    CSSSelectorList selectorList;
    parser.parseSelector(selectorString, selectorList);

    SelectorChecker selectorChecker(*document);

    Vector<Ref<Node>> nodeList;
    HashSet<Node*> seenNodes;

    for (auto& descendant : composedTreeDescendants(*document)) {
        if (!is<Element>(descendant))
            continue;

        auto& descendantElement = downcast<Element>(descendant);

        auto isInUserAgentShadowTree = descendantElement.isInUserAgentShadowTree();
        auto pseudoId = descendantElement.pseudoId();
        auto& pseudo = descendantElement.pseudo();

        for (const auto* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
            if (isInUserAgentShadowTree && (selector->match() != CSSSelector::PseudoElement || selector->value() != pseudo))
                continue;

            SelectorChecker::CheckingContext context(SelectorChecker::Mode::ResolvingStyle);
            context.pseudoId = pseudoId;

            if (selectorChecker.match(*selector, descendantElement, context)) {
                if (seenNodes.add(&descendantElement))
                    nodeList.append(descendantElement);
            }

            if (context.pseudoIDSet) {
                auto pseudoIDs = PseudoIdSet::fromMask(context.pseudoIDSet.data());

                if (pseudoIDs.has(PseudoId::Before)) {
                    pseudoIDs.remove(PseudoId::Before);
                    if (auto* beforePseudoElement = descendantElement.beforePseudoElement()) {
                        if (seenNodes.add(beforePseudoElement))
                            nodeList.append(*beforePseudoElement);
                    }
                }

                if (pseudoIDs.has(PseudoId::After)) {
                    pseudoIDs.remove(PseudoId::After);
                    if (auto* afterPseudoElement = descendantElement.afterPseudoElement()) {
                        if (seenNodes.add(afterPseudoElement))
                            nodeList.append(*afterPseudoElement);
                    }
                }

                if (pseudoIDs) {
                    if (seenNodes.add(&descendantElement))
                        nodeList.append(descendantElement);
                }
            }
        }
    }

    m_overlay->highlightNodeList(StaticNodeList::create(WTFMove(nodeList)), *highlightConfig);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightNode(Ref<JSON::Object>&& highlightInspectorObject, Optional<Protocol::DOM::NodeId>&& nodeId, const Protocol::Runtime::RemoteObjectId& objectId)
{
    Protocol::ErrorString errorString;

    Node* node = nullptr;
    if (nodeId)
        node = assertNode(errorString, *nodeId);
    else if (!!objectId) {
        node = nodeForObjectId(objectId);
        errorString = "Missing node for given objectId"_s;
    } else
        errorString = "Either nodeId or objectId must be specified"_s;

    if (!node)
        return makeUnexpected(errorString);

    std::unique_ptr<HighlightConfig> highlightConfig = highlightConfigFromInspectorObject(errorString, WTFMove(highlightInspectorObject));
    if (!highlightConfig)
        return makeUnexpected(errorString);

    m_overlay->highlightNode(node, *highlightConfig);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightNodeList(Ref<JSON::Array>&& nodeIds, Ref<JSON::Object>&& highlightInspectorObject)
{
    Protocol::ErrorString errorString;

    Vector<Ref<Node>> nodes;
    for (auto& nodeIdValue : nodeIds.get()) {
        auto nodeId = nodeIdValue->asInteger();
        if (!nodeId)
            return makeUnexpected("Unexpected non-integer item in given nodeIds"_s);

        // In the case that a node is removed in the time between when highlightNodeList is invoked
        // by the frontend and it is executed by the backend, we should still attempt to highlight
        // as many nodes as possible. As such, we should ignore any errors generated when attempting
        // to get a Node from a given nodeId. 
        Protocol::ErrorString ignored;
        Node* node = assertNode(ignored, *nodeId);
        if (!node)
            continue;

        nodes.append(*node);
    }

    std::unique_ptr<HighlightConfig> highlightConfig = highlightConfigFromInspectorObject(errorString, WTFMove(highlightInspectorObject));
    if (!highlightConfig)
        return makeUnexpected(errorString);

    m_overlay->highlightNodeList(StaticNodeList::create(WTFMove(nodes)), *highlightConfig);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightFrame(const Protocol::Network::FrameId& frameId, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor)
{
    Protocol::ErrorString errorString;

    auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
    if (!pageAgent)
        return makeUnexpected("Page domain must be enabled"_s);

    auto* frame = pageAgent->assertFrame(errorString, frameId);
    if (!frame)
        return makeUnexpected(errorString);

    if (frame->ownerElement()) {
        auto highlightConfig = makeUnique<HighlightConfig>();
        highlightConfig->showInfo = true; // Always show tooltips for frames.
        highlightConfig->content = parseColor(WTFMove(color));
        highlightConfig->contentOutline = parseColor(WTFMove(outlineColor));
        m_overlay->highlightNode(frame->ownerElement(), *highlightConfig);
    }

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::hideHighlight()
{
    m_overlay->hideHighlight();

    return { };
}

Protocol::ErrorStringOr<Protocol::DOM::NodeId> InspectorDOMAgent::moveTo(Protocol::DOM::NodeId nodeId, Protocol::DOM::NodeId targetNodeId, Optional<Protocol::DOM::NodeId>&& insertBeforeNodeId)
{
    Protocol::ErrorString errorString;

    Node* node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    Element* targetElement = assertEditableElement(errorString, targetNodeId);
    if (!targetElement)
        return makeUnexpected(errorString);

    Node* anchorNode = nullptr;
    if (insertBeforeNodeId && *insertBeforeNodeId) {
        anchorNode = assertEditableNode(errorString, *insertBeforeNodeId);
        if (!anchorNode)
            return makeUnexpected(errorString);
        if (anchorNode->parentNode() != targetElement)
            return makeUnexpected("Given insertBeforeNodeId must be a child of given targetNodeId"_s);
    }

    if (!m_domEditor->insertBefore(*targetElement, *node, anchorNode, errorString))
        return makeUnexpected(errorString);

    return pushNodePathToFrontend(errorString, node);
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::undo()
{
    auto result = m_history->undo();
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::redo()
{
    auto result = m_history->redo();
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::markUndoableState()
{
    m_history->markUndoableState();

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::focus(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    Element* element = assertElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);
    if (!element->isFocusable())
        return makeUnexpected("Element for given nodeId is not focusable"_s);

    element->focus();

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::setInspectedNode(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    if (node->isInUserAgentShadowTree() && !m_allowEditingUserAgentShadowTrees)
        return makeUnexpected("Node for given nodeId is in a shadow tree"_s);

    m_inspectedNode = node;

    if (auto& commandLineAPIHost = static_cast<WebInjectedScriptManager&>(m_injectedScriptManager).commandLineAPIHost())
        commandLineAPIHost->addInspectedObject(makeUnique<InspectableNode>(node));

    m_suppressEventListenerChangedEvent = false;

    return { };
}

Protocol::ErrorStringOr<Ref<Protocol::Runtime::RemoteObject>> InspectorDOMAgent::resolveNode(Protocol::DOM::NodeId nodeId, const String& objectGroup)
{
    Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    auto object = resolveNode(node, objectGroup);
    if (!object)
        return makeUnexpected("Missing injected script for given nodeId"_s);

    return object.releaseNonNull();
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<String>>> InspectorDOMAgent::getAttributes(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    Element* element = assertElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    return buildArrayForElementAttributes(element);
}

Protocol::ErrorStringOr<Protocol::DOM::NodeId> InspectorDOMAgent::requestNode(const Protocol::Runtime::RemoteObjectId& objectId)
{
    Protocol::ErrorString errorString;

    Node* node = nodeForObjectId(objectId);
    if (!node)
        return makeUnexpected("Missing node for given objectId"_s);

    auto nodeId = pushNodePathToFrontend(errorString, node);
    if (!nodeId)
        return makeUnexpected(errorString);

    return nodeId;
}

String InspectorDOMAgent::documentURLString(Document* document)
{
    if (!document || document->url().isNull())
        return emptyString();
    return document->url().string();
}

static String documentBaseURLString(Document* document)
{
    return document->completeURL(emptyString()).string();
}

static bool pseudoElementType(PseudoId pseudoId, Protocol::DOM::PseudoType* type)
{
    switch (pseudoId) {
    case PseudoId::Before:
        *type = Protocol::DOM::PseudoType::Before;
        return true;
    case PseudoId::After:
        *type = Protocol::DOM::PseudoType::After;
        return true;
    default:
        return false;
    }
}

static Protocol::DOM::ShadowRootType shadowRootType(ShadowRootMode mode)
{
    switch (mode) {
    case ShadowRootMode::UserAgent:
        return Protocol::DOM::ShadowRootType::UserAgent;
    case ShadowRootMode::Closed:
        return Protocol::DOM::ShadowRootType::Closed;
    case ShadowRootMode::Open:
        return Protocol::DOM::ShadowRootType::Open;
    }

    ASSERT_NOT_REACHED();
    return Protocol::DOM::ShadowRootType::UserAgent;
}

static Protocol::DOM::CustomElementState customElementState(const Element& element)
{
    if (element.isDefinedCustomElement())
        return Protocol::DOM::CustomElementState::Custom;
    if (element.isFailedCustomElement())
        return Protocol::DOM::CustomElementState::Failed;
    if (element.isUndefinedCustomElement() || element.isCustomElementUpgradeCandidate())
        return Protocol::DOM::CustomElementState::Waiting;
    return Protocol::DOM::CustomElementState::Builtin;
}

static String computeContentSecurityPolicySHA256Hash(const Element& element)
{
    // FIXME: Compute the digest with respect to the raw bytes received from the page.
    // See <https://bugs.webkit.org/show_bug.cgi?id=155184>.
    TextEncoding documentEncoding = element.document().textEncoding();
    const TextEncoding& encodingToUse = documentEncoding.isValid() ? documentEncoding : UTF8Encoding();
    auto content = encodingToUse.encode(TextNodeTraversal::contentsAsString(element), UnencodableHandling::Entities);
    auto cryptoDigest = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    cryptoDigest->addBytes(content.data(), content.size());
    auto digest = cryptoDigest->computeHash();
    return makeString("sha256-", base64Encode(digest.data(), digest.size()));
}

Ref<Protocol::DOM::Node> InspectorDOMAgent::buildObjectForNode(Node* node, int depth, NodeToIdMap* nodesMap)
{
    auto id = bind(node, nodesMap);
    String nodeName;
    String localName;
    String nodeValue;

    switch (node->nodeType()) {
    case Node::PROCESSING_INSTRUCTION_NODE:
        nodeName = node->nodeName();
        localName = node->localName();
        FALLTHROUGH;
    case Node::TEXT_NODE:
    case Node::COMMENT_NODE:
    case Node::CDATA_SECTION_NODE:
        nodeValue = node->nodeValue();
        if (nodeValue.length() > maxTextSize) {
            nodeValue = nodeValue.left(maxTextSize);
            nodeValue.append(ellipsisUChar);
        }
        break;
    case Node::ATTRIBUTE_NODE:
        localName = node->localName();
        break;
    case Node::DOCUMENT_FRAGMENT_NODE:
    case Node::DOCUMENT_NODE:
    case Node::ELEMENT_NODE:
    default:
        nodeName = node->nodeName();
        localName = node->localName();
        break;
    }

    auto value = Protocol::DOM::Node::create()
        .setNodeId(id)
        .setNodeType(static_cast<int>(node->nodeType()))
        .setNodeName(nodeName)
        .setLocalName(localName)
        .setNodeValue(nodeValue)
        .release();

    if (node->isContainerNode()) {
        int nodeCount = innerChildNodeCount(node);
        value->setChildNodeCount(nodeCount);
        auto children = buildArrayForContainerChildren(node, depth, nodesMap);
        if (children->length() > 0)
            value->setChildren(WTFMove(children));
    }

    auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
    if (pageAgent) {
        if (auto* frameView = node->document().view())
            value->setFrameId(pageAgent->frameId(&frameView->frame()));
    }

    if (is<Element>(*node)) {
        Element& element = downcast<Element>(*node);
        value->setAttributes(buildArrayForElementAttributes(&element));
        if (is<HTMLFrameOwnerElement>(element)) {
            if (auto* document = downcast<HTMLFrameOwnerElement>(element).contentDocument())
                value->setContentDocument(buildObjectForNode(document, 0, nodesMap));
        }

        if (ShadowRoot* root = element.shadowRoot()) {
            auto shadowRoots = JSON::ArrayOf<Protocol::DOM::Node>::create();
            shadowRoots->addItem(buildObjectForNode(root, 0, nodesMap));
            value->setShadowRoots(WTFMove(shadowRoots));
        }

        if (is<HTMLTemplateElement>(element))
            value->setTemplateContent(buildObjectForNode(&downcast<HTMLTemplateElement>(element).content(), 0, nodesMap));

        if (is<HTMLStyleElement>(element) || (is<HTMLScriptElement>(element) && !element.hasAttributeWithoutSynchronization(HTMLNames::srcAttr)))
            value->setContentSecurityPolicyHash(computeContentSecurityPolicySHA256Hash(element));

        auto state = customElementState(element);
        if (state != Protocol::DOM::CustomElementState::Builtin)
            value->setCustomElementState(state);

        if (element.pseudoId() != PseudoId::None) {
            Protocol::DOM::PseudoType pseudoType;
            if (pseudoElementType(element.pseudoId(), &pseudoType))
                value->setPseudoType(pseudoType);
        } else {
            if (auto pseudoElements = buildArrayForPseudoElements(element, nodesMap))
                value->setPseudoElements(pseudoElements.releaseNonNull());
        }
    } else if (is<Document>(*node)) {
        Document& document = downcast<Document>(*node);
        if (pageAgent)
            value->setFrameId(pageAgent->frameId(document.frame()));
        value->setDocumentURL(documentURLString(&document));
        value->setBaseURL(documentBaseURLString(&document));
        value->setXmlVersion(document.xmlVersion());
    } else if (is<DocumentType>(*node)) {
        DocumentType& docType = downcast<DocumentType>(*node);
        value->setPublicId(docType.publicId());
        value->setSystemId(docType.systemId());
    } else if (is<Attr>(*node)) {
        Attr& attribute = downcast<Attr>(*node);
        value->setName(attribute.name());
        value->setValue(attribute.value());
    } else if (is<ShadowRoot>(*node)) {
        ShadowRoot& shadowRoot = downcast<ShadowRoot>(*node);
        value->setShadowRootType(shadowRootType(shadowRoot.mode()));
    }

    return value;
}

Ref<JSON::ArrayOf<String>> InspectorDOMAgent::buildArrayForElementAttributes(Element* element)
{
    auto attributesValue = JSON::ArrayOf<String>::create();
    // Go through all attributes and serialize them.
    if (!element->hasAttributes())
        return attributesValue;
    for (const Attribute& attribute : element->attributesIterator()) {
        // Add attribute pair
        attributesValue->addItem(attribute.name().toString());
        attributesValue->addItem(attribute.value());
    }
    return attributesValue;
}

Ref<JSON::ArrayOf<Protocol::DOM::Node>> InspectorDOMAgent::buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap)
{
    auto children = JSON::ArrayOf<Protocol::DOM::Node>::create();
    if (depth == 0) {
        // Special-case the only text child - pretend that container's children have been requested.
        Node* firstChild = container->firstChild();
        if (firstChild && firstChild->nodeType() == Node::TEXT_NODE && !firstChild->nextSibling()) {
            children->addItem(buildObjectForNode(firstChild, 0, nodesMap));
            m_childrenRequested.add(bind(container, nodesMap));
        }
        return children;
    }

    Node* child = innerFirstChild(container);
    depth--;
    m_childrenRequested.add(bind(container, nodesMap));

    while (child) {
        children->addItem(buildObjectForNode(child, depth, nodesMap));
        child = innerNextSibling(child);
    }
    return children;
}

RefPtr<JSON::ArrayOf<Protocol::DOM::Node>> InspectorDOMAgent::buildArrayForPseudoElements(const Element& element, NodeToIdMap* nodesMap)
{
    PseudoElement* beforeElement = element.beforePseudoElement();
    PseudoElement* afterElement = element.afterPseudoElement();
    if (!beforeElement && !afterElement)
        return nullptr;

    auto pseudoElements = JSON::ArrayOf<Protocol::DOM::Node>::create();
    if (beforeElement)
        pseudoElements->addItem(buildObjectForNode(beforeElement, 0, nodesMap));
    if (afterElement)
        pseudoElements->addItem(buildObjectForNode(afterElement, 0, nodesMap));
    return pseudoElements;
}

Ref<Protocol::DOM::EventListener> InspectorDOMAgent::buildObjectForEventListener(const RegisteredEventListener& registeredEventListener, Protocol::DOM::EventListenerId identifier, EventTarget& eventTarget, const AtomString& eventType, bool disabled, const RefPtr<JSC::Breakpoint>& breakpoint)
{
    Ref<EventListener> eventListener = registeredEventListener.callback();

    String handlerName;
    int lineNumber = 0;
    int columnNumber = 0;
    String scriptID;
    if (is<JSEventListener>(eventListener.get())) {
        auto& scriptListener = downcast<JSEventListener>(eventListener.get());

        Document* document = nullptr;
        if (auto* scriptExecutionContext = eventTarget.scriptExecutionContext()) {
            if (is<Document>(scriptExecutionContext))
                document = downcast<Document>(scriptExecutionContext);
        } else if (is<Node>(eventTarget))
            document = &downcast<Node>(eventTarget).document();

        JSC::JSObject* handlerObject = nullptr;
        JSC::JSGlobalObject* exec = nullptr;

        JSC::JSLockHolder lock(scriptListener.isolatedWorld().vm());

        if (document) {
            handlerObject = scriptListener.ensureJSFunction(*document);
            exec = execStateFromNode(scriptListener.isolatedWorld(), document);
        }

        if (handlerObject && exec) {
            JSC::VM& vm = exec->vm();
            JSC::JSFunction* handlerFunction = JSC::jsDynamicCast<JSC::JSFunction*>(vm, handlerObject);

            if (!handlerFunction) {
                auto scope = DECLARE_CATCH_SCOPE(vm);

                // If the handler is not actually a function, see if it implements the EventListener interface and use that.
                auto handleEventValue = handlerObject->get(exec, JSC::Identifier::fromString(vm, "handleEvent"));

                if (UNLIKELY(scope.exception()))
                    scope.clearException();

                if (handleEventValue)
                    handlerFunction = JSC::jsDynamicCast<JSC::JSFunction*>(vm, handleEventValue);
            }

            if (handlerFunction && !handlerFunction->isHostOrBuiltinFunction()) {
                // If the listener implements the EventListener interface, use the class name instead of
                // "handleEvent", unless it is a plain object.
                if (handlerFunction != handlerObject)
                    handlerName = JSC::JSObject::calculatedClassName(handlerObject);
                if (handlerName.isEmpty() || handlerName == "Object"_s)
                    handlerName = handlerFunction->calculatedDisplayName(vm);

                if (auto executable = handlerFunction->jsExecutable()) {
                    lineNumber = executable->firstLine() - 1;
                    columnNumber = executable->startColumn() - 1;
                    scriptID = executable->sourceID() == JSC::SourceProvider::nullID ? emptyString() : String::number(executable->sourceID());
                }
            }
        }
    }

    auto value = Protocol::DOM::EventListener::create()
        .setEventListenerId(identifier)
        .setType(eventType)
        .setUseCapture(registeredEventListener.useCapture())
        .setIsAttribute(eventListener->isAttribute())
        .release();
    if (is<Node>(eventTarget))
        value->setNodeId(pushNodePathToFrontend(&downcast<Node>(eventTarget)));
    else if (is<DOMWindow>(eventTarget))
        value->setOnWindow(true);
    if (!scriptID.isNull()) {
        auto location = Protocol::Debugger::Location::create()
            .setScriptId(scriptID)
            .setLineNumber(lineNumber)
            .release();
        location->setColumnNumber(columnNumber);
        value->setLocation(WTFMove(location));
    }
    if (!handlerName.isEmpty())
        value->setHandlerName(handlerName);
    if (registeredEventListener.isPassive())
        value->setPassive(true);
    if (registeredEventListener.isOnce())
        value->setOnce(true);
    if (disabled)
        value->setDisabled(disabled);
    if (breakpoint)
        value->setHasBreakpoint(breakpoint);
    return value;
}

void InspectorDOMAgent::processAccessibilityChildren(AXCoreObject& axObject, JSON::ArrayOf<Protocol::DOM::NodeId>& childNodeIds)
{
    const auto& children = axObject.children();
    if (!children.size())
        return;

    for (const auto& childObject : children) {
        if (Node* childNode = childObject->node())
            childNodeIds.addItem(pushNodePathToFrontend(childNode));
        else
            processAccessibilityChildren(*childObject, childNodeIds);
    }
}
    
Ref<Protocol::DOM::AccessibilityProperties> InspectorDOMAgent::buildObjectForAccessibilityProperties(Node& node)
{
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();

    Node* activeDescendantNode = nullptr;
    bool busy = false;
    auto checked = Protocol::DOM::AccessibilityProperties::Checked::False;
    RefPtr<JSON::ArrayOf<Protocol::DOM::NodeId>> childNodeIds;
    RefPtr<JSON::ArrayOf<Protocol::DOM::NodeId>> controlledNodeIds;
    auto currentState = Protocol::DOM::AccessibilityProperties::Current::False;
    bool exists = false;
    bool expanded = false;
    bool disabled = false;
    RefPtr<JSON::ArrayOf<Protocol::DOM::NodeId>> flowedNodeIds;
    bool focused = false;
    bool ignored = true;
    bool ignoredByDefault = false;
    auto invalid = Protocol::DOM::AccessibilityProperties::Invalid::False;
    bool hidden = false;
    String label;
    bool liveRegionAtomic = false;
    RefPtr<JSON::ArrayOf<String>> liveRegionRelevant;
    auto liveRegionStatus = Protocol::DOM::AccessibilityProperties::LiveRegionStatus::Off;
    Node* mouseEventNode = nullptr;
    RefPtr<JSON::ArrayOf<Protocol::DOM::NodeId>> ownedNodeIds;
    Node* parentNode = nullptr;
    bool pressed = false;
    bool readonly = false;
    bool required = false;
    String role;
    bool selected = false;
    RefPtr<JSON::ArrayOf<Protocol::DOM::NodeId>> selectedChildNodeIds;
    bool supportsChecked = false;
    bool supportsExpanded = false;
    bool supportsLiveRegion = false;
    bool supportsPressed = false;
    bool supportsRequired = false;
    bool supportsFocused = false;
    bool isPopupButton = false;
    int headingLevel = 0;
    unsigned hierarchicalLevel = 0;
    unsigned level = 0;

    if (AXObjectCache* axObjectCache = node.document().axObjectCache()) {
        if (AXCoreObject* axObject = axObjectCache->getOrCreate(&node)) {

            if (AXCoreObject* activeDescendant = axObject->activeDescendant())
                activeDescendantNode = activeDescendant->node();

            // An AX object is "busy" if it or any ancestor has aria-busy="true" set.
            AXCoreObject* current = axObject;
            while (!busy && current) {
                busy = current->isBusy();
                current = current->parentObject();
            }

            supportsChecked = axObject->supportsChecked();
            if (supportsChecked) {
                AccessibilityButtonState checkValue = axObject->checkboxOrRadioValue(); // Element using aria-checked.
                if (checkValue == AccessibilityButtonState::On)
                    checked = Protocol::DOM::AccessibilityProperties::Checked::True;
                else if (checkValue == AccessibilityButtonState::Mixed)
                    checked = Protocol::DOM::AccessibilityProperties::Checked::Mixed;
                else if (axObject->isChecked()) // Native checkbox.
                    checked = Protocol::DOM::AccessibilityProperties::Checked::True;
            }
            
            if (!axObject->children().isEmpty()) {
                childNodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
                processAccessibilityChildren(*axObject, *childNodeIds);
            }
            
            Vector<Element*> controlledElements;
            axObject->elementsFromAttribute(controlledElements, aria_controlsAttr);
            if (controlledElements.size()) {
                controlledNodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
                for (Element* controlledElement : controlledElements) {
                    if (auto controlledElementId = pushNodePathToFrontend(controlledElement))
                        controlledNodeIds->addItem(controlledElementId);
                }
            }
            
            switch (axObject->currentState()) {
            case AccessibilityCurrentState::False:
                currentState = Protocol::DOM::AccessibilityProperties::Current::False;
                break;
            case AccessibilityCurrentState::Page:
                currentState = Protocol::DOM::AccessibilityProperties::Current::Page;
                break;
            case AccessibilityCurrentState::Step:
                currentState = Protocol::DOM::AccessibilityProperties::Current::Step;
                break;
            case AccessibilityCurrentState::Location:
                currentState = Protocol::DOM::AccessibilityProperties::Current::Location;
                break;
            case AccessibilityCurrentState::Date:
                currentState = Protocol::DOM::AccessibilityProperties::Current::Date;
                break;
            case AccessibilityCurrentState::Time:
                currentState = Protocol::DOM::AccessibilityProperties::Current::Time;
                break;
            case AccessibilityCurrentState::True:
                currentState = Protocol::DOM::AccessibilityProperties::Current::True;
                break;
            }

            disabled = !axObject->isEnabled();
            exists = true;
            
            supportsExpanded = axObject->supportsExpanded();
            if (supportsExpanded)
                expanded = axObject->isExpanded();

            Vector<Element*> flowedElements;
            axObject->elementsFromAttribute(flowedElements, aria_flowtoAttr);
            if (flowedElements.size()) {
                flowedNodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
                for (Element* flowedElement : flowedElements) {
                    if (auto flowedElementId = pushNodePathToFrontend(flowedElement))
                        flowedNodeIds->addItem(flowedElementId);
                }
            }
            
            if (is<Element>(node)) {
                supportsFocused = axObject->canSetFocusAttribute();
                if (supportsFocused)
                    focused = axObject->isFocused();
            }

            ignored = axObject->accessibilityIsIgnored();
            ignoredByDefault = axObject->accessibilityIsIgnoredByDefault();
            
            String invalidValue = axObject->invalidStatus();
            if (invalidValue == "false")
                invalid = Protocol::DOM::AccessibilityProperties::Invalid::False;
            else if (invalidValue == "grammar")
                invalid = Protocol::DOM::AccessibilityProperties::Invalid::Grammar;
            else if (invalidValue == "spelling")
                invalid = Protocol::DOM::AccessibilityProperties::Invalid::Spelling;
            else // Future versions of ARIA may allow additional truthy values. Ex. format, order, or size.
                invalid = Protocol::DOM::AccessibilityProperties::Invalid::True;
            
            if (axObject->isAXHidden() || axObject->isDOMHidden())
                hidden = true;
            
            label = axObject->computedLabel();

            if (axObject->supportsLiveRegion()) {
                supportsLiveRegion = true;
                liveRegionAtomic = axObject->liveRegionAtomic();

                String ariaRelevantAttrValue = axObject->liveRegionRelevant();
                if (!ariaRelevantAttrValue.isEmpty()) {
                    // FIXME: Pass enum values rather than strings once unblocked. http://webkit.org/b/133711
                    String ariaRelevantAdditions = Protocol::Helpers::getEnumConstantValue(Protocol::DOM::LiveRegionRelevant::Additions);
                    String ariaRelevantRemovals = Protocol::Helpers::getEnumConstantValue(Protocol::DOM::LiveRegionRelevant::Removals);
                    String ariaRelevantText = Protocol::Helpers::getEnumConstantValue(Protocol::DOM::LiveRegionRelevant::Text);
                    liveRegionRelevant = JSON::ArrayOf<String>::create();
                    const SpaceSplitString& values = SpaceSplitString(ariaRelevantAttrValue, true);
                    // @aria-relevant="all" is exposed as ["additions","removals","text"], in order.
                    // This order is controlled in WebCore and expected in WebInspectorUI.
                    if (values.contains("all")) {
                        liveRegionRelevant->addItem(ariaRelevantAdditions);
                        liveRegionRelevant->addItem(ariaRelevantRemovals);
                        liveRegionRelevant->addItem(ariaRelevantText);
                    } else {
                        if (values.contains(ariaRelevantAdditions))
                            liveRegionRelevant->addItem(ariaRelevantAdditions);
                        if (values.contains(ariaRelevantRemovals))
                            liveRegionRelevant->addItem(ariaRelevantRemovals);
                        if (values.contains(ariaRelevantText))
                            liveRegionRelevant->addItem(ariaRelevantText);
                    }
                }

                String ariaLive = axObject->liveRegionStatus();
                if (ariaLive == "assertive")
                    liveRegionStatus = Protocol::DOM::AccessibilityProperties::LiveRegionStatus::Assertive;
                else if (ariaLive == "polite")
                    liveRegionStatus = Protocol::DOM::AccessibilityProperties::LiveRegionStatus::Polite;
            }

            if (is<AccessibilityNodeObject>(*axObject))
                mouseEventNode = downcast<AccessibilityNodeObject>(*axObject).mouseButtonListener(MouseButtonListenerResultFilter::IncludeBodyElement);

            if (axObject->supportsARIAOwns()) {
                Vector<Element*> ownedElements;
                axObject->elementsFromAttribute(ownedElements, aria_ownsAttr);
                if (ownedElements.size()) {
                    ownedNodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
                    for (Element* ownedElement : ownedElements) {
                        if (auto ownedElementId = pushNodePathToFrontend(ownedElement))
                            ownedNodeIds->addItem(ownedElementId);
                    }
                }
            }

            if (AXCoreObject* parentObject = axObject->parentObjectUnignored())
                parentNode = parentObject->node();

            supportsPressed = axObject->pressedIsPresent();
            if (supportsPressed)
                pressed = axObject->isPressed();
            
            if (axObject->isTextControl())
                readonly = !axObject->canSetValueAttribute();

            supportsRequired = axObject->supportsRequiredAttribute();
            if (supportsRequired)
                required = axObject->isRequired();
            
            role = axObject->computedRoleString();
            selected = axObject->isSelected();

            AXCoreObject::AccessibilityChildrenVector selectedChildren;
            axObject->selectedChildren(selectedChildren);
            if (selectedChildren.size()) {
                selectedChildNodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
                for (auto& selectedChildObject : selectedChildren) {
                    if (Node* selectedChildNode = selectedChildObject->node()) {
                        if (auto selectedChildNodeId = pushNodePathToFrontend(selectedChildNode))
                            selectedChildNodeIds->addItem(selectedChildNodeId);
                    }
                }
            }
            
            headingLevel = axObject->headingLevel();
            hierarchicalLevel = axObject->hierarchicalLevel();
            
            level = hierarchicalLevel ? hierarchicalLevel : headingLevel;
            isPopupButton = axObject->isPopUpButton() || axObject->hasPopup();
        }
    }
    
    auto value = Protocol::DOM::AccessibilityProperties::create()
        .setExists(exists)
        .setLabel(label)
        .setRole(role)
        .setNodeId(pushNodePathToFrontend(&node))
        .release();

    if (exists) {
        if (activeDescendantNode) {
            if (auto activeDescendantNodeId = pushNodePathToFrontend(activeDescendantNode))
                value->setActiveDescendantNodeId(activeDescendantNodeId);
        }
        if (busy)
            value->setBusy(busy);
        if (supportsChecked)
            value->setChecked(checked);
        if (childNodeIds)
            value->setChildNodeIds(childNodeIds.releaseNonNull());
        if (controlledNodeIds)
            value->setControlledNodeIds(controlledNodeIds.releaseNonNull());
        if (currentState != Protocol::DOM::AccessibilityProperties::Current::False)
            value->setCurrent(currentState);
        if (disabled)
            value->setDisabled(disabled);
        if (supportsExpanded)
            value->setExpanded(expanded);
        if (flowedNodeIds)
            value->setFlowedNodeIds(flowedNodeIds.releaseNonNull());
        if (supportsFocused)
            value->setFocused(focused);
        if (ignored)
            value->setIgnored(ignored);
        if (ignoredByDefault)
            value->setIgnoredByDefault(ignoredByDefault);
        if (invalid != Protocol::DOM::AccessibilityProperties::Invalid::False)
            value->setInvalid(invalid);
        if (hidden)
            value->setHidden(hidden);
        if (supportsLiveRegion) {
            value->setLiveRegionAtomic(liveRegionAtomic);
            if (liveRegionRelevant->length())
                value->setLiveRegionRelevant(liveRegionRelevant.releaseNonNull());
            value->setLiveRegionStatus(liveRegionStatus);
        }
        if (mouseEventNode) {
            if (auto mouseEventNodeId = pushNodePathToFrontend(mouseEventNode))
                value->setMouseEventNodeId(mouseEventNodeId);
        }
        if (ownedNodeIds)
            value->setOwnedNodeIds(ownedNodeIds.releaseNonNull());
        if (parentNode) {
            if (auto parentNodeId = pushNodePathToFrontend(parentNode))
                value->setParentNodeId(parentNodeId);
        }
        if (supportsPressed)
            value->setPressed(pressed);
        if (readonly)
            value->setReadonly(readonly);
        if (supportsRequired)
            value->setRequired(required);
        if (selected)
            value->setSelected(selected);
        if (selectedChildNodeIds)
            value->setSelectedChildNodeIds(selectedChildNodeIds.releaseNonNull());
        
        // H1 -- H6 always have a headingLevel property that can be complimented by a hierarchicalLevel
        // property when aria-level is set on the element, in which case we want to remain calling
        // this value the "Heading Level" in the inspector.
        // Also, we do not want it to say Hierarchy Level: 0
        if (headingLevel)
            value->setHeadingLevel(level);
        else if (level)
            value->setHierarchyLevel(level);
        if (isPopupButton)
            value->setIsPopUpButton(isPopupButton);
    }

    return value;
}

static bool containsOnlyHTMLWhitespace(Node* node)
{
    // FIXME: Respect ignoreWhitespace setting from inspector front end?
    return is<Text>(node) && downcast<Text>(*node).data().isAllSpecialCharacters<isHTMLSpace>();
}

Node* InspectorDOMAgent::innerFirstChild(Node* node)
{
    node = node->firstChild();
    while (containsOnlyHTMLWhitespace(node))
        node = node->nextSibling();
    return node;
}

Node* InspectorDOMAgent::innerNextSibling(Node* node)
{
    do {
        node = node->nextSibling();
    } while (containsOnlyHTMLWhitespace(node));
    return node;
}

Node* InspectorDOMAgent::innerPreviousSibling(Node* node)
{
    do {
        node = node->previousSibling();
    } while (containsOnlyHTMLWhitespace(node));
    return node;
}

unsigned InspectorDOMAgent::innerChildNodeCount(Node* node)
{
    unsigned count = 0;
    for (Node* child = innerFirstChild(node); child; child = innerNextSibling(child))
        ++count;
    return count;
}

Node* InspectorDOMAgent::innerParentNode(Node* node)
{
    ASSERT(node);
    if (is<Document>(*node))
        return downcast<Document>(*node).ownerElement();
    if (is<ShadowRoot>(*node))
        return downcast<ShadowRoot>(*node).host();
    return node->parentNode();
}

void InspectorDOMAgent::didCommitLoad(Document* document)
{
    if (m_nodeToFocus && &m_nodeToFocus->document() == document)
        m_nodeToFocus = nullptr;

    if (m_mousedOverNode && &m_mousedOverNode->document() == document)
        m_mousedOverNode = nullptr;

    if (m_inspectedNode && &m_inspectedNode->document() == document)
        m_inspectedNode = nullptr;

    RefPtr<Element> frameOwner = document->ownerElement();
    if (!frameOwner)
        return;

    auto frameOwnerId = m_documentNodeToIdMap.get(frameOwner);
    if (!frameOwnerId)
        return;

    // Re-add frame owner element together with its new children.
    auto parentId = m_documentNodeToIdMap.get(innerParentNode(frameOwner.get()));
    m_frontendDispatcher->childNodeRemoved(parentId, frameOwnerId);
    unbind(frameOwner.get(), &m_documentNodeToIdMap);

    auto value = buildObjectForNode(frameOwner.get(), 0, &m_documentNodeToIdMap);
    Node* previousSibling = innerPreviousSibling(frameOwner.get());
    auto prevId = previousSibling ? m_documentNodeToIdMap.get(previousSibling) : 0;
    m_frontendDispatcher->childNodeInserted(parentId, prevId, WTFMove(value));
}

Protocol::DOM::NodeId InspectorDOMAgent::identifierForNode(Node& node)
{
    return pushNodePathToFrontend(&node);
}

void InspectorDOMAgent::addEventListenersToNode(Node& node)
{
#if ENABLE(VIDEO)
    auto callback = EventFiredCallback::create(*this);

    auto createEventListener = [&] (const AtomString& eventName) {
        node.addEventListener(eventName, callback.copyRef(), false);
    };

#if ENABLE(FULLSCREEN_API)
    if (is<Document>(node) || is<HTMLMediaElement>(node))
        createEventListener(eventNames().webkitfullscreenchangeEvent);
#endif // ENABLE(FULLSCREEN_API)

    if (is<HTMLMediaElement>(node)) {
        createEventListener(eventNames().abortEvent);
        createEventListener(eventNames().canplayEvent);
        createEventListener(eventNames().canplaythroughEvent);
        createEventListener(eventNames().emptiedEvent);
        createEventListener(eventNames().endedEvent);
        createEventListener(eventNames().loadeddataEvent);
        createEventListener(eventNames().loadedmetadataEvent);
        createEventListener(eventNames().loadstartEvent);
        createEventListener(eventNames().pauseEvent);
        createEventListener(eventNames().playEvent);
        createEventListener(eventNames().playingEvent);
        createEventListener(eventNames().seekedEvent);
        createEventListener(eventNames().seekingEvent);
        createEventListener(eventNames().stalledEvent);
        createEventListener(eventNames().suspendEvent);
        createEventListener(eventNames().waitingEvent);

        if (!m_mediaMetricsTimer.isActive())
            m_mediaMetricsTimer.start(0_s, 1_s / 15.);
    }
#else
    UNUSED_PARAM(node);
#endif // ENABLE(VIDEO)
}

void InspectorDOMAgent::didInsertDOMNode(Node& node)
{
    if (containsOnlyHTMLWhitespace(&node))
        return;

    // We could be attaching existing subtree. Forget the bindings.
    unbind(&node, &m_documentNodeToIdMap);

    ContainerNode* parent = node.parentNode();
    if (!parent)
        return;

    auto parentId = m_documentNodeToIdMap.get(parent);
    // Return if parent is not mapped yet.
    if (!parentId)
        return;

    if (!m_childrenRequested.contains(parentId)) {
        // No children are mapped yet -> only notify on changes of hasChildren.
        m_frontendDispatcher->childNodeCountUpdated(parentId, innerChildNodeCount(parent));
    } else {
        // Children have been requested -> return value of a new child.
        Node* prevSibling = innerPreviousSibling(&node);
        auto prevId = prevSibling ? m_documentNodeToIdMap.get(prevSibling) : 0;
        auto value = buildObjectForNode(&node, 0, &m_documentNodeToIdMap);
        m_frontendDispatcher->childNodeInserted(parentId, prevId, WTFMove(value));
    }
}

void InspectorDOMAgent::didRemoveDOMNode(Node& node)
{
    if (containsOnlyHTMLWhitespace(&node))
        return;

    ContainerNode* parent = node.parentNode();

    // If parent is not mapped yet -> ignore the event.
    if (!m_documentNodeToIdMap.contains(parent))
        return;

    auto parentId = m_documentNodeToIdMap.get(parent);

    if (!m_childrenRequested.contains(parentId)) {
        // No children are mapped yet -> only notify on changes of hasChildren.
        if (innerChildNodeCount(parent) == 1)
            m_frontendDispatcher->childNodeCountUpdated(parentId, 0);
    } else
        m_frontendDispatcher->childNodeRemoved(parentId, m_documentNodeToIdMap.get(&node));
    unbind(&node, &m_documentNodeToIdMap);
}

void InspectorDOMAgent::willModifyDOMAttr(Element&, const AtomString& oldValue, const AtomString& newValue)
{
    m_suppressAttributeModifiedEvent = (oldValue == newValue);
}

void InspectorDOMAgent::didModifyDOMAttr(Element& element, const AtomString& name, const AtomString& value)
{
    bool shouldSuppressEvent = m_suppressAttributeModifiedEvent;
    m_suppressAttributeModifiedEvent = false;
    if (shouldSuppressEvent)
        return;

    auto id = boundNodeId(&element);
    if (!id)
        return;

    if (auto* cssAgent = m_instrumentingAgents.enabledCSSAgent())
        cssAgent->didModifyDOMAttr(element);

    m_frontendDispatcher->attributeModified(id, name, value);
}

void InspectorDOMAgent::didRemoveDOMAttr(Element& element, const AtomString& name)
{
    auto id = boundNodeId(&element);
    if (!id)
        return;

    if (auto* cssAgent = m_instrumentingAgents.enabledCSSAgent())
        cssAgent->didModifyDOMAttr(element);

    m_frontendDispatcher->attributeRemoved(id, name);
}

void InspectorDOMAgent::styleAttributeInvalidated(const Vector<Element*>& elements)
{
    auto nodeIds = JSON::ArrayOf<Protocol::DOM::NodeId>::create();
    for (auto& element : elements) {
        auto id = boundNodeId(element);
        if (!id)
            continue;

        if (auto* cssAgent = m_instrumentingAgents.enabledCSSAgent())
            cssAgent->didModifyDOMAttr(*element);

        nodeIds->addItem(id);
    }
    m_frontendDispatcher->inlineStyleInvalidated(WTFMove(nodeIds));
}

void InspectorDOMAgent::characterDataModified(CharacterData& characterData)
{
    auto id = m_documentNodeToIdMap.get(&characterData);
    if (!id) {
        // Push text node if it is being created.
        didInsertDOMNode(characterData);
        return;
    }
    m_frontendDispatcher->characterDataModified(id, characterData.data());
}

void InspectorDOMAgent::didInvalidateStyleAttr(Element& element)
{
    auto id = m_documentNodeToIdMap.get(&element);
    if (!id)
        return;

    if (!m_revalidateStyleAttrTask)
        m_revalidateStyleAttrTask = makeUnique<RevalidateStyleAttributeTask>(this);
    m_revalidateStyleAttrTask->scheduleFor(&element);
}

void InspectorDOMAgent::didPushShadowRoot(Element& host, ShadowRoot& root)
{
    auto hostId = m_documentNodeToIdMap.get(&host);
    if (hostId)
        m_frontendDispatcher->shadowRootPushed(hostId, buildObjectForNode(&root, 0, &m_documentNodeToIdMap));
}

void InspectorDOMAgent::willPopShadowRoot(Element& host, ShadowRoot& root)
{
    auto hostId = m_documentNodeToIdMap.get(&host);
    auto rootId = m_documentNodeToIdMap.get(&root);
    if (hostId && rootId)
        m_frontendDispatcher->shadowRootPopped(hostId, rootId);
}

void InspectorDOMAgent::didChangeCustomElementState(Element& element)
{
    auto elementId = m_documentNodeToIdMap.get(&element);
    if (!elementId)
        return;

    m_frontendDispatcher->customElementStateChanged(elementId, customElementState(element));
}

void InspectorDOMAgent::frameDocumentUpdated(Frame& frame)
{
    Document* document = frame.document();
    if (!document)
        return;

    if (!frame.isMainFrame())
        return;

    // Only update the main frame document, nested frame document updates are not required
    // (will be handled by didCommitLoad()).
    setDocument(document);
}

void InspectorDOMAgent::pseudoElementCreated(PseudoElement& pseudoElement)
{
    Element* parent = pseudoElement.hostElement();
    if (!parent)
        return;

    auto parentId = m_documentNodeToIdMap.get(parent);
    if (!parentId)
        return;

    pushChildNodesToFrontend(parentId, 1);
    m_frontendDispatcher->pseudoElementAdded(parentId, buildObjectForNode(&pseudoElement, 0, &m_documentNodeToIdMap));
}

void InspectorDOMAgent::pseudoElementDestroyed(PseudoElement& pseudoElement)
{
    auto pseudoElementId = m_documentNodeToIdMap.get(&pseudoElement);
    if (!pseudoElementId)
        return;

    // If a PseudoElement is bound, its parent element must have been bound.
    Element* parent = pseudoElement.hostElement();
    ASSERT(parent);
    auto parentId = m_documentNodeToIdMap.get(parent);
    ASSERT(parentId);

    unbind(&pseudoElement, &m_documentNodeToIdMap);
    m_frontendDispatcher->pseudoElementRemoved(parentId, pseudoElementId);
}

void InspectorDOMAgent::didAddEventListener(EventTarget& target)
{
    if (!is<Node>(target))
        return;

    auto& node = downcast<Node>(target);
    if (!node.contains(m_inspectedNode.get()))
        return;

    auto nodeId = boundNodeId(&node);
    if (!nodeId)
        return;

    if (m_suppressEventListenerChangedEvent)
        return;

    m_suppressEventListenerChangedEvent = true;

    m_frontendDispatcher->didAddEventListener(nodeId);
}

void InspectorDOMAgent::willRemoveEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    if (!is<Node>(target))
        return;

    auto& node = downcast<Node>(target);
    if (!node.contains(m_inspectedNode.get()))
        return;

    auto nodeId = boundNodeId(&node);
    if (!nodeId)
        return;

    bool listenerExists = false;
    for (auto& item : node.eventListeners(eventType)) {
        if (item->callback() == listener && item->useCapture() == capture) {
            listenerExists = true;
            break;
        }
    }

    if (!listenerExists)
        return;

    m_eventListenerEntries.removeIf([&] (auto& entry) {
        return entry.value.matches(target, eventType, listener, capture);
    });

    if (m_suppressEventListenerChangedEvent)
        return;

    m_suppressEventListenerChangedEvent = true;

    m_frontendDispatcher->willRemoveEventListener(nodeId);
}

bool InspectorDOMAgent::isEventListenerDisabled(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    for (auto& inspectorEventListener : m_eventListenerEntries.values()) {
        if (inspectorEventListener.matches(target, eventType, listener, capture))
            return inspectorEventListener.disabled;
    }
    return false;
}

void InspectorDOMAgent::eventDidResetAfterDispatch(const Event& event)
{
    m_dispatchedEvents.remove(&event);
}

RefPtr<JSC::Breakpoint> InspectorDOMAgent::breakpointForEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    for (auto& inspectorEventListener : m_eventListenerEntries.values()) {
        if (inspectorEventListener.matches(target, eventType, listener, capture))
            return inspectorEventListener.breakpoint;
    }
    return nullptr;
}

Protocol::DOM::EventListenerId InspectorDOMAgent::idForEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    for (auto& inspectorEventListener : m_eventListenerEntries.values()) {
        if (inspectorEventListener.matches(target, eventType, listener, capture))
            return inspectorEventListener.identifier;
    }
    return 0;
}

#if ENABLE(VIDEO)
void InspectorDOMAgent::mediaMetricsTimerFired()
{
    // FIXME: remove metrics information for any media element when it's destroyed

    if (HTMLMediaElement::allMediaElements().isEmpty()) {
        if (m_mediaMetricsTimer.isActive())
            m_mediaMetricsTimer.stop();
        m_mediaMetrics.clear();
        return;
    }

    for (auto* mediaElement : HTMLMediaElement::allMediaElements()) {
        if (!is<HTMLVideoElement>(mediaElement) || !mediaElement->isPlaying())
            continue;

        auto videoPlaybackQuality = mediaElement->getVideoPlaybackQuality();
        unsigned displayCompositedVideoFrames = videoPlaybackQuality->displayCompositedVideoFrames();

        auto iterator = m_mediaMetrics.find(mediaElement);
        if (iterator == m_mediaMetrics.end()) {
            m_mediaMetrics.set(mediaElement, MediaMetrics(displayCompositedVideoFrames));
            continue;
        }

        bool isPowerEfficient = (displayCompositedVideoFrames - iterator->value.displayCompositedFrames) > 0;
        if (iterator->value.isPowerEfficient != isPowerEfficient) {
            iterator->value.isPowerEfficient = isPowerEfficient;

            if (auto nodeId = pushNodePathToFrontend(mediaElement)) {
                auto timestamp = m_environment.executionStopwatch().elapsedTime().seconds();
                m_frontendDispatcher->powerEfficientPlaybackStateChanged(nodeId, timestamp, iterator->value.isPowerEfficient);
            }
        }

        iterator->value.displayCompositedFrames = displayCompositedVideoFrames;
    }

    m_mediaMetrics.removeIf([&] (auto& entry) {
        return !HTMLMediaElement::allMediaElements().contains(entry.key);
    });
}
#endif

Node* InspectorDOMAgent::nodeForPath(const String& path)
{
    // The path is of form "1,HTML,2,BODY,1,DIV"
    if (!m_document)
        return nullptr;

    Node* node = m_document.get();
    Vector<String> pathTokens = path.split(',');
    if (!pathTokens.size())
        return nullptr;

    for (size_t i = 0; i < pathTokens.size() - 1; i += 2) {
        bool success = true;
        unsigned childNumber = pathTokens[i].toUInt(&success);
        if (!success)
            return nullptr;

        Node* child;
        if (is<HTMLFrameOwnerElement>(*node)) {
            ASSERT(!childNumber);
            auto& frameOwner = downcast<HTMLFrameOwnerElement>(*node);
            child = frameOwner.contentDocument();
        } else {
            if (childNumber >= innerChildNodeCount(node))
                return nullptr;

            child = innerFirstChild(node);
            for (size_t j = 0; child && j < childNumber; ++j)
                child = innerNextSibling(child);
        }

        const auto& childName = pathTokens[i + 1];
        if (!child || child->nodeName() != childName)
            return nullptr;
        node = child;
    }
    return node;
}

Node* InspectorDOMAgent::nodeForObjectId(const Protocol::Runtime::RemoteObjectId& objectId)
{
    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue())
        return nullptr;

    return scriptValueAsNode(injectedScript.findObjectById(objectId));
}

Protocol::ErrorStringOr<Protocol::DOM::NodeId> InspectorDOMAgent::pushNodeByPathToFrontend(const String& path)
{
    Protocol::ErrorString errorString;

    if (Node* node = nodeForPath(path)) {
        if (auto nodeId = pushNodePathToFrontend(errorString, node))
            return nodeId;
        return makeUnexpected(errorString);
    }

    return makeUnexpected("Missing node for given path"_s);
}

RefPtr<Protocol::Runtime::RemoteObject> InspectorDOMAgent::resolveNode(Node* node, const String& objectGroup)
{
    Document* document = &node->document();
    if (auto* templateHost = document->templateDocumentHost())
        document = templateHost;
    auto* frame =  document->frame();
    if (!frame)
        return nullptr;

    auto& state = *mainWorldExecState(frame);
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&state);
    if (injectedScript.hasNoValue())
        return nullptr;

    return injectedScript.wrapObject(nodeAsScriptValue(state, node), objectGroup);
}

Node* InspectorDOMAgent::scriptValueAsNode(JSC::JSValue value)
{
    if (!value || !value.isObject())
        return nullptr;
    return JSNode::toWrapped(value.getObject()->vm(), value.getObject());
}

JSC::JSValue InspectorDOMAgent::nodeAsScriptValue(JSC::JSGlobalObject& state, Node* node)
{
    JSC::JSLockHolder lock(&state);
    return toJS(&state, deprecatedGlobalObjectForPrototype(&state), BindingSecurity::checkSecurityForNode(state, node));
}

Protocol::ErrorStringOr<void> InspectorDOMAgent::setAllowEditingUserAgentShadowTrees(bool allow)
{
    m_allowEditingUserAgentShadowTrees = allow;

    return { };
}

} // namespace WebCore
