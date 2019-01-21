/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Node.h"

#include "AXObjectCache.h"
#include "Attr.h"
#include "BeforeLoadEvent.h"
#include "ChildListMutationScope.h"
#include "CommonVM.h"
#include "ComposedTreeAncestorIterator.h"
#include "ContainerNodeAlgorithms.h"
#include "ContextMenuController.h"
#include "DOMWindow.h"
#include "DataTransfer.h"
#include "DocumentType.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "EventDispatcher.h"
#include "EventHandler.h"
#include "FrameView.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLSlotElement.h"
#include "HTMLStyleElement.h"
#include "InputEvent.h"
#include "InspectorController.h"
#include "KeyboardEvent.h"
#include "Logging.h"
#include "MutationEvent.h"
#include "NodeRenderStyle.h"
#include "ProcessingInstruction.h"
#include "ProgressEvent.h"
#include "Range.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "ScopedEventQueue.h"
#include "ScriptDisallowedScope.h"
#include "StorageEvent.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include "TemplateContentDocumentFragment.h"
#include "TextEvent.h"
#include "TouchEvent.h"
#include "WheelEvent.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/SHA1.h>
#include <wtf/Variant.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Node);

using namespace HTMLNames;

#if DUMP_NODE_STATISTICS
static HashSet<Node*>& liveNodeSet()
{
    static NeverDestroyed<HashSet<Node*>> liveNodes;
    return liveNodes;
}

static const char* stringForRareDataUseType(NodeRareData::UseType useType)
{
    switch (useType) {
    case NodeRareData::UseType::ConnectedFrameCount:
        return "ConnectedFrameCount";
    case NodeRareData::UseType::NodeList:
        return "NodeList";
    case NodeRareData::UseType::MutationObserver:
        return "MutationObserver";
    case NodeRareData::UseType::TabIndex:
        return "TabIndex";
    case NodeRareData::UseType::StyleFlags:
        return "StyleFlags";
    case NodeRareData::UseType::MinimumSize:
        return "MinimumSize";
    case NodeRareData::UseType::ScrollingPosition:
        return "ScrollingPosition";
    case NodeRareData::UseType::ComputedStyle:
        return "ComputedStyle";
    case NodeRareData::UseType::Dataset:
        return "Dataset";
    case NodeRareData::UseType::ClassList:
        return "ClassList";
    case NodeRareData::UseType::ShadowRoot:
        return "ShadowRoot";
    case NodeRareData::UseType::CustomElementQueue:
        return "CustomElementQueue";
    case NodeRareData::UseType::AttributeMap:
        return "AttributeMap";
    case NodeRareData::UseType::InteractionObserver:
        return "InteractionObserver";
    case NodeRareData::UseType::PseudoElements:
        return "PseudoElements";
    }
    return nullptr;
}

#endif

void Node::dumpStatistics()
{
#if DUMP_NODE_STATISTICS
    size_t nodesWithRareData = 0;

    size_t elementNodes = 0;
    size_t attrNodes = 0;
    size_t textNodes = 0;
    size_t cdataNodes = 0;
    size_t commentNodes = 0;
    size_t piNodes = 0;
    size_t documentNodes = 0;
    size_t docTypeNodes = 0;
    size_t fragmentNodes = 0;
    size_t shadowRootNodes = 0;

    HashMap<String, size_t> perTagCount;

    size_t attributes = 0;
    size_t attributesWithAttr = 0;
    size_t elementsWithAttributeStorage = 0;
    size_t elementsWithRareData = 0;
    size_t elementsWithNamedNodeMap = 0;

    HashMap<uint16_t, size_t> rareDataSingleUseTypeCounts;
    size_t mixedRareDataUseCount = 0;

    for (auto* node : liveNodeSet()) {
        if (node->hasRareData()) {
            ++nodesWithRareData;
            if (is<Element>(*node)) {
                ++elementsWithRareData;
                if (downcast<Element>(*node).hasNamedNodeMap())
                    ++elementsWithNamedNodeMap;
            }
            auto* rareData = node->rareData();
            auto useTypes = is<Element>(node) ? static_cast<ElementRareData*>(rareData)->useTypes() : rareData->useTypes();
            unsigned useTypeCount = 0;
            for (auto type : useTypes) {
                UNUSED_PARAM(type);
                useTypeCount++;
            }
            if (useTypeCount == 1) {
                auto result = rareDataSingleUseTypeCounts.add(static_cast<uint16_t>(*useTypes.begin()), 0);
                result.iterator->value++;
            } else
                mixedRareDataUseCount++;
        }

        switch (node->nodeType()) {
            case ELEMENT_NODE: {
                ++elementNodes;

                // Tag stats
                Element& element = downcast<Element>(*node);
                HashMap<String, size_t>::AddResult result = perTagCount.add(element.tagName(), 1);
                if (!result.isNewEntry)
                    result.iterator->value++;

                if (const ElementData* elementData = element.elementData()) {
                    unsigned length = elementData->length();
                    attributes += length;
                    ++elementsWithAttributeStorage;
                    for (unsigned i = 0; i < length; ++i) {
                        const Attribute& attr = elementData->attributeAt(i);
                        if (element.attrIfExists(attr.name()))
                            ++attributesWithAttr;
                    }
                }
                break;
            }
            case ATTRIBUTE_NODE: {
                ++attrNodes;
                break;
            }
            case TEXT_NODE: {
                ++textNodes;
                break;
            }
            case CDATA_SECTION_NODE: {
                ++cdataNodes;
                break;
            }
            case PROCESSING_INSTRUCTION_NODE: {
                ++piNodes;
                break;
            }
            case COMMENT_NODE: {
                ++commentNodes;
                break;
            }
            case DOCUMENT_NODE: {
                ++documentNodes;
                break;
            }
            case DOCUMENT_TYPE_NODE: {
                ++docTypeNodes;
                break;
            }
            case DOCUMENT_FRAGMENT_NODE: {
                if (node->isShadowRoot())
                    ++shadowRootNodes;
                else
                    ++fragmentNodes;
                break;
            }
        }
    }

    printf("Number of Nodes: %d\n\n", liveNodeSet().size());
    printf("Number of Nodes with RareData: %zu\n", nodesWithRareData);
    printf("  Mixed use: %zu\n", mixedRareDataUseCount);
    for (auto it : rareDataSingleUseTypeCounts)
        printf("  %s: %zu\n", stringForRareDataUseType(static_cast<NodeRareData::UseType>(it.key)), it.value);
    printf("\n");


    printf("NodeType distribution:\n");
    printf("  Number of Element nodes: %zu\n", elementNodes);
    printf("  Number of Attribute nodes: %zu\n", attrNodes);
    printf("  Number of Text nodes: %zu\n", textNodes);
    printf("  Number of CDATASection nodes: %zu\n", cdataNodes);
    printf("  Number of Comment nodes: %zu\n", commentNodes);
    printf("  Number of ProcessingInstruction nodes: %zu\n", piNodes);
    printf("  Number of Document nodes: %zu\n", documentNodes);
    printf("  Number of DocumentType nodes: %zu\n", docTypeNodes);
    printf("  Number of DocumentFragment nodes: %zu\n", fragmentNodes);
    printf("  Number of ShadowRoot nodes: %zu\n", shadowRootNodes);

    printf("Element tag name distibution:\n");
    for (auto& stringSizePair : perTagCount)
        printf("  Number of <%s> tags: %zu\n", stringSizePair.key.utf8().data(), stringSizePair.value);

    printf("Attributes:\n");
    printf("  Number of Attributes (non-Node and Node): %zu [%zu]\n", attributes, sizeof(Attribute));
    printf("  Number of Attributes with an Attr: %zu\n", attributesWithAttr);
    printf("  Number of Elements with attribute storage: %zu [%zu]\n", elementsWithAttributeStorage, sizeof(ElementData));
    printf("  Number of Elements with RareData: %zu\n", elementsWithRareData);
    printf("  Number of Elements with NamedNodeMap: %zu [%zu]\n", elementsWithNamedNodeMap, sizeof(NamedNodeMap));
#endif
}

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, nodeCounter, ("WebCoreNode"));

#ifndef NDEBUG
static bool shouldIgnoreLeaks = false;

static HashSet<Node*>& ignoreSet()
{
    static NeverDestroyed<HashSet<Node*>> ignore;

    return ignore;
}

#endif

void Node::startIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = true;
#endif
}

void Node::stopIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = false;
#endif
}

void Node::trackForDebugging()
{
#ifndef NDEBUG
    if (shouldIgnoreLeaks)
        ignoreSet().add(this);
    else
        nodeCounter.increment();
#endif

#if DUMP_NODE_STATISTICS
    liveNodeSet().add(this);
#endif
}

Node::Node(Document& document, ConstructionType type)
    : m_refCount(1)
    , m_nodeFlags(type)
    , m_treeScope(&document)
{
    ASSERT(isMainThread());

    document.incrementReferencingNodeCount();

#if !defined(NDEBUG) || (defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS)
    trackForDebugging();
#endif
}

Node::~Node()
{
    ASSERT(isMainThread());
    ASSERT(!m_refCount);
    ASSERT(m_deletionHasBegun);
    ASSERT(!m_adoptionIsRequired);

#ifndef NDEBUG
    if (!ignoreSet().remove(this))
        nodeCounter.decrement();
#endif

#if DUMP_NODE_STATISTICS
    liveNodeSet().remove(this);
#endif

    RELEASE_ASSERT(!renderer());
    ASSERT(!parentNode());
    ASSERT(!m_previous);
    ASSERT(!m_next);

    if (hasRareData())
        clearRareData();

    if (!isContainerNode())
        willBeDeletedFrom(document());

    if (hasEventTargetData())
        clearEventTargetData();

    document().decrementReferencingNodeCount();

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY) && (!ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS))
    for (auto* document : Document::allDocuments()) {
        ASSERT_WITH_SECURITY_IMPLICATION(!document->touchEventListenersContain(*this));
        ASSERT_WITH_SECURITY_IMPLICATION(!document->touchEventHandlersContain(*this));
        ASSERT_WITH_SECURITY_IMPLICATION(!document->touchEventTargetsContain(*this));
    }
#endif
}

void Node::willBeDeletedFrom(Document& document)
{
    if (hasEventTargetData()) {
        document.didRemoveWheelEventHandler(*this, EventHandlerRemoval::All);
#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(IOS_FAMILY)
        document.removeTouchEventListener(*this, EventHandlerRemoval::All);
#endif
        document.didRemoveTouchEventHandler(*this, EventHandlerRemoval::All);
#endif
    }

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
    document.removeTouchEventHandler(*this, EventHandlerRemoval::All);
#endif

    if (auto* cache = document.existingAXObjectCache())
        cache->remove(*this);
}

