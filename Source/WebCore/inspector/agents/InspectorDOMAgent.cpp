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
#include "AddEventListenerOptions.h"
#include "Attr.h"
#include "AudioTrack.h"
#include "AudioTrackConfiguration.h"
#include "AudioTrackList.h"
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
#include "CodecUtilities.h"
#include "CommandLineAPIHost.h"
#include "ComposedTreeIterator.h"
#include "ContainerNode.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DOMEditor.h"
#include "DOMException.h"
#include "DOMPatchSupport.h"
#include "DocumentInlines.h"
#include "DocumentType.h"
#include "Editing.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameTree.h"
#include "FullscreenManager.h"
#include "HTMLElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
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
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMWindowCustom.h"
#include "JSEventListener.h"
#include "JSMediaControlsHost.h"
#include "JSNode.h"
#include "JSVideoColorPrimaries.h"
#include "JSVideoMatrixCoefficients.h"
#include "JSVideoTransferCharacteristics.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MutationEvent.h"
#include "Node.h"
#include "NodeList.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PseudoElement.h"
#include "RenderGrid.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "RenderStyleConstants.h"
#include "ScriptController.h"
#include "SelectorChecker.h"
#include "ShadowRoot.h"
#include "StaticNodeList.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleSheetList.h"
#include "Styleable.h"
#include "Text.h"
#include "TextNodeTraversal.h"
#include "Timer.h"
#include "VideoPlaybackQuality.h"
#include "VideoTrack.h"
#include "VideoTrackConfiguration.h"
#include "VideoTrackList.h"
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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorDOMAgent);

using namespace Inspector;

using namespace HTMLNames;

static const size_t maxTextSize = 10000;
static const UChar horizontalEllipsisUChar[] = { horizontalEllipsis, 0 };

static std::optional<Color> parseColor(RefPtr<JSON::Object>&& colorObject)
{
    if (!colorObject)
        return std::nullopt;

    auto r = colorObject->getInteger("r"_s);
    auto g = colorObject->getInteger("g"_s);
    auto b = colorObject->getInteger("b"_s);
    if (!r || !g || !b)
        return std::nullopt;

    auto a = colorObject->getDouble("a"_s);
    if (!a)
        return { makeFromComponentsClamping<SRGBA<uint8_t>>(*r, *g, *b) };
    return { makeFromComponentsClampingExceptAlpha<SRGBA<uint8_t>>(*r, *g, *b, convertFloatAlphaTo<uint8_t>(*a)) };
}

static std::optional<Color> parseRequiredConfigColor(const String& fieldName, JSON::Object& configObject)
{
    return parseColor(configObject.getObject(fieldName));
}

static Color parseOptionalConfigColor(const String& fieldName, JSON::Object& configObject)
{
    return parseRequiredConfigColor(fieldName, configObject).value_or(Color::transparentBlack);
}

static bool parseQuad(Ref<JSON::Array>&& quadArray, FloatQuad* quad)
{
    std::array<double, 8> coordinates;
    if (quadArray->length() != coordinates.size())
        return false;
    for (size_t i = 0; i < coordinates.size(); ++i) {
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

class RevalidateStyleAttributeTask final : public CanMakeCheckedPtr<RevalidateStyleAttributeTask> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RevalidateStyleAttributeTask);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RevalidateStyleAttributeTask);
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

    void handleEvent(ScriptExecutionContext&, Event& event) final
    {
        RefPtr node = dynamicDowncast<Node>(event.target());
        if (!node || m_domAgent.m_dispatchedEvents.contains(&event))
            return;

        auto nodeId = m_domAgent.pushNodePathToFrontend(node.get());
        if (!nodeId)
            return;

        m_domAgent.m_dispatchedEvents.add(&event);

        RefPtr<JSON::Object> data = JSON::Object::create();

#if ENABLE(FULLSCREEN_API)
        if (event.type() == eventNames().webkitfullscreenchangeEvent || event.type() == eventNames().fullscreenchangeEvent)
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
    return static_cast<bool>(ec) ? String(DOMException::name(ec)) : emptyString();
}

String InspectorDOMAgent::toErrorString(Exception&& exception)
{
    return DOMException::name(exception.code());
}

InspectorDOMAgent::InspectorDOMAgent(PageAgentContext& context, InspectorOverlay& overlay)
    : InspectorAgentBase("DOM"_s, context)
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_frontendDispatcher(makeUnique<Inspector::DOMFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::DOMBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
    , m_overlay(overlay)
    , m_destroyedNodesTimer(*this, &InspectorDOMAgent::destroyedNodesTimerFired)
#if ENABLE(VIDEO)
    , m_mediaMetricsTimer(*this, &InspectorDOMAgent::mediaMetricsTimerFired)
#endif
{
}

InspectorDOMAgent::~InspectorDOMAgent() = default;

Ref<InspectorOverlay> InspectorDOMAgent::protectedOverlay() const
{
    return m_overlay.get();
}

void InspectorDOMAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
    m_history = makeUnique<InspectorHistory>();
    m_domEditor = makeUnique<DOMEditor>(*m_history);

    m_instrumentingAgents.setPersistentDOMAgent(this);
    auto* localMainFrame = dynamicDowncast<LocalFrame>(m_inspectedPage.mainFrame());
    if (!localMainFrame)
        return;
    m_document = localMainFrame->document();

    // Force a layout so that we can collect additional information from the layout process.
    relayoutDocument();

#if ENABLE(VIDEO)
    if (m_document)
        addEventListenersToNode(*m_document);

    for (auto& mediaElement : HTMLMediaElement::allMediaElements())
        addEventListenersToNode(Ref { mediaElement.get() });
#endif
}

void InspectorDOMAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_history.reset();
    m_domEditor.reset();
    m_nodeToFocus = nullptr;
    m_mousedOverNode = nullptr;
    m_inspectedNode = nullptr;

    Inspector::Protocol::ErrorString ignored;
    setSearchingForNode(ignored, false, nullptr, nullptr, nullptr, false);
    hideHighlight();

    Ref overlay = m_overlay.get();
    overlay->clearAllGridOverlays();
    overlay->clearAllFlexOverlays();

    m_instrumentingAgents.setPersistentDOMAgent(nullptr);
    m_documentRequested = false;
    reset();
}

