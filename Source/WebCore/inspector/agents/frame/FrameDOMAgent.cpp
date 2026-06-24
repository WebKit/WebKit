/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameDOMAgent.h"

#include "Attr.h"
#include "CharacterData.h"
#include "ContainerNode.h"
#include "DOMEditor.h"
#include "DOMPatchSupport.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentType.h"
#include "Editing.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLSlotElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTemplateElement.h"
#include "InspectorDOMAgent.h"
#include "InspectorHistory.h"
#include "InspectorNodeFinder.h"
#include "InstrumentingAgents.h"
#include "JSEventListener.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "NodeList.h"
#include "PseudoElement.h"
#include "RegisteredEventListener.h"
#include "ScriptController.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "TextNodeTraversal.h"
#include "markup.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TopExceptionScope.h>
#include <pal/crypto/CryptoDigest.h>
#include <pal/text/TextEncoding.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameDOMAgent);

// FIXME: <https://webkit.org/b/298980> Extract shared tree-building and node-binding logic into a base class shared with InspectorDOMAgent.

static const size_t frameMaxTextSize = 10000;
static const char16_t frameHorizontalEllipsisUTF16[] = { horizontalEllipsis, 0 };

static bool frameContainsOnlyASCIIWhitespace(Node* node)
{
    auto* text = dynamicDowncast<Text>(node);
    return text && text->containsOnlyASCIIWhitespace();
}

static Inspector::Protocol::DOM::ShadowRootType frameShadowRootType(ShadowRootMode mode)
{
    switch (mode) {
    case ShadowRootMode::UserAgent:
        return Inspector::Protocol::DOM::ShadowRootType::UserAgent;
    case ShadowRootMode::Closed:
        return Inspector::Protocol::DOM::ShadowRootType::Closed;
    case ShadowRootMode::Open:
        return Inspector::Protocol::DOM::ShadowRootType::Open;
    }
    ASSERT_NOT_REACHED();
    return Inspector::Protocol::DOM::ShadowRootType::UserAgent;
}

static Inspector::Protocol::DOM::CustomElementState frameCustomElementState(const Element& element)
{
    if (element.isDefinedCustomElement())
        return Inspector::Protocol::DOM::CustomElementState::Custom;
    if (element.isFailedOrPrecustomizedCustomElement())
        return Inspector::Protocol::DOM::CustomElementState::Failed;
    if (element.isCustomElementUpgradeCandidate())
        return Inspector::Protocol::DOM::CustomElementState::Waiting;
    return Inspector::Protocol::DOM::CustomElementState::Builtin;
}

static bool framePseudoElementType(PseudoElementType pseudoElementType, Inspector::Protocol::DOM::PseudoType* type)
{
    switch (pseudoElementType) {
    case PseudoElementType::Before:
        *type = Inspector::Protocol::DOM::PseudoType::Before;
        return true;
    case PseudoElementType::After:
        *type = Inspector::Protocol::DOM::PseudoType::After;
        return true;
    default:
        return false;
    }
}

static String frameComputeContentSecurityPolicySHA256Hash(const Element& element)
{
    Ref document = element.document();
    PAL::TextEncoding documentEncoding = document->textEncoding();
    const PAL::TextEncoding& encodingToUse = documentEncoding.isValid() ? documentEncoding : PAL::UTF8Encoding();
    auto content = encodingToUse.encode(TextNodeTraversal::contentsAsString(element), PAL::UnencodableHandling::Entities);
    auto cryptoDigest = PAL::Crypto::CryptoDigest::create(PAL::Crypto::CryptoDigest::Algorithm::SHA_256);
    cryptoDigest->addBytes(content.span());
    auto digest = cryptoDigest->computeHash();
    return makeString("sha256-"_s, base64Encoded(digest));
}

// MARK: - FrameDOMAgent

FrameDOMAgent::FrameDOMAgent(FrameAgentContext& context)
    : InspectorAgentBase("DOM"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::DOMFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::DOMBackendDispatcher::create(Ref { context.backendDispatcher }, this))
    , m_instrumentingAgents(context.instrumentingAgents)
    , m_inspectedFrame(context.inspectedFrame)
    , m_destroyedNodesTimer(*this, &FrameDOMAgent::destroyedNodesTimerFired)
{
}

FrameDOMAgent::~FrameDOMAgent() = default;

void FrameDOMAgent::didCreateFrontendAndBackend()
{
    m_history = makeUnique<InspectorHistory>();
    m_domEditor = makeUnique<DOMEditor>(*m_history);

    Ref { m_instrumentingAgents.get() }->setPersistentFrameDOMAgent(this);

    RefPtr frame = m_inspectedFrame.get();
    if (frame)
        m_document = frame->document();
}

void FrameDOMAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_domEditor.reset();
    m_history.reset();

    Ref { m_instrumentingAgents.get() }->setPersistentFrameDOMAgent(nullptr);
    m_documentRequested = false;
    reset();
}

// MARK: - Node Binding

Inspector::Protocol::DOM::NodeId FrameDOMAgent::bind(Node& node)
{
    return m_nodeToId.ensure(node, [&] {
        auto id = m_lastNodeId++;
        m_idToNode.set(id, node);
        return id;
    }).iterator->value;
}