void Node::materializeRareData()
{
    NodeRareData* data;
    if (is<Element>(*this))
        data = std::make_unique<ElementRareData>(downcast<RenderElement>(m_data.m_renderer)).release();
    else
        data = std::make_unique<NodeRareData>(m_data.m_renderer).release();
    ASSERT(data);

    m_data.m_rareData = data;
    setFlag(HasRareDataFlag);
}

void Node::clearRareData()
{
    ASSERT(hasRareData());
    ASSERT(!transientMutationObserverRegistry() || transientMutationObserverRegistry()->isEmpty());

    RenderObject* renderer = m_data.m_rareData->renderer();
    if (isElementNode())
        delete static_cast<ElementRareData*>(m_data.m_rareData);
    else
        delete static_cast<NodeRareData*>(m_data.m_rareData);
    m_data.m_renderer = renderer;
    clearFlag(HasRareDataFlag);
}

bool Node::isNode() const
{
    return true;
}

String Node::nodeValue() const
{
    return String();
}

ExceptionOr<void> Node::setNodeValue(const String&)
{
    // By default, setting nodeValue has no effect.
    return { };
}

RefPtr<NodeList> Node::childNodes()
{
    if (is<ContainerNode>(*this))
        return ensureRareData().ensureNodeLists().ensureChildNodeList(downcast<ContainerNode>(*this));
    return ensureRareData().ensureNodeLists().ensureEmptyChildNodeList(*this);
}

Node *Node::lastDescendant() const
{
    Node *n = const_cast<Node *>(this);
    while (n && n->lastChild())
        n = n->lastChild();
    return n;
}

Node* Node::firstDescendant() const
{
    Node *n = const_cast<Node *>(this);
    while (n && n->firstChild())
        n = n->firstChild();
    return n;
}

Element* Node::previousElementSibling() const
{
    return ElementTraversal::previousSibling(*this);
}

Element* Node::nextElementSibling() const
{
    return ElementTraversal::nextSibling(*this);
}

ExceptionOr<void> Node::insertBefore(Node& newChild, Node* refChild)
{
    if (!is<ContainerNode>(*this))
        return Exception { HierarchyRequestError };
    return downcast<ContainerNode>(*this).insertBefore(newChild, refChild);
}

ExceptionOr<void> Node::replaceChild(Node& newChild, Node& oldChild)
{
    if (!is<ContainerNode>(*this))
        return Exception { HierarchyRequestError };
    return downcast<ContainerNode>(*this).replaceChild(newChild, oldChild);
}

ExceptionOr<void> Node::removeChild(Node& oldChild)
{
    if (!is<ContainerNode>(*this))
        return Exception { NotFoundError };
    return downcast<ContainerNode>(*this).removeChild(oldChild);
}

ExceptionOr<void> Node::appendChild(Node& newChild)
{
    if (!is<ContainerNode>(*this))
        return Exception { HierarchyRequestError };
    return downcast<ContainerNode>(*this).appendChild(newChild);
}

static HashSet<RefPtr<Node>> nodeSetPreTransformedFromNodeOrStringVector(const Vector<NodeOrString>& vector)
{
    HashSet<RefPtr<Node>> nodeSet;
    for (const auto& variant : vector) {
        WTF::switchOn(variant,
            [&] (const RefPtr<Node>& node) { nodeSet.add(const_cast<Node*>(node.get())); },
            [] (const String&) { }
        );
    }
    return nodeSet;
}

static RefPtr<Node> firstPrecedingSiblingNotInNodeSet(Node& context, const HashSet<RefPtr<Node>>& nodeSet)
{
    for (auto* sibling = context.previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (!nodeSet.contains(sibling))
            return sibling;
    }
    return nullptr;
}

static RefPtr<Node> firstFollowingSiblingNotInNodeSet(Node& context, const HashSet<RefPtr<Node>>& nodeSet)
{
    for (auto* sibling = context.nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (!nodeSet.contains(sibling))
            return sibling;
    }
    return nullptr;
}

ExceptionOr<RefPtr<Node>> Node::convertNodesOrStringsIntoNode(Vector<NodeOrString>&& nodeOrStringVector)
{
    if (nodeOrStringVector.isEmpty())
        return nullptr;

    Vector<Ref<Node>> nodes;
    nodes.reserveInitialCapacity(nodeOrStringVector.size());
    for (auto& variant : nodeOrStringVector) {
        WTF::switchOn(variant,
            [&](RefPtr<Node>& node) { nodes.uncheckedAppend(*node.get()); },
            [&](String& string) { nodes.uncheckedAppend(Text::create(document(), string)); }
        );
    }

    if (nodes.size() == 1)
        return RefPtr<Node> { WTFMove(nodes.first()) };

    auto nodeToReturn = DocumentFragment::create(document());
    for (auto& node : nodes) {
        auto appendResult = nodeToReturn->appendChild(node);
        if (appendResult.hasException())
            return appendResult.releaseException();
    }
    return RefPtr<Node> { WTFMove(nodeToReturn) };
}

ExceptionOr<void> Node::before(Vector<NodeOrString>&& nodeOrStringVector)
{
    RefPtr<ContainerNode> parent = parentNode();
    if (!parent)
        return { };

    auto nodeSet = nodeSetPreTransformedFromNodeOrStringVector(nodeOrStringVector);
    auto viablePreviousSibling = firstPrecedingSiblingNotInNodeSet(*this, nodeSet);

    auto result = convertNodesOrStringsIntoNode(WTFMove(nodeOrStringVector));
    if (result.hasException())
        return result.releaseException();
    auto node = result.releaseReturnValue();
    if (!node)
        return { };

    if (viablePreviousSibling)
        viablePreviousSibling = viablePreviousSibling->nextSibling();
    else
        viablePreviousSibling = parent->firstChild();

    return parent->insertBefore(*node, viablePreviousSibling.get());
}

ExceptionOr<void> Node::after(Vector<NodeOrString>&& nodeOrStringVector)
{
    RefPtr<ContainerNode> parent = parentNode();
    if (!parent)
        return { };

    auto nodeSet = nodeSetPreTransformedFromNodeOrStringVector(nodeOrStringVector);
    auto viableNextSibling = firstFollowingSiblingNotInNodeSet(*this, nodeSet);

    auto result = convertNodesOrStringsIntoNode(WTFMove(nodeOrStringVector));
    if (result.hasException())
        return result.releaseException();
    auto node = result.releaseReturnValue();
    if (!node)
        return { };

    return parent->insertBefore(*node, viableNextSibling.get());
}

ExceptionOr<void> Node::replaceWith(Vector<NodeOrString>&& nodeOrStringVector)
{
    RefPtr<ContainerNode> parent = parentNode();
    if (!parent)
        return { };

    auto nodeSet = nodeSetPreTransformedFromNodeOrStringVector(nodeOrStringVector);
    auto viableNextSibling = firstFollowingSiblingNotInNodeSet(*this, nodeSet);

    auto result = convertNodesOrStringsIntoNode(WTFMove(nodeOrStringVector));
    if (result.hasException())
        return result.releaseException();

    if (parentNode() == parent) {
        if (auto node = result.releaseReturnValue())
            return parent->replaceChild(*node, *this);
        return parent->removeChild(*this);
    }

    if (auto node = result.releaseReturnValue())
        return parent->insertBefore(*node, viableNextSibling.get());
    return { };
}

ExceptionOr<void> Node::remove()
{
    auto* parent = parentNode();
    if (!parent)
        return { };
    return parent->removeChild(*this);
}

void Node::normalize()
{
    // Go through the subtree beneath us, normalizing all nodes. This means that
    // any two adjacent text nodes are merged and any empty text nodes are removed.

    RefPtr<Node> node = this;
    while (Node* firstChild = node->firstChild())
        node = firstChild;
    while (node) {
        NodeType type = node->nodeType();
        if (type == ELEMENT_NODE)
            downcast<Element>(*node).normalizeAttributes();

        if (node == this)
            break;

        if (type != TEXT_NODE) {
            node = NodeTraversal::nextPostOrder(*node);
            continue;
        }

        RefPtr<Text> text = downcast<Text>(node.get());

        // Remove empty text nodes.
        if (!text->length()) {
            // Care must be taken to get the next node before removing the current node.
            node = NodeTraversal::nextPostOrder(*node);
            text->remove();
            continue;
        }

        // Merge text nodes.
        while (Node* nextSibling = node->nextSibling()) {
            if (nextSibling->nodeType() != TEXT_NODE)
                break;
            Ref<Text> nextText = downcast<Text>(*nextSibling);

            // Remove empty text nodes.
            if (!nextText->length()) {
                nextText->remove();
                continue;
            }

            // Both non-empty text nodes. Merge them.
            unsigned offset = text->length();
            text->appendData(nextText->data());
            document().textNodesMerged(nextText, offset);
            nextText->remove();
        }

        node = NodeTraversal::nextPostOrder(*node);
    }
}

ExceptionOr<Ref<Node>> Node::cloneNodeForBindings(bool deep)
{
    if (UNLIKELY(isShadowRoot()))
        return Exception { NotSupportedError };
    return cloneNode(deep);
}

const AtomicString& Node::prefix() const
{
    // For nodes other than elements and attributes, the prefix is always null
    return nullAtom();
}

ExceptionOr<void> Node::setPrefix(const AtomicString&)
{
    // The spec says that for nodes other than elements and attributes, prefix is always null.
    // It does not say what to do when the user tries to set the prefix on another type of
    // node, however Mozilla throws a NamespaceError exception.
    return Exception { NamespaceError };
}

const AtomicString& Node::localName() const
{
    return nullAtom();
}

const AtomicString& Node::namespaceURI() const
{
    return nullAtom();
}

bool Node::isContentEditable()
{
    return computeEditability(UserSelectAllDoesNotAffectEditability, ShouldUpdateStyle::Update) != Editability::ReadOnly;
}

bool Node::isContentRichlyEditable()
{
    return computeEditability(UserSelectAllIsAlwaysNonEditable, ShouldUpdateStyle::Update) == Editability::CanEditRichly;
}

void Node::inspect()
{
    if (document().page())
        document().page()->inspectorController().inspect(this);
}

static Node::Editability computeEditabilityFromComputedStyle(const Node& startNode, Node::UserSelectAllTreatment treatment)
{
    // Ideally we'd call ASSERT(!needsStyleRecalc()) here, but
    // ContainerNode::setFocus() calls invalidateStyleForSubtree(), so the assertion
    // would fire in the middle of Document::setFocusedElement().

    for (const Node* node = &startNode; node; node = node->parentNode()) {
        auto* style = node->isDocumentNode() ? node->renderStyle() : const_cast<Node*>(node)->computedStyle();
        if (!style)
            continue;
        if (style->display() == DisplayType::None)
            continue;
#if ENABLE(USERSELECT_ALL)
        // Elements with user-select: all style are considered atomic
        // therefore non editable.
        if (treatment == Node::UserSelectAllIsAlwaysNonEditable && style->userSelect() == UserSelect::All)
            return Node::Editability::ReadOnly;
#else
        UNUSED_PARAM(treatment);
#endif
        switch (style->userModify()) {
        case UserModify::ReadOnly:
            return Node::Editability::ReadOnly;
        case UserModify::ReadWrite:
            return Node::Editability::CanEditRichly;
        case UserModify::ReadWritePlaintextOnly:
            return Node::Editability::CanEditPlainText;
        }
        ASSERT_NOT_REACHED();
        return Node::Editability::ReadOnly;
    }
    return Node::Editability::ReadOnly;
}

