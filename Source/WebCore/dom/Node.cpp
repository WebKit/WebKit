/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#include "ChildListMutationScope.h"
#include "CommonAtomStrings.h"
#include "CommonVM.h"
#include "ComposedTreeAncestorIterator.h"
#include "ContainerNodeAlgorithms.h"
#include "ContainerNodeInlines.h"
#include "ContextMenuController.h"
#include "CustomElementRegistry.h"
#include "DataTransfer.h"
#include "DocumentInlines.h"
#include "DocumentQuirks.h"
#include "DocumentType.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "EventDispatcher.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "FrameInlines.h"
#include "GCReachableRef.h"
#include "HTMLAreaElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDialogElement.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLSlotElement.h"
#include "HTMLStyleElement.h"
#include "InputEvent.h"
#include "InspectorInstrumentation.h"
#include "KeyboardEvent.h"
#include "LiveNodeListInlines.h"
#include "LocalDOMWindow.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MouseEventTypes.h"
#include "MutationEvent.h"
#include "NodeName.h"
#include "NodeRareDataInlines.h"
#include "NodeRenderStyle.h"
#include "PageInspectorController.h"
#include "PointerEvent.h"
#include "ProcessingInstruction.h"
#include "ProgressEvent.h"
#include "Quirks.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderObjectInlines.h"
#include "RenderTextControl.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "SVGElementInlines.h"
#include "ScopedEventQueue.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "StorageEvent.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include "SubmitEvent.h"
#include "TemplateContentDocumentFragment.h"
#include "TextEvent.h"
#include "TextManipulationController.h"
#include "TouchEvent.h"
#include "TreeScopeInlines.h"
#include "WebCoreOpaqueRoot.h"
#include "WheelEvent.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include <JavaScriptCore/HeapInlines.h>
#include <wtf/HexNumber.h>
#include <wtf/SHA1.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

#if ENABLE(CONTENT_CHANGE_OBSERVER)
#include "ContentChangeObserver.h"
#endif

namespace WebCore {

WTF_MAKE_PREFERABLY_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL(Node);

using namespace HTMLNames;

struct SameSizeAsNode : EventTarget, CanMakeCheckedPtr<SameSizeAsNode> {
    WTF_MAKE_TZONE_ALLOCATED(SameSizeAsNode);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SameSizeAsNode);
public:
#if ASSERT_ENABLED
    bool inRemovedLastRefFunction;
    bool adoptionIsRequired;
    bool deletionHasBegun;
#endif
    uint32_t refCountAndParentBit;
    uint32_t nodeFlags;
    uint32_t stateFlags;
    void* parentNode;
    void* treeScope;
    void* previous;
    void* next;
    void* renderer;
    uint8_t rareDataWithBitfields[8];
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(SameSizeAsNode);

static_assert(sizeof(Node) == sizeof(SameSizeAsNode), "Node should stay small");

#if DUMP_NODE_STATISTICS
static WeakHashSet<Node>& liveNodeSet()
{
    static NeverDestroyed<WeakHashSet<Node>> liveNodes;
    return liveNodes;
}

static ASCIILiteral stringForRareDataUseType(NodeRareData::UseType useType)
{
    switch (useType) {
    case NodeRareData::UseType::TabIndex:
        return "TabIndex"_s;
    case NodeRareData::UseType::ChildIndex:
        return "ChildIndex"_s;
    case NodeRareData::UseType::NodeList:
        return "NodeList"_s;
    case NodeRareData::UseType::MutationObserver:
        return "MutationObserver"_s;
    case NodeRareData::UseType::ManuallyAssignedSlot:
        return "ManuallyAssignedSlot"_s;
    case NodeRareData::UseType::ScrollingPosition:
        return "ScrollingPosition"_s;
    case NodeRareData::UseType::ComputedStyle:
        return "ComputedStyle"_s;
    case NodeRareData::UseType::DisplayContentsOrNoneStyle:
        return "DisplayContentsOrNoneStyle"_s;
    case NodeRareData::UseType::EffectiveLang:
        return "EffectiveLang"_s;
    case NodeRareData::UseType::Dataset:
        return "Dataset"_s;
    case NodeRareData::UseType::ClassList:
        return "ClassList"_s;
    case NodeRareData::UseType::ShadowRoot:
        return "ShadowRoot"_s;
    case NodeRareData::UseType::CustomElementReactionQueue:
        return "CustomElementReactionQueue"_s;
    case NodeRareData::UseType::CustomElementDefaultARIA:
        return "CustomElementDefaultARIA"_s;
    case NodeRareData::UseType::FormAssociatedCustomElement:
        return "FormAssociatedCustomElement"_s;
    case NodeRareData::UseType::AttributeMap:
        return "AttributeMap"_s;
    case NodeRareData::UseType::InteractionObserver:
        return "InteractionObserver"_s;
    case NodeRareData::UseType::ResizeObserver:
        return "ResizeObserver"_s;
    case NodeRareData::UseType::Animations:
        return "Animations"_s;
    case NodeRareData::UseType::PseudoElements:
        return "PseudoElements"_s;
    case NodeRareData::UseType::AttributeStyleMap:
        return "AttributeStyleMap"_s;
    case NodeRareData::UseType::ComputedStyleMap:
        return "ComputedStyleMap"_s;
    case NodeRareData::UseType::PartList:
        return "PartList"_s;
    case NodeRareData::UseType::PartNames:
        return "PartNames"_s;
    case NodeRareData::UseType::Nonce:
        return "Nonce"_s;
    case NodeRareData::UseType::ExplicitlySetAttrElementsMap:
        return "ExplicitlySetAttrElementsMap"_s;
    case NodeRareData::UseType::Popover:
        return "Popover"_s;
    case NodeRareData::UseType::UserInfo:
        return "UserInfo"_s;
    }
    return { };
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

    HashMap<uint32_t, size_t> rareDataSingleUseTypeCounts;
    size_t mixedRareDataUseCount = 0;

    for (auto& node : liveNodeSet()) {
        if (node.hasRareData()) {
            ++nodesWithRareData;
            if (auto* element = dynamicDowncast<Element>(node)) {
                ++elementsWithRareData;
                if (element->hasNamedNodeMap())
                    ++elementsWithNamedNodeMap;
            }
            auto* rareData = node.rareData();
            auto useTypes = is<Element>(node) ? downcast<ElementRareData>(rareData)->useTypes() : rareData->useTypes();
            unsigned useTypeCount = 0;
            for (auto type : useTypes) {
                UNUSED_PARAM(type);
                useTypeCount++;
            }
            if (useTypeCount == 1) {
                auto result = rareDataSingleUseTypeCounts.add(enumToUnderlyingType(*useTypes.begin()), 0);
                result.iterator->value++;
            } else
                mixedRareDataUseCount++;
        }

        switch (node.nodeType()) {
            case ELEMENT_NODE: {
                ++elementNodes;

                // Tag stats
                Element& element = uncheckedDowncast<Element>(node);
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
                if (node.isShadowRoot())
                    ++shadowRootNodes;
                else
                    ++fragmentNodes;
                break;
            }
        }
    }

    printf("Number of Nodes: %d\n\n", liveNodeSet().computeSize());
    printf("Number of Nodes with RareData: %zu\n", nodesWithRareData);
    printf("  Mixed use: %zu\n", mixedRareDataUseCount);
    for (auto it : rareDataSingleUseTypeCounts)
        SAFE_PRINTF("  %s: %zu\n", stringForRareDataUseType(static_cast<NodeRareData::UseType>(it.key)), it.value);
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
        SAFE_PRINTF("  Number of <%s> tags: %zu\n", stringSizePair.key.utf8(), stringSizePair.value);

    printf("Attributes:\n");
    printf("  Number of Attributes (non-Node and Node): %zu [%zu]\n", attributes, sizeof(Attribute));
    printf("  Number of Attributes with an Attr: %zu\n", attributesWithAttr);
    printf("  Number of Elements with attribute storage: %zu [%zu]\n", elementsWithAttributeStorage, sizeof(ElementData));
    printf("  Number of Elements with RareData: %zu\n", elementsWithRareData);
    printf("  Number of Elements with NamedNodeMap: %zu [%zu]\n", elementsWithNamedNodeMap, sizeof(NamedNodeMap));

#endif // DUMP_NODE_STATISTICS
}

void Node::trackForDebugging()
{
#if DUMP_NODE_STATISTICS
    liveNodeSet().add(*this);
#endif
}

inline void NodeRareData::operator delete(NodeRareData* nodeRareData, std::destroying_delete_t)
{
    auto destroyAndFree = [&]<typename RareDataType> (RareDataType& value) {
        std::destroy_at(&value);
        RareDataType::freeAfterDestruction(&value);
    };

    if (auto* elementRareData = dynamicDowncast<ElementRareData>(*nodeRareData))
        destroyAndFree(*elementRareData);
    else
        destroyAndFree(*nodeRareData);
}

Node::Node(Document& document, NodeType type, OptionSet<TypeFlag> flags)
    : EventTarget(ConstructNode)
    , m_typeBitFields(constructBitFieldsFromNodeTypeAndFlags(type, flags))
    , m_treeScope((isDocumentNode() || isShadowRoot()) ? nullptr : &document)
{
    ASSERT(nodeType() == type);
    ASSERT(isMainThread());

    // Allow code to ref the Document while it is being constructed to make our life easier.
    if (isDocumentNode())
        relaxAdoptionRequirement();
    else
        document.incrementReferencingNodeCount();

#if !defined(NDEBUG) || DUMP_NODE_STATISTICS
    trackForDebugging();
#endif
}

static HashMap<WeakRef<Node, WeakPtrImplWithEventTargetData>, NodeIdentifier>& nodeIdentifiersMap()
{
    static MainThreadNeverDestroyed<HashMap<WeakRef<Node, WeakPtrImplWithEventTargetData>, NodeIdentifier>> map;
    return map;
}

Node::~Node()
{
    ASSERT(isMainThread());
    ASSERT(deletionHasBegun());
    ASSERT(!m_adoptionIsRequired);

    InspectorInstrumentation::willDestroyDOMNode(*this);

    if (hasStateFlag(StateFlag::HasNodeIdentifier)) [[unlikely]]
        nodeIdentifiersMap().remove(*this);
    else
        ASSERT(!nodeIdentifiersMap().contains(*this));

#if DUMP_NODE_STATISTICS
    liveNodeSet().remove(*this);
#endif

    ASSERT(!renderer());
    ASSERT(!parentNode());
    ASSERT(!m_previousSibling);
    ASSERT(!m_next);

    {
        // Not refing document because it may be in the middle of destruction.
        auto& document = this->document(); // Store document before clearing out m_treeScope.

        // The call to decrementReferencingNodeCount() below may destroy the document so we need to clear our
        // m_treeScope CheckedPtr beforehand.
        m_treeScope = nullptr;

        if (!isDocumentNode())
            document.decrementReferencingNodeCount(); // This may destroy the document.
    }

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY) && ASSERT_ENABLED
    for (auto& document : Document::allDocuments()) {
        ASSERT(!document->touchEventListenersContain(*this));
        ASSERT(!document->touchEventHandlersContain(*this));
        ASSERT(!document->touchEventTargetsContain(*this));
    }
#endif

#if ASSERT_ENABLED
    if (m_refCountAndParentBit != s_refCountIncrement)
        WTF::RefCountedBase::printRefDuringDestructionLogAndCrash(this);
#endif
    RELEASE_ASSERT(m_refCountAndParentBit == s_refCountIncrement);
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