void FrameDOMAgent::unbind(Node& node)
{
    auto id = m_nodeToId.take(node);
    if (!id)
        return;

    m_idToNode.remove(id);

    if (auto* frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(node)) {
        // FIXME: <https://webkit.org/b/298980> Skip if child frame has its own FrameDOMAgent.
        if (RefPtr contentDocument = frameOwner->contentDocument())
            unbind(*contentDocument);
    }

    if (RefPtr element = dynamicDowncast<Element>(node)) {
        if (RefPtr root = element->shadowRoot())
            unbind(*root);
        if (RefPtr before = element->beforePseudoElement())
            unbind(*before);
        if (RefPtr after = element->afterPseudoElement())
            unbind(*after);
    }

    if (m_childrenRequested.remove(id)) {
        for (RefPtr child = InspectorDOMAgent::innerFirstChild(&node); child; child = InspectorDOMAgent::innerNextSibling(child.get()))
            unbind(*child);
    }
}

Inspector::Protocol::DOM::NodeId FrameDOMAgent::boundNodeId(const Node* node)
{
    if (!node)
        return 0;
    return m_nodeToId.get(*node);
}

Node* FrameDOMAgent::nodeForId(Inspector::Protocol::DOM::NodeId id)
{
    if (!m_idToNode.isValidKey(id))
        return nullptr;
    return m_idToNode.get(id);
}

void FrameDOMAgent::discardBindings()
{
    m_nodeToId.clear();
    m_idToNode.clear();
    m_childrenRequested.clear();
}

RefPtr<Node> FrameDOMAgent::assertNode(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    RefPtr node = nodeForId(nodeId);
    if (!node)
        errorString = "Missing node for given nodeId"_s;
    return node;
}

RefPtr<Element> FrameDOMAgent::assertElement(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    RefPtr node = assertNode(errorString, nodeId);
    if (!node)
        return nullptr;
    RefPtr element = dynamicDowncast<Element>(*node);
    if (!element)
        errorString = "Node for given nodeId is not an element"_s;
    return element;
}