Node::Editability Node::computeEditability(UserSelectAllTreatment treatment, ShouldUpdateStyle shouldUpdateStyle) const
{
    if (!document().hasLivingRenderTree() || isPseudoElement())
        return Editability::ReadOnly;

    if (isInShadowTree())
        return HTMLElement::editabilityFromContentEditableAttr(*this);

    if (document().frame() && document().frame()->page() && document().frame()->page()->isEditable())
        return Editability::CanEditRichly;

    if (shouldUpdateStyle == ShouldUpdateStyle::Update && document().needsStyleRecalc()) {
        if (!document().usesStyleBasedEditability())
            return HTMLElement::editabilityFromContentEditableAttr(*this);
        document().updateStyleIfNeeded();
    }
    return computeEditabilityFromComputedStyle(*this, treatment);
}

RenderBox* Node::renderBox() const
{
    RenderObject* renderer = this->renderer();
    return is<RenderBox>(renderer) ? downcast<RenderBox>(renderer) : nullptr;
}

RenderBoxModelObject* Node::renderBoxModelObject() const
{
    RenderObject* renderer = this->renderer();
    return is<RenderBoxModelObject>(renderer) ? downcast<RenderBoxModelObject>(renderer) : nullptr;
}
    
LayoutRect Node::renderRect(bool* isReplaced)
{    
    RenderObject* hitRenderer = this->renderer();
    ASSERT(hitRenderer);
    RenderObject* renderer = hitRenderer;
    while (renderer && !renderer->isBody() && !renderer->isDocumentElementRenderer()) {
        if (renderer->isRenderBlock() || renderer->isInlineBlockOrInlineTable() || renderer->isReplaced()) {
            *isReplaced = renderer->isReplaced();
            return renderer->absoluteBoundingBoxRect();
        }
        renderer = renderer->parent();
    }
    return LayoutRect();    
}

void Node::refEventTarget()
{
    ref();
}

void Node::derefEventTarget()
{
    deref();
}

void Node::adjustStyleValidity(Style::Validity validity, Style::InvalidationMode mode)
{
    if (validity > styleValidity()) {
        m_nodeFlags &= ~StyleValidityMask;
        m_nodeFlags |= static_cast<unsigned>(validity) << StyleValidityShift;
    }
    if (mode == Style::InvalidationMode::RecompositeLayer)
        setFlag(StyleResolutionShouldRecompositeLayerFlag);
}

inline void Node::updateAncestorsForStyleRecalc()
{
    auto composedAncestors = composedTreeAncestors(*this);
    auto it = composedAncestors.begin();
    auto end = composedAncestors.end();
    if (it != end) {
        it->setDirectChildNeedsStyleRecalc();

        for (; it != end; ++it) {
            // Iterator skips over shadow roots.
            if (auto* shadowRoot = it->shadowRoot())
                shadowRoot->setChildNeedsStyleRecalc();
            if (it->childNeedsStyleRecalc())
                break;
            it->setChildNeedsStyleRecalc();
        }
    }

    auto* documentElement = document().documentElement();
    if (!documentElement)
        return;
    if (!documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return;
    document().setChildNeedsStyleRecalc();
    document().scheduleStyleRecalc();
}

void Node::invalidateStyle(Style::Validity validity, Style::InvalidationMode mode)
{
    ASSERT(validity != Style::Validity::Valid);
    if (!inRenderedDocument())
        return;

    // FIXME: This should eventually be an ASSERT.
    if (document().inRenderTreeUpdate())
        return;

    // FIXME: Why the second condition?
    bool markAncestors = styleValidity() == Style::Validity::Valid || validity == Style::Validity::SubtreeAndRenderersInvalid;

    adjustStyleValidity(validity, mode);

    if (markAncestors)
        updateAncestorsForStyleRecalc();
}

unsigned Node::computeNodeIndex() const
{
    unsigned count = 0;
    for (Node* sibling = previousSibling(); sibling; sibling = sibling->previousSibling())
        ++count;
    return count;
}

template<unsigned type>
bool shouldInvalidateNodeListCachesForAttr(const unsigned nodeListCounts[], const QualifiedName& attrName)
{
    if (nodeListCounts[type] && shouldInvalidateTypeOnAttributeChange(static_cast<NodeListInvalidationType>(type), attrName))
        return true;
    return shouldInvalidateNodeListCachesForAttr<type + 1>(nodeListCounts, attrName);
}

template<>
bool shouldInvalidateNodeListCachesForAttr<numNodeListInvalidationTypes>(const unsigned[], const QualifiedName&)
{
    return false;
}

inline bool Document::shouldInvalidateNodeListAndCollectionCaches() const
{
    for (int type = 0; type < numNodeListInvalidationTypes; ++type) {
        if (m_nodeListAndCollectionCounts[type])
            return true;
    }
    return false;
}

inline bool Document::shouldInvalidateNodeListAndCollectionCachesForAttribute(const QualifiedName& attrName) const
{
    return shouldInvalidateNodeListCachesForAttr<DoNotInvalidateOnAttributeChanges + 1>(m_nodeListAndCollectionCounts, attrName);
}

template <typename InvalidationFunction>
void Document::invalidateNodeListAndCollectionCaches(InvalidationFunction invalidate)
{
    for (auto* list : copyToVectorSpecialization<Vector<LiveNodeList*, 8>>(m_listsInvalidatedAtDocument))
        invalidate(*list);

    for (auto* collection : copyToVectorSpecialization<Vector<HTMLCollection*, 8>>(m_collectionsInvalidatedAtDocument))
        invalidate(*collection);
}

void Node::invalidateNodeListAndCollectionCachesInAncestors()
{
    if (hasRareData()) {
        if (auto* lists = rareData()->nodeLists())
            lists->clearChildNodeListCache();
    }

    if (!document().shouldInvalidateNodeListAndCollectionCaches())
        return;

    document().invalidateNodeListAndCollectionCaches([](auto& list) {
        list.invalidateCache();
    });

    for (auto* node = this; node; node = node->parentNode()) {
        if (!node->hasRareData())
            continue;

        if (auto* lists = node->rareData()->nodeLists())
            lists->invalidateCaches();
    }
}

void Node::invalidateNodeListAndCollectionCachesInAncestorsForAttribute(const QualifiedName& attrName)
{
    ASSERT(is<Element>(*this));

    if (!document().shouldInvalidateNodeListAndCollectionCachesForAttribute(attrName))
        return;

    document().invalidateNodeListAndCollectionCaches([&attrName](auto& list) {
        list.invalidateCacheForAttribute(attrName);
    });

    for (auto* node = this; node; node = node->parentNode()) {
        if (!node->hasRareData())
            continue;

        if (auto* lists = node->rareData()->nodeLists())
            lists->invalidateCachesForAttribute(attrName);
    }
}

NodeListsNodeData* Node::nodeLists()
{
    return hasRareData() ? rareData()->nodeLists() : nullptr;
}

void Node::clearNodeLists()
{
    rareData()->clearNodeLists();
}

ExceptionOr<void> Node::checkSetPrefix(const AtomicString& prefix)
{
    // Perform error checking as required by spec for setting Node.prefix. Used by
    // Element::setPrefix() and Attr::setPrefix()

    if (!prefix.isEmpty() && !Document::isValidName(prefix))
        return Exception { InvalidCharacterError };

    // FIXME: Raise NamespaceError if prefix is malformed per the Namespaces in XML specification.

    auto& namespaceURI = this->namespaceURI();
    if (namespaceURI.isEmpty() && !prefix.isEmpty())
        return Exception { NamespaceError };
    if (prefix == xmlAtom() && namespaceURI != XMLNames::xmlNamespaceURI)
        return Exception { NamespaceError };

    // Attribute-specific checks are in Attr::setPrefix().

    return { };
}

bool Node::isDescendantOf(const Node& other) const
{
    // Return true if other is an ancestor of this, otherwise false
    if (!other.hasChildNodes() || isConnected() != other.isConnected())
        return false;
    if (other.isDocumentNode())
        return &document() == &other && !isDocumentNode() && isConnected();
    for (const auto* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        if (ancestor == &other)
            return true;
    }
    return false;
}

bool Node::isDescendantOrShadowDescendantOf(const Node* other) const
{
    if (!other) 
        return false;
    if (isDescendantOf(*other))
        return true;
    const Node* shadowAncestorNode = deprecatedShadowAncestorNode();
    if (!shadowAncestorNode)
        return false;
    return shadowAncestorNode == other || shadowAncestorNode->isDescendantOf(*other);
}

bool Node::contains(const Node* node) const
{
    if (!node)
        return false;
    return this == node || node->isDescendantOf(*this);
}

bool Node::containsIncludingShadowDOM(const Node* node) const
{
    for (; node; node = node->parentOrShadowHostNode()) {
        if (node == this)
            return true;
    }
    return false;
}

bool Node::containsIncludingHostElements(const Node* node) const
{
    while (node) {
        if (node == this)
            return true;
        if (node->isDocumentFragment() && static_cast<const DocumentFragment*>(node)->isTemplateContent())
            node = static_cast<const TemplateContentDocumentFragment*>(node)->host();
        else
            node = node->parentOrShadowHostNode();
    }
    return false;
}

Node* Node::pseudoAwarePreviousSibling() const
{
    Element* parentOrHost = is<PseudoElement>(*this) ? downcast<PseudoElement>(*this).hostElement() : parentElement();
    if (parentOrHost && !previousSibling()) {
        if (isAfterPseudoElement() && parentOrHost->lastChild())
            return parentOrHost->lastChild();
        if (!isBeforePseudoElement())
            return parentOrHost->beforePseudoElement();
    }
    return previousSibling();
}

Node* Node::pseudoAwareNextSibling() const
{
    Element* parentOrHost = is<PseudoElement>(*this) ? downcast<PseudoElement>(*this).hostElement() : parentElement();
    if (parentOrHost && !nextSibling()) {
        if (isBeforePseudoElement() && parentOrHost->firstChild())
            return parentOrHost->firstChild();
        if (!isAfterPseudoElement())
            return parentOrHost->afterPseudoElement();
    }
    return nextSibling();
}

Node* Node::pseudoAwareFirstChild() const
{
    if (is<Element>(*this)) {
        const Element& currentElement = downcast<Element>(*this);
        Node* first = currentElement.beforePseudoElement();
        if (first)
            return first;
        first = currentElement.firstChild();
        if (!first)
            first = currentElement.afterPseudoElement();
        return first;
    }
    return firstChild();
}

Node* Node::pseudoAwareLastChild() const
{
    if (is<Element>(*this)) {
        const Element& currentElement = downcast<Element>(*this);
        Node* last = currentElement.afterPseudoElement();
        if (last)
            return last;
        last = currentElement.lastChild();
        if (!last)
            last = currentElement.beforePseudoElement();
        return last;
    }
    return lastChild();
}

const RenderStyle* Node::computedStyle(PseudoId pseudoElementSpecifier)
{
    auto* composedParent = composedTreeAncestors(*this).first();
    if (!composedParent)
        return nullptr;
    return composedParent->computedStyle(pseudoElementSpecifier);
}

int Node::maxCharacterOffset() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks questions about HTML in the core DOM class
// is obviously misplaced.
bool Node::canStartSelection() const
{
    if (hasEditableStyle())
        return true;

    if (renderer()) {
        const RenderStyle& style = renderer()->style();
        // We allow selections to begin within an element that has -webkit-user-select: none set,
        // but if the element is draggable then dragging should take priority over selection.
        if (style.userDrag() == UserDrag::Element && style.userSelect() == UserSelect::None)
            return false;
    }
    return parentOrShadowHostNode() ? parentOrShadowHostNode()->canStartSelection() : true;
}

Element* Node::shadowHost() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return root->host();
    return nullptr;
}