    if (CheckedPtr cache = document.existingAXObjectCache())
        cache->remove(*this);
}

void Node::materializeRareData()
{
    if (is<Element>(*this))
        m_rareDataWithBitfields.setPointer(makeUnique<ElementRareData>());
    else
        m_rareDataWithBitfields.setPointer(makeUnique<NodeRareData>());
}

void Node::clearRareData()
{
    ASSERT(hasRareData());
    ASSERT(!transientMutationObserverRegistry() || transientMutationObserverRegistry()->isEmptyIgnoringNullReferences());

    m_rareDataWithBitfields.setPointer(nullptr);
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
    if (auto* containerNode = dynamicDowncast<ContainerNode>(*this))
        return ensureRareData().ensureNodeLists().ensureChildNodeList(*containerNode);
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

ExceptionOr<void> Node::insertBefore(Node& newChild, RefPtr<Node>&& refChild)
{
    if (auto* containerNode = dynamicDowncast<ContainerNode>(*this))
        return containerNode->insertBefore(newChild, WTFMove(refChild));
    return Exception { ExceptionCode::HierarchyRequestError };
}

ExceptionOr<void> Node::replaceChild(Node& newChild, Node& oldChild)
{
    if (auto* containerNode = dynamicDowncast<ContainerNode>(*this))
        return containerNode->replaceChild(newChild, oldChild);
    return Exception { ExceptionCode::HierarchyRequestError };
}

ExceptionOr<void> Node::removeChild(Node& oldChild)
{
    if (auto* containerNode = dynamicDowncast<ContainerNode>(*this))
        return containerNode->removeChild(oldChild);
    return Exception { ExceptionCode::NotFoundError };
}

ExceptionOr<void> Node::appendChild(Node& newChild)
{
    if (auto* containerNode = dynamicDowncast<ContainerNode>(*this))
        return containerNode->appendChild(newChild);
    return Exception { ExceptionCode::HierarchyRequestError };
}

static HashSet<RefPtr<Node>> nodeSetPreTransformedFromNodeOrStringVector(const FixedVector<NodeOrString>& vector)
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

// https://dom.spec.whatwg.org/#converting-nodes-into-a-node
ExceptionOr<RefPtr<Node>> Node::convertNodesOrStringsIntoNode(FixedVector<NodeOrString>&& nodeOrStringVector)
{
    if (nodeOrStringVector.isEmpty())
        return nullptr;

    Ref document = this->document();
    auto nodes = WTF::map(WTFMove(nodeOrStringVector), [&](auto&& variant) -> Ref<Node> {
        return WTF::switchOn(WTFMove(variant),
            [&](RefPtr<Node>&& node) { return node.releaseNonNull(); },
            [&](String&& string) -> Ref<Node> { return Text::create(document, WTFMove(string)); }
        );
    });

    if (nodes.size() == 1)
        return RefPtr<Node> { WTFMove(nodes.first()) };

    auto nodeToReturn = DocumentFragment::create(document);
    for (auto& node : nodes) {
        auto appendResult = nodeToReturn->appendChild(node);
        if (appendResult.hasException())
            return appendResult.releaseException();
    }
    return RefPtr<Node> { WTFMove(nodeToReturn) };
}

// https://dom.spec.whatwg.org/#converting-nodes-into-a-node except this returns a NodeVector
ExceptionOr<NodeVector> Node::convertNodesOrStringsIntoNodeVector(FixedVector<NodeOrString>&& nodeOrStringVector)
{
    if (nodeOrStringVector.isEmpty())
        return NodeVector { };

    Ref document = this->document();
    NodeVector nodeVector;
    nodeVector.reserveInitialCapacity(nodeOrStringVector.size());
    for (auto& variant : nodeOrStringVector) {
        if (std::holds_alternative<String>(variant)) {
            nodeVector.append(Text::create(document, WTFMove(std::get<String>(variant))));
            continue;
        }

        ASSERT(std::holds_alternative<RefPtr<Node>>(variant));
        RefPtr node = WTFMove(std::get<RefPtr<Node>>(variant));
        ASSERT(node);
        if (auto* fragment = dynamicDowncast<DocumentFragment>(node.get()); fragment) [[unlikely]] {
            for (auto* child = fragment->firstChild(); child; child = child->nextSibling())
                nodeVector.append(*child);
        } else
            nodeVector.append(node.releaseNonNull());
    }

    if (nodeVector.size() == 1)
        return nodeVector; // step 3, if nodes contains one node, then set node to nodes[0].

    for (auto& node : nodeVector) {
        auto result = node->remove();
        if (result.hasException())
            return result.releaseException();
    }

    return nodeVector;
}

ExceptionOr<void> Node::before(FixedVector<NodeOrString>&& nodeOrStringVector)
{
    RefPtr parent = parentNode();
    if (!parent)
        return { };

    auto nodeSet = nodeSetPreTransformedFromNodeOrStringVector(nodeOrStringVector);
    RefPtr viablePreviousSibling = firstPrecedingSiblingNotInNodeSet(*this, nodeSet);

    auto result = convertNodesOrStringsIntoNodeVector(WTFMove(nodeOrStringVector));
    if (result.hasException())
        return result.releaseException();

    auto newChildren = result.releaseReturnValue();
    if (auto checkResult = parent->ensurePreInsertionValidityForPhantomDocumentFragment(newChildren); checkResult.hasException())
        return checkResult;

    RefPtr viableNextSibling = viablePreviousSibling ? viablePreviousSibling->nextSibling() : parent->firstChild();
    return parent->insertChildrenBeforeWithoutPreInsertionValidityCheck(WTFMove(newChildren), viableNextSibling.get());
}

ExceptionOr<void> Node::after(FixedVector<NodeOrString>&& nodeOrStringVector)
{
    RefPtr parent = parentNode();
    if (!parent)
        return { };

    auto nodeSet = nodeSetPreTransformedFromNodeOrStringVector(nodeOrStringVector);
    RefPtr viableNextSibling = firstFollowingSiblingNotInNodeSet(*this, nodeSet);

    auto result = convertNodesOrStringsIntoNodeVector(WTFMove(nodeOrStringVector));
    if (result.hasException())
        return result.releaseException();

    auto newChildren = result.releaseReturnValue();
    if (auto checkResult = parent->ensurePreInsertionValidityForPhantomDocumentFragment(newChildren); checkResult.hasException())
        return checkResult;

    return parent->insertChildrenBeforeWithoutPreInsertionValidityCheck(WTFMove(newChildren), viableNextSibling.get());
}

ExceptionOr<void> Node::replaceWith(FixedVector<NodeOrString>&& nodeOrStringVector)
{
    RefPtr parent = parentNode();
    if (!parent)
        return { };

    auto nodeSet = nodeSetPreTransformedFromNodeOrStringVector(nodeOrStringVector);
    RefPtr viableNextSibling = firstFollowingSiblingNotInNodeSet(*this, nodeSet);

    auto result = convertNodesOrStringsIntoNode(WTFMove(nodeOrStringVector));
    if (result.hasException())
        return result.releaseException();

    if (parentNode() == parent) {
        if (RefPtr node = result.releaseReturnValue())
            return parent->replaceChild(*node, *this);
        return parent->removeChild(*this);
    }

    if (RefPtr node = result.releaseReturnValue())
        return parent->insertBefore(*node, WTFMove(viableNextSibling));
    return { };
}

ExceptionOr<void> Node::remove()
{
    RefPtr parent = parentNode();
    if (!parent)
        return { };
    return parent->removeChild(*this);
}

ExceptionOr<void> Node::normalize()
{
    // Go through the subtree beneath us, normalizing all nodes. This means that
    // any two adjacent text nodes are merged and any empty text nodes are removed.

    Ref document = this->document();
    RefPtr node = this;
    while (RefPtr firstChild = node->firstChild())
        node = WTFMove(firstChild);
    while (node) {
        if (auto* element = dynamicDowncast<Element>(*node))
            element->normalizeAttributes();

        if (node == this)
            break;

        if (node->nodeType() != TEXT_NODE) {
            node = NodeTraversal::nextPostOrder(*node);
            continue;
        }

        Ref text = uncheckedDowncast<Text>(*node);

        // Remove empty text nodes.
        if (!text->length()) {
            // Care must be taken to get the next node before removing the current node.
            node = NodeTraversal::nextPostOrder(*node);
            text->remove();
            continue;
        }

        // Merge text nodes.
        while (RefPtr nextSibling = node->nextSibling()) {
            if (nextSibling->nodeType() != TEXT_NODE)
                break;
            Ref nextText = uncheckedDowncast<Text>(nextSibling.releaseNonNull());

            // Remove empty text nodes.
            if (!nextText->length()) {
                nextText->remove();
                continue;
            }

            // Both non-empty text nodes. Merge them.
            unsigned offset = text->length();

            if (nextText->length() > StringImpl::MaxLength - offset) {
                return Exception { ExceptionCode::InvalidModificationError,
                    "Normalized Node String representation exceeds implementation maximum length."_s };
            }

            // Update start/end for any affected Ranges before appendData since modifying contents might trigger mutation events that modify ordering.
            document->textNodesMerged(nextText, offset);

            // FIXME: DOM spec requires contents to be replaced all at once (see https://dom.spec.whatwg.org/#dom-node-normalize).
            // Appending once per sibling may trigger mutation events too many times.
            text->appendData(nextText->data());            
            nextText->remove();
        }

        node = NodeTraversal::nextPostOrder(*node);
    }

    return { };
}

Ref<Node> Node::cloneNode(bool deep) const
{
    ASSERT(!isShadowRoot());
    RefPtr registry = CustomElementRegistry::registryForNodeOrTreeScope(*this, treeScope());
    return cloneNodeInternal(document(), deep ? CloningOperation::Everything : CloningOperation::SelfOnly, registry.get());
}

ExceptionOr<Ref<Node>> Node::cloneNodeForBindings(bool deep) const
{
    if (isShadowRoot()) [[unlikely]]
        return Exception { ExceptionCode::NotSupportedError };
    return cloneNode(deep);
}

const AtomString& Node::prefix() const
{
    // For nodes other than elements and attributes, the prefix is always null
    return nullAtom();
}

ExceptionOr<void> Node::setPrefix(const AtomString&)
{
    // The spec says that for nodes other than elements and attributes, prefix is always null.
    // It does not say what to do when the user tries to set the prefix on another type of
    // node, however Mozilla throws a NamespaceError exception.
    return Exception { ExceptionCode::NamespaceError };
}

const AtomString& Node::localName() const
{
    return nullAtom();
}

const AtomString& Node::namespaceURI() const
{
    return nullAtom();
}

bool Node::isContentEditable() const
{
    return computeEditability(UserSelectAllTreatment::Editable, ShouldUpdateStyle::Update) != Editability::ReadOnly;
}

bool Node::isContentRichlyEditable() const
{
    return computeEditability(UserSelectAllTreatment::NotEditable, ShouldUpdateStyle::Update) == Editability::CanEditRichly;
}

void Node::inspect()
{
    if (RefPtr page = document().page())
        page->inspectorController().inspect(this);
}

static Node::Editability computeEditabilityFromComputedStyle(const RenderStyle& style, Node::UserSelectAllTreatment treatment, PageIsEditable pageIsEditable)
{
    // Ideally we'd call ASSERT(!needsStyleRecalc()) here, but
    // ContainerNode::setFocus() calls invalidateStyleForSubtree(), so the assertion
    // would fire in the middle of Document::setFocusedElement().

    // Elements with user-select: all style are considered atomic
    // therefore non editable.
    if (treatment == Node::UserSelectAllTreatment::NotEditable && style.usedUserSelect() == UserSelect::All)
        return Node::Editability::ReadOnly;

    if (pageIsEditable == PageIsEditable::Yes)
        return Node::Editability::CanEditRichly;

    switch (style.usedUserModify()) {
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

Node::Editability Node::computeEditabilityWithStyle(const RenderStyle* incomingStyle, UserSelectAllTreatment treatment, ShouldUpdateStyle shouldUpdateStyle) const
{
    if (!document().hasLivingRenderTree() || isPseudoElement())
        return Editability::ReadOnly;

    Ref document = this->document();
    auto pageIsEditable = document->page() && document->page()->isEditable() ? PageIsEditable::Yes : PageIsEditable::No;

    if (isInShadowTree())
        return HTMLElement::editabilityFromContentEditableAttr(*this, pageIsEditable);

    if (shouldUpdateStyle == ShouldUpdateStyle::Update && document->needsStyleRecalc()) {
        if (!document->usesStyleBasedEditability())
            return HTMLElement::editabilityFromContentEditableAttr(*this, pageIsEditable);
        document->updateStyleIfNeeded();
    }

    auto* style = [&] {
        if (incomingStyle)
            return incomingStyle;
        if (isDocumentNode())
            return renderStyle();
        RefPtr element = dynamicDowncast<Element>(*this);
        if (!element)
            element = parentElementInComposedTree();
        return element ? const_cast<Element&>(*element).computedStyleForEditability() : nullptr;
    }();

    if (!style)
        return Editability::ReadOnly;

    return computeEditabilityFromComputedStyle(*style, treatment, pageIsEditable);
}

Node::Editability Node::computeEditability(UserSelectAllTreatment treatment, ShouldUpdateStyle shouldUpdateStyle) const
{
    return computeEditabilityWithStyle(nullptr, treatment, shouldUpdateStyle);
}

LayoutRect Node::absoluteBoundingRect(bool* isReplaced)
{
    RenderObject* hitRenderer = this->renderer();
    if (!hitRenderer) {
        if (auto* area = dynamicDowncast<HTMLAreaElement>(*this)) {
            if (RefPtr imageElement = area->imageElement())
                hitRenderer = imageElement->renderer();
        }
    }
    RenderObject* renderer = hitRenderer;
    while (renderer && !renderer->isBody() && !renderer->isDocumentElementRenderer()) {
        if (renderer->isRenderBlock() || renderer->isNonReplacedAtomicInlineLevelBox() || renderer->isBlockLevelReplacedOrAtomicInline()) {
            // FIXME: Is this really what callers want for the "isReplaced" flag?
            *isReplaced = renderer->isBlockLevelReplacedOrAtomicInline();
            return renderer->absoluteBoundingBoxRect();
        }
        renderer = renderer->parent();
    }
    *isReplaced = false;
    return { };
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
        auto bitfields = styleBitfields();
        bitfields.setStyleValidity(validity);
        setStyleBitfields(bitfields);
    }

    switch (mode) {
    case Style::InvalidationMode::Normal:
        break;
    case Style::InvalidationMode::RecompositeLayer:
        setStateFlag(StateFlag::StyleResolutionShouldRecompositeLayer);
        break;
    case Style::InvalidationMode::RebuildRenderer:
    case Style::InvalidationMode::InsertedIntoAncestor:
        setStateFlag(StateFlag::HasInvalidRenderer);
        break;
    };
}

void Node::updateAncestorsForStyleRecalc()
{
    markAncestorsForInvalidatedStyle();

    auto* documentElement = document().documentElement();
    if (!documentElement)
        return;
    if (!documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return;

    Ref document = this->document();
    document->setChildNeedsStyleRecalc();
    document->scheduleStyleRecalc();
}

void Node::markAncestorsForInvalidatedStyle()
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
}

void Node::invalidateStyle(Style::Validity validity, Style::InvalidationMode mode)
{
    if (!inRenderedDocument())
        return;

    // FIXME: This should eventually be an ASSERT.
    if (document().inRenderTreeUpdate())
        return;

    if (validity != Style::Validity::Valid)
        setStateFlag(StateFlag::IsComputedStyleInvalidFlag);

    bool markAncestors = styleValidity() == Style::Validity::Valid || mode == Style::InvalidationMode::InsertedIntoAncestor;

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
bool shouldInvalidateNodeListCachesForAttr(std::span<const unsigned, numNodeListInvalidationTypes> nodeListCounts, const QualifiedName& attrName)
{
    if constexpr (type >= numNodeListInvalidationTypes)
        return false;
    else {
        if (nodeListCounts[type] && shouldInvalidateTypeOnAttributeChange(static_cast<NodeListInvalidationType>(type), attrName))
            return true;
        return shouldInvalidateNodeListCachesForAttr<type + 1>(nodeListCounts, attrName);
    }
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
    return shouldInvalidateNodeListCachesForAttr<enumToUnderlyingType(NodeListInvalidationType::DoNotInvalidateOnAttributeChanges) + 1>(m_nodeListAndCollectionCounts, attrName);
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

    document().invalidateQuerySelectorAllResults(*this);

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

void Node::invalidateNodeListCollectionAndInnerHTMLPrefixCachesInAncestorsForAttribute(const QualifiedName& attrName, const IsMutationBySetInnerHTML isMutationBySetInnerHTML)
{
    ASSERT(is<Element>(*this));

    bool shouldInvalidate = document().shouldInvalidateNodeListAndCollectionCachesForAttribute(attrName);

    WeakPtr cachedContainer = document().cachedSetInnerHTML().cachedContainer;
    bool shouldSetMutationBit = isMutationBySetInnerHTML == IsMutationBySetInnerHTML::No && cachedContainer;

    if (!shouldInvalidate && !shouldSetMutationBit)
        return;

    if (shouldInvalidate) {
        document().invalidateNodeListAndCollectionCaches([&attrName](auto& list) {
            list.invalidateCacheForAttribute(attrName);
        });
    }

    for (auto* node = this; node; node = node->parentNode()) {
        if (shouldSetMutationBit && !node->hasDidMutateSubtreeAfterSetInnerHTML()) {
            node->setDidMutateSubtreeAfterSetInnerHTML();
            if (node == cachedContainer.get())
                shouldSetMutationBit = false;
        }

        if (!shouldInvalidate || !node->hasRareData())
            continue;

        if (auto* lists = node->rareData()->nodeLists())
            lists->invalidateCachesForAttribute(attrName);
    }
}

void Node::setDidMutateSubtreeAfterSetInnerHTMLOnAncestors()
{
    WeakPtr cachedContainer = document().cachedSetInnerHTML().cachedContainer;
    if (!cachedContainer)
        return;

    for (auto* node = this; node; node = node->parentNode()) {
        node->setDidMutateSubtreeAfterSetInnerHTML();
        if (node == cachedContainer.get())
            break;
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

ExceptionOr<void> Node::checkSetPrefix(const AtomString& prefix)
{
    // Perform error checking as required by spec for setting Node.prefix. Used by
    // Element::setPrefix() and Attr::setPrefix()

    if (!prefix.isEmpty() && !Document::isValidName(prefix))
        return Exception { ExceptionCode::InvalidCharacterError };

    // FIXME: Raise NamespaceError if prefix is malformed per the Namespaces in XML specification.

    auto& namespaceURI = this->namespaceURI();
    if (namespaceURI.isEmpty() && !prefix.isEmpty())
        return Exception { ExceptionCode::NamespaceError };
    if (prefix == xmlAtom() && namespaceURI != XMLNames::xmlNamespaceURI)
        return Exception { ExceptionCode::NamespaceError };

    // Attribute-specific checks are in Attr::setPrefix().

    return { };
}

// https://dom.spec.whatwg.org/#concept-tree-descendant
bool Node::isDescendantOf(const Node& other) const
{
    // Return true if other is an ancestor of this.
    if (other.isTreeScope())
        return &treeScope().rootNode() == &other && !isTreeScope() && isInTreeScope();
    if (!other.hasChildNodes() || isConnected() != other.isConnected())
        return false;
    for (auto ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        if (ancestor == &other)
            return true;
    }
    return false;
}

// https://dom.spec.whatwg.org/#concept-shadow-including-inclusive-ancestor
bool Node::isShadowIncludingDescendantOf(const Node& other) const
{
    if (isDescendantOf(other))
        return true;

    for (auto host = shadowHost(); host; host = host->shadowHost()) {
        if (other.contains(*host))
            return true;
    }

    return false;
}

bool Node::isComposedTreeDescendantOf(const Node& node) const
{
    for (CheckedPtr currentAncestor = parentElementInComposedTree(); currentAncestor; currentAncestor = currentAncestor->parentElementInComposedTree()) {
        if (currentAncestor.get() == &node)
            return true;
    }
    return false;
}

Node* Node::pseudoAwarePreviousSibling() const
{
    auto* pseudoElement = dynamicDowncast<PseudoElement>(*this);
    Element* parentOrHost = pseudoElement ? pseudoElement->hostElement() : parentElement();
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
    auto* pseudoElement = dynamicDowncast<PseudoElement>(*this);
    Element* parentOrHost = pseudoElement ? pseudoElement->hostElement() : parentElement();
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
    if (auto* currentElement = dynamicDowncast<Element>(*this)) {
        Node* first = currentElement->beforePseudoElement();
        if (first)
            return first;
        first = currentElement->firstChild();
        if (!first)
            first = currentElement->afterPseudoElement();
        return first;
    }
    return firstChild();
}

Node* Node::pseudoAwareLastChild() const
{
    if (auto* currentElement = dynamicDowncast<Element>(*this)) {
        Node* last = currentElement->afterPseudoElement();
        if (last)
            return last;
        last = currentElement->lastChild();
        if (!last)
            last = currentElement->beforePseudoElement();
        return last;
    }
    return lastChild();
}

const RenderStyle* Node::computedStyle()
{
    return computedStyle(std::nullopt);
}

const RenderStyle* Node::computedStyle(const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    RefPtr composedParent = parentElementInComposedTree();
    return composedParent ? composedParent->computedStyle(pseudoElementIdentifier) : nullptr;
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
        if (style.userDrag() == UserDrag::Element && style.usedUserSelect() == UserSelect::None)
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

RefPtr<Element> Node::protectedShadowHost() const
{
    return shadowHost();
}

ShadowRoot* Node::containingShadowRoot() const
{
    return dynamicDowncast<ShadowRoot>(treeScope().rootNode());
}

RefPtr<ShadowRoot> Node::protectedContainingShadowRoot() const
{
    return containingShadowRoot();
}

#if ASSERT_ENABLED
// https://dom.spec.whatwg.org/#concept-closed-shadow-hidden
static bool isClosedShadowHiddenUsingSpecDefinition(const Node& A, const Node& B)
{
    return A.isInShadowTree()
        && !A.rootNode().isShadowIncludingInclusiveAncestorOf(&B)
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
        auto* shadowRoot = dynamicDowncast<ShadowRoot>(treeScopeThatCanAccessOtherNode->rootNode());
        if (shadowRoot && shadowRoot->mode() != ShadowRootMode::Open)
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

HTMLSlotElement* Node::manuallyAssignedSlot() const
{
    if (!hasRareData())
        return nullptr;
    return rareData()->manuallyAssignedSlot();
}

void Node::setManuallyAssignedSlot(HTMLSlotElement* slotElement)
{
    if (RefPtr element = dynamicDowncast<Element>(*this))
        RenderTreeUpdater::tearDownRenderers(*element);
    else if (RefPtr text = dynamicDowncast<Text>(*this))
        RenderTreeUpdater::tearDownRenderer(*text);

    ensureRareData().setManuallyAssignedSlot(slotElement);

    InspectorInstrumentation::didChangeAssignedSlot(*this);
}

bool Node::hasEverPaintedImages() const
{
    return hasRareData() && rareData()->hasEverPaintedImages();
}

void Node::setHasEverPaintedImages(bool hasEverPaintedImages)
{
    ensureRareData().setHasEverPaintedImages(hasEverPaintedImages);
}

ContainerNode* Node::parentInComposedTree() const
{
    ASSERT(isMainThreadOrGCThread());
    if (auto* slot = assignedSlot())
        return slot;
    if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(*this))
        return shadowRoot->host();
    return parentNode();
}

Element* Node::parentElementInComposedTree() const
{
    if (auto* slot = assignedSlot())
        return slot;
    if (auto* pseudoElement = dynamicDowncast<PseudoElement>(*this))
        return pseudoElement->hostElement();
    if (auto* parent = parentNode()) {
        if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(*parent))
            return shadowRoot->host();
        if (auto* element = dynamicDowncast<Element>(*parent))
            return element;
    }
    return nullptr;
}

TreeScope& Node::treeScopeForSVGReferences() const
{
    if (auto* shadowRoot = containingShadowRoot(); shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent) {
        if (shadowRoot->host() && shadowRoot->host()->elementName() == ElementNames::SVG::use) {
            ASSERT(treeScope().parentTreeScope());
            return *treeScope().parentTreeScope();
        }
    }
    return treeScope();
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
    auto* parent = parentOrShadowHostNode();
    if (!parent)
        return nullptr;

    if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(*parent))
        return shadowRoot->host();

    return dynamicDowncast<Element>(*parent);
}

Node& Node::traverseToRootNode() const
{
    return traverseToRootNodeInternal(*this);
}

// https://dom.spec.whatwg.org/#concept-shadow-including-root
Node& Node::shadowIncludingRoot() const
{
    auto& root = this->rootNode();
    if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(root)) {
        auto* host = shadowRoot->host();
        return host ? host->shadowIncludingRoot() : root;
    }
    return root;
}

Node& Node::getRootNode(const GetRootNodeOptions& options) const
{
    return options.composed ? shadowIncludingRoot() : rootNode();
}

void Node::queueTaskKeepingThisNodeAlive(TaskSource source, Function<void ()>&& task)
{
    document().eventLoop().queueTask(source, [protectedThis = GCReachableRef(*this), task = WTFMove(task)] () {
        task();
    });
}

void Node::queueTaskToDispatchEvent(TaskSource source, Ref<Event>&& event)
{
    queueTaskKeepingThisNodeAlive(source, [protectedThis = Ref { *this }, event = WTFMove(event)]() {
        protectedThis->dispatchEvent(event);
    });
}

Node::InsertedIntoAncestorResult Node::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    ASSERT(!containsSelectionEndPoint());
    if (insertionType.connectedToDocument)
        setEventTargetFlag(EventTargetFlag::IsConnected);
    if (parentOfInsertedTree.isInShadowTree())
        setEventTargetFlag(EventTargetFlag::IsInShadowTree);

    invalidateStyle(Style::Validity::SubtreeInvalid, Style::InvalidationMode::InsertedIntoAncestor);

    return InsertedIntoAncestorResult::Done;
}

void Node::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    ASSERT(!containsSelectionEndPoint());
    if (removalType.disconnectedFromDocument)
        clearEventTargetFlag(EventTargetFlag::IsConnected);
    if (isInShadowTree() && !treeScope().rootNode().isShadowRoot())
        clearEventTargetFlag(EventTargetFlag::IsInShadowTree);
    if (removalType.disconnectedFromDocument) {
        if (CheckedPtr cache = oldParentOfRemovedTree.document().existingAXObjectCache())
            cache->remove(*this);
    }
}

bool Node::isRootEditableElement() const
{
    return hasEditableStyle() && isElementNode() && (!parentNode() || !parentNode()->hasEditableStyle()
        || !parentNode()->isElementNode() || document().body() == this);
}

Element* Node::rootEditableElement() const
{
    Element* result = nullptr;
    for (Node* node = const_cast<Node*>(this); node && node->hasEditableStyle(); node = node->parentNode()) {
        if (auto* element = dynamicDowncast<Element>(*node))
            result = element;
        if (document().body() == node)
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
    return url.isNull() ? aboutBlankURL() : url;
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
        auto& thisDocType = uncheckedDowncast<DocumentType>(*this);
        auto& otherDocType = uncheckedDowncast<DocumentType>(*other);
        if (thisDocType.name() != otherDocType.name())
            return false;
        if (thisDocType.publicId() != otherDocType.publicId())
            return false;
        if (thisDocType.systemId() != otherDocType.systemId())
            return false;
        break;
        }
    case Node::ELEMENT_NODE: {
        auto& thisElement = uncheckedDowncast<Element>(*this);
        auto& otherElement = uncheckedDowncast<Element>(*other);
        if (thisElement.tagQName() != otherElement.tagQName())
            return false;
        if (!thisElement.hasEquivalentAttributes(otherElement))
            return false;
        break;
        }
    case Node::PROCESSING_INSTRUCTION_NODE: {
        auto& thisProcessingInstruction = uncheckedDowncast<ProcessingInstruction>(*this);
        auto& otherProcessingInstruction = uncheckedDowncast<ProcessingInstruction>(*other);
        if (thisProcessingInstruction.target() != otherProcessingInstruction.target())
            return false;
        if (thisProcessingInstruction.data() != otherProcessingInstruction.data())
            return false;
        break;
        }
    case Node::CDATA_SECTION_NODE:
    case Node::TEXT_NODE:
    case Node::COMMENT_NODE: {
        auto& thisCharacterData = uncheckedDowncast<CharacterData>(*this);
        auto& otherCharacterData = uncheckedDowncast<CharacterData>(*other);
        if (thisCharacterData.data() != otherCharacterData.data())
            return false;
        break;
        }
    case Node::ATTRIBUTE_NODE: {
        auto& thisAttribute = uncheckedDowncast<Attr>(*this);
        auto& otherAttribute = uncheckedDowncast<Attr>(*other);
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
static const AtomString& locateDefaultNamespace(const Node& node, const AtomString& prefix)
{
    switch (node.nodeType()) {
    case Node::ELEMENT_NODE: {
        if (prefix == xmlAtom())
            return XMLNames::xmlNamespaceURI.get();
        if (prefix == xmlnsAtom())
            return XMLNSNames::xmlnsNamespaceURI.get();

        auto& element = uncheckedDowncast<Element>(node);
        auto& namespaceURI = element.namespaceURI();
        if (!namespaceURI.isNull() && element.prefix() == prefix)
            return namespaceURI;

        if (element.hasAttributes()) {
            for (auto& attribute : element.attributes()) {
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
        if (auto* documentElement = uncheckedDowncast<Document>(node).documentElement())
            return locateDefaultNamespace(*documentElement, prefix);
        return nullAtom();
    case Node::DOCUMENT_TYPE_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        return nullAtom();
    case Node::ATTRIBUTE_NODE:
        if (auto* ownerElement = uncheckedDowncast<Attr>(node).ownerElement())
            return locateDefaultNamespace(*ownerElement, prefix);
        return nullAtom();
    default:
        if (auto* parent = node.parentElement())
            return locateDefaultNamespace(*parent, prefix);
        return nullAtom();
    }
}

// https://dom.spec.whatwg.org/#dom-node-isdefaultnamespace
bool Node::isDefaultNamespace(const AtomString& potentiallyEmptyNamespace) const
{
    const AtomString& namespaceURI = potentiallyEmptyNamespace.isEmpty() ? nullAtom() : potentiallyEmptyNamespace;
    return locateDefaultNamespace(*this, nullAtom()) == namespaceURI;
}

// https://dom.spec.whatwg.org/#dom-node-lookupnamespaceuri
const AtomString& Node::lookupNamespaceURI(const AtomString& potentiallyEmptyPrefix) const
{
    const AtomString& prefix = potentiallyEmptyPrefix.isEmpty() ? nullAtom() : potentiallyEmptyPrefix;
    return locateDefaultNamespace(*this, prefix);
}

// https://dom.spec.whatwg.org/#locate-a-namespace-prefix
static const AtomString& locateNamespacePrefix(const Element& element, const AtomString& namespaceURI)
{
    if (element.namespaceURI() == namespaceURI)
        return element.prefix();

    if (element.hasAttributes()) {
        for (auto& attribute : element.attributes()) {
            if (attribute.prefix() == xmlnsAtom() && attribute.value() == namespaceURI)
                return attribute.localName();
        }
    }
    auto* parent = element.parentElement();
    return parent ? locateNamespacePrefix(*parent, namespaceURI) : nullAtom();
}

// https://dom.spec.whatwg.org/#dom-node-lookupprefix
const AtomString& Node::lookupPrefix(const AtomString& namespaceURI) const
{
    if (namespaceURI.isEmpty())
        return nullAtom();
    
    switch (nodeType()) {
    case ELEMENT_NODE:
        return locateNamespacePrefix(uncheckedDowncast<Element>(*this), namespaceURI);
    case DOCUMENT_NODE:
        if (auto* documentElement = uncheckedDowncast<Document>(*this).documentElement())
            return locateNamespacePrefix(*documentElement, namespaceURI);
        return nullAtom();
    case DOCUMENT_FRAGMENT_NODE:
    case DOCUMENT_TYPE_NODE:
        return nullAtom();
    case ATTRIBUTE_NODE:
        if (auto* ownerElement = uncheckedDowncast<Attr>(*this).ownerElement())
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
        content.append(uncheckedDowncast<CharacterData>(*node).data());
        break;

    case Node::PROCESSING_INSTRUCTION_NODE:
        isNullString = false;
        content.append(uncheckedDowncast<ProcessingInstruction>(*node).data());
        break;
    
    case Node::ATTRIBUTE_NODE:
        isNullString = false;
        content.append(uncheckedDowncast<Attr>(*node).value());
        break;

    case Node::ELEMENT_NODE:
        if (node->hasTagName(brTag) && convertBRsToNewlines) {
            isNullString = false;
            content.append('\n');
            break;
        }
        [[fallthrough]];
    case Node::DOCUMENT_FRAGMENT_NODE:
        isNullString = false;
        for (RefPtr child = node->firstChild(); child; child = child->nextSibling()) {
            if (child->nodeType() == Node::COMMENT_NODE || child->nodeType() == Node::PROCESSING_INSTRUCTION_NODE)
                continue;
            appendTextContent(child.get(), convertBRsToNewlines, isNullString, content);
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

ExceptionOr<void> Node::setTextContent(String&& text)
{
    switch (nodeType()) {
    case ATTRIBUTE_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
        return setNodeValue(WTFMove(text));
    case ELEMENT_NODE:
    case DOCUMENT_FRAGMENT_NODE:
        uncheckedDowncast<ContainerNode>(*this).stringReplaceAll(WTFMove(text));
        return { };
    case DOCUMENT_NODE:
    case DOCUMENT_TYPE_NODE:
        // Do nothing.
        return { };
    }
    ASSERT_NOT_REACHED();

    return { };
}

static SHA1::Digest hashPointer(const void* pointer)
{
    SHA1 sha1;
    sha1.addBytes(asByteSpan(pointer));
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

    unsigned short direction = is_gt(compareSpans(std::span { firstHash }, std::span { secondHash })) ? Node::DOCUMENT_POSITION_PRECEDING : Node::DOCUMENT_POSITION_FOLLOWING;

    return Node::DOCUMENT_POSITION_DISCONNECTED | Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
}

bool connectedInSameTreeScope(const Node* a, const Node* b)
{
    // Note that we avoid comparing Attr nodes here, since they return false from isConnected() all the time (which seems like a bug).
    return a && b && a->isConnected() == b->isConnected() && &a->treeScope() == &b->treeScope();
}

// FIXME: Refactor so this calls treeOrder, with additional code for any exotic inefficient things that are needed only here.
unsigned short Node::compareDocumentPosition(Node& otherNode)
{
    if (&otherNode == this)
        return DOCUMENT_POSITION_EQUIVALENT;
    
    auto* attr1 = dynamicDowncast<Attr>(*this);
    auto* attr2 = dynamicDowncast<Attr>(otherNode);
    
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
        for (auto& attribute : owner1->attributes()) {
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
    // If the nodes have different owning documents, they must be disconnected.
    if (!connectedInSameTreeScope(start1, start2))
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

String Node::description() const
{
    auto name = nodeName();
    return makeString(name.isEmpty() ? "<none>"_s : ""_s, name);
}

String Node::debugDescription() const
{
    auto name = nodeName();
    return makeString(name.isEmpty() ? "<none>"_s : ""_s, name, " 0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase));
}

#if ENABLE(TREE_DEBUGGING)

static void appendAttributeDesc(const Node* node, StringBuilder& stringBuilder, const QualifiedName& name, ASCIILiteral attrDesc)
{
    auto* element = dynamicDowncast<Element>(*node);
    if (!element)
        return;

    const AtomString& attr = element->getAttribute(name);
    if (attr.isEmpty())
        return;

    stringBuilder.append(attrDesc);
    stringBuilder.append(attr);
}

void Node::showNode(ASCIILiteral prefix) const
{
    if (prefix.isNull())
        prefix = ""_s;
    if (isTextNode()) {
        String value = makeStringByReplacingAll(nodeValue(), '\\', "\\\\"_s);
        value = makeStringByReplacingAll(value, '\n', "\\n"_s);
        SAFE_FPRINTF(stderr, "%s%s\t%p \"%s\"\n", prefix, nodeName().utf8(), this, value.utf8());
    } else {
        StringBuilder attrs;
        appendAttributeDesc(this, attrs, classAttr, " CLASS="_s);
        appendAttributeDesc(this, attrs, styleAttr, " STYLE="_s);
        SAFE_FPRINTF(stderr, "%s%s\t%p (renderer %p) %s%s%s\n", prefix, nodeName().utf8(), this, renderer(), attrs.toString().utf8(), needsStyleRecalc() ? " (needs style recalc)"_s : ""_s, childNeedsStyleRecalc() ? " (child needs style recalc)"_s : ""_s);
    }
}

void Node::showTreeForThis() const
{
    showTreeAndMark(this, "*"_s);
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
        if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(*node)) {
            int count = 0;
            for (; shadowRoot && shadowRoot != node; shadowRoot = shadowRoot->shadowRoot())
                ++count;
            fprintf(stderr, "/#shadow-root[%d]", count);
            continue;
        }

        switch (node->nodeType()) {
        case ELEMENT_NODE: {
            SAFE_FPRINTF(stderr, "/%s", node->nodeName().utf8());

            const Element& element = uncheckedDowncast<Element>(*node);
            const AtomString& idattr = element.getIdAttribute();
            bool hasIdAttr = !idattr.isNull() && !idattr.isEmpty();
            if (node->previousSibling() || node->nextSibling()) {
                int count = 0;
                for (Node* previous = node->previousSibling(); previous; previous = previous->previousSibling())
                    if (previous->nodeName() == node->nodeName())
                        ++count;
                if (hasIdAttr)
                    SAFE_FPRINTF(stderr, "[@id=\"%s\" and position()=%d]", idattr.string().utf8(), count);
                else
                    fprintf(stderr, "[%d]", count);
            } else if (hasIdAttr)
                SAFE_FPRINTF(stderr, "[@id=\"%s\"]", idattr.string().utf8());
            break;
        }
        case TEXT_NODE:
            fprintf(stderr, "/text()");
            break;
        case ATTRIBUTE_NODE:
            SAFE_FPRINTF(stderr, "/@%s", node->nodeName().utf8());
            break;
        default:
            break;
        }
    }
    fprintf(stderr, "\n");
}

static void traverseTreeAndMark(const String& baseIndent, const Node* rootNode, const Node* markedNode1, ASCIILiteral markedLabel1, const Node* markedNode2, ASCIILiteral markedLabel2)
{
    for (const Node* node = rootNode; node; node = NodeTraversal::next(*node)) {
        if (node == markedNode1)
            SAFE_FPRINTF(stderr, "%s", markedLabel1);
        if (node == markedNode2)
            SAFE_FPRINTF(stderr, "%s", markedLabel2);

        StringBuilder indent;
        indent.append(baseIndent);
        for (const Node* tmpNode = node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentOrShadowHostNode())
            indent.append('\t');
        SAFE_FPRINTF(stderr, "%s", indent.toString().utf8());
        node->showNode();
        indent.append('\t');
        if (!node->isShadowRoot()) {
            if (RefPtr shadowRoot = node->shadowRoot())
                traverseTreeAndMark(indent.toString(), shadowRoot.get(), markedNode1, markedLabel1, markedNode2, markedLabel2);
        }
    }
}

void Node::showTreeAndMark(const Node* markedNode1, ASCIILiteral markedLabel1, const Node* markedNode2, ASCIILiteral markedLabel2) const
{
    const Node* node = this;
    while (node->parentOrShadowHostNode() && !node->hasTagName(bodyTag))
        node = node->parentOrShadowHostNode();
    RefPtr rootNode = node;

    String startingIndent;
    traverseTreeAndMark(startingIndent, rootNode.get(), markedNode1, markedLabel1, markedNode2, markedLabel2);
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
        SAFE_FPRINTF(stderr, "*");
    SAFE_FPRINTF(stderr, "%s\n", indent.utf8());
    node->showNode();
    if (!node->isShadowRoot()) {
        if (auto* frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(node))
            showSubTreeAcrossFrame(frameOwner->protectedContentDocument().get(), markedNode, makeString(indent, '\t'));
        if (RefPtr shadowRoot = node->shadowRoot())
            showSubTreeAcrossFrame(shadowRoot.get(), markedNode, makeString(indent, '\t'));
    }
    for (RefPtr child = node->firstChild(); child; child = child->nextSibling())
        showSubTreeAcrossFrame(child.get(), markedNode, makeString(indent, '\t'));
}

void Node::showTreeForThisAcrossFrame() const
{
    Node* rootNode = const_cast<Node*>(this);
    while (parentOrShadowHostOrFrameOwner(rootNode))
        rootNode = parentOrShadowHostOrFrameOwner(rootNode);
    showSubTreeAcrossFrame(rootNode, this, emptyString());
}

#endif // ENABLE(TREE_DEBUGGING)

// --------

void NodeListsNodeData::invalidateCaches()
{
    for (auto& atomName : m_atomNameCaches)
        atomName.value->invalidateCache();

    for (auto& collection : m_cachedCollections)
        collection.value->invalidateCache();

    for (auto& tagCollection : m_tagCollectionNSCache)
        tagCollection.value->invalidateCache();
}

void NodeListsNodeData::invalidateCachesForAttribute(const QualifiedName& attrName)
{
    for (auto& atomName : m_atomNameCaches)
        atomName.value->invalidateCacheForAttribute(attrName);

    for (auto& collection : m_cachedCollections)
        collection.value->invalidateCacheForAttribute(attrName);
}

void Node::getSubresourceURLs(ListHashSet<URL>& urls) const
{
    addSubresourceAttributeURLs(urls);
}

void Node::getCandidateSubresourceURLs(ListHashSet<URL>& urls) const
{
    addCandidateSubresourceURLs(urls);
}

Element* Node::enclosingLinkEventParentOrSelf()
{
    for (Node* node = this; node; node = node->parentInComposedTree()) {
        // For imagemaps, the enclosing link element is the associated area element not the image itself.
        // So we don't let images be the enclosing link element, even though isLink sometimes returns
        // true for them.
        if (auto* element = dynamicDowncast<Element>(*node); element && element->isLink() && !is<HTMLImageElement>(*element))
            return element;
    }

    return nullptr;
}

enum EventTargetInterfaceType Node::eventTargetInterface() const
{
    return EventTargetInterfaceType::Node;
}

template <typename MoveNodeFunction, typename MoveShadowRootFunction>
static unsigned traverseSubtreeToUpdateTreeScope(Node& root, NOESCAPE const MoveNodeFunction& moveNode, NOESCAPE const MoveShadowRootFunction& moveShadowRoot)
{
    unsigned count = 0;
    for (Node* node = &root; node; node = NodeTraversal::next(*node, &root)) {
        moveNode(*node);
        ++count;

        auto* element = dynamicDowncast<Element>(*node);
        if (!element)
            continue;

        if (element->hasSyntheticAttrChildNodes()) {
            for (auto& attr : element->attrNodeList()) {
                moveNode(*attr);
                ++count;
            }
        }

        if (auto* shadow = element->shadowRoot())
            count += moveShadowRoot(*shadow);
    }
    return count;
}

static ALWAYS_INLINE bool isDocumentEligibleForFastAdoption(Document& oldDocument, Document& newDocument)
{
    return !oldDocument.hasNodeIterators()
        && !oldDocument.hasRanges()
        && !oldDocument.hasNodeWithEventListeners()
        && !oldDocument.hasMutationObservers()
        && !oldDocument.textManipulationControllerIfExists()
        && !oldDocument.shouldInvalidateNodeListAndCollectionCaches()
        && !oldDocument.numberOfIntersectionObservers()
        && (!AXObjectCache::accessibilityEnabled() || !oldDocument.existingAXObjectCache())
        && oldDocument.inQuirksMode() == newDocument.inQuirksMode();
}

static ALWAYS_INLINE void adoptCustomElementRegistryIfNotExplicitlySet(ShadowRoot& shadowRoot, Document& newDocument)
{
    if (shadowRoot.hasScopedCustomElementRegistry() || !shadowRoot.usesNullCustomElementRegistry() || newDocument.usesNullCustomElementRegistry()) [[likely]]
        return;
    shadowRoot.clearUsesNullCustomElementRegistry();
    shadowRoot.setCustomElementRegistry(newDocument.customElementRegistry());
}

inline unsigned Node::moveShadowTreeToNewDocumentFastCase(ShadowRoot& shadowRoot, Document& oldDocument, Document& newDocument)
{
    ASSERT(isDocumentEligibleForFastAdoption(oldDocument, newDocument));
    adoptCustomElementRegistryIfNotExplicitlySet(shadowRoot, newDocument);
    return traverseSubtreeToUpdateTreeScope(shadowRoot, [&](Node& node) {
        node.moveNodeToNewDocumentFastCase(oldDocument, newDocument);
    }, [&oldDocument, &newDocument](ShadowRoot& innerShadowRoot) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&innerShadowRoot.document() == &oldDocument);
        innerShadowRoot.moveShadowRootToNewDocument(oldDocument, newDocument);
        return moveShadowTreeToNewDocumentFastCase(innerShadowRoot, oldDocument, newDocument);
    });
}

inline void Node::moveShadowTreeToNewDocumentSlowCase(ShadowRoot& shadowRoot, Document& oldDocument, Document& newDocument)
{
    ASSERT(!isDocumentEligibleForFastAdoption(oldDocument, newDocument));
    adoptCustomElementRegistryIfNotExplicitlySet(shadowRoot, newDocument);
    traverseSubtreeToUpdateTreeScope(shadowRoot, [&](Node& node) {
        node.moveNodeToNewDocumentSlowCase(oldDocument, newDocument);
    }, [&oldDocument, &newDocument](ShadowRoot& innerShadowRoot) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&innerShadowRoot.document() == &oldDocument);
        innerShadowRoot.moveShadowRootToNewDocument(oldDocument, newDocument);
        moveShadowTreeToNewDocumentSlowCase(innerShadowRoot, oldDocument, newDocument);
        return 0; // Unused.
    });
}

void Node::moveTreeToNewScope(Node& root, TreeScope& oldScope, TreeScope& newScope)
{
    ASSERT(&oldScope != &newScope);

    Document& oldDocument = oldScope.documentScope();
    Document& newDocument = newScope.documentScope();
    bool newScopeIsUAShadowTree = newScope.rootNode().hasBeenInUserAgentShadowTree();
    if (&oldDocument != &newDocument) {
        oldDocument.incrementReferencingNodeCount();
        bool isFastCase = isDocumentEligibleForFastAdoption(oldDocument, newDocument) && !newScopeIsUAShadowTree;
        if (isFastCase) {
            unsigned nodeCount = traverseSubtreeToUpdateTreeScope(root, [&](Node& node) {
                ASSERT(!node.isTreeScope());
                RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&node.treeScope() == &oldScope);
                node.setTreeScope(newScope);
                node.moveNodeToNewDocumentFastCase(oldDocument, newDocument);
            }, [&](ShadowRoot& shadowRoot) {
                ASSERT_WITH_SECURITY_IMPLICATION(&shadowRoot.document() == &oldDocument);
                shadowRoot.moveShadowRootToNewParentScope(newScope, newDocument);
                return moveShadowTreeToNewDocumentFastCase(shadowRoot, oldDocument, newDocument);
            });
//            UNUSED_PARAM(nodeCount);
            newDocument.incrementReferencingNodeCount(nodeCount);
            oldDocument.decrementReferencingNodeCount(nodeCount);
        } else {
            traverseSubtreeToUpdateTreeScope(root, [&](Node& node) {
                ASSERT(!node.isTreeScope());
                RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&node.treeScope() == &oldScope);
                if (newScopeIsUAShadowTree)
                    node.setEventTargetFlag(EventTargetFlag::HasBeenInUserAgentShadowTree);
                node.setTreeScope(newScope);
                node.moveNodeToNewDocumentSlowCase(oldDocument, newDocument);
            }, [&](ShadowRoot& shadowRoot) {
                ASSERT_WITH_SECURITY_IMPLICATION(&shadowRoot.document() == &oldDocument);
                shadowRoot.moveShadowRootToNewParentScope(newScope, newDocument);
                moveShadowTreeToNewDocumentSlowCase(shadowRoot, oldDocument, newDocument);
                return 0; // Unused
            });
        }
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&oldScope.documentScope() == &oldDocument && &newScope.documentScope() == &newDocument);
        oldDocument.decrementReferencingNodeCount();
    } else {
        traverseSubtreeToUpdateTreeScope(root, [&](Node& node) {
            ASSERT(!node.isTreeScope());
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&node.treeScope() == &oldScope);
            if (newScopeIsUAShadowTree)
                node.setEventTargetFlag(EventTargetFlag::HasBeenInUserAgentShadowTree);
            node.setTreeScope(newScope);
            if (!node.hasRareData()) [[likely]]
                return;
            if (auto* nodeLists = node.rareData()->nodeLists())
                nodeLists->adoptTreeScope();
        }, [&newScope](ShadowRoot& shadowRoot) {
            shadowRoot.setParentTreeScope(newScope);
            return 0; // Unused.
        });
    }
}

void Node::moveNodeToNewDocumentFastCase(Document& oldDocument, Document& newDocument)
{
    ASSERT(!oldDocument.shouldInvalidateNodeListAndCollectionCaches());
    ASSERT(!oldDocument.hasNodeIterators());
    ASSERT(!oldDocument.hasRanges());
    ASSERT(!AXObjectCache::accessibilityEnabled() || !oldDocument.existingAXObjectCache());
    ASSERT(!oldDocument.textManipulationControllerIfExists());
    ASSERT(!oldDocument.hasNodeWithEventListeners());
    ASSERT(!hasEventListeners());
    ASSERT(!mutationObserverRegistry());
    ASSERT(!transientMutationObserverRegistry());
    ASSERT(!oldDocument.numberOfIntersectionObservers());

    if (usesNullCustomElementRegistry() && !newDocument.usesNullCustomElementRegistry()) [[unlikely]]
        clearUsesNullCustomElementRegistry();

    if (!hasTypeFlag(TypeFlag::HasDidMoveToNewDocument) && !hasEventTargetFlag(EventTargetFlag::HasLangAttr) && !hasEventTargetFlag(EventTargetFlag::HasXMLLangAttr)
        && !isDefinedCustomElement())
        return;

    if (auto* element = dynamicDowncast<Element>(*this))
        element->didMoveToNewDocument(oldDocument, newDocument);
}

void Node::moveNodeToNewDocumentSlowCase(Document& oldDocument, Document& newDocument)
{
    newDocument.incrementReferencingNodeCount();
    oldDocument.decrementReferencingNodeCount();

    if (usesNullCustomElementRegistry() && !newDocument.usesNullCustomElementRegistry()) [[unlikely]]
        clearUsesNullCustomElementRegistry();

    if (hasRareData()) {
        if (auto* nodeLists = rareData()->nodeLists())
            nodeLists->adoptDocument(oldDocument, newDocument);
        if (auto* registry = mutationObserverRegistry()) {
            for (auto& registration : *registry)
                newDocument.addMutationObserverTypes(registration->mutationTypes());
        }
        if (auto* transientRegistry = transientMutationObserverRegistry()) {
            for (auto& registration : *transientRegistry)
                newDocument.addMutationObserverTypes(registration.mutationTypes());
        }
    } else {
        ASSERT(!mutationObserverRegistry());
        ASSERT(!transientMutationObserverRegistry());
    }

    oldDocument.moveNodeIteratorsToNewDocument(*this, newDocument);

    if (!parentNode())
        oldDocument.parentlessNodeMovedToNewDocument(*this);

    if (AXObjectCache::accessibilityEnabled()) {
        if (CheckedPtr cache = oldDocument.existingAXObjectCache())
            cache->remove(*this);
    }

    auto* textManipulationController = oldDocument.textManipulationControllerIfExists();
    if (textManipulationController) [[unlikely]]
        textManipulationController->removeNode(*this);

    if (hasEventTargetData()) {
        unsigned numWheelEventHandlers = 0;
        unsigned numTouchEventListeners = 0;
#if ENABLE(TOUCH_EVENTS) && ENABLE(IOS_GESTURE_EVENTS)
        unsigned numGestureEventListeners = 0;
#endif

        auto touchEventCategory = EventCategory::TouchRelated;
#if ENABLE(TOUCH_EVENTS)
        if (newDocument.quirks().shouldDispatchSimulatedMouseEvents(this))
            touchEventCategory = EventCategory::ExtendedTouchRelated;
#endif

        auto& eventNames = WebCore::eventNames();
        enumerateEventListenerTypes([&](auto& eventType, unsigned count) {
            oldDocument.didRemoveEventListenersOfType(eventType, count);
            newDocument.didAddEventListenersOfType(eventType, count);

            auto typeInfo = eventNames.typeInfoForEvent(eventType);
            if (typeInfo.isInCategory(EventCategory::Wheel))
                numWheelEventHandlers += count;
            else if (typeInfo.isInCategory(touchEventCategory))
                numTouchEventListeners += count;
#if ENABLE(TOUCH_EVENTS) && ENABLE(IOS_GESTURE_EVENTS)
            else if (typeInfo.isInCategory(EventCategory::Gesture))
                numGestureEventListeners += count;
#endif
        });

        for (unsigned i = 0; i < numWheelEventHandlers; ++i) {
            oldDocument.didRemoveWheelEventHandler(*this);
            newDocument.didAddWheelEventHandler(*this);
        }

        for (unsigned i = 0; i < numTouchEventListeners; ++i) {
            oldDocument.didRemoveTouchEventHandler(*this);
            newDocument.didAddTouchEventHandler(*this);
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
            oldDocument.removeTouchEventListener(*this);
            newDocument.addTouchEventListener(*this);
#endif
        }

#if ENABLE(TOUCH_EVENTS) && ENABLE(IOS_GESTURE_EVENTS)
        for (unsigned i = 0; i < numGestureEventListeners; ++i) {
            oldDocument.removeTouchEventHandler(*this);
            newDocument.addTouchEventHandler(*this);
        }
#endif
    }

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
    ASSERT_WITH_SECURITY_IMPLICATION(!oldDocument.touchEventListenersContain(*this));
    ASSERT_WITH_SECURITY_IMPLICATION(!oldDocument.touchEventHandlersContain(*this));
#endif
#if ENABLE(TOUCH_EVENTS) && ENABLE(IOS_GESTURE_EVENTS)
    ASSERT_WITH_SECURITY_IMPLICATION(!oldDocument.touchEventTargetsContain(*this));
#endif
#endif

    if (auto* element = dynamicDowncast<Element>(*this))
        element->didMoveToNewDocument(oldDocument, newDocument);
}

bool isTouchRelatedEventType(const EventTypeInfo& eventType, const EventTarget& target)
{
#if ENABLE(TOUCH_EVENTS)
    if (auto* node = dynamicDowncast<Node>(target); eventType.isInCategory(EventCategory::ExtendedTouchRelated)
        && node && node->document().quirks().shouldDispatchSimulatedMouseEvents(&target))
        return true;
#endif
    UNUSED_PARAM(target);
    return eventType.isInCategory(EventCategory::TouchRelated);
}

static inline bool tryAddEventListener(Node* targetNode, const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (!targetNode->EventTarget::addEventListener(eventType, listener.copyRef(), options))
        return false;

    Ref document = targetNode->document();
    document->didAddEventListenersOfType(eventType);

    auto& eventNames = WebCore::eventNames();
    auto typeInfo = eventNames.typeInfoForEvent(eventType);
    if (typeInfo.isInCategory(EventCategory::Wheel)) {
        document->didAddWheelEventHandler(*targetNode);
        document->invalidateEventListenerRegions();
    } else if (isTouchRelatedEventType(typeInfo, *targetNode))
        document->didAddTouchEventHandler(*targetNode);
    else if (typeInfo.isInCategory(EventCategory::MouseClickRelated))
        document->didAddOrRemoveMouseEventHandler(*targetNode);

#if PLATFORM(IOS_FAMILY)
    if (targetNode == document.ptr() && typeInfo.type() == EventType::scroll) {
        if (RefPtr window = document->window())
            window->incrementScrollEventListenersCount();
    }

#if ENABLE(TOUCH_EVENTS)
    if (isTouchRelatedEventType(typeInfo, *targetNode))
        document->addTouchEventListener(*targetNode);
#endif
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(IOS_GESTURE_EVENTS) && ENABLE(TOUCH_EVENTS)
    if (typeInfo.isInCategory(EventCategory::Gesture))
        document->addTouchEventHandler(*targetNode);
#endif

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    if (typeInfo.isInCategory(EventCategory::MouseMoveRelated)) {
        if (WeakPtr observer = document->contentChangeObserverIfExists())
            observer->didAddMouseMoveRelatedEventListener(eventType, *targetNode);
    }
#endif

    if (CheckedPtr cache = document->existingAXObjectCache())
        cache->onEventListenerAdded(*targetNode, eventType);

    return true;
}

bool Node::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    return tryAddEventListener(this, eventType, WTFMove(listener), options);
}

static inline bool didRemoveEventListenerOfType(Node& targetNode, const AtomString& eventType)
{
    Ref document = targetNode.document();
    document->didRemoveEventListenersOfType(eventType);

    // FIXME: Notify Document that the listener has vanished. We need to keep track of a number of
    // listeners for each type, not just a bool - see https://bugs.webkit.org/show_bug.cgi?id=33861
    auto& eventNames = WebCore::eventNames();
    auto typeInfo = eventNames.typeInfoForEvent(eventType);
    if (typeInfo.isInCategory(EventCategory::Wheel)) {
        document->didRemoveWheelEventHandler(targetNode);
        document->invalidateEventListenerRegions();
    } else if (isTouchRelatedEventType(typeInfo, targetNode))
        document->didRemoveTouchEventHandler(targetNode);
    else if (typeInfo.isInCategory(EventCategory::MouseClickRelated))
        document->didAddOrRemoveMouseEventHandler(targetNode);

#if PLATFORM(IOS_FAMILY)
    if (&targetNode == document.ptr() && typeInfo.type() == EventType::scroll) {
        if (RefPtr window = document->window())
            window->decrementScrollEventListenersCount();
    }

#if ENABLE(TOUCH_EVENTS)
    if (isTouchRelatedEventType(typeInfo, targetNode))
        document->removeTouchEventListener(targetNode);
#endif
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(IOS_GESTURE_EVENTS) && ENABLE(TOUCH_EVENTS)
    if (typeInfo.isInCategory(EventCategory::Gesture))
        document->removeTouchEventHandler(targetNode);
#endif

    if (CheckedPtr cache = document->existingAXObjectCache())
        cache->onEventListenerRemoved(targetNode, eventType);

    return true;
}

bool Node::removeEventListener(const AtomString& eventType, EventListener& listener, const EventListenerOptions& options)
{
    if (!EventTarget::removeEventListener(eventType, listener, options))
        return false;
    didRemoveEventListenerOfType(*this, eventType);
    return true;
}

void Node::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();
    enumerateEventListenerTypes([&](const AtomString& type, unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            didRemoveEventListenerOfType(*this, type);
    });
}

Vector<std::unique_ptr<MutationObserverRegistration>>* Node::mutationObserverRegistry()
{
    if (!hasRareData())
        return nullptr;
    auto* data = rareData()->mutationObserverDataIfExists();
    if (!data)
        return nullptr;
    return &data->registry;
}

WeakHashSet<MutationObserverRegistration>* Node::transientMutationObserverRegistry()
{
    if (!hasRareData())
        return nullptr;
    auto* data = rareData()->mutationObserverDataIfExists();
    if (!data)
        return nullptr;
    return &data->transientRegistry;
}

HashMap<Ref<MutationObserver>, MutationRecordDeliveryOptions> Node::registeredMutationObservers(MutationObserverOptionType type, const QualifiedName* attributeName)
{
    HashMap<Ref<MutationObserver>, MutationRecordDeliveryOptions> observers;
    ASSERT((type == MutationObserverOptionType::Attributes && attributeName) || !attributeName);

    auto collectMatchingObserversForMutation = [&](MutationObserverRegistration& registration) {
        if (registration.shouldReceiveMutationFrom(*this, type, attributeName)) {
            auto deliveryOptions = registration.deliveryOptions();
            auto result = observers.add(registration.observer(), deliveryOptions);
            if (!result.isNewEntry)
                result.iterator->value.add(deliveryOptions);
        }
    };

    for (RefPtr node = this; node; node = node->parentNode()) {
        if (auto* registry = node->mutationObserverRegistry()) {
            for (auto& registration : *registry)
                collectMatchingObserversForMutation(*registration);
        }
        if (auto* registry = node->transientMutationObserverRegistry()) {
            for (auto& registration : *registry)
                collectMatchingObserversForMutation(registration);
        }
    }

    return observers;
}

void Node::registerMutationObserver(MutationObserver& observer, MutationObserverOptions options, const MemoryCompactLookupOnlyRobinHoodHashSet<AtomString>& attributeFilter)
{
    MutationObserverRegistration* registration = nullptr;
    auto& registry = ensureRareData().mutationObserverData().registry;

    for (auto& candidateRegistration : registry) {
        if (&candidateRegistration->observer() == &observer) {
            registration = candidateRegistration.get();
            registration->resetObservation(options, attributeFilter);
        }
    }

    if (!registration) {
        registry.append(makeUnique<MutationObserverRegistration>(observer, *this, options, attributeFilter));
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
    ensureRareData().mutationObserverData().transientRegistry.add(registration);
}

void Node::unregisterTransientMutationObserver(MutationObserverRegistration& registration)
{
    auto* transientRegistry = transientMutationObserverRegistry();
    ASSERT(transientRegistry);
    if (!transientRegistry)
        return;

    ASSERT(transientRegistry->contains(registration));
    transientRegistry->remove(registration);
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
            for (auto& registration : *transientRegistry)
                registration.observedSubtreeNodeWillDetach(*this);
        }
    }
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
    if (isInShadowTree() || document().shouldNotFireMutationEvents())
        return;

    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(*this));

    if (!document().hasListenerType(Document::ListenerType::DOMSubtreeModified))
        return;
    const AtomString& subtreeModifiedEventName = eventNames().DOMSubtreeModifiedEvent;
    if (!parentNode() && !hasEventListeners(subtreeModifiedEventName))
        return;

    dispatchScopedEvent(MutationEvent::create(subtreeModifiedEventName, Event::CanBubble::Yes));
}