RefPtr<Node> FrameDOMAgent::assertEditableNode(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    RefPtr node = assertNode(errorString, nodeId);
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

RefPtr<Element> FrameDOMAgent::assertEditableElement(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    RefPtr node = assertEditableNode(errorString, nodeId);
    if (!node)
        return nullptr;
    RefPtr element = dynamicDowncast<Element>(*node);
    if (!element)
        errorString = "Node for given nodeId is not an element"_s;
    return element;
}

// MARK: - Tree Building

Ref<Inspector::Protocol::DOM::Node> FrameDOMAgent::buildObjectForNode(Node* node, int depth)
{
    auto id = bind(*node);
    String nodeName;
    String localName;
    String nodeValue;

    switch (node->nodeType()) {
    case NodeType::ProcessingInstruction:
        nodeName = node->nodeName();
        localName = node->localName();
        [[fallthrough]];
    case NodeType::Text:
    case NodeType::Comment:
    case NodeType::CDATASection:
        nodeValue = node->nodeValue();
        if (nodeValue.length() > frameMaxTextSize)
            nodeValue = makeString(StringView(nodeValue).left(frameMaxTextSize), frameHorizontalEllipsisUTF16);
        break;
    case NodeType::Attribute:
        localName = node->localName();
        break;
    case NodeType::DocumentFragment:
    case NodeType::Document:
    case NodeType::Element:
    default:
        nodeName = node->nodeName();
        localName = node->localName();
        break;
    }

    auto value = Inspector::Protocol::DOM::Node::create()
        .setNodeId(id)
        .setNodeType(std::to_underlying(node->nodeType()))
        .setNodeName(nodeName)
        .setLocalName(localName)
        .setNodeValue(nodeValue)
        .release();

    if (node->isContainerNode()) {
        int nodeCount = InspectorDOMAgent::innerChildNodeCount(node);
        value->setChildNodeCount(nodeCount);
        auto children = buildArrayForContainerChildren(node, depth);
        if (children->length() > 0)
            value->setChildren(WTF::move(children));
    }

    if (RefPtr element = dynamicDowncast<Element>(*node)) {
        value->setAttributes(buildArrayForElementAttributes(element.get()));

        // FIXME: <https://webkit.org/b/298980> Skip if child frame has its own FrameDOMAgent.
        if (RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(*element)) {
            if (RefPtr document = frameOwner->contentDocument())
                value->setContentDocument(buildObjectForNode(document.get(), 0));
        }

        if (RefPtr root = element->shadowRoot()) {
            auto shadowRoots = JSON::ArrayOf<Inspector::Protocol::DOM::Node>::create();
            shadowRoots->addItem(buildObjectForNode(root.get(), 0));
            value->setShadowRoots(WTF::move(shadowRoots));
        }

        if (RefPtr templateElement = dynamicDowncast<HTMLTemplateElement>(*element))
            value->setTemplateContent(buildObjectForNode(protect(templateElement->content()).ptr(), 0));

        if (is<HTMLStyleElement>(element) || (is<HTMLScriptElement>(element) && !element->hasAttributeWithoutSynchronization(HTMLNames::srcAttr)))
            value->setContentSecurityPolicyHash(frameComputeContentSecurityPolicySHA256Hash(*element));

        auto state = frameCustomElementState(*element);
        if (state != Inspector::Protocol::DOM::CustomElementState::Builtin)
            value->setCustomElementState(state);

        if (element->pseudoElementIdentifier()) {
            Inspector::Protocol::DOM::PseudoType pseudoType;
            if (framePseudoElementType(element->pseudoElementIdentifier()->type, &pseudoType))
                value->setPseudoType(pseudoType);
        } else {
            if (auto pseudoElements = buildArrayForPseudoElements(*element))
                value->setPseudoElements(pseudoElements.releaseNonNull());
        }
    } else if (RefPtr document = dynamicDowncast<Document>(*node)) {
        value->setDocumentURL(InspectorDOMAgent::documentURLString(document.get()));
        // FIXME: <https://webkit.org/b/298980> Set frameId for frame targets to enable frontend frame-to-target association.
    } else if (RefPtr doctype = dynamicDowncast<DocumentType>(*node)) {
        value->setPublicId(doctype->publicId());
        value->setSystemId(doctype->systemId());
    } else if (RefPtr attribute = dynamicDowncast<Attr>(*node)) {
        value->setName(attribute->name());
        value->setValue(attribute->value());
    } else if (RefPtr shadowRoot = dynamicDowncast<ShadowRoot>(*node))
        value->setShadowRootType(frameShadowRootType(shadowRoot->mode()));

    return value;
}

Ref<JSON::ArrayOf<String>> FrameDOMAgent::buildArrayForElementAttributes(Element* element)
{
    auto attributesValue = JSON::ArrayOf<String>::create();
    if (!element->hasAttributes())
        return attributesValue;
    for (auto& attribute : element->attributes()) {
        attributesValue->addItem(attribute.name().toString());
        attributesValue->addItem(attribute.value());
    }
    return attributesValue;
}

Ref<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> FrameDOMAgent::buildArrayForContainerChildren(Node* container, int depth)
{
    auto children = JSON::ArrayOf<Inspector::Protocol::DOM::Node>::create();
    if (!depth) {
        RefPtr firstChild = container->firstChild();
        if (firstChild && firstChild->nodeType() == NodeType::Text && !firstChild->nextSibling()) {
            children->addItem(buildObjectForNode(firstChild.get(), 0));
            m_childrenRequested.add(bind(*container));
        }
        return children;
    }

    RefPtr child = InspectorDOMAgent::innerFirstChild(container);
    depth--;
    m_childrenRequested.add(bind(*container));

    while (child) {
        children->addItem(buildObjectForNode(child.get(), depth));
        child = InspectorDOMAgent::innerNextSibling(child.get());
    }
    return children;
}

RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> FrameDOMAgent::buildArrayForPseudoElements(const Element& element)
{
    RefPtr beforeElement = element.beforePseudoElement();
    RefPtr afterElement = element.afterPseudoElement();
    if (!beforeElement && !afterElement)
        return nullptr;

    auto pseudoElements = JSON::ArrayOf<Inspector::Protocol::DOM::Node>::create();
    if (beforeElement)
        pseudoElements->addItem(buildObjectForNode(beforeElement.get(), 0));
    if (afterElement)
        pseudoElements->addItem(buildObjectForNode(afterElement.get(), 0));
    return pseudoElements;
}

void FrameDOMAgent::pushChildNodesToFrontend(Inspector::Protocol::DOM::NodeId nodeId, int depth)
{
    RefPtr node = nodeForId(nodeId);
    if (!node || (node->nodeType() != NodeType::Element && node->nodeType() != NodeType::Document && node->nodeType() != NodeType::DocumentFragment))
        return;

    if (m_childrenRequested.contains(nodeId)) {
        if (depth <= 1)
            return;

        depth--;
        for (node = InspectorDOMAgent::innerFirstChild(node.get()); node; node = InspectorDOMAgent::innerNextSibling(node.get())) {
            auto childNodeId = boundNodeId(node.get());
            ASSERT(childNodeId);
            pushChildNodesToFrontend(childNodeId, depth);
        }
        return;
    }

    auto children = buildArrayForContainerChildren(node.get(), depth);
    m_frontendDispatcher->setChildNodes(nodeId, WTF::move(children));
}

Inspector::Protocol::DOM::NodeId FrameDOMAgent::pushNodePathToFrontend(Node* nodeToPush)
{
    Inspector::Protocol::ErrorString ignored;
    return pushNodePathToFrontend(ignored, nodeToPush);
}

Inspector::Protocol::DOM::NodeId FrameDOMAgent::pushNodePathToFrontend(Inspector::Protocol::ErrorString& errorString, Node* nodeToPush)
{
    ASSERT(nodeToPush);

    if (!m_document) {
        errorString = "Missing document"_s;
        return 0;
    }

    if (!m_nodeToId.contains(*m_document)) {
        errorString = "Document must have been requested"_s;
        return 0;
    }

    if (auto result = boundNodeId(nodeToPush))
        return result;

    RefPtr node = nodeToPush;
    Vector<Node*> path;

    while (true) {
        RefPtr parent = InspectorDOMAgent::innerParentNode(node.get());
        if (!parent) {
            auto children = JSON::ArrayOf<Inspector::Protocol::DOM::Node>::create();
            children->addItem(buildObjectForNode(node.get(), 0));
            m_frontendDispatcher->setChildNodes(0, WTF::move(children));
            break;
        }

        path.append(parent.get());
        if (boundNodeId(parent.get()))
            break;
        node = parent;
    }

    for (int i = path.size() - 1; i >= 0; --i) {
        RefPtr pathNode = path.at(i);
        auto nodeId = boundNodeId(pathNode.get());
        ASSERT(nodeId);
        pushChildNodesToFrontend(nodeId);
    }
    return boundNodeId(nodeToPush);
}

void FrameDOMAgent::setDocument(Document* document)
{
    if (document == m_document.get())
        return;

    reset();
    m_document = document;

    if (!m_documentRequested)
        return;

    if (!document || !document->parsing())
        m_frontendDispatcher->documentUpdated();
}

void FrameDOMAgent::reset()
{
    if (m_history)
        m_history->reset();
    discardBindings();
    m_document = nullptr;
    m_searchResults.clear();
    m_eventListenerEntries.clear();
    m_lastEventListenerId = 1;

    m_destroyedDetachedNodeIdentifiers.clear();
    m_destroyedAttachedNodeIdentifiers.clear();
    if (m_destroyedNodesTimer.isActive())
        m_destroyedNodesTimer.stop();
}

// MARK: - Protocol Commands

Inspector::CommandResult<Ref<Inspector::Protocol::DOM::Node>> FrameDOMAgent::getDocument()
{
    m_documentRequested = true;

    if (!m_document)
        return makeUnexpected("Internal error: missing document"_s);

    RefPtr<Document> document = m_document;
    reset();
    m_document = document;

    return buildObjectForNode(m_document.get(), 2);
}

Inspector::CommandResult<void> FrameDOMAgent::requestChildNodes(int nodeId, std::optional<int>&& depth)
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

Inspector::CommandResult<Ref<JSON::ArrayOf<String>>> FrameDOMAgent::getAttributes(int nodeId)
{
    Inspector::Protocol::ErrorString errorString;
    RefPtr element = assertElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    return buildArrayForElementAttributes(element.get());
}

Inspector::CommandResult<std::optional<int>> FrameDOMAgent::requestAssignedSlot(int nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    RefPtr slotElement = node->assignedSlot();
    if (!slotElement)
        return { std::optional<int>() };

    auto slotElementId = pushNodePathToFrontend(errorString, slotElement.get());
    if (!slotElementId)
        return makeUnexpected(errorString);

    return { std::optional<int>(slotElementId) };
}

Inspector::CommandResult<Ref<JSON::ArrayOf<int>>> FrameDOMAgent::requestAssignedNodes(int slotElementId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertNode(errorString, slotElementId);
    if (!node)
        return makeUnexpected(errorString);

    RefPtr slotElement = dynamicDowncast<HTMLSlotElement>(node);
    if (!slotElement)
        return makeUnexpected("Node for given nodeId is not a slot element"_s);

    auto assignedNodeIds = JSON::ArrayOf<int>::create();
    if (const auto* weakAssignedNodes = slotElement->assignedNodes()) {
        for (const auto& weakAssignedNode : *weakAssignedNodes) {
            if (RefPtr assignedNode = weakAssignedNode.get()) {
                auto assignedNodeId = pushNodePathToFrontend(errorString, assignedNode.get());
                if (!assignedNodeId)
                    return makeUnexpected(errorString);
                assignedNodeIds->addItem(assignedNodeId);
            }
        }
    }
    return assignedNodeIds;
}

// MARK: - InspectorInstrumentation Hooks

void FrameDOMAgent::didInsertDOMNode(Node& node)
{
    if (frameContainsOnlyASCIIWhitespace(&node))
        return;

    unbind(node);

    RefPtr parent = node.parentNode();
    auto parentId = boundNodeId(parent.get());
    if (!parentId)
        return;

    if (!m_childrenRequested.contains(parentId)) {
        // No children are mapped yet -> only notify on changes of hasChildren.
        m_frontendDispatcher->childNodeCountUpdated(parentId, InspectorDOMAgent::innerChildNodeCount(parent.get()));
    } else {
        RefPtr prevSibling = InspectorDOMAgent::innerPreviousSibling(&node);
        auto prevId = boundNodeId(prevSibling.get());
        auto value = buildObjectForNode(&node, 0);
        m_frontendDispatcher->childNodeInserted(parentId, prevId, WTF::move(value));
    }
}

void FrameDOMAgent::didRemoveDOMNode(Node& node)
{
    if (frameContainsOnlyASCIIWhitespace(&node))
        return;

    RefPtr parent = node.parentNode();
    auto parentId = boundNodeId(parent.get());
    if (!parentId)
        return;

    if (!m_childrenRequested.contains(parentId)) {
        if (InspectorDOMAgent::innerChildNodeCount(parent.get()) == 1)
            m_frontendDispatcher->childNodeCountUpdated(parentId, 0);
    } else
        m_frontendDispatcher->childNodeRemoved(parentId, boundNodeId(&node));
    unbind(node);
}

void FrameDOMAgent::willDestroyDOMNode(Node& node)
{
    if (frameContainsOnlyASCIIWhitespace(&node))
        return;

    auto nodeId = m_nodeToId.take(node);
    if (!nodeId)
        return;

    m_idToNode.remove(nodeId);
    m_childrenRequested.remove(nodeId);

    RefPtr parentNode = node.parentNode();
    if (auto parentId = boundNodeId(parentNode.get()))
        m_destroyedAttachedNodeIdentifiers.append({ parentId, nodeId });
    else
        m_destroyedDetachedNodeIdentifiers.append(nodeId);

    if (!m_destroyedNodesTimer.isActive())
        m_destroyedNodesTimer.startOneShot(0_s);
}

void FrameDOMAgent::destroyedNodesTimerFired()
{
    for (auto& [parentId, nodeId] : std::exchange(m_destroyedAttachedNodeIdentifiers, { })) {
        if (!m_childrenRequested.contains(parentId)) {
            RefPtr parent = nodeForId(parentId);
            if (parent && InspectorDOMAgent::innerChildNodeCount(parent.get()) == 1)
                m_frontendDispatcher->childNodeCountUpdated(parentId, 0);
        } else
            m_frontendDispatcher->childNodeRemoved(parentId, nodeId);
    }

    for (auto nodeId : std::exchange(m_destroyedDetachedNodeIdentifiers, { }))
        m_frontendDispatcher->willDestroyDOMNode(nodeId);
}

void FrameDOMAgent::willModifyDOMAttr(Element&, const AtomString& oldValue, const AtomString& newValue)
{
    m_suppressAttributeModifiedEvent = (oldValue == newValue);
}

void FrameDOMAgent::didModifyDOMAttr(Element& element, const AtomString& name, const AtomString& value)
{
    bool shouldSuppressEvent = m_suppressAttributeModifiedEvent;
    m_suppressAttributeModifiedEvent = false;
    if (shouldSuppressEvent)
        return;

    auto id = boundNodeId(&element);
    if (!id)
        return;

    m_frontendDispatcher->attributeModified(id, name, value);
}

void FrameDOMAgent::didRemoveDOMAttr(Element& element, const AtomString& name)
{
    auto id = boundNodeId(&element);
    if (!id)
        return;

    m_frontendDispatcher->attributeRemoved(id, name);
}

void FrameDOMAgent::characterDataModified(CharacterData& characterData)
{
    auto id = boundNodeId(&characterData);
    if (!id) {
        didInsertDOMNode(characterData);
        return;
    }
    m_frontendDispatcher->characterDataModified(id, characterData.data());
}

void FrameDOMAgent::didInvalidateStyleAttr(Element& element)
{
    auto id = boundNodeId(&element);
    if (!id)
        return;

    auto nodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
    nodeIds->addItem(id);
    m_frontendDispatcher->inlineStyleInvalidated(WTF::move(nodeIds));
}

void FrameDOMAgent::didPushShadowRoot(Element& host, ShadowRoot& root)
{
    auto hostId = boundNodeId(&host);
    if (hostId)
        m_frontendDispatcher->shadowRootPushed(hostId, buildObjectForNode(&root, 0));
}

void FrameDOMAgent::willPopShadowRoot(Element& host, ShadowRoot& root)
{
    auto hostId = boundNodeId(&host);
    auto rootId = boundNodeId(&root);
    if (hostId && rootId)
        m_frontendDispatcher->shadowRootPopped(hostId, rootId);
}

void FrameDOMAgent::didChangeCustomElementState(Element& element)
{
    auto elementId = boundNodeId(&element);
    if (!elementId)
        return;

    m_frontendDispatcher->customElementStateChanged(elementId, frameCustomElementState(element));
}

void FrameDOMAgent::pseudoElementCreated(PseudoElement& pseudoElement)
{
    RefPtr parent = pseudoElement.hostElement();
    if (!parent)
        return;

    auto parentId = boundNodeId(parent.get());
    if (!parentId)
        return;

    pushChildNodesToFrontend(parentId, 1);
    m_frontendDispatcher->pseudoElementAdded(parentId, buildObjectForNode(&pseudoElement, 0));
}

void FrameDOMAgent::pseudoElementDestroyed(PseudoElement& pseudoElement)
{
    auto pseudoElementId = boundNodeId(&pseudoElement);
    if (!pseudoElementId)
        return;

    RefPtr parent = pseudoElement.hostElement();
    ASSERT(parent);
    auto parentId = boundNodeId(parent.get());
    ASSERT(parentId);

    unbind(pseudoElement);
    m_frontendDispatcher->pseudoElementRemoved(parentId, pseudoElementId);
}

void FrameDOMAgent::frameDocumentUpdated(LocalFrame& frame)
{
    RefPtr inspectedFrame = m_inspectedFrame.get();
    if (!inspectedFrame || &frame != inspectedFrame.get())
        return;

    RefPtr document = frame.document();
    setDocument(document.get());
}

Inspector::CommandResult<std::optional<int>> FrameDOMAgent::querySelector(int nodeId, const String& selector)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);
    RefPtr containerNode = dynamicDowncast<ContainerNode>(*node);
    if (!containerNode)
        return makeUnexpected("Node for given nodeId is not a container node"_s);

    auto queryResult = containerNode->querySelector(selector);
    if (queryResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(queryResult.releaseException()));

    RefPtr queryResultNode = queryResult.releaseReturnValue();
    if (!queryResultNode)
        return { };

    auto resultNodeId = pushNodePathToFrontend(errorString, queryResultNode.get());
    if (!resultNodeId)
        return makeUnexpected(errorString);

    return { resultNodeId };
}