Node* Node::deprecatedShadowAncestorNode() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return root->host();

    return const_cast<Node*>(this);
}

ShadowRoot* Node::containingShadowRoot() const
{
    ContainerNode& root = treeScope().rootNode();
    return is<ShadowRoot>(root) ? downcast<ShadowRoot>(&root) : nullptr;
}

#if !ASSERT_DISABLED
// https://dom.spec.whatwg.org/#concept-closed-shadow-hidden
static bool isClosedShadowHiddenUsingSpecDefinition(const Node& A, const Node& B)
{
    return A.isInShadowTree()
        && !A.rootNode().containsIncludingShadowDOM(&B)
        && (A.containingShadowRoot()->mode() != ShadowRootMode::Open || isClosedShadowHiddenUsingSpecDefinition(*A.shadowHost(), B));
}
#endif

// http://w3c.github.io/webcomponents/spec/shadow/#dfn-unclosed-node
bool Node::isClosedShadowHidden(const Node& otherNode) const
{
    // Use Vector instead of HashSet since we expect the number of ancestor tree scopes to be small.
    Vector<TreeScope*, 8> ancestorScopesOfThisNode;

    for (auto* scope = &treeScope(); scope; scope = scope->parentTreeScope())
        ancestorScopesOfThisNode.append(scope);

    for (auto* treeScopeThatCanAccessOtherNode = &otherNode.treeScope(); treeScopeThatCanAccessOtherNode; treeScopeThatCanAccessOtherNode = treeScopeThatCanAccessOtherNode->parentTreeScope()) {
        for (auto* scope : ancestorScopesOfThisNode) {
            if (scope == treeScopeThatCanAccessOtherNode) {
                ASSERT(!isClosedShadowHiddenUsingSpecDefinition(otherNode, *this));
                return false; // treeScopeThatCanAccessOtherNode is a shadow-including inclusive ancestor of this node.
            }
        }
        auto& root = treeScopeThatCanAccessOtherNode->rootNode();
        if (is<ShadowRoot>(root) && downcast<ShadowRoot>(root).mode() != ShadowRootMode::Open)
            break;
    }

    ASSERT(isClosedShadowHiddenUsingSpecDefinition(otherNode, *this));
    return true;
}

static inline ShadowRoot* parentShadowRoot(const Node& node)
{
    if (auto* parent = node.parentElement())
        return parent->shadowRoot();
    return nullptr;
}

HTMLSlotElement* Node::assignedSlot() const
{
    if (auto* shadowRoot = parentShadowRoot(*this))
        return shadowRoot->findAssignedSlot(*this);
    return nullptr;
}

HTMLSlotElement* Node::assignedSlotForBindings() const
{
    auto* shadowRoot = parentShadowRoot(*this);
    if (shadowRoot && shadowRoot->mode() == ShadowRootMode::Open)
        return shadowRoot->findAssignedSlot(*this);
    return nullptr;
}

ContainerNode* Node::parentInComposedTree() const
{
    ASSERT(isMainThreadOrGCThread());
    if (auto* slot = assignedSlot())
        return slot;
    if (is<ShadowRoot>(*this))
        return downcast<ShadowRoot>(*this).host();
    return parentNode();
}

Element* Node::parentElementInComposedTree() const
{
    if (auto* slot = assignedSlot())
        return slot;
    if (is<PseudoElement>(*this))
        return downcast<PseudoElement>(*this).hostElement();
    if (auto* parent = parentNode()) {
        if (is<ShadowRoot>(*parent))
            return downcast<ShadowRoot>(*parent).host();
        if (is<Element>(*parent))
            return downcast<Element>(parent);
    }
    return nullptr;
}

bool Node::isInUserAgentShadowTree() const
{
    auto* shadowRoot = containingShadowRoot();
    return shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent;
}

Node* Node::nonBoundaryShadowTreeRootNode()
{
    ASSERT(!isShadowRoot());
    Node* root = this;
    while (root) {
        if (root->isShadowRoot())
            return root;
        Node* parent = root->parentNodeGuaranteedHostFree();
        if (parent && parent->isShadowRoot())
            return root;
        root = parent;
    }
    return 0;
}

ContainerNode* Node::nonShadowBoundaryParentNode() const
{
    ContainerNode* parent = parentNode();
    return parent && !parent->isShadowRoot() ? parent : nullptr;
}

Element* Node::parentOrShadowHostElement() const
{
    ContainerNode* parent = parentOrShadowHostNode();
    if (!parent)
        return nullptr;

    if (is<ShadowRoot>(*parent))
        return downcast<ShadowRoot>(*parent).host();

    if (!is<Element>(*parent))
        return nullptr;

    return downcast<Element>(parent);
}

Node& Node::traverseToRootNode() const
{
    Node* node = const_cast<Node*>(this);
    Node* highest = node;
    for (; node; node = node->parentNode())
        highest = node;
    return *highest;
}

// https://dom.spec.whatwg.org/#concept-shadow-including-root
Node& Node::shadowIncludingRoot() const
{
    auto& root = rootNode();
    if (!is<ShadowRoot>(root))
        return root;
    auto* host = downcast<ShadowRoot>(root).host();
    return host ? host->shadowIncludingRoot() : root;
}

Node& Node::getRootNode(const GetRootNodeOptions& options) const
{
    return options.composed ? shadowIncludingRoot() : rootNode();
}

Node::InsertedIntoAncestorResult Node::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    if (insertionType.connectedToDocument)
        setFlag(IsConnectedFlag);
    if (parentOfInsertedTree.isInShadowTree())
        setFlag(IsInShadowTreeFlag);

    invalidateStyle(Style::Validity::SubtreeAndRenderersInvalid);

    return InsertedIntoAncestorResult::Done;
}

void Node::removedFromAncestor(RemovalType removalType, ContainerNode&)
{
    if (removalType.disconnectedFromDocument)
        clearFlag(IsConnectedFlag);
    if (isInShadowTree() && !treeScope().rootNode().isShadowRoot())
        clearFlag(IsInShadowTreeFlag);
}

bool Node::isRootEditableElement() const
{
    return hasEditableStyle() && isElementNode() && (!parentNode() || !parentNode()->hasEditableStyle()
        || !parentNode()->isElementNode() || hasTagName(bodyTag));
}

Element* Node::rootEditableElement() const
{
    Element* result = nullptr;
    for (Node* node = const_cast<Node*>(this); node && node->hasEditableStyle(); node = node->parentNode()) {
        if (is<Element>(*node))
            result = downcast<Element>(node);
        if (is<HTMLBodyElement>(*node))
            break;
    }
    return result;
}

// FIXME: End of obviously misplaced HTML editing functions.  Try to move these out of Node.

Document* Node::ownerDocument() const
{
    Document* document = &this->document();
    return document == this ? nullptr : document;
}

const URL& Node::baseURI() const
{
    auto& url = document().baseURL();
    return url.isNull() ? WTF::blankURL() : url;
}

bool Node::isEqualNode(Node* other) const
{
    if (!other)
        return false;
    
    NodeType nodeType = this->nodeType();
    if (nodeType != other->nodeType())
        return false;
    
    switch (nodeType) {
    case Node::DOCUMENT_TYPE_NODE: {
        auto& thisDocType = downcast<DocumentType>(*this);
        auto& otherDocType = downcast<DocumentType>(*other);
        if (thisDocType.name() != otherDocType.name())
            return false;
        if (thisDocType.publicId() != otherDocType.publicId())
            return false;
        if (thisDocType.systemId() != otherDocType.systemId())
            return false;
        break;
        }
    case Node::ELEMENT_NODE: {
        auto& thisElement = downcast<Element>(*this);
        auto& otherElement = downcast<Element>(*other);
        if (thisElement.tagQName() != otherElement.tagQName())
            return false;
        if (!thisElement.hasEquivalentAttributes(otherElement))
            return false;
        break;
        }
    case Node::PROCESSING_INSTRUCTION_NODE: {
        auto& thisProcessingInstruction = downcast<ProcessingInstruction>(*this);
        auto& otherProcessingInstruction = downcast<ProcessingInstruction>(*other);
        if (thisProcessingInstruction.target() != otherProcessingInstruction.target())
            return false;
        if (thisProcessingInstruction.data() != otherProcessingInstruction.data())
            return false;
        break;
        }
    case Node::CDATA_SECTION_NODE:
    case Node::TEXT_NODE:
    case Node::COMMENT_NODE: {
        auto& thisCharacterData = downcast<CharacterData>(*this);
        auto& otherCharacterData = downcast<CharacterData>(*other);
        if (thisCharacterData.data() != otherCharacterData.data())
            return false;
        break;
        }
    case Node::ATTRIBUTE_NODE: {
        auto& thisAttribute = downcast<Attr>(*this);
        auto& otherAttribute = downcast<Attr>(*other);
        if (thisAttribute.qualifiedName() != otherAttribute.qualifiedName())
            return false;
        if (thisAttribute.value() != otherAttribute.value())
            return false;
        break;
        }
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        break;
    }
    
    Node* child = firstChild();
    Node* otherChild = other->firstChild();
    
    while (child) {
        if (!child->isEqualNode(otherChild))
            return false;
        
        child = child->nextSibling();
        otherChild = otherChild->nextSibling();
    }
    
    if (otherChild)
        return false;
    
    return true;
}