void Node::dispatchDOMActivateEvent(Event& underlyingClickEvent)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    auto* uiEvent = dynamicDowncast<UIEvent>(underlyingClickEvent);
    int detail = uiEvent ? uiEvent->detail() : 0;
    Ref event = UIEvent::create(eventNames().DOMActivateEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes, Event::IsComposed::Yes, document().windowProxy(), detail);
    event->setUnderlyingEvent(&underlyingClickEvent);
    dispatchScopedEvent(event);
    if (event->defaultHandled())
        underlyingClickEvent.setDefaultHandled();
}

void Node::dispatchInputEvent()
{
    dispatchScopedEvent(Event::create(eventNames().inputEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
}

void Node::dispatchWebKitSubmitEvent(Event& underlyingSubmitEvent)
{
    RefPtr submitEvent = dynamicDowncast<SubmitEvent>(underlyingSubmitEvent);
    if (!submitEvent)
        return;

    SubmitEvent::Init init { };
    init.bubbles = true;
    init.cancelable = true;
    init.composed = true;
    init.submitter = submitEvent->submitter();
    Ref webkitSubmitEvent = SubmitEvent::create(eventNames().webkitsubmitEvent, WTFMove(init));
    webkitSubmitEvent->setIsAutofillEvent();
    dispatchScopedEvent(webkitSubmitEvent);
}

void Node::defaultEventHandler(Event& event)
{
    if (event.target() != this)
        return;
    auto& eventType = event.type();
    auto& eventNames = WebCore::eventNames();
    auto typeInfo = eventNames.typeInfoForEvent(eventType);
    switch (typeInfo.type()) {
    case EventType::keydown:
    case EventType::keypress:
    case EventType::keyup:
        if (RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event)) {
            if (RefPtr frame = document().frame())
                frame->eventHandler().defaultKeyboardEventHandler(*keyboardEvent);
        }
        break;
    case EventType::click:
        dispatchDOMActivateEvent(event);
        break;
#if ENABLE(CONTEXT_MENUS)
    case EventType::contextmenu:
        if (RefPtr frame = document().frame()) {
            if (RefPtr page = frame->page())
                page->contextMenuController().handleContextMenuEvent(event);
        }
        break;
#endif
    case EventType::submit:
        dispatchWebKitSubmitEvent(event);
        break;
    case EventType::textInput:
        if (RefPtr textEvent = dynamicDowncast<TextEvent>(event)) {
            if (RefPtr frame = document().frame())
                frame->eventHandler().defaultTextInputEventHandler(*textEvent);
        }
        break;
#if ENABLE(PAN_SCROLLING)
    case EventType::mousedown:
        if (auto* mouseEvent = dynamicDowncast<MouseEvent>(event); mouseEvent && mouseEvent->button() == MouseButton::Middle) {
            if (enclosingLinkEventParentOrSelf())
                return;

            for (auto* renderer = this->renderer(); renderer; renderer = renderer->parent()) {
                CheckedPtr renderBox = dynamicDowncast<RenderBox>(*renderer);
                if (renderBox && renderBox->canBeScrolledAndHasScrollableArea()) {
                    if (RefPtr frame = document().frame())
                        frame->eventHandler().startPanScrolling(*renderBox);
                    break;
                }
            }
        }
        break;
#endif
    case EventType::mousewheel:
    case EventType::wheel:
        ASSERT(typeInfo.isInCategory(EventCategory::Wheel));
        if (auto* wheelEvent = dynamicDowncast<WheelEvent>(event); wheelEvent) {
            // If we don't have a renderer, send the wheel event to the first node we find with a renderer.
            // This is needed for <option> and <optgroup> elements so that <select>s get a wheel scroll.
            Node* startNode = this;
            while (startNode && !startNode->renderer())
                startNode = startNode->parentOrShadowHostNode();
        
            if (startNode && startNode->renderer()) {
                if (RefPtr frame = document().frame())
                    frame->eventHandler().defaultWheelEventHandler(RefPtr { startNode }.get(), *wheelEvent);
            }
        }
        break;
    default:
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
        if (auto* touchEvent = dynamicDowncast<TouchEvent>(event); touchEvent && isTouchRelatedEventType(typeInfo, *this)) {
            // Capture the target node's visibility state before dispatching touchStart.
            if (auto* element = dynamicDowncast<Element>(*this); element && typeInfo.type() == EventType::touchstart) {
#if ENABLE(CONTENT_CHANGE_OBSERVER)
                auto& contentChangeObserver = document().contentChangeObserver();
                if (ContentChangeObserver::isVisuallyHidden(*this))
                    contentChangeObserver.setHiddenTouchTarget(*element);
                else
                    contentChangeObserver.resetHiddenTouchTarget();
#endif
            }

            RenderObject* renderer = this->renderer();
            for (; renderer; renderer = renderer->parent()) {
                auto* renderBox = dynamicDowncast<RenderBox>(*renderer);
                if (renderBox && renderBox->canBeScrolledAndHasScrollableArea())
                    break;
            }

            if (renderer && renderer->node()) {
                if (RefPtr frame = document().frame()) {
                    RefPtr rendererNode = renderer->node();
                    frame->eventHandler().defaultTouchEventHandler(*rendererNode, *touchEvent);
                }
            }
        }
#endif
        break;
    }
}

bool Node::willRespondToMouseMoveEvents() const
{
    // FIXME: Why is the iOS code path different from the non-iOS code path?
#if !PLATFORM(IOS_FAMILY)
    auto* element = dynamicDowncast<Element>(*this);
    if (!element)
        return false;
    if (element->isDisabledFormControl())
        return false;
#endif
    auto& eventNames = WebCore::eventNames();
    return eventTypes().containsIf([&](const auto& type) {
        return eventNames.typeInfoForEvent(type).isInCategory(EventCategory::MouseMoveRelated);
    });
}

bool Node::willRespondToTouchEvents() const
{
    auto& eventNames = WebCore::eventNames();
    return eventTypes().containsIf([&](const auto& type) {
        return eventNames.typeInfoForEvent(type).isInCategory(EventCategory::TouchRelated);
    });
}

Node::Editability Node::computeEditabilityForMouseClickEvents(const RenderStyle* style) const
{
    // FIXME: Why is the iOS code path different from the non-iOS code path?
#if PLATFORM(IOS_FAMILY)    
    auto userSelectAllTreatment = UserSelectAllTreatment::Editable;
#else
    auto userSelectAllTreatment = UserSelectAllTreatment::NotEditable;
#endif

    return computeEditabilityWithStyle(style, userSelectAllTreatment, style ? ShouldUpdateStyle::DoNotUpdate : ShouldUpdateStyle::Update);
}

bool Node::willRespondToMouseClickEvents(const RenderStyle* styleToUse) const
{
    return willRespondToMouseClickEventsWithEditability(computeEditabilityForMouseClickEvents(styleToUse));
}

bool Node::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    // FIXME: Why is the iOS code path different from the non-iOS code path?
#if !PLATFORM(IOS_FAMILY)
    auto* element = dynamicDowncast<Element>(*this);
    if (!element)
        return false;
    if (element->isDisabledFormControl())
        return false;
#endif
    if (editability != Editability::ReadOnly)
        return true;

    auto& eventNames = WebCore::eventNames();
    return eventTypes().containsIf([&](const auto& type) {
        return eventNames.typeInfoForEvent(type).isInCategory(EventCategory::MouseClickRelated);
    });
}