Inspector::CommandResult<Ref<JSON::ArrayOf<int>>> FrameDOMAgent::querySelectorAll(int nodeId, const String& selector)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);
    RefPtr containerNode = dynamicDowncast<ContainerNode>(*node);
    if (!containerNode)
        return makeUnexpected("Node for given nodeId is not a container node"_s);

    auto queryResult = containerNode->querySelectorAll(selector);
    if (queryResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(queryResult.releaseException()));

    auto nodes = queryResult.releaseReturnValue();

    auto nodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
    for (unsigned i = 0; i < nodes->length(); ++i) {
        RefPtr node = nodes->item(i);
        nodeIds->addItem(pushNodePathToFrontend(node.get()));
    }
    return nodeIds;
}

Inspector::CommandResult<String> FrameDOMAgent::getOuterHTML(int nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    return serializeFragment(*node, SerializedNodes::SubtreeIncludingNode);
}

Inspector::CommandResult<Ref<JSON::ArrayOf<String>>> FrameDOMAgent::getSupportedEventNames()
{
    auto list = JSON::ArrayOf<String>::create();

    for (auto& event : eventNames().allEventNames())
        list->addItem(event);

    return list;
}

Inspector::CommandResultOf<String, int> FrameDOMAgent::performSearch(const String& query, RefPtr<JSON::Array>&& nodeIds, std::optional<bool>&& caseSensitive)
{
    Inspector::Protocol::ErrorString errorString;

    // FIXME: <https://webkit.org/b/316549> Search works with node granularity - number of matches within node is not calculated.
    InspectorNodeFinder finder(query, caseSensitive && *caseSensitive);

    if (nodeIds) {
        for (auto& nodeValue : *nodeIds) {
            auto nodeId = nodeValue->asInteger();
            if (!nodeId)
                return makeUnexpected("Unexpected non-integer item in given nodeIds"_s);

            RefPtr node = assertNode(errorString, *nodeId);
            if (!node)
                return makeUnexpected(errorString);

            finder.performSearch(node.get());
        }
    } else
        finder.performSearch(m_document.get());

    auto searchId = IdentifiersFactory::createIdentifier();

    auto& resultsVector = m_searchResults.add(searchId, Vector<RefPtr<Node>>()).iterator->value;
    for (auto& result : finder.results())
        resultsVector.append(result);

    return { { searchId, resultsVector.size() } };
}