// https://dom.spec.whatwg.org/#locate-a-namespace
static const AtomicString& locateDefaultNamespace(const Node& node, const AtomicString& prefix)
{
    switch (node.nodeType()) {
    case Node::ELEMENT_NODE: {
        auto& element = downcast<Element>(node);
        auto& namespaceURI = element.namespaceURI();
        if (!namespaceURI.isNull() && element.prefix() == prefix)
            return namespaceURI;

        if (element.hasAttributes()) {
            for (auto& attribute : element.attributesIterator()) {
                if (attribute.namespaceURI() != XMLNSNames::xmlnsNamespaceURI)
                    continue;

                if ((prefix.isNull() && attribute.prefix().isNull() && attribute.localName() == xmlnsAtom()) || (attribute.prefix() == xmlnsAtom() && attribute.localName() == prefix)) {
                    auto& result = attribute.value();
                    return result.isEmpty() ? nullAtom() : result;
                }
            }
        }
        auto* parent = node.parentElement();
        return parent ? locateDefaultNamespace(*parent, prefix) : nullAtom();
    }
    case Node::DOCUMENT_NODE:
        if (auto* documentElement = downcast<Document>(node).documentElement())
            return locateDefaultNamespace(*documentElement, prefix);
        return nullAtom();
    case Node::DOCUMENT_TYPE_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        return nullAtom();
    case Node::ATTRIBUTE_NODE:
        if (auto* ownerElement = downcast<Attr>(node).ownerElement())
            return locateDefaultNamespace(*ownerElement, prefix);
        return nullAtom();
    default:
        if (auto* parent = node.parentElement())
            return locateDefaultNamespace(*parent, prefix);
        return nullAtom();
    }
}

// https://dom.spec.whatwg.org/#dom-node-isdefaultnamespace
bool Node::isDefaultNamespace(const AtomicString& potentiallyEmptyNamespace) const
{
    const AtomicString& namespaceURI = potentiallyEmptyNamespace.isEmpty() ? nullAtom() : potentiallyEmptyNamespace;
    return locateDefaultNamespace(*this, nullAtom()) == namespaceURI;
}

// https://dom.spec.whatwg.org/#dom-node-lookupnamespaceuri
const AtomicString& Node::lookupNamespaceURI(const AtomicString& potentiallyEmptyPrefix) const
{
    const AtomicString& prefix = potentiallyEmptyPrefix.isEmpty() ? nullAtom() : potentiallyEmptyPrefix;
    return locateDefaultNamespace(*this, prefix);
}

// https://dom.spec.whatwg.org/#locate-a-namespace-prefix
static const AtomicString& locateNamespacePrefix(const Element& element, const AtomicString& namespaceURI)
{
    if (element.namespaceURI() == namespaceURI)
        return element.prefix();

    if (element.hasAttributes()) {
        for (auto& attribute : element.attributesIterator()) {
            if (attribute.prefix() == xmlnsAtom() && attribute.value() == namespaceURI)
                return attribute.localName();
        }
    }
    auto* parent = element.parentElement();
    return parent ? locateNamespacePrefix(*parent, namespaceURI) : nullAtom();
}

// https://dom.spec.whatwg.org/#dom-node-lookupprefix
const AtomicString& Node::lookupPrefix(const AtomicString& namespaceURI) const
{
    if (namespaceURI.isEmpty())
        return nullAtom();
    
    switch (nodeType()) {
    case ELEMENT_NODE:
        return locateNamespacePrefix(downcast<Element>(*this), namespaceURI);
    case DOCUMENT_NODE:
        if (auto* documentElement = downcast<Document>(*this).documentElement())
            return locateNamespacePrefix(*documentElement, namespaceURI);
        return nullAtom();
    case DOCUMENT_FRAGMENT_NODE:
    case DOCUMENT_TYPE_NODE:
        return nullAtom();
    case ATTRIBUTE_NODE:
        if (auto* ownerElement = downcast<Attr>(*this).ownerElement())
            return locateNamespacePrefix(*ownerElement, namespaceURI);
        return nullAtom();
    default:
        if (auto* parent = parentElement())
            return locateNamespacePrefix(*parent, namespaceURI);
        return nullAtom();
    }
}

static void appendTextContent(const Node* node, bool convertBRsToNewlines, bool& isNullString, StringBuilder& content)
{
    switch (node->nodeType()) {
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::COMMENT_NODE:
        isNullString = false;
        content.append(downcast<CharacterData>(*node).data());
        break;

    case Node::PROCESSING_INSTRUCTION_NODE:
        isNullString = false;
        content.append(downcast<ProcessingInstruction>(*node).data());
        break;
    
    case Node::ATTRIBUTE_NODE:
        isNullString = false;
        content.append(downcast<Attr>(*node).value());
        break;

    case Node::ELEMENT_NODE:
        if (node->hasTagName(brTag) && convertBRsToNewlines) {
            isNullString = false;
            content.append('\n');
            break;
        }
        FALLTHROUGH;
    case Node::DOCUMENT_FRAGMENT_NODE:
        isNullString = false;
        for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
            if (child->nodeType() == Node::COMMENT_NODE || child->nodeType() == Node::PROCESSING_INSTRUCTION_NODE)
                continue;
            appendTextContent(child, convertBRsToNewlines, isNullString, content);
        }
        break;

    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_TYPE_NODE:
        break;
    }
}

String Node::textContent(bool convertBRsToNewlines) const
{
    StringBuilder content;
    bool isNullString = true;
    appendTextContent(this, convertBRsToNewlines, isNullString, content);
    return isNullString ? String() : content.toString();
}