// It's important not to inline removedLastRef, because we don't want to inline the code to
// delete a Node at each deref call site.
void Node::removedLastRef()
{
    RELEASE_ASSERT(m_refCountAndParentBit == s_refCountIncrement);

    // An explicit check for Document here is better than a virtual function since it is
    // faster for non-Document nodes, and because the call to removedLastRef that is inlined
    // at all deref call sites is smaller if it's a non-virtual function.
    if (auto* document = dynamicDowncast<Document>(*this)) {
        document->removedLastRef();
        return;
    }

    // This paragraph runs before Node destruction as a workaround for the fact
    // that detachAllProperties() can transitively call virtual functions on our
    // derived SVG class.

    // Properties may outlive an SVGElement, but no commit will be carried out
    // unless a property has attached to a new owner.

    // FIXME: Make the registry automatically weak, or manually clear it in
    // subclass destructors, so we can remove this workaround.
    if (auto* svgElement = dynamicDowncast<SVGElement>(*this))
        svgElement->detachAllProperties();

#if ASSERT_ENABLED
    m_deletionHasBegun = true;
#endif
    delete this;
}

void Node::incrementConnectedSubframeCount(unsigned amount)
{
    static_assert(RareDataBitFields { Page::maxNumberOfFrames, 0, 0, 0, 0 }.connectedSubframeCount == Page::maxNumberOfFrames, "connectedSubframeCount must fit Page::maxNumberOfFrames");

    ASSERT(isContainerNode());
    auto bitfields = rareDataBitfields();
    bitfields.connectedSubframeCount += amount;
    RELEASE_ASSERT(bitfields.connectedSubframeCount == rareDataBitfields().connectedSubframeCount + amount);
    setRareDataBitfields(bitfields);
}