Inspector::CommandResult<Ref<JSON::ArrayOf<int>>> FrameDOMAgent::getSearchResults(const String& searchId, int fromIndex, int toIndex)
{
    auto it = m_searchResults.find(searchId);
    if (it == m_searchResults.end())
        return makeUnexpected("Missing search result for given searchId"_s);

    int size = it->value.size();
    if (fromIndex < 0 || toIndex > size || fromIndex >= toIndex)
        return makeUnexpected("Invalid search result range for given fromIndex and toIndex"_s);

    auto nodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
    for (int i = fromIndex; i < toIndex; ++i)
        nodeIds->addItem(pushNodePathToFrontend((it->value)[i].get()));
    return nodeIds;
}

Inspector::CommandResult<void> FrameDOMAgent::discardSearchResults(const String& searchId)
{
    m_searchResults.remove(searchId);
    return { };
}

RefPtr<Node> FrameDOMAgent::nodeForPath(const String& path)
{
    // The path is of form "1,HTML,2,BODY,1,DIV"
    if (!m_document)
        return nullptr;

    RefPtr<Node> node = m_document;
    auto pathTokens = StringView(path).split(',');
    auto it = pathTokens.begin();
    if (it == pathTokens.end())
        return nullptr;

    for (; it != pathTokens.end(); ++it) {
        auto childNumberView = *it;
        if (++it == pathTokens.end())
            break;
        auto childNumber = parseIntegerAllowingTrailingJunk<unsigned>(childNumberView);
        if (!childNumber)
            return nullptr;

        RefPtr<Node> child;
        if (RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(*node)) {
            ASSERT(!*childNumber);
            child = frameOwner->contentDocument();
        } else {
            if (*childNumber >= InspectorDOMAgent::innerChildNodeCount(node.get()))
                return nullptr;
            child = InspectorDOMAgent::innerFirstChild(node.get());
            for (size_t j = 0; child && j < *childNumber; ++j)
                child = InspectorDOMAgent::innerNextSibling(child.get());
        }

        auto childName = *it;
        if (!child || child->nodeName() != childName)
            return nullptr;
        node = child;
    }

    return node;
}