ExceptionOr<void> Node::setTextContent(const String& text)
{           
    switch (nodeType()) {
    case ATTRIBUTE_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
        return setNodeValue(text);
    case ELEMENT_NODE:
    case DOCUMENT_FRAGMENT_NODE: {
        auto& container = downcast<ContainerNode>(*this);
        if (text.isEmpty())
            container.replaceAllChildren(nullptr);
        else
            container.replaceAllChildren(document().createTextNode(text));
        return { };
    }
    case DOCUMENT_NODE:
    case DOCUMENT_TYPE_NODE:
        // Do nothing.
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

static SHA1::Digest hashPointer(void* pointer)
{
    SHA1 sha1;
    sha1.addBytes(reinterpret_cast<const uint8_t*>(&pointer), sizeof(pointer));
    SHA1::Digest digest;
    sha1.computeHash(digest);
    return digest;
}

static inline unsigned short compareDetachedElementsPosition(Node& firstNode, Node& secondNode)
{
    // If the 2 nodes are not in the same tree, return the result of adding DOCUMENT_POSITION_DISCONNECTED,
    // DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC, and either DOCUMENT_POSITION_PRECEDING or
    // DOCUMENT_POSITION_FOLLOWING, with the constraint that this is to be consistent. Whether to return
    // DOCUMENT_POSITION_PRECEDING or DOCUMENT_POSITION_FOLLOWING is implemented by comparing cryptographic
    // hashes of Node pointers.
    // See step 3 in https://dom.spec.whatwg.org/#dom-node-comparedocumentposition
    SHA1::Digest firstHash = hashPointer(&firstNode);
    SHA1::Digest secondHash = hashPointer(&secondNode);

    unsigned short direction = memcmp(firstHash.data(), secondHash.data(), SHA1::hashSize) > 0 ? Node::DOCUMENT_POSITION_PRECEDING : Node::DOCUMENT_POSITION_FOLLOWING;

    return Node::DOCUMENT_POSITION_DISCONNECTED | Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
}

unsigned short Node::compareDocumentPosition(Node& otherNode)
{
    if (&otherNode == this)
        return DOCUMENT_POSITION_EQUIVALENT;
    
    Attr* attr1 = is<Attr>(*this) ? downcast<Attr>(this) : nullptr;
    Attr* attr2 = is<Attr>(otherNode) ? &downcast<Attr>(otherNode) : nullptr;
    
    Node* start1 = attr1 ? attr1->ownerElement() : this;
    Node* start2 = attr2 ? attr2->ownerElement() : &otherNode;
    
    // If either of start1 or start2 is null, then we are disconnected, since one of the nodes is
    // an orphaned attribute node.
    if (!start1 || !start2)
        return compareDetachedElementsPosition(*this, otherNode);

    Vector<Node*, 16> chain1;
    Vector<Node*, 16> chain2;
    if (attr1)
        chain1.append(attr1);
    if (attr2)
        chain2.append(attr2);
    
    if (attr1 && attr2 && start1 == start2 && start1) {
        // We are comparing two attributes on the same node. Crawl our attribute map and see which one we hit first.
        Element* owner1 = attr1->ownerElement();
        owner1->synchronizeAllAttributes();
        for (const Attribute& attribute : owner1->attributesIterator()) {
            // If neither of the two determining nodes is a child node and nodeType is the same for both determining nodes, then an
            // implementation-dependent order between the determining nodes is returned. This order is stable as long as no nodes of
            // the same nodeType are inserted into or removed from the direct container. This would be the case, for example, 
            // when comparing two attributes of the same element, and inserting or removing additional attributes might change 
            // the order between existing attributes.
            if (attr1->qualifiedName() == attribute.name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_FOLLOWING;
            if (attr2->qualifiedName() == attribute.name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_PRECEDING;
        }
        
        ASSERT_NOT_REACHED();
        return DOCUMENT_POSITION_DISCONNECTED;
    }

    // If one node is in the document and the other is not, we must be disconnected.
    // If the nodes have different owning documents, they must be disconnected.  Note that we avoid
    // comparing Attr nodes here, since they return false from isConnected() all the time (which seems like a bug).
    if (start1->isConnected() != start2->isConnected() || &start1->treeScope() != &start2->treeScope())
        return compareDetachedElementsPosition(*this, otherNode);

    // We need to find a common ancestor container, and then compare the indices of the two immediate children.
    Node* current;
    for (current = start1; current; current = current->parentNode())
        chain1.append(current);
    for (current = start2; current; current = current->parentNode())
        chain2.append(current);

    unsigned index1 = chain1.size();
    unsigned index2 = chain2.size();

    // If the two elements don't have a common root, they're not in the same tree.
    if (chain1[index1 - 1] != chain2[index2 - 1])
        return compareDetachedElementsPosition(*this, otherNode);

    // Walk the two chains backwards and look for the first difference.
    for (unsigned i = std::min(index1, index2); i; --i) {
        Node* child1 = chain1[--index1];
        Node* child2 = chain2[--index2];
        if (child1 != child2) {
            // If one of the children is an attribute, it wins.
            if (child1->nodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_FOLLOWING;
            if (child2->nodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_PRECEDING;
            
            if (!child2->nextSibling())
                return DOCUMENT_POSITION_FOLLOWING;
            if (!child1->nextSibling())
                return DOCUMENT_POSITION_PRECEDING;

            // Otherwise we need to see which node occurs first.  Crawl backwards from child2 looking for child1.
            for (Node* child = child2->previousSibling(); child; child = child->previousSibling()) {
                if (child == child1)
                    return DOCUMENT_POSITION_FOLLOWING;
            }
            return DOCUMENT_POSITION_PRECEDING;
        }
    }
    
    // There was no difference between the two parent chains, i.e., one was a subset of the other.  The shorter
    // chain is the ancestor.
    return index1 < index2 ? 
               DOCUMENT_POSITION_FOLLOWING | DOCUMENT_POSITION_CONTAINED_BY :
               DOCUMENT_POSITION_PRECEDING | DOCUMENT_POSITION_CONTAINS;
}

FloatPoint Node::convertToPage(const FloatPoint& p) const
{
    // If there is a renderer, just ask it to do the conversion
    if (renderer())
        return renderer()->localToAbsolute(p, UseTransforms);
    
    // Otherwise go up the tree looking for a renderer
    if (auto* parent = parentElement())
        return parent->convertToPage(p);

    // No parent - no conversion needed
    return p;
}

FloatPoint Node::convertFromPage(const FloatPoint& p) const
{
    // If there is a renderer, just ask it to do the conversion
    if (renderer())
        return renderer()->absoluteToLocal(p, UseTransforms);

    // Otherwise go up the tree looking for a renderer
    if (auto* parent = parentElement())
        return parent->convertFromPage(p);

    // No parent - no conversion needed
    return p;
}

#if ENABLE(TREE_DEBUGGING)

static void appendAttributeDesc(const Node* node, StringBuilder& stringBuilder, const QualifiedName& name, const char* attrDesc)
{
    if (!is<Element>(*node))
        return;

    const AtomicString& attr = downcast<Element>(*node).getAttribute(name);
    if (attr.isEmpty())
        return;

    stringBuilder.append(attrDesc);
    stringBuilder.append(attr);
}

void Node::showNode(const char* prefix) const
{
    if (!prefix)
        prefix = "";
    if (isTextNode()) {
        String value = nodeValue();
        value.replaceWithLiteral('\\', "\\\\");
        value.replaceWithLiteral('\n', "\\n");
        fprintf(stderr, "%s%s\t%p \"%s\"\n", prefix, nodeName().utf8().data(), this, value.utf8().data());
    } else {
        StringBuilder attrs;
        appendAttributeDesc(this, attrs, classAttr, " CLASS=");
        appendAttributeDesc(this, attrs, styleAttr, " STYLE=");
        fprintf(stderr, "%s%s\t%p (renderer %p) %s%s%s\n", prefix, nodeName().utf8().data(), this, renderer(), attrs.toString().utf8().data(), needsStyleRecalc() ? " (needs style recalc)" : "", childNeedsStyleRecalc() ? " (child needs style recalc)" : "");
    }
}

void Node::showTreeForThis() const
{
    showTreeAndMark(this, "*");
}

void Node::showNodePathForThis() const
{
    Vector<const Node*, 16> chain;
    const Node* node = this;
    while (node->parentOrShadowHostNode()) {
        chain.append(node);
        node = node->parentOrShadowHostNode();
    }
    for (unsigned index = chain.size(); index > 0; --index) {
        const Node* node = chain[index - 1];
        if (is<ShadowRoot>(*node)) {
            int count = 0;
            for (const ShadowRoot* shadowRoot = downcast<ShadowRoot>(node); shadowRoot && shadowRoot != node; shadowRoot = shadowRoot->shadowRoot())
                ++count;
            fprintf(stderr, "/#shadow-root[%d]", count);
            continue;
        }

        switch (node->nodeType()) {
        case ELEMENT_NODE: {
            fprintf(stderr, "/%s", node->nodeName().utf8().data());

            const Element& element = downcast<Element>(*node);
            const AtomicString& idattr = element.getIdAttribute();
            bool hasIdAttr = !idattr.isNull() && !idattr.isEmpty();
            if (node->previousSibling() || node->nextSibling()) {
                int count = 0;
                for (Node* previous = node->previousSibling(); previous; previous = previous->previousSibling())
                    if (previous->nodeName() == node->nodeName())
                        ++count;
                if (hasIdAttr)
                    fprintf(stderr, "[@id=\"%s\" and position()=%d]", idattr.string().utf8().data(), count);
                else
                    fprintf(stderr, "[%d]", count);
            } else if (hasIdAttr)
                fprintf(stderr, "[@id=\"%s\"]", idattr.string().utf8().data());
            break;
        }
        case TEXT_NODE:
            fprintf(stderr, "/text()");
            break;
        case ATTRIBUTE_NODE:
            fprintf(stderr, "/@%s", node->nodeName().utf8().data());
            break;
        default:
            break;
        }
    }
    fprintf(stderr, "\n");
}

static void traverseTreeAndMark(const String& baseIndent, const Node* rootNode, const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2)
{
    for (const Node* node = rootNode; node; node = NodeTraversal::next(*node)) {
        if (node == markedNode1)
            fprintf(stderr, "%s", markedLabel1);
        if (node == markedNode2)
            fprintf(stderr, "%s", markedLabel2);

        StringBuilder indent;
        indent.append(baseIndent);
        for (const Node* tmpNode = node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentOrShadowHostNode())
            indent.append('\t');
        fprintf(stderr, "%s", indent.toString().utf8().data());
        node->showNode();
        indent.append('\t');
        if (!node->isShadowRoot()) {
            if (ShadowRoot* shadowRoot = node->shadowRoot())
                traverseTreeAndMark(indent.toString(), shadowRoot, markedNode1, markedLabel1, markedNode2, markedLabel2);
        }
    }
}

void Node::showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2) const
{
    const Node* rootNode;
    const Node* node = this;
    while (node->parentOrShadowHostNode() && !node->hasTagName(bodyTag))
        node = node->parentOrShadowHostNode();
    rootNode = node;

    String startingIndent;
    traverseTreeAndMark(startingIndent, rootNode, markedNode1, markedLabel1, markedNode2, markedLabel2);
}

void Node::formatForDebugger(char* buffer, unsigned length) const
{
    String result;
    String s;

    s = nodeName();
    if (s.isEmpty())
        result = "<none>";
    else
        result = s;

    strncpy(buffer, result.utf8().data(), length - 1);
}

static ContainerNode* parentOrShadowHostOrFrameOwner(const Node* node)
{
    ContainerNode* parent = node->parentOrShadowHostNode();
    if (!parent && node->document().frame())
        parent = node->document().frame()->ownerElement();
    return parent;
}

static void showSubTreeAcrossFrame(const Node* node, const Node* markedNode, const String& indent)
{
    if (node == markedNode)
        fputs("*", stderr);
    fputs(indent.utf8().data(), stderr);
    node->showNode();
    if (!node->isShadowRoot()) {
        if (node->isFrameOwnerElement())
            showSubTreeAcrossFrame(static_cast<const HTMLFrameOwnerElement*>(node)->contentDocument(), markedNode, indent + "\t");
        if (ShadowRoot* shadowRoot = node->shadowRoot())
            showSubTreeAcrossFrame(shadowRoot, markedNode, indent + "\t");
    }
    for (Node* child = node->firstChild(); child; child = child->nextSibling())
        showSubTreeAcrossFrame(child, markedNode, indent + "\t");
}

void Node::showTreeForThisAcrossFrame() const
{
    Node* rootNode = const_cast<Node*>(this);
    while (parentOrShadowHostOrFrameOwner(rootNode))
        rootNode = parentOrShadowHostOrFrameOwner(rootNode);
    showSubTreeAcrossFrame(rootNode, this, "");
}

#endif // ENABLE(TREE_DEBUGGING)

// --------

void NodeListsNodeData::invalidateCaches()
{
    for (auto& atomicName : m_atomicNameCaches)
        atomicName.value->invalidateCache();

    for (auto& collection : m_cachedCollections)
        collection.value->invalidateCache();

    for (auto& tagCollection : m_tagCollectionNSCache)
        tagCollection.value->invalidateCache();
}

void NodeListsNodeData::invalidateCachesForAttribute(const QualifiedName& attrName)
{
    for (auto& atomicName : m_atomicNameCaches)
        atomicName.value->invalidateCacheForAttribute(attrName);

    for (auto& collection : m_cachedCollections)
        collection.value->invalidateCacheForAttribute(attrName);
}

void Node::getSubresourceURLs(ListHashSet<URL>& urls) const
{
    addSubresourceAttributeURLs(urls);
}

Element* Node::enclosingLinkEventParentOrSelf()
{
    for (Node* node = this; node; node = node->parentInComposedTree()) {
        // For imagemaps, the enclosing link element is the associated area element not the image itself.
        // So we don't let images be the enclosing link element, even though isLink sometimes returns
        // true for them.
        if (node->isLink() && !is<HTMLImageElement>(*node))
            return downcast<Element>(node);
    }

    return nullptr;
}

EventTargetInterface Node::eventTargetInterface() const
{
    return NodeEventTargetInterfaceType;
}

template <typename MoveNodeFunction, typename MoveShadowRootFunction>
static void traverseSubtreeToUpdateTreeScope(Node& root, MoveNodeFunction moveNode, MoveShadowRootFunction moveShadowRoot)
{
    for (Node* node = &root; node; node = NodeTraversal::next(*node, &root)) {
        moveNode(*node);

        if (!is<Element>(*node))
            continue;
        Element& element = downcast<Element>(*node);

        if (element.hasSyntheticAttrChildNodes()) {
            for (auto& attr : element.attrNodeList())
                moveNode(*attr);
        }

        if (auto* shadow = element.shadowRoot())
            moveShadowRoot(*shadow);
    }
}

inline void Node::moveShadowTreeToNewDocument(ShadowRoot& shadowRoot, Document& oldDocument, Document& newDocument)
{
    traverseSubtreeToUpdateTreeScope(shadowRoot, [&oldDocument, &newDocument](Node& node) {
        node.moveNodeToNewDocument(oldDocument, newDocument);
    }, [&oldDocument, &newDocument](ShadowRoot& innerShadowRoot) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&innerShadowRoot.document() == &oldDocument);
        innerShadowRoot.moveShadowRootToNewDocument(newDocument);
        moveShadowTreeToNewDocument(innerShadowRoot, oldDocument, newDocument);
    });
}

void Node::moveTreeToNewScope(Node& root, TreeScope& oldScope, TreeScope& newScope)
{
    ASSERT(&oldScope != &newScope);

    Document& oldDocument = oldScope.documentScope();
    Document& newDocument = newScope.documentScope();
    if (&oldDocument != &newDocument) {
        oldDocument.incrementReferencingNodeCount();
        traverseSubtreeToUpdateTreeScope(root, [&](Node& node) {
            ASSERT(!node.isTreeScope());
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&node.treeScope() == &oldScope);
            node.setTreeScope(newScope);
            node.moveNodeToNewDocument(oldDocument, newDocument);
        }, [&](ShadowRoot& shadowRoot) {
            ASSERT_WITH_SECURITY_IMPLICATION(&shadowRoot.document() == &oldDocument);
            shadowRoot.moveShadowRootToNewParentScope(newScope, newDocument);
            moveShadowTreeToNewDocument(shadowRoot, oldDocument, newDocument);
        });
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&oldScope.documentScope() == &oldDocument && &newScope.documentScope() == &newDocument);
        oldDocument.decrementReferencingNodeCount();
    } else {
        traverseSubtreeToUpdateTreeScope(root, [&](Node& node) {
            ASSERT(!node.isTreeScope());
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&node.treeScope() == &oldScope);
            node.setTreeScope(newScope);
            if (UNLIKELY(!node.hasRareData()))
                return;
            if (auto* nodeLists = node.rareData()->nodeLists())
                nodeLists->adoptTreeScope();
        }, [&newScope](ShadowRoot& shadowRoot) {
            shadowRoot.setParentTreeScope(newScope);
        });
    }
}