Vector<Document*> InspectorDOMAgent::documents()
{
    Vector<Document*> result;
    for (Frame* frame = m_document->frame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* document = localFrame->document();
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

    m_destroyedDetachedNodeIdentifiers.clear();
    m_destroyedAttachedNodeIdentifiers.clear();
    if (m_destroyedNodesTimer.isActive())
        m_destroyedNodesTimer.stop();
}

void InspectorDOMAgent::setDocument(Document* document)
{
    if (document == m_document.get())
        return;

    reset();

    m_document = document;

    // Force a layout so that we can collect additional information from the layout process.
    relayoutDocument();

    if (!m_documentRequested)
        return;

    // Immediately communicate null document or document that has finished loading.
    if (!document || !document->parsing())
        m_frontendDispatcher->documentUpdated();
}

void InspectorDOMAgent::relayoutDocument()
{
    if (!m_document)
        return;

    m_flexibleBoxRendererCachedItemsAtStartOfLine.clear();
    
    m_document->updateLayout();
}

Inspector::Protocol::DOM::NodeId InspectorDOMAgent::bind(Node& node)
{
    return m_nodeToId.ensure(node, [&] {
        auto id = m_lastNodeId++;
        m_idToNode.set(id, node);
        return id;
    }).iterator->value;
}

void InspectorDOMAgent::unbind(Node& node)
{
    auto id = m_nodeToId.take(node);
    if (!id)
        return;

    m_idToNode.remove(id);

    if (auto* frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(node)) {
        if (RefPtr contentDocument = frameOwner->contentDocument())
            unbind(*contentDocument);
    }

    if (RefPtr element = dynamicDowncast<Element>(node)) {
        if (RefPtr root = element->shadowRoot())
            unbind(*root);
        if (RefPtr beforeElement = element->beforePseudoElement())
            unbind(*beforeElement);
        if (RefPtr afterElement = element->afterPseudoElement())
            unbind(*afterElement);
    }

    if (auto* cssAgent = m_instrumentingAgents.enabledCSSAgent())
        cssAgent->didRemoveDOMNode(node, id);

    if (m_childrenRequested.remove(id)) {
        // FIXME: Would be better to do this iteratively rather than recursively.
        for (Node* child = innerFirstChild(&node); child; child = innerNextSibling(child))
            unbind(*child);
    }
}

Node* InspectorDOMAgent::assertNode(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    RefPtr node = nodeForId(nodeId);
    if (!node) {
        errorString = "Missing node for given nodeId"_s;
        return nullptr;
    }
    return node.get();
}

Document* InspectorDOMAgent::assertDocument(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    RefPtr node = assertNode(errorString, nodeId);
    if (!node)
        return nullptr;
    RefPtr document = dynamicDowncast<Document>(*node);
    if (!document)
        errorString = "Node for given nodeId is not a document"_s;
    return document.get();
}

Element* InspectorDOMAgent::assertElement(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    RefPtr node = assertNode(errorString, nodeId);
    if (!node)
        return nullptr;
    RefPtr element = dynamicDowncast<Element>(*node);
    if (!element)
        errorString = "Node for given nodeId is not an element"_s;
    return element.get();
}

Node* InspectorDOMAgent::assertEditableNode(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
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
    return node.get();
}

Element* InspectorDOMAgent::assertEditableElement(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId nodeId)
{
    RefPtr node = assertEditableNode(errorString, nodeId);
    if (!node)
        return nullptr;
    RefPtr element = dynamicDowncast<Element>(node);
    if (!element)
        errorString = "Node for given nodeId is not an element"_s;
    return element.get();
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::DOM::Node>> InspectorDOMAgent::getDocument()
{
    m_documentRequested = true;

    if (!m_document)
        return makeUnexpected("Internal error: missing document"_s);

    // Reset backend state.
    RefPtr<Document> document = m_document;
    reset();
    m_document = document;

    auto root = buildObjectForNode(m_document.get(), 2);

    if (m_nodeToFocus)
        focusNode();

    return root;
}

void InspectorDOMAgent::pushChildNodesToFrontend(Inspector::Protocol::DOM::NodeId nodeId, int depth)
{
    Node* node = nodeForId(nodeId);
    if (!node || (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE && node->nodeType() != Node::DOCUMENT_FRAGMENT_NODE))
        return;

    if (m_childrenRequested.contains(nodeId)) {
        if (depth <= 1)
            return;

        depth--;

        for (node = innerFirstChild(node); node; node = innerNextSibling(node)) {
            auto childNodeId = boundNodeId(node);
            ASSERT(childNodeId);
            pushChildNodesToFrontend(childNodeId, depth);
        }

        return;
    }

    auto children = buildArrayForContainerChildren(node, depth);
    m_frontendDispatcher->setChildNodes(nodeId, WTFMove(children));
}

void InspectorDOMAgent::discardBindings()
{
    m_nodeToId.clear();
    m_idToNode.clear();
    m_dispatchedEvents.clear();
    m_eventListenerEntries.clear();
    m_childrenRequested.clear();
}

static Element* elementToPushForStyleable(const Styleable& styleable)
{
    auto* element = &styleable.element;
    // FIXME: We want to get rid of PseudoElement.
    if (styleable.pseudoElementIdentifier) {
        if (styleable.pseudoElementIdentifier->pseudoId == PseudoId::Before)
            return element->beforePseudoElement();
        if (styleable.pseudoElementIdentifier->pseudoId == PseudoId::After)
            return element->afterPseudoElement();
    }
    return element;
}

Inspector::Protocol::DOM::NodeId InspectorDOMAgent::pushStyleableElementToFrontend(const Styleable& styleable)
{
    auto* element = elementToPushForStyleable(styleable);
    return pushNodeToFrontend(element ? element : &styleable.element);
}

Inspector::Protocol::DOM::NodeId InspectorDOMAgent::pushNodeToFrontend(Node* nodeToPush)
{
    if (!nodeToPush)
        return 0;

    // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented

    Inspector::Protocol::ErrorString ignored;
    return pushNodeToFrontend(ignored, boundNodeId(&nodeToPush->document()), nodeToPush);
}

Inspector::Protocol::DOM::NodeId InspectorDOMAgent::pushNodeToFrontend(Inspector::Protocol::ErrorString& errorString, Inspector::Protocol::DOM::NodeId documentNodeId, Node* nodeToPush)
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

Node* InspectorDOMAgent::nodeForId(Inspector::Protocol::DOM::NodeId id)
{
    if (!m_idToNode.isValidKey(id))
        return nullptr;

    return m_idToNode.get(id).get();
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::requestChildNodes(Inspector::Protocol::DOM::NodeId nodeId, std::optional<int>&& depth)
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

Inspector::Protocol::ErrorStringOr<std::optional<Inspector::Protocol::DOM::NodeId>> InspectorDOMAgent::querySelector(Inspector::Protocol::DOM::NodeId nodeId, const String& selector)
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

    auto* queryResultNode = queryResult.releaseReturnValue();
    if (!queryResultNode)
        return { };

    auto resultNodeId = pushNodePathToFrontend(errorString, queryResultNode);
    if (!resultNodeId)
        return makeUnexpected(errorString);

    return { resultNodeId };
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> InspectorDOMAgent::querySelectorAll(Inspector::Protocol::DOM::NodeId nodeId, const String& selector)
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
    for (unsigned i = 0; i < nodes->length(); ++i)
        nodeIds->addItem(pushNodePathToFrontend(nodes->item(i)));
    return nodeIds;
}

Inspector::Protocol::DOM::NodeId InspectorDOMAgent::pushNodePathToFrontend(Node* nodeToPush)
{
    Inspector::Protocol::ErrorString ignored;
    return pushNodePathToFrontend(ignored, nodeToPush);
}

Ref<Inspector::Protocol::DOM::Styleable> InspectorDOMAgent::pushStyleablePathToFrontend(Inspector::Protocol::ErrorString errorString, const Styleable& styleable)
{
    auto* element = elementToPushForStyleable(styleable);
    auto nodeId = pushNodePathToFrontend(errorString, element ? element : &styleable.element);

    auto protocolStyleable = Inspector::Protocol::DOM::Styleable::create()
        .setNodeId(nodeId)
        .release();

    // FIXME: This should support PseudoElementIdentifier name argument.
    if (styleable.pseudoElementIdentifier) {
        if (auto pseudoId = InspectorCSSAgent::protocolValueForPseudoId(styleable.pseudoElementIdentifier->pseudoId))
            protocolStyleable->setPseudoId(*pseudoId);
    }

    return protocolStyleable;
}

Inspector::Protocol::DOM::NodeId InspectorDOMAgent::pushNodePathToFrontend(Inspector::Protocol::ErrorString errorString, Node* nodeToPush)
{
    ASSERT(nodeToPush);  // Invalid input

    if (!m_document) {
        errorString = "Missing document"_s;
        return 0;
    }

    // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
    if (!m_nodeToId.contains(*m_document)) {
        errorString = "Document must have been requested"_s;
        return 0;
    }

    // Return id in case the node is known.
    if (auto result = boundNodeId(nodeToPush))
        return result;

    Node* node = nodeToPush;
    Vector<Node*> path;

    while (true) {
        Node* parent = innerParentNode(node);
        if (!parent) {
            // Node being pushed is detached -> push subtree root.
            auto children = JSON::ArrayOf<Inspector::Protocol::DOM::Node>::create();
            children->addItem(buildObjectForNode(node, 0));
            m_frontendDispatcher->setChildNodes(0, WTFMove(children));
            break;
        } else {
            path.append(parent);
            if (boundNodeId(parent))
                break;
            node = parent;
        }
    }

    for (int i = path.size() - 1; i >= 0; --i) {
        auto nodeId = boundNodeId(path.at(i));
        ASSERT(nodeId);
        pushChildNodesToFrontend(nodeId);
    }
    return boundNodeId(nodeToPush);
}

Inspector::Protocol::DOM::NodeId InspectorDOMAgent::boundNodeId(const Node* node)
{
    if (!node)
        return 0;

    return m_nodeToId.get(*node);
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setAttributeValue(Inspector::Protocol::DOM::NodeId nodeId, const String& name, const String& value)
{
    Inspector::Protocol::ErrorString errorString;

    Element* element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!m_domEditor->setAttribute(*element, AtomString { name }, AtomString { value }, errorString))
        return makeUnexpected(errorString);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setAttributesAsText(Inspector::Protocol::DOM::NodeId nodeId, const String& text, const String& name)
{
    Inspector::Protocol::ErrorString errorString;

    Element* element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    auto parsedElement = createHTMLElement(element->document(), spanTag);
    auto result = parsedElement.get().setInnerHTML(makeString("<span "_s, text, "></span>"_s));
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    Node* child = parsedElement->firstChild();
    if (!child)
        return makeUnexpected("Could not parse given text"_s);

    Element* childElement = downcast<Element>(child);
    if (!childElement->hasAttributes() && !!name) {
        if (!m_domEditor->removeAttribute(*element, AtomString { name }, errorString))
            return makeUnexpected(errorString);
        return { };
    }

    bool foundOriginalAttribute = false;
    for (const Attribute& attribute : childElement->attributesIterator()) {
        // Add attribute pair
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

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::removeAttribute(Inspector::Protocol::DOM::NodeId nodeId, const String& name)
{
    Inspector::Protocol::ErrorString errorString;

    Element* element = assertEditableElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!m_domEditor->removeAttribute(*element, AtomString { name }, errorString))
        return makeUnexpected(errorString);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::removeNode(Inspector::Protocol::DOM::NodeId nodeId)
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

Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> InspectorDOMAgent::setNodeName(Inspector::Protocol::DOM::NodeId nodeId, const String& tagName)
{
    Inspector::Protocol::ErrorString errorString;

    auto oldNode = assertElement(errorString, nodeId);
    if (!oldNode)
        return makeUnexpected(errorString);

    auto createElementResult = oldNode->document().createElementForBindings(AtomString { tagName });
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

Inspector::Protocol::ErrorStringOr<String> InspectorDOMAgent::getOuterHTML(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    return serializeFragment(*node, SerializedNodes::SubtreeIncludingNode);
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setOuterHTML(Inspector::Protocol::DOM::NodeId nodeId, const String& outerHTML)
{
    Inspector::Protocol::ErrorString errorString;

    if (!nodeId) {
        DOMPatchSupport { *m_domEditor, *m_document }.patchDocument(outerHTML);
        return { };
    }

    RefPtr node = assertEditableNode(errorString, nodeId);
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

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::insertAdjacentHTML(Inspector::Protocol::DOM::NodeId nodeId, const String& position, const String& html)
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

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setNodeValue(Inspector::Protocol::DOM::NodeId nodeId, const String& value)
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

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<String>>> InspectorDOMAgent::getSupportedEventNames()
{
    auto list = JSON::ArrayOf<String>::create();

    for (auto& event : eventNames().allEventNames())
        list->addItem(event);

    return list;
}

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::DataBinding>>> InspectorDOMAgent::getDataBindingsForNode(Inspector::Protocol::DOM::NodeId)
{
    return makeUnexpected("Not supported"_s);
}

Inspector::Protocol::ErrorStringOr<String> InspectorDOMAgent::getAssociatedDataForNode(Inspector::Protocol::DOM::NodeId)
{
    return makeUnexpected("Not supported"_s);
}
#endif

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::EventListener>>> InspectorDOMAgent::getEventListenersForNode(Inspector::Protocol::DOM::NodeId nodeId, std::optional<bool>&& includeAncestors)
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
        if (auto* window = node->document().domWindow())
            ancestors.append(window);
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
                eventInformation.append({ ancestor, eventType, WTFMove(filteredListeners) });
        }
    }

    auto listeners = JSON::ArrayOf<Inspector::Protocol::DOM::EventListener>::create();

    auto addListener = [&] (RegisteredEventListener& listener, const EventListenerInfo& info) {
        Inspector::Protocol::DOM::EventListenerId identifier = 0;
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

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setEventListenerDisabled(Inspector::Protocol::DOM::EventListenerId eventListenerId, bool disabled)
{
    auto it = m_eventListenerEntries.find(eventListenerId);
    if (it == m_eventListenerEntries.end())
        return makeUnexpected("Missing event listener for given eventListenerId"_s);

    it->value.disabled = disabled;

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setBreakpointForEventListener(Inspector::Protocol::DOM::EventListenerId eventListenerId, RefPtr<JSON::Object>&& options)
{
    Inspector::Protocol::ErrorString errorString;

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

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::removeBreakpointForEventListener(Inspector::Protocol::DOM::EventListenerId eventListenerId)
{
    auto it = m_eventListenerEntries.find(eventListenerId);
    if (it == m_eventListenerEntries.end())
        return makeUnexpected("Missing event listener for given eventListenerId"_s);

    if (!it->value.breakpoint)
        return makeUnexpected("Breakpoint for given eventListenerId missing"_s);

    it->value.breakpoint = nullptr;

    return { };
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::DOM::AccessibilityProperties>> InspectorDOMAgent::getAccessibilityPropertiesForNode(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    return buildObjectForAccessibilityProperties(*node);
}

Inspector::Protocol::ErrorStringOr<std::tuple<String /* searchId */, int /* resultCount */>> InspectorDOMAgent::performSearch(const String& query, RefPtr<JSON::Array>&& nodeIds, std::optional<bool>&& caseSensitive)
{
    Inspector::Protocol::ErrorString errorString;

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

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> InspectorDOMAgent::getSearchResults(const String& searchId, int fromIndex, int toIndex)
{
    SearchResults::iterator it = m_searchResults.find(searchId);
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

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::discardSearchResults(const String& searchId)
{
    m_searchResults.remove(searchId);

    return { };
}

bool InspectorDOMAgent::handleMousePress()
{
    if (!m_searchingForNode)
        return false;

    if (RefPtr node = protectedOverlay()->highlightedNode()) {
        inspect(node.get());
        return true;
    }
    return false;
}

bool InspectorDOMAgent::handleTouchEvent(Node& node)
{
    if (!m_searchingForNode)
        return false;

    if (m_inspectModeHighlightConfig) {
        protectedOverlay()->highlightNode(&node, *m_inspectModeHighlightConfig, m_inspectModeGridOverlayConfig, m_inspectModeFlexOverlayConfig, m_inspectModeShowRulers);
        inspect(&node);
        return true;
    }

    return false;
}

void InspectorDOMAgent::inspect(Node* inspectedNode)
{
    Inspector::Protocol::ErrorString ignored;
    RefPtr<Node> node = inspectedNode;
    setSearchingForNode(ignored, false, nullptr, nullptr, nullptr, false);

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
    auto node = std::exchange(m_nodeToFocus, nullptr);
    auto frame = node->document().frame();
    if (!frame)
        return;

    auto& globalObject = mainWorldGlobalObject(*frame);
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&globalObject);
    if (injectedScript.hasNoValue())
        return;

    injectedScript.inspectObject(nodeAsScriptValue(globalObject, node.get()));
}

void InspectorDOMAgent::mouseDidMoveOverElement(const HitTestResult& result, OptionSet<PlatformEventModifier>)
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
    if (!node)
        return;

    if (m_inspectModeHighlightConfig)
        protectedOverlay()->highlightNode(node, *m_inspectModeHighlightConfig, m_inspectModeGridOverlayConfig, m_inspectModeFlexOverlayConfig, m_inspectModeShowRulers);
}

void InspectorDOMAgent::setSearchingForNode(Inspector::Protocol::ErrorString& errorString, bool enabled, RefPtr<JSON::Object>&& highlightInspectorObject, RefPtr<JSON::Object>&& gridOverlayInspectorObject, RefPtr<JSON::Object>&& flexOverlayInspectorObject, bool showRulers)
{
    if (m_searchingForNode == enabled)
        return;

    m_searchingForNode = enabled;

    if (m_searchingForNode) {
        m_inspectModeHighlightConfig = highlightConfigFromInspectorObject(errorString, WTFMove(highlightInspectorObject));
        if (!m_inspectModeHighlightConfig)
            return;

        bool providedGridOverlayConfig = gridOverlayInspectorObject;
        m_inspectModeGridOverlayConfig = gridOverlayConfigFromInspectorObject(errorString, WTFMove(gridOverlayInspectorObject));
        if (providedGridOverlayConfig && !m_inspectModeGridOverlayConfig)
            return;

        bool providedFlexOverlayConfig = flexOverlayInspectorObject;
        m_inspectModeFlexOverlayConfig = flexOverlayConfigFromInspectorObject(errorString, WTFMove(flexOverlayInspectorObject));
        if (providedFlexOverlayConfig && !m_inspectModeFlexOverlayConfig)
            return;

        m_inspectModeShowRulers = showRulers;

        highlightMousedOverNode();
    } else
        hideHighlight();

    protectedOverlay()->didSetSearchingForNode(m_searchingForNode);

    if (InspectorClient* client = m_inspectedPage.inspectorController().inspectorClient())
        client->elementSelectionChanged(m_searchingForNode);
}

std::unique_ptr<InspectorOverlay::Highlight::Config> InspectorDOMAgent::highlightConfigFromInspectorObject(Inspector::Protocol::ErrorString& errorString, RefPtr<JSON::Object>&& highlightInspectorObject)
{
    if (!highlightInspectorObject) {
        errorString = "Internal error: highlight configuration parameter is missing"_s;
        return nullptr;
    }

    auto highlightConfig = makeUnique<InspectorOverlay::Highlight::Config>();
    highlightConfig->showInfo = highlightInspectorObject->getBoolean("showInfo"_s).value_or(false);
    highlightConfig->content = parseOptionalConfigColor("contentColor"_s, *highlightInspectorObject);
    highlightConfig->padding = parseOptionalConfigColor("paddingColor"_s, *highlightInspectorObject);
    highlightConfig->border = parseOptionalConfigColor("borderColor"_s, *highlightInspectorObject);
    highlightConfig->margin = parseOptionalConfigColor("marginColor"_s, *highlightInspectorObject);
    return highlightConfig;
}

std::optional<InspectorOverlay::Grid::Config> InspectorDOMAgent::gridOverlayConfigFromInspectorObject(Inspector::Protocol::ErrorString& errorString, RefPtr<JSON::Object>&& gridOverlayInspectorObject)
{
    if (!gridOverlayInspectorObject)
        return std::nullopt;

    auto gridColor = parseRequiredConfigColor("gridColor"_s, *gridOverlayInspectorObject);
    if (!gridColor) {
        errorString = "Internal error: grid color property of grid overlay configuration parameter is missing"_s;
        return std::nullopt;
    }

    InspectorOverlay::Grid::Config gridOverlayConfig;
    gridOverlayConfig.gridColor = *gridColor;
    gridOverlayConfig.showLineNames = gridOverlayInspectorObject->getBoolean("showLineNames"_s).value_or(false);
    gridOverlayConfig.showLineNumbers = gridOverlayInspectorObject->getBoolean("showLineNumbers"_s).value_or(false);
    gridOverlayConfig.showExtendedGridLines = gridOverlayInspectorObject->getBoolean("showExtendedGridLines"_s).value_or(false);
    gridOverlayConfig.showTrackSizes = gridOverlayInspectorObject->getBoolean("showTrackSizes"_s).value_or(false);
    gridOverlayConfig.showAreaNames = gridOverlayInspectorObject->getBoolean("showAreaNames"_s).value_or(false);
    return gridOverlayConfig;
}

std::optional<InspectorOverlay::Flex::Config> InspectorDOMAgent::flexOverlayConfigFromInspectorObject(Inspector::Protocol::ErrorString& errorString, RefPtr<JSON::Object>&& flexOverlayInspectorObject)
{
    if (!flexOverlayInspectorObject)
        return std::nullopt;

    auto flexColor = parseRequiredConfigColor("flexColor"_s, *flexOverlayInspectorObject);
    if (!flexColor) {
        errorString = "Internal error: flex color property of flex overlay configuration parameter is missing"_s;
        return std::nullopt;
    }

    InspectorOverlay::Flex::Config flexOverlayConfig;
    flexOverlayConfig.flexColor = *flexColor;
    flexOverlayConfig.showOrderNumbers = flexOverlayInspectorObject->getBoolean("showOrderNumbers"_s).value_or(false);
    return flexOverlayConfig;
}

#if PLATFORM(IOS_FAMILY)
Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setInspectModeEnabled(bool enabled, RefPtr<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig)
{
    Inspector::Protocol::ErrorString errorString;

    setSearchingForNode(errorString, enabled, WTFMove(highlightConfig), WTFMove(gridOverlayConfig), WTFMove(flexOverlayConfig), false);

    if (!!errorString)
        return makeUnexpected(errorString);

    return { };
}
#else
Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setInspectModeEnabled(bool enabled, RefPtr<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig, std::optional<bool>&& showRulers)
{
    Inspector::Protocol::ErrorString errorString;

    setSearchingForNode(errorString, enabled, WTFMove(highlightConfig), WTFMove(gridOverlayConfig), WTFMove(flexOverlayConfig), showRulers && *showRulers);

    if (!!errorString)
        return makeUnexpected(errorString);

    return { };
}
#endif

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightRect(int x, int y, int width, int height, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, std::optional<bool>&& usePageCoordinates)
{
    auto quad = makeUnique<FloatQuad>(FloatRect(x, y, width, height));
    innerHighlightQuad(WTFMove(quad), WTFMove(color), WTFMove(outlineColor), WTFMove(usePageCoordinates));

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightQuad(Ref<JSON::Array>&& quadObject, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, std::optional<bool>&& usePageCoordinates)
{
    auto quad = makeUnique<FloatQuad>();
    if (!parseQuad(WTFMove(quadObject), quad.get()))
        return makeUnexpected("Unexpected invalid quad"_s);

    innerHighlightQuad(WTFMove(quad), WTFMove(color), WTFMove(outlineColor), WTFMove(usePageCoordinates));

    return { };
}

void InspectorDOMAgent::innerHighlightQuad(std::unique_ptr<FloatQuad> quad, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, std::optional<bool>&& usePageCoordinates)
{
    auto highlightConfig = makeUnique<InspectorOverlay::Highlight::Config>();
    highlightConfig->content = parseColor(WTFMove(color)).value_or(Color::transparentBlack);
    highlightConfig->contentOutline = parseColor(WTFMove(outlineColor)).value_or(Color::transparentBlack);
    highlightConfig->usePageCoordinates = usePageCoordinates ? *usePageCoordinates : false;
    protectedOverlay()->highlightQuad(WTFMove(quad), *highlightConfig);
}

#if PLATFORM(IOS_FAMILY)

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightSelector(const String& selectorString, const Inspector::Protocol::Network::FrameId& frameId, Ref<JSON::Object>&& highlightInspectorObject, RefPtr<JSON::Object>&& gridOverlayInspectorObject, RefPtr<JSON::Object>&& flexOverlayInspectorObject)
{
    return highlightSelector(selectorString, frameId, WTFMove(highlightInspectorObject), WTFMove(gridOverlayInspectorObject), WTFMove(flexOverlayInspectorObject), std::nullopt);
}

#endif // PLATFORM(IOS_FAMILY)

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightSelector(const String& selectorString, const Inspector::Protocol::Network::FrameId& frameId, Ref<JSON::Object>&& highlightInspectorObject, RefPtr<JSON::Object>&& gridOverlayInspectorObject, RefPtr<JSON::Object>&& flexOverlayInspectorObject, std::optional<bool>&& showRulers)
{
    Inspector::Protocol::ErrorString errorString;

    auto highlightConfig = highlightConfigFromInspectorObject(errorString, WTFMove(highlightInspectorObject));
    if (!highlightConfig)
        return makeUnexpected(errorString);

    bool providedGridOverlayConfig = gridOverlayInspectorObject;
    auto gridOverlayConfig = gridOverlayConfigFromInspectorObject(errorString, WTFMove(gridOverlayInspectorObject));
    if (providedGridOverlayConfig && !gridOverlayConfig)
        return makeUnexpected(errorString);

    bool providedFlexOverlayConfig = flexOverlayInspectorObject;
    auto flexOverlayConfig = flexOverlayConfigFromInspectorObject(errorString, WTFMove(flexOverlayInspectorObject));
    if (providedFlexOverlayConfig && !flexOverlayConfig)
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
    auto selectorList = parser.parseSelectorList(selectorString);
    if (!selectorList)
        return { };

    SelectorChecker selectorChecker(*document);

    Vector<Ref<Node>> nodeList;
    HashSet<Ref<Node>> seenNodes;

    for (auto& descendant : composedTreeDescendants(*document)) {
        RefPtr descendantElement = dynamicDowncast<Element>(descendant);
        if (!descendantElement)
            continue;

        auto isInUserAgentShadowTree = descendantElement->isInUserAgentShadowTree();
        auto pseudoId = descendantElement->pseudoId();

        for (const auto* selector = selectorList->first(); selector; selector = CSSSelectorList::next(selector)) {
            if (isInUserAgentShadowTree && (selector->match() != CSSSelector::Match::PseudoElement || selector->value() != descendantElement->userAgentPart()))
                continue;

            SelectorChecker::CheckingContext context(SelectorChecker::Mode::ResolvingStyle);
            context.pseudoId = pseudoId;

            if (selectorChecker.match(*selector, *descendantElement, context)) {
                if (seenNodes.add(*descendantElement))
                    nodeList.append(*descendantElement);
            }

            if (context.pseudoIDSet) {
                auto pseudoIDs = PseudoIdSet::fromMask(context.pseudoIDSet.data());

                if (pseudoIDs.has(PseudoId::Before)) {
                    pseudoIDs.remove(PseudoId::Before);
                    if (RefPtr beforePseudoElement = descendantElement->beforePseudoElement()) {
                        if (seenNodes.add(*beforePseudoElement))
                            nodeList.append(*beforePseudoElement);
                    }
                }

                if (pseudoIDs.has(PseudoId::After)) {
                    pseudoIDs.remove(PseudoId::After);
                    if (RefPtr afterPseudoElement = descendantElement->afterPseudoElement()) {
                        if (seenNodes.add(*afterPseudoElement))
                            nodeList.append(*afterPseudoElement);
                    }
                }

                if (pseudoIDs) {
                    if (seenNodes.add(*descendantElement))
                        nodeList.append(*descendantElement);
                }
            }
        }
    }

    protectedOverlay()->highlightNodeList(StaticNodeList::create(WTFMove(nodeList)), *highlightConfig, WTFMove(gridOverlayConfig), WTFMove(flexOverlayConfig), showRulers && *showRulers);

    return { };
}

#if PLATFORM(IOS_FAMILY)

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightNode(std::optional<Inspector::Protocol::DOM::NodeId>&& nodeId, const Inspector::Protocol::Runtime::RemoteObjectId& objectId, Ref<JSON::Object>&& highlightInspectorObject, RefPtr<JSON::Object>&& gridOverlayInspectorObject, RefPtr<JSON::Object>&& flexOverlayInspectorObject)
{
    return highlightNode(WTFMove(nodeId), objectId, WTFMove(highlightInspectorObject), WTFMove(gridOverlayInspectorObject), WTFMove(flexOverlayInspectorObject), std::nullopt);
}

#endif // PLATFORM(IOS_FAMILY)

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightNode(std::optional<Inspector::Protocol::DOM::NodeId>&& nodeId, const Inspector::Protocol::Runtime::RemoteObjectId& objectId, Ref<JSON::Object>&& highlightInspectorObject, RefPtr<JSON::Object>&& gridOverlayInspectorObject, RefPtr<JSON::Object>&& flexOverlayInspectorObject, std::optional<bool>&& showRulers)
{
    Inspector::Protocol::ErrorString errorString;

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

    std::unique_ptr<InspectorOverlay::Highlight::Config> highlightConfig = highlightConfigFromInspectorObject(errorString, WTFMove(highlightInspectorObject));
    if (!highlightConfig)
        return makeUnexpected(errorString);

    bool providedGridOverlayConfig = gridOverlayInspectorObject;
    auto gridOverlayConfig = gridOverlayConfigFromInspectorObject(errorString, WTFMove(gridOverlayInspectorObject));
    if (providedGridOverlayConfig && !gridOverlayConfig)
        return makeUnexpected(errorString);

    bool providedFlexOverlayConfig = flexOverlayInspectorObject;
    auto flexOverlayConfig = flexOverlayConfigFromInspectorObject(errorString, WTFMove(flexOverlayInspectorObject));
    if (providedFlexOverlayConfig && !flexOverlayConfig)
        return makeUnexpected(errorString);

    protectedOverlay()->highlightNode(node, *highlightConfig, WTFMove(gridOverlayConfig), WTFMove(flexOverlayConfig), showRulers && *showRulers);

    return { };
}

#if PLATFORM(IOS_FAMILY)

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightNodeList(Ref<JSON::Array>&& nodeIds, Ref<JSON::Object>&& highlightInspectorObject, RefPtr<JSON::Object>&& gridOverlayInspectorObject, RefPtr<JSON::Object>&& flexOverlayInspectorObject)
{
    return highlightNodeList(WTFMove(nodeIds), WTFMove(highlightInspectorObject), WTFMove(gridOverlayInspectorObject), WTFMove(flexOverlayInspectorObject), std::nullopt);
}

#endif // PLATFORM(IOS_FAMILY)

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightNodeList(Ref<JSON::Array>&& nodeIds, Ref<JSON::Object>&& highlightInspectorObject, RefPtr<JSON::Object>&& gridOverlayInspectorObject, RefPtr<JSON::Object>&& flexOverlayInspectorObject, std::optional<bool>&& showRulers)
{
    Inspector::Protocol::ErrorString errorString;

    Vector<Ref<Node>> nodes;
    for (auto& nodeIdValue : nodeIds.get()) {
        auto nodeId = nodeIdValue->asInteger();
        if (!nodeId)
            return makeUnexpected("Unexpected non-integer item in given nodeIds"_s);

        // In the case that a node is removed in the time between when highlightNodeList is invoked
        // by the frontend and it is executed by the backend, we should still attempt to highlight
        // as many nodes as possible. As such, we should ignore any errors generated when attempting
        // to get a Node from a given nodeId. 
        Inspector::Protocol::ErrorString ignored;
        Node* node = assertNode(ignored, *nodeId);
        if (!node)
            continue;

        nodes.append(*node);
    }

    std::unique_ptr<InspectorOverlay::Highlight::Config> highlightConfig = highlightConfigFromInspectorObject(errorString, WTFMove(highlightInspectorObject));
    if (!highlightConfig)
        return makeUnexpected(errorString);

    bool providedGridOverlayConfig = gridOverlayInspectorObject;
    auto gridOverlayConfig = gridOverlayConfigFromInspectorObject(errorString, WTFMove(gridOverlayInspectorObject));
    if (providedGridOverlayConfig && !gridOverlayConfig)
        return makeUnexpected(errorString);

    bool providedFlexOverlayConfig = flexOverlayInspectorObject;
    auto flexOverlayConfig = flexOverlayConfigFromInspectorObject(errorString, WTFMove(flexOverlayInspectorObject));
    if (providedFlexOverlayConfig && !flexOverlayConfig)
        return makeUnexpected(errorString);

    protectedOverlay()->highlightNodeList(StaticNodeList::create(WTFMove(nodes)), *highlightConfig, WTFMove(gridOverlayConfig), WTFMove(flexOverlayConfig), showRulers && *showRulers);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::highlightFrame(const Inspector::Protocol::Network::FrameId& frameId, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor)
{
    Inspector::Protocol::ErrorString errorString;

    auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
    if (!pageAgent)
        return makeUnexpected("Page domain must be enabled"_s);

    auto* frame = pageAgent->assertFrame(errorString, frameId);
    if (!frame)
        return makeUnexpected(errorString);

    if (frame->ownerElement()) {
        auto highlightConfig = makeUnique<InspectorOverlay::Highlight::Config>();
        highlightConfig->showInfo = true; // Always show tooltips for frames.
        highlightConfig->content = parseColor(WTFMove(color)).value_or(Color::transparentBlack);
        highlightConfig->contentOutline = parseColor(WTFMove(outlineColor)).value_or(Color::transparentBlack);
        protectedOverlay()->highlightNode(frame->ownerElement(), *highlightConfig);
    }

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::hideHighlight()
{
    protectedOverlay()->hideHighlight();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::showGridOverlay(Inspector::Protocol::DOM::NodeId nodeId,  Ref<JSON::Object>&& gridOverlayInspectorObject)
{
    Inspector::Protocol::ErrorString errorString;
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    auto config = gridOverlayConfigFromInspectorObject(errorString, WTFMove(gridOverlayInspectorObject));
    if (!config)
        return makeUnexpected(errorString);

    protectedOverlay()->setGridOverlayForNode(*node, *config);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::hideGridOverlay(std::optional<Inspector::Protocol::DOM::NodeId>&& nodeId)
{
    if (nodeId) {
        Inspector::Protocol::ErrorString errorString;
        auto node = assertNode(errorString, *nodeId);
        if (!node)
            return makeUnexpected(errorString);

        return protectedOverlay()->clearGridOverlayForNode(*node);
}

    protectedOverlay()->clearAllGridOverlays();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::showFlexOverlay(Inspector::Protocol::DOM::NodeId nodeId, Ref<JSON::Object>&& flexOverlayInspectorObject)
{
    Inspector::Protocol::ErrorString errorString;
    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    auto config = flexOverlayConfigFromInspectorObject(errorString, WTFMove(flexOverlayInspectorObject));
    if (!config)
        return makeUnexpected(errorString);

    protectedOverlay()->setFlexOverlayForNode(*node, *config);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::hideFlexOverlay(std::optional<Inspector::Protocol::DOM::NodeId>&& nodeId)
{
    if (nodeId) {
        Inspector::Protocol::ErrorString errorString;
        auto node = assertNode(errorString, *nodeId);
        if (!node)
            return makeUnexpected(errorString);

        return protectedOverlay()->clearFlexOverlayForNode(*node);
    }

    protectedOverlay()->clearAllFlexOverlays();

    return { };
}

Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> InspectorDOMAgent::moveTo(Inspector::Protocol::DOM::NodeId nodeId, Inspector::Protocol::DOM::NodeId targetNodeId, std::optional<Inspector::Protocol::DOM::NodeId>&& insertBeforeNodeId)
{
    Inspector::Protocol::ErrorString errorString;

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

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::undo()
{
    auto result = m_history->undo();
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::redo()
{
    auto result = m_history->redo();
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::markUndoableState()
{
    m_history->markUndoableState();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::focus(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    Element* element = assertElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);
    if (!element->isFocusable())
        return makeUnexpected("Element for given nodeId is not focusable"_s);

    element->focus();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setInspectedNode(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;

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

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> InspectorDOMAgent::resolveNode(Inspector::Protocol::DOM::NodeId nodeId, const String& objectGroup)
{
    Inspector::Protocol::ErrorString errorString;

    Node* node = assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    auto object = resolveNode(node, objectGroup);
    if (!object)
        return makeUnexpected("Missing injected script for given nodeId"_s);

    return object.releaseNonNull();
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<String>>> InspectorDOMAgent::getAttributes(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    Element* element = assertElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    return buildArrayForElementAttributes(element);
}

Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> InspectorDOMAgent::requestNode(const Inspector::Protocol::Runtime::RemoteObjectId& objectId)
{
    Inspector::Protocol::ErrorString errorString;

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

static bool pseudoElementType(PseudoId pseudoId, Inspector::Protocol::DOM::PseudoType* type)
{
    switch (pseudoId) {
    case PseudoId::Before:
        *type = Inspector::Protocol::DOM::PseudoType::Before;
        return true;
    case PseudoId::After:
        *type = Inspector::Protocol::DOM::PseudoType::After;
        return true;
    default:
        return false;
    }
}

static Inspector::Protocol::DOM::ShadowRootType shadowRootType(ShadowRootMode mode)
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

static Inspector::Protocol::DOM::CustomElementState customElementState(const Element& element)
{
    if (element.isDefinedCustomElement())
        return Inspector::Protocol::DOM::CustomElementState::Custom;
    if (element.isFailedOrPrecustomizedCustomElement())
        return Inspector::Protocol::DOM::CustomElementState::Failed;
    if (element.isCustomElementUpgradeCandidate())
        return Inspector::Protocol::DOM::CustomElementState::Waiting;
    return Inspector::Protocol::DOM::CustomElementState::Builtin;
}

static String computeContentSecurityPolicySHA256Hash(const Element& element)
{
    // FIXME: Compute the digest with respect to the raw bytes received from the page.
    // See <https://bugs.webkit.org/show_bug.cgi?id=155184>.
    PAL::TextEncoding documentEncoding = element.document().textEncoding();
    const PAL::TextEncoding& encodingToUse = documentEncoding.isValid() ? documentEncoding : PAL::UTF8Encoding();
    auto content = encodingToUse.encode(TextNodeTraversal::contentsAsString(element), PAL::UnencodableHandling::Entities);
    auto cryptoDigest = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    cryptoDigest->addBytes(content.span());
    auto digest = cryptoDigest->computeHash();
    return makeString("sha256-"_s, base64Encoded(digest));
}

Ref<Inspector::Protocol::DOM::Node> InspectorDOMAgent::buildObjectForNode(Node* node, int depth)
{
    auto id = bind(*node);
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
        if (nodeValue.length() > maxTextSize)
            nodeValue = makeString(StringView(nodeValue).left(maxTextSize), horizontalEllipsisUChar);
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

    auto value = Inspector::Protocol::DOM::Node::create()
        .setNodeId(id)
        .setNodeType(enumToUnderlyingType(node->nodeType()))
        .setNodeName(nodeName)
        .setLocalName(localName)
        .setNodeValue(nodeValue)
        .release();

    if (node->isContainerNode()) {
        int nodeCount = innerChildNodeCount(node);
        value->setChildNodeCount(nodeCount);
        auto children = buildArrayForContainerChildren(node, depth);
        if (children->length() > 0)
            value->setChildren(WTFMove(children));
    }

    if (auto* cssAgent = m_instrumentingAgents.enabledCSSAgent()) {
        if (auto layoutFlags = cssAgent->protocolLayoutFlagsForNode(*node))
            value->setLayoutFlags(layoutFlags.releaseNonNull());
    }

    auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
    if (pageAgent) {
        if (auto* frameView = node->document().view())
            value->setFrameId(pageAgent->frameId(&frameView->frame()));
    }

    if (RefPtr element = dynamicDowncast<Element>(*node)) {
        value->setAttributes(buildArrayForElementAttributes(element.get()));
        if (RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(*element)) {
            if (RefPtr document = frameOwner->contentDocument())
                value->setContentDocument(buildObjectForNode(document.get(), 0));
        }

        if (RefPtr root = element->shadowRoot()) {
            auto shadowRoots = JSON::ArrayOf<Inspector::Protocol::DOM::Node>::create();
            shadowRoots->addItem(buildObjectForNode(root.get(), 0));
            value->setShadowRoots(WTFMove(shadowRoots));
        }

        if (RefPtr templateElement = dynamicDowncast<HTMLTemplateElement>(*element))
            value->setTemplateContent(buildObjectForNode(&templateElement->content(), 0));

        if (is<HTMLStyleElement>(element) || (is<HTMLScriptElement>(element) && !element->hasAttributeWithoutSynchronization(HTMLNames::srcAttr)))
            value->setContentSecurityPolicyHash(computeContentSecurityPolicySHA256Hash(*element));

        auto state = customElementState(*element);
        if (state != Inspector::Protocol::DOM::CustomElementState::Builtin)
            value->setCustomElementState(state);

        if (element->pseudoId() != PseudoId::None) {
            Inspector::Protocol::DOM::PseudoType pseudoType;
            if (pseudoElementType(element->pseudoId(), &pseudoType))
                value->setPseudoType(pseudoType);
        } else {
            if (auto pseudoElements = buildArrayForPseudoElements(*element))
                value->setPseudoElements(pseudoElements.releaseNonNull());
        }
    } else if (RefPtr document = dynamicDowncast<Document>(*node)) {
        if (pageAgent)
            value->setFrameId(pageAgent->frameId(document->frame()));
        value->setDocumentURL(documentURLString(document.get()));
        value->setBaseURL(documentBaseURLString(document.get()));
        value->setXmlVersion(document->xmlVersion());
    } else if (RefPtr doctype = dynamicDowncast<DocumentType>(*node)) {
        value->setPublicId(doctype->publicId());
        value->setSystemId(doctype->systemId());
    } else if (RefPtr attribute = dynamicDowncast<Attr>(*node)) {
        value->setName(attribute->name());
        value->setValue(attribute->value());
    } else if (RefPtr shadowRoot = dynamicDowncast<ShadowRoot>(*node))
        value->setShadowRootType(shadowRootType(shadowRoot->mode()));

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

Ref<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> InspectorDOMAgent::buildArrayForContainerChildren(Node* container, int depth)
{
    auto children = JSON::ArrayOf<Inspector::Protocol::DOM::Node>::create();
    if (depth == 0) {
        // Special-case the only text child - pretend that container's children have been requested.
        Node* firstChild = container->firstChild();
        if (firstChild && firstChild->nodeType() == Node::TEXT_NODE && !firstChild->nextSibling()) {
            children->addItem(buildObjectForNode(firstChild, 0));
            m_childrenRequested.add(bind(*container));
        }
        return children;
    }

    Node* child = innerFirstChild(container);
    depth--;
    m_childrenRequested.add(bind(*container));

    while (child) {
        children->addItem(buildObjectForNode(child, depth));
        child = innerNextSibling(child);
    }
    return children;
}

RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> InspectorDOMAgent::buildArrayForPseudoElements(const Element& element)
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

Ref<Inspector::Protocol::DOM::EventListener> InspectorDOMAgent::buildObjectForEventListener(const RegisteredEventListener& registeredEventListener, Inspector::Protocol::DOM::EventListenerId identifier, EventTarget& eventTarget, const AtomString& eventType, bool disabled, const RefPtr<JSC::Breakpoint>& breakpoint)
{
    Ref<EventListener> eventListener = registeredEventListener.callback();

    String handlerName;
    int lineNumber = 0;
    int columnNumber = 0;
    String scriptID;
    if (auto* scriptListener = dynamicDowncast<JSEventListener>(eventListener.get()); scriptListener && scriptListener->isolatedWorld()) {
        RefPtr<Document> document;
        if (auto* scriptExecutionContext = eventTarget.scriptExecutionContext())
            document = dynamicDowncast<Document>(*scriptExecutionContext);
        else if (RefPtr node = dynamicDowncast<Node>(eventTarget))
            document = &node->document();

        JSC::JSObject* handlerObject = nullptr;
        JSC::JSGlobalObject* globalObject = nullptr;

        JSC::JSLockHolder lock(scriptListener->isolatedWorld()->vm());

        if (document) {
            handlerObject = scriptListener->ensureJSFunction(*document);
            if (auto frame = document->frame()) {
                // FIXME: Why do we need the canExecuteScripts check here?
                if (frame->script().canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
                    globalObject = frame->script().globalObject(*scriptListener->isolatedWorld());
            }
        }

        if (handlerObject && globalObject) {
            JSC::VM& vm = globalObject->vm();
            JSC::JSFunction* handlerFunction = JSC::jsDynamicCast<JSC::JSFunction*>(handlerObject);

            if (!handlerFunction) {
                auto scope = DECLARE_CATCH_SCOPE(vm);

                // If the handler is not actually a function, see if it implements the EventListener interface and use that.
                auto handleEventValue = handlerObject->get(globalObject, JSC::Identifier::fromString(vm, "handleEvent"_s));

                if (UNLIKELY(scope.exception()))
                    scope.clearException();

                if (handleEventValue)
                    handlerFunction = JSC::jsDynamicCast<JSC::JSFunction*>(handleEventValue);
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
        .setUseCapture(registeredEventListener.useCapture())
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

void InspectorDOMAgent::processAccessibilityChildren(AXCoreObject& axObject, JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>& childNodeIds)
{
    const auto& children = axObject.unignoredChildren();
    if (!children.size())
        return;

    for (const auto& childObject : children) {
        if (Node* childNode = childObject->node())
            childNodeIds.addItem(pushNodePathToFrontend(childNode));
        else
            processAccessibilityChildren(childObject.get(), childNodeIds);
    }
}
    
Ref<Inspector::Protocol::DOM::AccessibilityProperties> InspectorDOMAgent::buildObjectForAccessibilityProperties(Node& node)
{
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();

    Node* activeDescendantNode = nullptr;
    bool busy = false;
    auto checked = Inspector::Protocol::DOM::AccessibilityProperties::Checked::False;
    auto switchState = Inspector::Protocol::DOM::AccessibilityProperties::SwitchState::Off;
    RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>> childNodeIds;
    RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>> controlledNodeIds;
    auto currentState = Inspector::Protocol::DOM::AccessibilityProperties::Current::False;
    bool exists = false;
    bool expanded = false;
    bool disabled = false;
    RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>> flowedNodeIds;
    bool focused = false;
    bool ignored = true;
    bool ignoredByDefault = false;
    auto invalid = Inspector::Protocol::DOM::AccessibilityProperties::Invalid::False;
    bool hidden = false;
    String label;
    bool liveRegionAtomic = false;
    RefPtr<JSON::ArrayOf<String>> liveRegionRelevant;
    auto liveRegionStatus = Inspector::Protocol::DOM::AccessibilityProperties::LiveRegionStatus::Off;
    Node* mouseEventNode = nullptr;
    RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>> ownedNodeIds;
    Node* parentNode = nullptr;
    bool pressed = false;
    bool readonly = false;
    bool required = false;
    String role;
    bool selected = false;
    RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>> selectedChildNodeIds;
    bool isSwitch = false;
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

    if (auto* axObjectCache = node.document().axObjectCache()) {
        if (auto* axObject = axObjectCache->getOrCreate(node)) {

            if (AXCoreObject* activeDescendant = axObject->activeDescendant())
                activeDescendantNode = activeDescendant->node();

            // An AX object is "busy" if it or any ancestor has aria-busy="true" set.
            AXCoreObject* current = axObject;
            while (!busy && current) {
                busy = current->isBusy();
                current = current->parentObject();
            }

            isSwitch = axObject->isSwitch();
            supportsChecked = axObject->supportsChecked();
            if (supportsChecked) {
                AccessibilityButtonState checkValue = axObject->checkboxOrRadioValue(); // Element using aria-checked.
                if (checkValue == AccessibilityButtonState::On) {
                    if (isSwitch)
                        switchState = Inspector::Protocol::DOM::AccessibilityProperties::SwitchState::On;
                    else
                        checked = Inspector::Protocol::DOM::AccessibilityProperties::Checked::True;
                } else if (checkValue == AccessibilityButtonState::Mixed && !isSwitch)
                    checked = Inspector::Protocol::DOM::AccessibilityProperties::Checked::Mixed;
                else if (axObject->isChecked()) {
                    // Native checkbox or switch.
                    if (isSwitch)
                        switchState = Inspector::Protocol::DOM::AccessibilityProperties::SwitchState::On;
                    else
                        checked = Inspector::Protocol::DOM::AccessibilityProperties::Checked::True;
                }
            }

            if (!axObject->children().isEmpty()) {
                childNodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
                processAccessibilityChildren(*axObject, *childNodeIds);
            }

            auto controlledElements = axObject->elementsFromAttribute(aria_controlsAttr);
            if (controlledElements.size()) {
                controlledNodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
                for (auto& controlledElement : controlledElements) {
                    if (auto controlledElementId = pushNodePathToFrontend(controlledElement.ptr()))
                        controlledNodeIds->addItem(controlledElementId);
                }
            }
            
            switch (axObject->currentState()) {
            case AccessibilityCurrentState::False:
                currentState = Inspector::Protocol::DOM::AccessibilityProperties::Current::False;
                break;
            case AccessibilityCurrentState::Page:
                currentState = Inspector::Protocol::DOM::AccessibilityProperties::Current::Page;
                break;
            case AccessibilityCurrentState::Step:
                currentState = Inspector::Protocol::DOM::AccessibilityProperties::Current::Step;
                break;
            case AccessibilityCurrentState::Location:
                currentState = Inspector::Protocol::DOM::AccessibilityProperties::Current::Location;
                break;
            case AccessibilityCurrentState::Date:
                currentState = Inspector::Protocol::DOM::AccessibilityProperties::Current::Date;
                break;
            case AccessibilityCurrentState::Time:
                currentState = Inspector::Protocol::DOM::AccessibilityProperties::Current::Time;
                break;
            case AccessibilityCurrentState::True:
                currentState = Inspector::Protocol::DOM::AccessibilityProperties::Current::True;
                break;
            }

            disabled = !axObject->isEnabled();
            exists = true;
            
            supportsExpanded = axObject->supportsExpanded();
            if (supportsExpanded)
                expanded = axObject->isExpanded();

            auto flowedElements = axObject->elementsFromAttribute(aria_flowtoAttr);
            if (flowedElements.size()) {
                flowedNodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
                for (auto& flowedElement : flowedElements) {
                    if (auto flowedElementId = pushNodePathToFrontend(flowedElement.ptr()))
                        flowedNodeIds->addItem(flowedElementId);
                }
            }

            if (is<Element>(node)) {
                supportsFocused = axObject->canSetFocusAttribute();
                if (supportsFocused)
                    focused = axObject->isFocused();
            }

            ignored = axObject->isIgnored();
            ignoredByDefault = axObject->isIgnoredByDefault();
            
            String invalidValue = axObject->invalidStatus();
            if (invalidValue == "false"_s)
                invalid = Inspector::Protocol::DOM::AccessibilityProperties::Invalid::False;
            else if (invalidValue == "grammar"_s)
                invalid = Inspector::Protocol::DOM::AccessibilityProperties::Invalid::Grammar;
            else if (invalidValue == "spelling"_s)
                invalid = Inspector::Protocol::DOM::AccessibilityProperties::Invalid::Spelling;
            else // Future versions of ARIA may allow additional truthy values. Ex. format, order, or size.
                invalid = Inspector::Protocol::DOM::AccessibilityProperties::Invalid::True;
            
            if (axObject->isHidden())
                hidden = true;
            
            label = axObject->computedLabel();

            if (axObject->supportsLiveRegion()) {
                supportsLiveRegion = true;
                liveRegionAtomic = axObject->liveRegionAtomic();

                auto ariaRelevantAttrValue = axObject->liveRegionRelevant();
                if (!ariaRelevantAttrValue.isEmpty()) {
                    // FIXME: Pass enum values rather than strings once unblocked. http://webkit.org/b/133711
                    String ariaRelevantAdditions = Inspector::Protocol::Helpers::getEnumConstantValue(Inspector::Protocol::DOM::LiveRegionRelevant::Additions);
                    String ariaRelevantRemovals = Inspector::Protocol::Helpers::getEnumConstantValue(Inspector::Protocol::DOM::LiveRegionRelevant::Removals);
                    String ariaRelevantText = Inspector::Protocol::Helpers::getEnumConstantValue(Inspector::Protocol::DOM::LiveRegionRelevant::Text);
                    liveRegionRelevant = JSON::ArrayOf<String>::create();
                    SpaceSplitString values(AtomString { ariaRelevantAttrValue }, SpaceSplitString::ShouldFoldCase::Yes);
                    // @aria-relevant="all" is exposed as ["additions","removals","text"], in order.
                    // This order is controlled in WebCore and expected in WebInspectorUI.
                    if (values.contains("all"_s)) {
                        liveRegionRelevant->addItem(ariaRelevantAdditions);
                        liveRegionRelevant->addItem(ariaRelevantRemovals);
                        liveRegionRelevant->addItem(ariaRelevantText);
                    } else {
                        if (values.contains(AtomString { ariaRelevantAdditions }))
                            liveRegionRelevant->addItem(ariaRelevantAdditions);
                        if (values.contains(AtomString { ariaRelevantRemovals }))
                            liveRegionRelevant->addItem(ariaRelevantRemovals);
                        if (values.contains(AtomString { ariaRelevantText }))
                            liveRegionRelevant->addItem(ariaRelevantText);
                    }
                }

                String ariaLive = axObject->liveRegionStatus();
                if (ariaLive == "assertive"_s)
                    liveRegionStatus = Inspector::Protocol::DOM::AccessibilityProperties::LiveRegionStatus::Assertive;
                else if (ariaLive == "polite"_s)
                    liveRegionStatus = Inspector::Protocol::DOM::AccessibilityProperties::LiveRegionStatus::Polite;
            }

            if (auto* clickableObject = axObject->clickableSelfOrAncestor(ClickHandlerFilter::IncludeBody))
                mouseEventNode = clickableObject->node();

            if (axObject->supportsARIAOwns()) {
                auto ownedElements = axObject->elementsFromAttribute(aria_ownsAttr);
                if (ownedElements.size()) {
                    ownedNodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
                    for (auto& ownedElement : ownedElements) {
                        if (auto ownedElementId = pushNodePathToFrontend(ownedElement.ptr()))
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

            auto selectedChildren = axObject->selectedChildren();
            if (selectedChildren && selectedChildren->size()) {
                selectedChildNodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
                for (auto& selectedChildObject : *selectedChildren) {
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
    
    auto value = Inspector::Protocol::DOM::AccessibilityProperties::create()
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

        // Switches `supportsChecked` (the underlying implementation is mostly shared with checkboxes),
        // but should report a switch state and not a checked state.
        if (isSwitch)
            value->setSwitchState(switchState);
        else if (supportsChecked)
            value->setChecked(checked);

        if (childNodeIds)
            value->setChildNodeIds(childNodeIds.releaseNonNull());
        if (controlledNodeIds)
            value->setControlledNodeIds(controlledNodeIds.releaseNonNull());
        if (currentState != Inspector::Protocol::DOM::AccessibilityProperties::Current::False)
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
        if (invalid != Inspector::Protocol::DOM::AccessibilityProperties::Invalid::False)
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

static bool containsOnlyASCIIWhitespace(Node* node)
{
    // FIXME: Respect ignoreWhitespace setting from inspector front end?
    // This static is invoked during node deletion so cannot use RefPtr.
    auto* text = dynamicDowncast<Text>(node);
    return text && text->containsOnlyASCIIWhitespace();
}

Node* InspectorDOMAgent::innerFirstChild(Node* node)
{
    node = node->firstChild();
    while (containsOnlyASCIIWhitespace(node))
        node = node->nextSibling();
    return node;
}

Node* InspectorDOMAgent::innerNextSibling(Node* node)
{
    do {
        node = node->nextSibling();
    } while (containsOnlyASCIIWhitespace(node));
    return node;
}

Node* InspectorDOMAgent::innerPreviousSibling(Node* node)
{
    do {
        node = node->previousSibling();
    } while (containsOnlyASCIIWhitespace(node));
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
    if (RefPtr document = dynamicDowncast<Document>(*node))
        return document->ownerElement();
    if (RefPtr shadowRoot = dynamicDowncast<ShadowRoot>(*node))
        return shadowRoot->host();
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

    auto frameOwnerId = boundNodeId(frameOwner.get());
    if (!frameOwnerId)
        return;

    // Re-add frame owner element together with its new children.
    auto parentId = boundNodeId(innerParentNode(frameOwner.get()));
    m_frontendDispatcher->childNodeRemoved(parentId, frameOwnerId);
    unbind(*frameOwner);

    auto value = buildObjectForNode(frameOwner.get(), 0);
    Node* previousSibling = innerPreviousSibling(frameOwner.get());
    auto prevId = boundNodeId(previousSibling);
    m_frontendDispatcher->childNodeInserted(parentId, prevId, WTFMove(value));
}

Inspector::Protocol::DOM::NodeId InspectorDOMAgent::identifierForNode(Node& node)
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
    if (containsOnlyASCIIWhitespace(&node))
        return;

    // We could be attaching existing subtree. Forget the bindings.
    unbind(node);

    ContainerNode* parent = node.parentNode();

    auto parentId = boundNodeId(parent);
    // Return if parent is not mapped yet.
    if (!parentId)
        return;

    if (!m_childrenRequested.contains(parentId)) {
        // No children are mapped yet -> only notify on changes of hasChildren.
        m_frontendDispatcher->childNodeCountUpdated(parentId, innerChildNodeCount(parent));
    } else {
        // Children have been requested -> return value of a new child.
        Node* prevSibling = innerPreviousSibling(&node);
        auto prevId = boundNodeId(prevSibling);
        auto value = buildObjectForNode(&node, 0);
        m_frontendDispatcher->childNodeInserted(parentId, prevId, WTFMove(value));
    }
}

void InspectorDOMAgent::didRemoveDOMNode(Node& node)
{
    if (containsOnlyASCIIWhitespace(&node))
        return;

    ContainerNode* parent = node.parentNode();

    auto parentId = boundNodeId(parent);
    // If parent is not mapped yet -> ignore the event.
    if (!parentId)
        return;

    // FIXME: <webkit.org/b/189687> Preserve DOM.NodeId if a node is removed and re-added
    if (!m_childrenRequested.contains(parentId)) {
        // No children are mapped yet -> only notify on changes of hasChildren.
        if (innerChildNodeCount(parent) == 1)
            m_frontendDispatcher->childNodeCountUpdated(parentId, 0);
    } else
        m_frontendDispatcher->childNodeRemoved(parentId, boundNodeId(&node));
    unbind(node);
}

void InspectorDOMAgent::willDestroyDOMNode(Node& node)
{
    if (containsOnlyASCIIWhitespace(&node))
        return;

    auto nodeId = m_nodeToId.take(node);
    if (!nodeId)
        return;

    m_idToNode.remove(nodeId);
    m_childrenRequested.remove(nodeId);

    if (auto* cssAgent = m_instrumentingAgents.enabledCSSAgent())
        cssAgent->didRemoveDOMNode(node, nodeId);

    // This can be called in response to GC. Due to the single-process model used in WebKit1, the
    // event must be dispatched from a timer to prevent the frontend from making JS allocations
    // while the GC is still active.

    // FIXME: <webkit.org/b/189687> Unify m_destroyedAttachedNodeIdentifiers and m_destroyedDetachedNodeIdentifiers.
    if (auto parentId = boundNodeId(node.parentNode()))
        m_destroyedAttachedNodeIdentifiers.append({ parentId, nodeId });
    else
        m_destroyedDetachedNodeIdentifiers.append(nodeId);

    if (!m_destroyedNodesTimer.isActive())
        m_destroyedNodesTimer.startOneShot(0_s);
}

void InspectorDOMAgent::destroyedNodesTimerFired()
{
    for (auto& [parentId, nodeId] : std::exchange(m_destroyedAttachedNodeIdentifiers, { })) {
        if (!m_childrenRequested.contains(parentId)) {
            auto* parent = nodeForId(parentId);
            if (parent && innerChildNodeCount(parent) == 1)
                m_frontendDispatcher->childNodeCountUpdated(parentId, 0);
        } else
            m_frontendDispatcher->childNodeRemoved(parentId, nodeId);
    }
    
    for (auto nodeId : std::exchange(m_destroyedDetachedNodeIdentifiers, { }))
        m_frontendDispatcher->willDestroyDOMNode(nodeId);
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
    auto nodeIds = JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>::create();
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
    auto id = boundNodeId(&characterData);
    if (!id) {
        // Push text node if it is being created.
        didInsertDOMNode(characterData);
        return;
    }
    m_frontendDispatcher->characterDataModified(id, characterData.data());
}

void InspectorDOMAgent::didInvalidateStyleAttr(Element& element)
{
    auto id = boundNodeId(&element);
    if (!id)
        return;

    if (!m_revalidateStyleAttrTask)
        m_revalidateStyleAttrTask = makeUnique<RevalidateStyleAttributeTask>(this);
    m_revalidateStyleAttrTask->scheduleFor(&element);
}

void InspectorDOMAgent::didPushShadowRoot(Element& host, ShadowRoot& root)
{
    auto hostId = boundNodeId(&host);
    if (hostId)
        m_frontendDispatcher->shadowRootPushed(hostId, buildObjectForNode(&root, 0));
}

void InspectorDOMAgent::willPopShadowRoot(Element& host, ShadowRoot& root)
{
    auto hostId = boundNodeId(&host);
    auto rootId = boundNodeId(&root);
    if (hostId && rootId)
        m_frontendDispatcher->shadowRootPopped(hostId, rootId);
}

void InspectorDOMAgent::didChangeCustomElementState(Element& element)
{
    auto elementId = boundNodeId(&element);
    if (!elementId)
        return;

    m_frontendDispatcher->customElementStateChanged(elementId, customElementState(element));
}

void InspectorDOMAgent::frameDocumentUpdated(LocalFrame& frame)
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

    auto parentId = boundNodeId(parent);
    if (!parentId)
        return;

    pushChildNodesToFrontend(parentId, 1);
    m_frontendDispatcher->pseudoElementAdded(parentId, buildObjectForNode(&pseudoElement, 0));
}

void InspectorDOMAgent::pseudoElementDestroyed(PseudoElement& pseudoElement)
{
    auto pseudoElementId = boundNodeId(&pseudoElement);
    if (!pseudoElementId)
        return;

    // If a PseudoElement is bound, its parent element must have been bound.
    Element* parent = pseudoElement.hostElement();
    ASSERT(parent);
    auto parentId = boundNodeId(parent);
    ASSERT(parentId);

    unbind(pseudoElement);
    m_frontendDispatcher->pseudoElementRemoved(parentId, pseudoElementId);
}

void InspectorDOMAgent::didAddEventListener(EventTarget& target)
{
    RefPtr node = dynamicDowncast<Node>(target);
    if (!node)
        return;

    if (!node->contains(m_inspectedNode.get()))
        return;

    auto nodeId = boundNodeId(node.get());
    if (!nodeId)
        return;

    if (m_suppressEventListenerChangedEvent)
        return;

    m_suppressEventListenerChangedEvent = true;

    m_frontendDispatcher->didAddEventListener(nodeId);
}

void InspectorDOMAgent::willRemoveEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    RefPtr node = dynamicDowncast<Node>(target);
    if (!node)
        return;

    if (!node->contains(m_inspectedNode.get()))
        return;

    auto nodeId = boundNodeId(node.get());
    if (!nodeId)
        return;

    bool listenerExists = false;
    for (auto& item : node->eventListeners(eventType)) {
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

void InspectorDOMAgent::flexibleBoxRendererBeganLayout(const RenderObject& renderer)
{
    m_flexibleBoxRendererCachedItemsAtStartOfLine.remove(renderer);
}

void InspectorDOMAgent::flexibleBoxRendererWrappedToNextLine(const RenderObject& renderer, size_t lineStartItemIndex)
{
    m_flexibleBoxRendererCachedItemsAtStartOfLine.ensure(renderer, [] {
        return Vector<size_t>();
    }).iterator->value.append(lineStartItemIndex);
}

Vector<size_t> InspectorDOMAgent::flexibleBoxRendererCachedItemsAtStartOfLine(const RenderObject& renderer)
{
    return m_flexibleBoxRendererCachedItemsAtStartOfLine.get(renderer);
}

RefPtr<JSC::Breakpoint> InspectorDOMAgent::breakpointForEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    for (auto& inspectorEventListener : m_eventListenerEntries.values()) {
        if (inspectorEventListener.matches(target, eventType, listener, capture))
            return inspectorEventListener.breakpoint;
    }
    return nullptr;
}

Inspector::Protocol::DOM::EventListenerId InspectorDOMAgent::idForEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
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

    for (auto& weakMediaElement : HTMLMediaElement::allMediaElements()) {
        Ref mediaElement = weakMediaElement.get();
        if (!is<HTMLVideoElement>(mediaElement) || !mediaElement->isPlaying())
            continue;

        auto videoPlaybackQuality = mediaElement->getVideoPlaybackQuality();
        unsigned displayCompositedVideoFrames = videoPlaybackQuality->displayCompositedVideoFrames();

        auto iterator = m_mediaMetrics.find(mediaElement);
        if (iterator == m_mediaMetrics.end()) {
            m_mediaMetrics.set(mediaElement.get(), MediaMetrics(displayCompositedVideoFrames));
            continue;
        }

        bool isPowerEfficient = (displayCompositedVideoFrames - iterator->value.displayCompositedFrames) > 0;
        if (iterator->value.isPowerEfficient != isPowerEfficient) {
            iterator->value.isPowerEfficient = isPowerEfficient;

            if (auto nodeId = pushNodePathToFrontend(mediaElement.ptr())) {
                auto timestamp = m_environment.executionStopwatch().elapsedTime().seconds();
                m_frontendDispatcher->powerEfficientPlaybackStateChanged(nodeId, timestamp, iterator->value.isPowerEfficient);
            }
        }

        iterator->value.displayCompositedFrames = displayCompositedVideoFrames;
    }

    m_mediaMetrics.removeIf([&] (auto& entry) {
        return !HTMLMediaElement::allMediaElements().contains(&entry.key);
    });
}
#endif

Node* InspectorDOMAgent::nodeForPath(const String& path)
{
    // The path is of form "1,HTML,2,BODY,1,DIV"
    if (!m_document)
        return nullptr;

    Node* node = m_document.get();
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

        Node* child;
        if (RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(*node)) {
            ASSERT(!*childNumber);
            child = frameOwner->contentDocument();
        } else {
            if (*childNumber >= innerChildNodeCount(node))
                return nullptr;
            child = innerFirstChild(node);
            for (size_t j = 0; child && j < *childNumber; ++j)
                child = innerNextSibling(child);
        }

        auto childName = *it;
        if (!child || child->nodeName() != childName)
            return nullptr;
        node = child;
    }

    return node;
}

Node* InspectorDOMAgent::nodeForObjectId(const Inspector::Protocol::Runtime::RemoteObjectId& objectId)
{
    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue())
        return nullptr;

    return scriptValueAsNode(injectedScript.findObjectById(objectId));
}

Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> InspectorDOMAgent::pushNodeByPathToFrontend(const String& path)
{
    Inspector::Protocol::ErrorString errorString;

    if (Node* node = nodeForPath(path)) {
        if (auto nodeId = pushNodePathToFrontend(errorString, node))
            return nodeId;
        return makeUnexpected(errorString);
    }

    return makeUnexpected("Missing node for given path"_s);
}

RefPtr<Inspector::Protocol::Runtime::RemoteObject> InspectorDOMAgent::resolveNode(Node* node, const String& objectGroup)
{
    Document* document = &node->document();
    if (auto* templateHost = document->templateDocumentHost())
        document = templateHost;
    auto* frame =  document->frame();
    if (!frame)
        return nullptr;

    auto& globalObject = mainWorldGlobalObject(*frame);
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&globalObject);
    if (injectedScript.hasNoValue())
        return nullptr;

    return injectedScript.wrapObject(nodeAsScriptValue(globalObject, node), objectGroup);
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

Inspector::Protocol::ErrorStringOr<void> InspectorDOMAgent::setAllowEditingUserAgentShadowTrees(bool allow)
{
    m_allowEditingUserAgentShadowTrees = allow;

    return { };
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::DOM::MediaStats>> InspectorDOMAgent::getMediaStats(Inspector::Protocol::DOM::NodeId nodeId)
{
#if ENABLE(VIDEO)
    Inspector::Protocol::ErrorString errorString;

    auto* element = assertElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    auto* mediaElement = dynamicDowncast<HTMLMediaElement>(element);
    if (!mediaElement)
        return makeUnexpected("Node for given nodeId is not a media element"_s);

    auto stats = Inspector::Protocol::DOM::MediaStats::create().release();

    if (auto quality = mediaElement->getVideoPlaybackQuality()) {
        auto jsonQuality = Inspector::Protocol::DOM::VideoPlaybackQuality::create()
            .setTotalVideoFrames(quality->totalVideoFrames())
            .setDroppedVideoFrames(quality->droppedVideoFrames())
            .setDisplayCompositedVideoFrames(quality->displayCompositedVideoFrames())
            .release();
        stats->setQuality(WTFMove(jsonQuality));
    }

    auto sourceType = mediaElement->localizedSourceType();
    if (!sourceType.isEmpty())
        stats->setSource(sourceType);

    auto* videoTrack = mediaElement->videoTracks() ? mediaElement->videoTracks()->selectedItem() : nullptr;
    auto* audioTrack = mediaElement->audioTracks() ? mediaElement->audioTracks()->firstEnabled() : nullptr;

    auto viewport = mediaElement->contentBoxRect().size();
    auto viewportJSON = Inspector::Protocol::DOM::ViewportSize::create()
        .setWidth(viewport.width())
        .setHeight(viewport.height())
        .release();
    stats->setViewport(WTFMove(viewportJSON));

    if (RefPtr window = mediaElement->document().domWindow())
        stats->setDevicePixelRatio(window->devicePixelRatio());

    if (videoTrack) {
        auto& configuration = videoTrack->configuration();
        auto colorSpace = configuration.colorSpace();
        auto colorSpaceJSON = Inspector::Protocol::DOM::VideoColorSpace::create().release();
        if (auto fullRange = colorSpace->fullRange())
            colorSpaceJSON->setFullRange(*fullRange);
        if (auto matrix = colorSpace->matrix())
            colorSpaceJSON->setMatrix(convertEnumerationToString(*matrix));
        if (auto primaries = colorSpace->primaries())
            colorSpaceJSON->setPrimaries(convertEnumerationToString(*primaries));
        if (auto transfer = colorSpace->transfer())
            colorSpaceJSON->setTransfer(convertEnumerationToString(*transfer));

        auto videoJSON = Inspector::Protocol::DOM::VideoMediaStats::create()
            .setBitrate(configuration.bitrate())
            .setCodec(configuration.codec())
            .setHumanReadableCodecString(humanReadableStringFromCodecString(configuration.codec()))
            .setColorSpace(WTFMove(colorSpaceJSON))
            .setFramerate(configuration.framerate())
            .setHeight(configuration.height())
            .setWidth(configuration.width())
            .release();
        stats->setVideo(WTFMove(videoJSON));
    }

    if (audioTrack) {
        auto& configuration = audioTrack->configuration();
        auto audioJSON = Inspector::Protocol::DOM::AudioMediaStats::create()
            .setBitrate(configuration.bitrate())
            .setCodec(configuration.codec())
            .setHumanReadableCodecString(humanReadableStringFromCodecString(configuration.codec()))
            .setNumberOfChannels(configuration.numberOfChannels())
            .setSampleRate(configuration.sampleRate())
            .release();
        stats->setAudio(WTFMove(audioJSON));
    }

    return stats;
#else
    UNUSED_PARAM(nodeId);
    return makeUnexpected("no media support"_s);
#endif
}

} // namespace WebCore