Inspector::CommandResult<int> FrameDOMAgent::pushNodeByPathToFrontend(const String& path)
{
    Inspector::Protocol::ErrorString errorString;

    if (RefPtr node = nodeForPath(path)) {
        if (auto nodeId = pushNodePathToFrontend(errorString, node.get()))
            return nodeId;
        return makeUnexpected(errorString);
    }

    return makeUnexpected("Missing node for given path"_s);
}

Inspector::CommandResult<void> FrameDOMAgent::setAllowEditingUserAgentShadowTrees(bool allow)
{
    m_allowEditingUserAgentShadowTrees = allow;
    return { };
}

Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::EventListener>>> FrameDOMAgent::getEventListenersForNode(int nodeId, std::optional<bool>&& includeAncestors)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    Vector<RefPtr<EventTarget>> ancestors;
    ancestors.append(node.get());
    if (includeAncestors.value_or(true)) {
        for (RefPtr ancestor = node->parentOrShadowHostNode(); ancestor; ancestor = ancestor->parentOrShadowHostNode())
            ancestors.append(ancestor.get());
        if (RefPtr window = node->document().window())
            ancestors.append(window.get());
    }

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
                eventInformation.append({ ancestor, eventType, WTF::move(filteredListeners) });
        }
    }

    auto listeners = JSON::ArrayOf<Inspector::Protocol::DOM::EventListener>::create();

    auto addListener = [&](const Ref<RegisteredEventListener>& listener, const EventListenerInfo& info) {
        Inspector::Protocol::DOM::EventListenerId identifier = 0;
        bool disabled = false;

        Ref eventTarget = *info.eventTarget;

        for (auto& inspectorEventListener : m_eventListenerEntries.values()) {
            if (inspectorEventListener.matches(eventTarget, info.eventType, listener->callback(), listener->useCapture())) {
                identifier = inspectorEventListener.identifier;
                disabled = inspectorEventListener.disabled;
                break;
            }
        }

        if (!identifier) {
            InspectorEventListener inspectorEventListener(m_lastEventListenerId++, eventTarget, info.eventType, listener->callback(), listener->useCapture());

            identifier = inspectorEventListener.identifier;
            disabled = inspectorEventListener.disabled;

            m_eventListenerEntries.add(identifier, inspectorEventListener);
        }

        listeners->addItem(buildObjectForEventListener(listener, identifier, eventTarget, info.eventType, disabled));
    };

    // Get Capturing Listeners (in this order)
    size_t eventInformationLength = eventInformation.size();
    for (auto& info : eventInformation) {
        for (auto& listener : info.eventListeners) {
            if (listener->useCapture())
                addListener(listener, info);
        }
    }

    // Get Bubbling Listeners (reverse order)
    for (size_t i = eventInformationLength; i; --i) {
        const EventListenerInfo& info = eventInformation[i - 1];
        for (auto& listener : info.eventListeners) {
            if (!listener->useCapture())
                addListener(listener, info);
        }
    }

    return listeners;
}