void Node::moveNodeToNewDocument(Document& oldDocument, Document& newDocument)
{
    newDocument.incrementReferencingNodeCount();
    oldDocument.decrementReferencingNodeCount();

    if (hasRareData()) {
        if (auto* nodeLists = rareData()->nodeLists())
            nodeLists->adoptDocument(oldDocument, newDocument);
        if (auto* registry = mutationObserverRegistry()) {
            for (auto& registration : *registry)
                newDocument.addMutationObserverTypes(registration->mutationTypes());
        }
        if (auto* transientRegistry = transientMutationObserverRegistry()) {
            for (auto& registration : *transientRegistry)
                newDocument.addMutationObserverTypes(registration->mutationTypes());
        }
    } else {
        ASSERT(!mutationObserverRegistry());
        ASSERT(!transientMutationObserverRegistry());
    }

    oldDocument.moveNodeIteratorsToNewDocument(*this, newDocument);

    if (AXObjectCache::accessibilityEnabled()) {
        if (auto* cache = oldDocument.existingAXObjectCache())
            cache->remove(*this);
    }

    if (auto* eventTargetData = this->eventTargetData()) {
        if (!eventTargetData->eventListenerMap.isEmpty()) {
            for (auto& type : eventTargetData->eventListenerMap.eventTypes())
                newDocument.addListenerTypeIfNeeded(type);
        }

        unsigned numWheelEventHandlers = eventListeners(eventNames().mousewheelEvent).size() + eventListeners(eventNames().wheelEvent).size();
        for (unsigned i = 0; i < numWheelEventHandlers; ++i) {
            oldDocument.didRemoveWheelEventHandler(*this);
            newDocument.didAddWheelEventHandler(*this);
        }

        unsigned numTouchEventListeners = 0;
        for (auto& name : eventNames().touchAndPointerEventNames())
            numTouchEventListeners += eventListeners(name).size();

        for (unsigned i = 0; i < numTouchEventListeners; ++i) {
            oldDocument.didRemoveTouchEventHandler(*this);
            newDocument.didAddTouchEventHandler(*this);
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
            oldDocument.removeTouchEventListener(*this);
            newDocument.addTouchEventListener(*this);
#endif
        }

#if ENABLE(TOUCH_EVENTS) && ENABLE(IOS_GESTURE_EVENTS)
        unsigned numGestureEventListeners = 0;
        for (auto& name : eventNames().gestureEventNames())
            numGestureEventListeners += eventListeners(name).size();

        for (unsigned i = 0; i < numGestureEventListeners; ++i) {
            oldDocument.removeTouchEventHandler(*this);
            newDocument.addTouchEventHandler(*this);
        }
#endif
    }

#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
    ASSERT_WITH_SECURITY_IMPLICATION(!oldDocument.touchEventListenersContain(*this));
    ASSERT_WITH_SECURITY_IMPLICATION(!oldDocument.touchEventHandlersContain(*this));
#endif
#if ENABLE(TOUCH_EVENTS) && ENABLE(IOS_GESTURE_EVENTS)
    ASSERT_WITH_SECURITY_IMPLICATION(!oldDocument.touchEventTargetsContain(*this));
#endif
#endif

    if (is<Element>(*this))
        downcast<Element>(*this).didMoveToNewDocument(oldDocument, newDocument);
}

static inline bool tryAddEventListener(Node* targetNode, const AtomicString& eventType, Ref<EventListener>&& listener, const EventTarget::AddEventListenerOptions& options)
{
    if (!targetNode->EventTarget::addEventListener(eventType, listener.copyRef(), options))
        return false;

    targetNode->document().addListenerTypeIfNeeded(eventType);
    if (eventNames().isWheelEventType(eventType))
        targetNode->document().didAddWheelEventHandler(*targetNode);
    else if (eventNames().isTouchEventType(eventType))
        targetNode->document().didAddTouchEventHandler(*targetNode);

#if PLATFORM(IOS_FAMILY)
    if (targetNode == &targetNode->document() && eventType == eventNames().scrollEvent)
        targetNode->document().domWindow()->incrementScrollEventListenersCount();

    // FIXME: Would it be sufficient to special-case this code for <body> and <frameset>?
    //
    // This code was added to address <rdar://problem/5846492> Onorientationchange event not working for document.body.
    // Forward this call to addEventListener() to the window since these are window-only events.
    if (eventType == eventNames().orientationchangeEvent || eventType == eventNames().resizeEvent)
        targetNode->document().domWindow()->addEventListener(eventType, WTFMove(listener), options);

#if ENABLE(TOUCH_EVENTS)
    if (eventNames().isTouchEventType(eventType))
        targetNode->document().addTouchEventListener(*targetNode);
#endif
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(IOS_GESTURE_EVENTS) && ENABLE(TOUCH_EVENTS)
    if (eventNames().isGestureEventType(eventType))
        targetNode->document().addTouchEventHandler(*targetNode);
#endif

    return true;
}

bool Node::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    return tryAddEventListener(this, eventType, WTFMove(listener), options);
}

static inline bool tryRemoveEventListener(Node* targetNode, const AtomicString& eventType, EventListener& listener, const EventTarget::ListenerOptions& options)
{
    if (!targetNode->EventTarget::removeEventListener(eventType, listener, options))
        return false;

    // FIXME: Notify Document that the listener has vanished. We need to keep track of a number of
    // listeners for each type, not just a bool - see https://bugs.webkit.org/show_bug.cgi?id=33861
    if (eventNames().isWheelEventType(eventType))
        targetNode->document().didRemoveWheelEventHandler(*targetNode);
    else if (eventNames().isTouchEventType(eventType))
        targetNode->document().didRemoveTouchEventHandler(*targetNode);

#if PLATFORM(IOS_FAMILY)
    if (targetNode == &targetNode->document() && eventType == eventNames().scrollEvent)
        targetNode->document().domWindow()->decrementScrollEventListenersCount();

    // FIXME: Would it be sufficient to special-case this code for <body> and <frameset>? See <rdar://problem/15647823>.
    // This code was added to address <rdar://problem/5846492> Onorientationchange event not working for document.body.
    // Forward this call to removeEventListener() to the window since these are window-only events.
    if (eventType == eventNames().orientationchangeEvent || eventType == eventNames().resizeEvent)
        targetNode->document().domWindow()->removeEventListener(eventType, listener, options);

#if ENABLE(TOUCH_EVENTS)
    if (eventNames().isTouchEventType(eventType))
        targetNode->document().removeTouchEventListener(*targetNode);
#endif
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(IOS_GESTURE_EVENTS) && ENABLE(TOUCH_EVENTS)
    if (eventNames().isGestureEventType(eventType))
        targetNode->document().removeTouchEventHandler(*targetNode);
#endif

    return true;
}

bool Node::removeEventListener(const AtomicString& eventType, EventListener& listener, const ListenerOptions& options)
{
    return tryRemoveEventListener(this, eventType, listener, options);
}

typedef HashMap<Node*, std::unique_ptr<EventTargetData>> EventTargetDataMap;

static EventTargetDataMap& eventTargetDataMap()
{
    static NeverDestroyed<EventTargetDataMap> map;

    return map;
}

static Lock s_eventTargetDataMapLock;

EventTargetData* Node::eventTargetData()
{
    return hasEventTargetData() ? eventTargetDataMap().get(this) : nullptr;
}

EventTargetData* Node::eventTargetDataConcurrently()
{
    // Not holding the lock when the world is stopped accelerates parallel constraint solving, which
    // calls this function from many threads. Parallel constraint solving can happen with the world
    // running or stopped, but if we do it with a running world, then we're usually mixing constraint
    // solving with other work. Therefore, the most likely time for contention on this lock is when the
    // world is stopped. We don't have to hold the lock when the world is stopped, because a stopped world
    // means that we will never mutate the event target data map.
    JSC::VM* vm = commonVMOrNull();
    auto locker = holdLockIf(s_eventTargetDataMapLock, vm && vm->heap.worldIsRunning());
    return hasEventTargetData() ? eventTargetDataMap().get(this) : nullptr;
}

EventTargetData& Node::ensureEventTargetData()
{
    if (hasEventTargetData())
        return *eventTargetDataMap().get(this);

    JSC::VM* vm = commonVMOrNull();
    RELEASE_ASSERT(!vm || vm->heap.worldIsRunning());

    auto locker = holdLock(s_eventTargetDataMapLock);
    setHasEventTargetData(true);
    return *eventTargetDataMap().add(this, std::make_unique<EventTargetData>()).iterator->value;
}

void Node::clearEventTargetData()
{
    JSC::VM* vm = commonVMOrNull();
    RELEASE_ASSERT(!vm || vm->heap.worldIsRunning());
    auto locker = holdLock(s_eventTargetDataMapLock);
    eventTargetDataMap().remove(this);
}

Vector<std::unique_ptr<MutationObserverRegistration>>* Node::mutationObserverRegistry()
{
    if (!hasRareData())
        return nullptr;
    auto* data = rareData()->mutationObserverData();
    if (!data)
        return nullptr;
    return &data->registry;
}

HashSet<MutationObserverRegistration*>* Node::transientMutationObserverRegistry()
{
    if (!hasRareData())
        return nullptr;
    auto* data = rareData()->mutationObserverData();
    if (!data)
        return nullptr;
    return &data->transientRegistry;
}

template<typename Registry> static inline void collectMatchingObserversForMutation(HashMap<Ref<MutationObserver>, MutationRecordDeliveryOptions>& observers, Registry* registry, Node& target, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    if (!registry)
        return;

    for (auto& registration : *registry) {
        if (registration->shouldReceiveMutationFrom(target, type, attributeName)) {
            auto deliveryOptions = registration->deliveryOptions();
            auto result = observers.add(registration->observer(), deliveryOptions);
            if (!result.isNewEntry)
                result.iterator->value |= deliveryOptions;
        }
    }
}

HashMap<Ref<MutationObserver>, MutationRecordDeliveryOptions> Node::registeredMutationObservers(MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    HashMap<Ref<MutationObserver>, MutationRecordDeliveryOptions> result;
    ASSERT((type == MutationObserver::Attributes && attributeName) || !attributeName);
    collectMatchingObserversForMutation(result, mutationObserverRegistry(), *this, type, attributeName);
    collectMatchingObserversForMutation(result, transientMutationObserverRegistry(), *this, type, attributeName);
    for (Node* node = parentNode(); node; node = node->parentNode()) {
        collectMatchingObserversForMutation(result, node->mutationObserverRegistry(), *this, type, attributeName);
        collectMatchingObserversForMutation(result, node->transientMutationObserverRegistry(), *this, type, attributeName);
    }
    return result;
}