void Node::decrementConnectedSubframeCount(unsigned amount)
{
    ASSERT(isContainerNode());
    auto bitfields = rareDataBitfields();
    RELEASE_ASSERT(amount <= bitfields.connectedSubframeCount);
    bitfields.connectedSubframeCount -= amount;
    setRareDataBitfields(bitfields);
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

TextDirection Node::effectiveTextDirection() const
{
    if (rareDataBitfields().usesEffectiveTextDirection)
        return static_cast<TextDirection>(rareDataBitfields().effectiveTextDirection);
    return document().documentElementTextDirection();
}

void Node::setEffectiveTextDirection(TextDirection direction)
{
    auto bitfields = rareDataBitfields();
    bitfields.effectiveTextDirection = enumToUnderlyingType(direction);
    setRareDataBitfields(bitfields);
}

void Node::setUsesEffectiveTextDirection(bool value)
{
    auto bitfields = rareDataBitfields();
    bitfields.usesEffectiveTextDirection = value;
    setRareDataBitfields(bitfields);
}

bool Node::inRenderedDocument() const
{
    return isConnected() && document().hasLivingRenderTree();
}

WebCoreOpaqueRoot Node::traverseToOpaqueRoot() const
{
    ASSERT_WITH_MESSAGE(!isConnected(), "Call opaqueRoot() or document() when the node is connected");
    const Node* node = this;
    for (;;) {
        const Node* nextNode = node->parentOrShadowHostNode();
        if (!nextNode)
            break;
        node = nextNode;
    }
    return WebCoreOpaqueRoot { const_cast<Node*>(node) };
}

void Node::notifyInspectorOfRendererChange()
{
    InspectorInstrumentation::didChangeRendererForDOMNode(*this);
}

ScriptExecutionContext* Node::scriptExecutionContext() const
{
    return &document().contextDocument();
}

template<> ContainerNode* parent<Tree>(const Node& node)
{
    return node.parentNode();
}

template<> ContainerNode* parent<ShadowIncludingTree>(const Node& node)
{
    return node.parentOrShadowHostNode();
}

template<> ContainerNode* parent<ComposedTree>(const Node& node)
{
    return node.parentInComposedTree();
}

template<TreeType treeType> size_t depth(const Node& node)
{
    size_t depth = 0;
    auto ancestor = &node;
    while ((ancestor = parent<treeType>(*ancestor)))
        ++depth;
    return depth;
}

struct AncestorAndChildren {
    const Node* commonAncestor;
    const Node* distinctAncestorA;
    const Node* distinctAncestorB;
};

template<TreeType treeType> AncestorAndChildren commonInclusiveAncestorAndChildren(const Node& a, const Node& b)
{
    // This check isn't needed for correctness, but it is cheap and likely to be
    // common enough to be worth optimizing so we don't have to walk to the root.
    if (&a == &b)
        return { &a, nullptr, nullptr };
    // FIXME: Could optimize cases where nodes are both in the same shadow tree.
    // FIXME: Could optimize cases where nodes are in different documents to quickly return false.
    // FIXME: Could optimize cases where one node is connected and the other is not to quickly return false.
    auto [depthA, depthB] = std::make_tuple(depth<treeType>(a), depth<treeType>(b));
    auto [x, y, difference] = depthA >= depthB
        ? std::make_tuple(&a, &b, depthA - depthB)
        : std::make_tuple(&b, &a, depthB - depthA);
    decltype(x) distinctAncestorA = nullptr;
    for (decltype(difference) i = 0; i < difference; ++i) {
        distinctAncestorA = x;
        x = parent<treeType>(*x);
    }
    decltype(y) distinctAncestorB = nullptr;
    while (x != y) {
        distinctAncestorA = x;
        distinctAncestorB = y;
        x = parent<treeType>(*x);
        y = parent<treeType>(*y);
    }
    if (depthA < depthB)
        std::swap(distinctAncestorA, distinctAncestorB);
    return { x, distinctAncestorA, distinctAncestorB };
}

template<TreeType treeType> Node* commonInclusiveAncestor(const Node& a, const Node& b)
{
    return const_cast<Node*>(commonInclusiveAncestorAndChildren<treeType>(a, b).commonAncestor);
}

template Node* commonInclusiveAncestor<Tree>(const Node&, const Node&);
template Node* commonInclusiveAncestor<ComposedTree>(const Node&, const Node&);
template Node* commonInclusiveAncestor<ShadowIncludingTree>(const Node&, const Node&);

static bool isSiblingSubsequent(const Node& siblingA, const Node& siblingB)
{
    ASSERT(siblingA.parentNode());
    ASSERT(siblingA.parentNode() == siblingB.parentNode());
    ASSERT(&siblingA != &siblingB);
    for (auto sibling = &siblingA; sibling; sibling = sibling->nextSibling()) {
        if (sibling == &siblingB)
            return true;
    }
    return false;
}

template<TreeType treeType> std::partial_ordering treeOrder(const Node& a, const Node& b)
{
    if (&a == &b)
        return std::partial_ordering::equivalent;
    auto result = commonInclusiveAncestorAndChildren<treeType>(a, b);
    if (!result.commonAncestor)
        return std::partial_ordering::unordered;
    if (!result.distinctAncestorA)
        return std::partial_ordering::less;
    if (!result.distinctAncestorB)
        return std::partial_ordering::greater;
    bool isShadowRootA = result.distinctAncestorA->isShadowRoot();
    bool isShadowRootB = result.distinctAncestorB->isShadowRoot();
    if (isShadowRootA || isShadowRootB) {
        if (!isShadowRootB)
            return std::partial_ordering::less;
        if (!isShadowRootA)
            return std::partial_ordering::greater;
        ASSERT_NOT_REACHED();
        return std::partial_ordering::unordered;
    }
    return isSiblingSubsequent(*result.distinctAncestorA, *result.distinctAncestorB) ? std::partial_ordering::less : std::partial_ordering::greater;
}

template std::partial_ordering treeOrder<Tree>(const Node&, const Node&);
template std::partial_ordering treeOrder<ShadowIncludingTree>(const Node&, const Node&);
template std::partial_ordering treeOrder<ComposedTree>(const Node&, const Node&);

std::partial_ordering treeOrderForTesting(TreeType type, const Node& a, const Node& b)
{
    switch (type) {
    case Tree:
        return treeOrder<Tree>(a, b);
    case ShadowIncludingTree:
        return treeOrder<ShadowIncludingTree>(a, b);
    case ComposedTree:
        return treeOrder<ComposedTree>(a, b);
    }
    ASSERT_NOT_REACHED();
    return std::partial_ordering::unordered;
}

TextStream& operator<<(TextStream& ts, const Node& node)
{
    ts << node.debugDescription();
    return ts;
}

NodeIdentifier Node::nodeIdentifier() const
{
    return nodeIdentifiersMap().ensure(const_cast<Node&>(*this), [&] {
        setStateFlag(StateFlag::HasNodeIdentifier);
        return NodeIdentifier::generate();
    }).iterator->value;
}

Node* Node::fromIdentifier(NodeIdentifier identifier)
{
    for (auto& [node, nodeIdentifier] : nodeIdentifiersMap()) {
        if (nodeIdentifier == identifier)
            return node.ptr();
    }
    return nullptr;
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