Inspector::CommandResult<void> FrameDOMAgent::setEventListenerDisabled(int eventListenerId, bool disabled)
{
    auto it = m_eventListenerEntries.find(eventListenerId);
    if (it == m_eventListenerEntries.end())
        return makeUnexpected("Missing event listener for given eventListenerId"_s);

    it->value.disabled = disabled;

    return { };
}

bool FrameDOMAgent::isEventListenerDisabled(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    for (auto& inspectorEventListener : m_eventListenerEntries.values()) {
        if (inspectorEventListener.matches(target, eventType, listener, capture))
            return inspectorEventListener.disabled;
    }
    return false;
}

Ref<Inspector::Protocol::DOM::EventListener> FrameDOMAgent::buildObjectForEventListener(const Ref<RegisteredEventListener>& registeredEventListener, Inspector::Protocol::DOM::EventListenerId identifier, EventTarget& eventTarget, const AtomString& eventType, bool disabled)
{
    Ref<EventListener> eventListener = registeredEventListener->callback();

    String handlerName;
    int lineNumber = 0;
    int columnNumber = 0;
    String scriptID;
    if (RefPtr scriptListener = dynamicDowncast<JSEventListener>(eventListener); scriptListener && scriptListener->isolatedWorld()) {
        RefPtr<Document> document;
        if (RefPtr scriptExecutionContext = eventTarget.scriptExecutionContext())
            document = dynamicDowncast<Document>(*scriptExecutionContext);
        else if (RefPtr node = dynamicDowncast<Node>(eventTarget))
            document = node->document();

        JSC::JSObject* handlerObject = nullptr;
        JSC::JSGlobalObject* globalObject = nullptr;

        RefPtr isolatedWorld = scriptListener->isolatedWorld();
        JSC::JSLockHolder lock(isolatedWorld->vm());

        if (document) {
            handlerObject = scriptListener->ensureJSFunction(*document);
            if (RefPtr frame = document->frame()) {
                CheckedRef script = frame->script();
                // FIXME: Why do we need the canExecuteScripts check here?
                if (script->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
                    globalObject = script->globalObject(*isolatedWorld);
            }
        }

        if (handlerObject && globalObject) {
            JSC::VM& vm = globalObject->vm();
            JSC::JSFunction* handlerFunction = dynamicDowncast<JSC::JSFunction>(handlerObject);

            if (!handlerFunction) {
                auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

                // If the handler is not actually a function, see if it implements the EventListener interface and use that.
                auto handleEventValue = handlerObject->get(globalObject, JSC::Identifier::fromString(vm, "handleEvent"_s));

                if (scope.exception()) [[unlikely]]
                    scope.clearException();

                if (handleEventValue)
                    handlerFunction = dynamicDowncast<JSC::JSFunction>(handleEventValue);
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

    auto value = Inspector::Protocol::DOM::EventListener::create()
        .setEventListenerId(identifier)
        .setType(eventType)
        .setUseCapture(registeredEventListener->useCapture())
        .setIsAttribute(eventListener->isAttribute())
        .release();
    if (RefPtr node = dynamicDowncast<Node>(eventTarget))
        value->setNodeId(pushNodePathToFrontend(node.get()));
    else if (is<LocalDOMWindow>(eventTarget))
        value->setOnWindow(true);
    if (!scriptID.isNull()) {
        auto location = Inspector::Protocol::Debugger::Location::create()
            .setScriptId(scriptID)
            .setLineNumber(lineNumber)
            .release();
        location->setColumnNumber(columnNumber);
        value->setLocation(WTF::move(location));
    }
    if (!handlerName.isEmpty())
        value->setHandlerName(handlerName);
    if (registeredEventListener->isPassive())
        value->setPassive(true);
    if (registeredEventListener->isOnce())
        value->setOnce(true);
    if (disabled)
        value->setDisabled(disabled);
    return value;
}

Inspector::CommandResult<void> FrameDOMAgent::setAttributeValue(int nodeId, const String& name, const String& value)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!m_domEditor->setAttribute(*element, AtomString { name }, AtomString { value }, errorString))
        return makeUnexpected(errorString);

    return { };
}

Inspector::CommandResult<void> FrameDOMAgent::setAttributesAsText(int nodeId, const String& text, const String& name)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    Ref parsedElement = createHTMLElement(protect(element->document()), HTMLNames::spanTag);
    auto result = parsedElement.get().setInnerHTML(makeString("<span "_s, text, "></span>"_s));
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    RefPtr child = parsedElement->firstChild();
    if (!child)
        return makeUnexpected("Could not parse given text"_s);

    RefPtr childElement = dynamicDowncast<Element>(*child);
    if (!childElement)
        return makeUnexpected("Could not parse given text"_s);

    if (!childElement->hasAttributes() && !!name) {
        if (!m_domEditor->removeAttribute(*element, AtomString { name }, errorString))
            return makeUnexpected(errorString);
        return { };
    }

    bool foundOriginalAttribute = false;
    for (auto& attribute : childElement->attributes()) {
        auto attributeName = attribute.name().toAtomString();
        foundOriginalAttribute = foundOriginalAttribute || attributeName == name;
        if (!m_domEditor->setAttribute(*element, attributeName, attribute.value(), errorString))
            return makeUnexpected(errorString);
    }

    if (!foundOriginalAttribute && name.find(deprecatedIsNotSpaceOrNewline) != notFound) {
        if (!m_domEditor->removeAttribute(*element, AtomString { name }, errorString))
            return makeUnexpected(errorString);
    }

    return { };
}

Inspector::CommandResult<void> FrameDOMAgent::removeAttribute(int nodeId, const String& name)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!m_domEditor->removeAttribute(*element, AtomString { name }, errorString))
        return makeUnexpected(errorString);

    return { };
}