void Node::registerMutationObserver(MutationObserver& observer, MutationObserverOptions options, const HashSet<AtomicString>& attributeFilter)
{
    MutationObserverRegistration* registration = nullptr;
    auto& registry = ensureRareData().ensureMutationObserverData().registry;

    for (auto& candidateRegistration : registry) {
        if (&candidateRegistration->observer() == &observer) {
            registration = candidateRegistration.get();
            registration->resetObservation(options, attributeFilter);
        }
    }

    if (!registration) {
        registry.append(std::make_unique<MutationObserverRegistration>(observer, *this, options, attributeFilter));
        registration = registry.last().get();
    }

    document().addMutationObserverTypes(registration->mutationTypes());
}

void Node::unregisterMutationObserver(MutationObserverRegistration& registration)
{
    auto* registry = mutationObserverRegistry();
    ASSERT(registry);
    if (!registry)
        return;

    registry->removeFirstMatching([&registration] (auto& current) {
        return current.get() == &registration;
    });
}

void Node::registerTransientMutationObserver(MutationObserverRegistration& registration)
{
    ensureRareData().ensureMutationObserverData().transientRegistry.add(&registration);
}

void Node::unregisterTransientMutationObserver(MutationObserverRegistration& registration)
{
    auto* transientRegistry = transientMutationObserverRegistry();
    ASSERT(transientRegistry);
    if (!transientRegistry)
        return;

    ASSERT(transientRegistry->contains(&registration));
    transientRegistry->remove(&registration);
}

void Node::notifyMutationObserversNodeWillDetach()
{
    if (!document().hasMutationObservers())
        return;

    for (Node* node = parentNode(); node; node = node->parentNode()) {
        if (auto* registry = node->mutationObserverRegistry()) {
            for (auto& registration : *registry)
                registration->observedSubtreeNodeWillDetach(*this);
        }
        if (auto* transientRegistry = node->transientMutationObserverRegistry()) {
            for (auto* registration : *transientRegistry)
                registration->observedSubtreeNodeWillDetach(*this);
        }
    }
}

void Node::handleLocalEvents(Event& event, EventInvokePhase phase)
{
    if (!hasEventTargetData())
        return;

    // FIXME: Should we deliver wheel events to disabled form controls or not?
    if (is<Element>(*this) && downcast<Element>(*this).isDisabledFormControl() && event.isMouseEvent() && !event.isWheelEvent())
        return;

    fireEventListeners(event, phase);
}

void Node::dispatchScopedEvent(Event& event)
{
    EventDispatcher::dispatchScopedEvent(*this, event);
}

void Node::dispatchEvent(Event& event)
{
    EventDispatcher::dispatchEvent(*this, event);
}

void Node::dispatchSubtreeModifiedEvent()
{
    if (isInShadowTree())
        return;

    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(*this));

    if (!document().hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return;
    const AtomicString& subtreeModifiedEventName = eventNames().DOMSubtreeModifiedEvent;
    if (!parentNode() && !hasEventListeners(subtreeModifiedEventName))
        return;

    dispatchScopedEvent(MutationEvent::create(subtreeModifiedEventName, Event::CanBubble::Yes));
}

void Node::dispatchDOMActivateEvent(Event& underlyingClickEvent)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    int detail = is<UIEvent>(underlyingClickEvent) ? downcast<UIEvent>(underlyingClickEvent).detail() : 0;
    auto event = UIEvent::create(eventNames().DOMActivateEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes, Event::IsComposed::Yes, document().windowProxy(), detail);
    event->setUnderlyingEvent(&underlyingClickEvent);
    dispatchScopedEvent(event);
    if (event->defaultHandled())
        underlyingClickEvent.setDefaultHandled();
}

bool Node::dispatchBeforeLoadEvent(const String& sourceURL)
{
    if (!document().hasListenerType(Document::BEFORELOAD_LISTENER))
        return true;

    Ref<Node> protectedThis(*this);
    auto event = BeforeLoadEvent::create(sourceURL);
    dispatchEvent(event);
    return !event->defaultPrevented();
}

void Node::dispatchInputEvent()
{
    dispatchScopedEvent(Event::create(eventNames().inputEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
}

void Node::defaultEventHandler(Event& event)
{
    if (event.target() != this)
        return;
    const AtomicString& eventType = event.type();
    if (eventType == eventNames().keydownEvent || eventType == eventNames().keypressEvent) {
        if (is<KeyboardEvent>(event)) {
            if (Frame* frame = document().frame())
                frame->eventHandler().defaultKeyboardEventHandler(downcast<KeyboardEvent>(event));
        }
    } else if (eventType == eventNames().clickEvent) {
        dispatchDOMActivateEvent(event);
#if ENABLE(CONTEXT_MENUS)
    } else if (eventType == eventNames().contextmenuEvent) {
        if (Frame* frame = document().frame())
            if (Page* page = frame->page())
                page->contextMenuController().handleContextMenuEvent(event);
#endif
    } else if (eventType == eventNames().textInputEvent) {
        if (is<TextEvent>(event)) {
            if (Frame* frame = document().frame())
                frame->eventHandler().defaultTextInputEventHandler(downcast<TextEvent>(event));
        }
#if ENABLE(PAN_SCROLLING)
    } else if (eventType == eventNames().mousedownEvent && is<MouseEvent>(event)) {
        if (downcast<MouseEvent>(event).button() == MiddleButton) {
            if (enclosingLinkEventParentOrSelf())
                return;

            RenderObject* renderer = this->renderer();
            while (renderer && (!is<RenderBox>(*renderer) || !downcast<RenderBox>(*renderer).canBeScrolledAndHasScrollableArea()))
                renderer = renderer->parent();

            if (renderer) {
                if (Frame* frame = document().frame())
                    frame->eventHandler().startPanScrolling(downcast<RenderBox>(*renderer));
            }
        }
#endif
    } else if (eventNames().isWheelEventType(eventType) && is<WheelEvent>(event)) {
        // If we don't have a renderer, send the wheel event to the first node we find with a renderer.
        // This is needed for <option> and <optgroup> elements so that <select>s get a wheel scroll.
        Node* startNode = this;
        while (startNode && !startNode->renderer())
            startNode = startNode->parentOrShadowHostNode();
        
        if (startNode && startNode->renderer())
            if (Frame* frame = document().frame())
                frame->eventHandler().defaultWheelEventHandler(startNode, downcast<WheelEvent>(event));
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
    } else if (is<TouchEvent>(event) && eventNames().isTouchEventType(eventType)) {
        RenderObject* renderer = this->renderer();
        while (renderer && (!is<RenderBox>(*renderer) || !downcast<RenderBox>(*renderer).canBeScrolledAndHasScrollableArea()))
            renderer = renderer->parent();

        if (renderer && renderer->node()) {
            if (Frame* frame = document().frame())
                frame->eventHandler().defaultTouchEventHandler(*renderer->node(), downcast<TouchEvent>(event));
        }
#endif
    }
}

bool Node::willRespondToMouseMoveEvents()
{
    // FIXME: Why is the iOS code path different from the non-iOS code path?
#if !PLATFORM(IOS_FAMILY)
    if (!is<Element>(*this))
        return false;
    if (downcast<Element>(*this).isDisabledFormControl())
        return false;
#endif
    return hasEventListeners(eventNames().mousemoveEvent) || hasEventListeners(eventNames().mouseoverEvent) || hasEventListeners(eventNames().mouseoutEvent);
}

bool Node::willRespondToMouseClickEvents()
{
    // FIXME: Why is the iOS code path different from the non-iOS code path?
#if PLATFORM(IOS_FAMILY)
    return isContentEditable() || hasEventListeners(eventNames().mouseupEvent) || hasEventListeners(eventNames().mousedownEvent) || hasEventListeners(eventNames().clickEvent);
#else
    if (!is<Element>(*this))
        return false;
    if (downcast<Element>(*this).isDisabledFormControl())
        return false;
    return computeEditability(UserSelectAllIsAlwaysNonEditable, ShouldUpdateStyle::Update) != Editability::ReadOnly
        || hasEventListeners(eventNames().mouseupEvent) || hasEventListeners(eventNames().mousedownEvent) || hasEventListeners(eventNames().clickEvent) || hasEventListeners(eventNames().DOMActivateEvent);
#endif
}

bool Node::willRespondToMouseWheelEvents()
{
    return hasEventListeners(eventNames().mousewheelEvent);
}

// It's important not to inline removedLastRef, because we don't want to inline the code to
// delete a Node at each deref call site.
void Node::removedLastRef()
{
    // An explicit check for Document here is better than a virtual function since it is
    // faster for non-Document nodes, and because the call to removedLastRef that is inlined
    // at all deref call sites is smaller if it's a non-virtual function.
    if (is<Document>(*this)) {
        downcast<Document>(*this).removedLastRef();
        return;
    }

#ifndef NDEBUG
    m_deletionHasBegun = true;
#endif
    delete this;
}

void Node::textRects(Vector<IntRect>& rects) const
{
    auto range = Range::create(document());
    range->selectNodeContents(const_cast<Node&>(*this));
    range->absoluteTextRects(rects);
}

unsigned Node::connectedSubframeCount() const
{
    return hasRareData() ? rareData()->connectedSubframeCount() : 0;
}

void Node::incrementConnectedSubframeCount(unsigned amount)
{
    ASSERT(isContainerNode());
    ensureRareData().incrementConnectedSubframeCount(amount);
}

void Node::decrementConnectedSubframeCount(unsigned amount)
{
    rareData()->decrementConnectedSubframeCount(amount);
}

void Node::updateAncestorConnectedSubframeCountForRemoval() const
{
    unsigned count = connectedSubframeCount();

    if (!count)
        return;

    for (Node* node = parentOrShadowHostNode(); node; node = node->parentOrShadowHostNode())
        node->decrementConnectedSubframeCount(count);
}

void Node::updateAncestorConnectedSubframeCountForInsertion() const
{
    unsigned count = connectedSubframeCount();

    if (!count)
        return;

    for (Node* node = parentOrShadowHostNode(); node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount(count);
}

bool Node::inRenderedDocument() const
{
    return isConnected() && document().hasLivingRenderTree();
}

void* Node::opaqueRootSlow() const
{
    const Node* node = this;
    for (;;) {
        const Node* nextNode = node->parentOrShadowHostNode();
        if (!nextNode)
            break;
        node = nextNode;
    }
    return const_cast<void*>(static_cast<const void*>(node));
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showTree(const WebCore::Node* node)
{
    if (node)
        node->showTreeForThis();
}

void showNodePath(const WebCore::Node* node)
{
    if (node)
        node->showNodePathForThis();
}

#endif // ENABLE(TREE_DEBUGGING)