Inspector::CommandResult<void> FrameDOMAgent::removeNode(int nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    RefPtr parentNode = node->parentNode();
    if (!parentNode)
        return makeUnexpected("Cannot remove detached node"_s);

    if (!m_domEditor->removeChild(*parentNode, *node, errorString))
        return makeUnexpected(errorString);

    return { };
}

Inspector::CommandResult<int> FrameDOMAgent::setNodeName(int nodeId, const String& tagName)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr oldNode = assertElement(errorString, nodeId);
    if (!oldNode)
        return makeUnexpected(errorString);

    auto createElementResult = protect(oldNode->document())->createElementForBindings(AtomString { tagName });
    if (createElementResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(createElementResult.releaseException()));

    auto newElement = createElementResult.releaseReturnValue();

    newElement->cloneAttributesFromElement(*oldNode);

    RefPtr<Node> child;
    while ((child = oldNode->firstChild())) {
        if (!m_domEditor->insertBefore(newElement, *child, 0, errorString))
            return makeUnexpected(errorString);
    }

    RefPtr<ContainerNode> parent = oldNode->parentNode();
    if (!m_domEditor->insertBefore(*parent, newElement.copyRef(), protect(oldNode->nextSibling()).get(), errorString))
        return makeUnexpected(errorString);
    if (!m_domEditor->removeChild(*parent, *oldNode, errorString))
        return makeUnexpected(errorString);

    auto resultNodeId = pushNodePathToFrontend(errorString, newElement.ptr());
    if (m_childrenRequested.contains(nodeId))
        pushChildNodesToFrontend(resultNodeId);

    return resultNodeId;
}

Inspector::CommandResult<void> FrameDOMAgent::setOuterHTML(int nodeId, const String& outerHTML)
{
    Inspector::Protocol::ErrorString errorString;

    if (!nodeId) {
        if (!m_document)
            return makeUnexpected("Internal error: missing document"_s);
        DOMPatchSupport { *m_domEditor, *m_document }.patchDocument(outerHTML);
        return { };
    }

    RefPtr node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    Ref document = node->document();
    if (!document->isHTMLDocument() && !document->isXMLDocument())
        return makeUnexpected("Document of node for given nodeId is not HTML/XML"_s);

    Node* newNode = nullptr;
    if (!m_domEditor->setOuterHTML(*node, outerHTML, newNode, errorString))
        return makeUnexpected(errorString);

    if (!newNode)
        return { };

    auto newId = pushNodePathToFrontend(errorString, newNode);

    if (m_childrenRequested.contains(nodeId))
        pushChildNodesToFrontend(newId);

    return { };
}

Inspector::CommandResult<void> FrameDOMAgent::insertAdjacentHTML(int nodeId, const String& position, const String& html)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    RefPtr element = dynamicDowncast<Element>(*node);
    if (!element)
        return makeUnexpected("Node for given nodeId is not an element"_s);

    if (!m_domEditor->insertAdjacentHTML(*element, position, html, errorString))
        return makeUnexpected(errorString);

    return { };
}

Inspector::CommandResult<void> FrameDOMAgent::setNodeValue(int nodeId, const String& value)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    RefPtr text = dynamicDowncast<Text>(*node);
    if (!text)
        return makeUnexpected("Node for given nodeId is not text"_s);

    if (!m_domEditor->replaceWholeText(*text, value, errorString))
        return makeUnexpected(errorString);

    return { };
}

Inspector::CommandResult<int> FrameDOMAgent::moveTo(int nodeId, int targetNodeId, std::optional<int>&& insertBeforeNodeId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr node = assertEditableNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    RefPtr targetElement = assertEditableElement(errorString, targetNodeId);
    if (!targetElement)
        return makeUnexpected(errorString);

    RefPtr<Node> anchorNode;
    if (insertBeforeNodeId && *insertBeforeNodeId) {
        anchorNode = assertEditableNode(errorString, *insertBeforeNodeId);
        if (!anchorNode)
            return makeUnexpected(errorString);
        if (anchorNode->parentNode() != targetElement)
            return makeUnexpected("Given insertBeforeNodeId must be a child of given targetNodeId"_s);
    }

    if (!m_domEditor->insertBefore(*targetElement, *node, anchorNode.get(), errorString))
        return makeUnexpected(errorString);

    return pushNodePathToFrontend(errorString, node.get());
}

Inspector::CommandResult<void> FrameDOMAgent::undo()
{
    auto result = m_history->undo();
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    return { };
}

Inspector::CommandResult<void> FrameDOMAgent::redo()
{
    auto result = m_history->redo();
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    return { };
}

Inspector::CommandResult<void> FrameDOMAgent::markUndoableState()
{
    m_history->markUndoableState();

    return { };
}

} // namespace WebCore
