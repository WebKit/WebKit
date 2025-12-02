/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 *
 */

#pragma once

#include <WebCore/EventTarget.h>
#include <WebCore/NodeIdentifier.h>
#include <WebCore/PlatformExportMacros.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleValidity.h>
#include <bit>
#include <compare>
#include <new>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/CompactPointerTuple.h>
#include <wtf/CompactUniquePtrTuple.h>
#include <wtf/FastMalloc.h>
#include <wtf/FixedVector.h>
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/MainThread.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TypeCasts.h>
#include <wtf/URLHash.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/ASCIILiteral.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class ContainerNode;
class CustomElementRegistry;
class Document;
class Element;
class EventTypeInfo;
class FloatPoint;
class HTMLQualifiedName;
class HTMLSlotElement;
class IntRect;
class LayoutRect;
class MathMLQualifiedName;
class MutationObserver;
class MutationObserverRegistration;
class NamedNodeMap;
class Node;
class NodeList;
class NodeListsNodeData;
class NodeRareData;
class QualifiedName;
class RenderBox;
class RenderBoxModelObject;
class RenderObject;
class RenderStyle;
class SVGQualifiedName;
class ShadowRoot;
class TouchEvent;
class TreeScope;
class WebCoreOpaqueRoot;

struct SerializedNode;

enum class TextDirection : bool;

template<typename T> class ExceptionOr;

namespace Style {
struct PseudoElementIdentifier;
}

}

WTF_ALLOW_COMPACT_POINTERS_TO_INCOMPLETE_TYPE(WebCore::NodeRareData);

namespace WebCore {

class JSDOMGlobalObject;

enum class MutationObserverOptionType : uint8_t;
enum class TaskSource : uint8_t;
using MutationObserverOptions = OptionSet<MutationObserverOptionType>;
using MutationRecordDeliveryOptions = OptionSet<MutationObserverOptionType>;

enum class IsMutationBySetInnerHTML : uint8_t { No, Yes };

using NodeOrString = Variant<RefPtr<Node>, String>;

const int initialNodeVectorSize = 11; // Covers 99.5%. See webkit.org/b/80706
typedef Vector<Ref<Node>, initialNodeVectorSize> NodeVector;

class Node : public EventTarget, public CanMakeCheckedPtr<Node> {
    WTF_MAKE_PREFERABLY_COMPACT_TZONE_OR_ISO_ALLOCATED(Node);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(Node);

    friend class Document;
    friend class TreeScope;
public:
    enum NodeType {
        ELEMENT_NODE = 1,
        ATTRIBUTE_NODE = 2,
        TEXT_NODE = 3,
        CDATA_SECTION_NODE = 4,
        PROCESSING_INSTRUCTION_NODE = 7,
        COMMENT_NODE = 8,
        DOCUMENT_NODE = 9,
        DOCUMENT_TYPE_NODE = 10,
        DOCUMENT_FRAGMENT_NODE = 11,
    };
    enum DeprecatedNodeType {
        ENTITY_REFERENCE_NODE = 5,
        ENTITY_NODE = 6,
        NOTATION_NODE = 12,
    };
    enum DocumentPosition {
        DOCUMENT_POSITION_EQUIVALENT = 0x00,
        DOCUMENT_POSITION_DISCONNECTED = 0x01,
        DOCUMENT_POSITION_PRECEDING = 0x02,
        DOCUMENT_POSITION_FOLLOWING = 0x04,
        DOCUMENT_POSITION_CONTAINS = 0x08,
        DOCUMENT_POSITION_CONTAINED_BY = 0x10,
        DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC = 0x20,
    };

    static void dumpStatistics();

    virtual ~Node();
    void willBeDeletedFrom(Document&);

    // DOM methods & attributes for Node

    bool hasTagName(const HTMLQualifiedName&) const;
    bool hasTagName(const MathMLQualifiedName&) const;
    inline bool hasTagName(const SVGQualifiedName&) const;
    virtual String nodeName() const = 0;
    virtual String nodeValue() const;
    virtual ExceptionOr<void> setNodeValue(const String&);
    NodeType nodeType() const { return nodeTypeFromBitFields(m_typeBitFields); }
    virtual size_t approximateMemoryCost() const { return sizeof(*this); }
    ContainerNode* parentNode() const;
    inline RefPtr<ContainerNode> protectedParentNode() const;
    static constexpr ptrdiff_t parentNodeMemoryOffset() { return OBJECT_OFFSETOF(Node, m_parentNode); }
    inline Element* parentElement() const;
    inline RefPtr<Element> protectedParentElement() const;
    Node* previousSibling() const { return m_previousSibling; }
    RefPtr<Node> protectedPreviousSibling() const { return m_previousSibling; }
    static constexpr ptrdiff_t previousSiblingMemoryOffset() { return OBJECT_OFFSETOF(Node, m_previousSibling); }
    Node* nextSibling() const { return m_next.get(); }
    RefPtr<Node> protectedNextSibling() const { return m_next.get(); }
    static constexpr ptrdiff_t nextSiblingMemoryOffset() { return OBJECT_OFFSETOF(Node, m_next); }
    WEBCORE_EXPORT RefPtr<NodeList> childNodes();
    inline Node* firstChild() const;
    inline RefPtr<Node> protectedFirstChild() const;
    inline Node* lastChild() const;
    inline RefPtr<Node> protectedLastChild() const;
    inline bool hasAttributes() const;
    inline NamedNodeMap* attributesMap() const;
    Node* pseudoAwareNextSibling() const;
    Node* pseudoAwarePreviousSibling() const;
    Node* pseudoAwareFirstChild() const;
    Node* pseudoAwareLastChild() const;

    WEBCORE_EXPORT const URL& baseURI() const;
    
    void getSubresourceURLs(ListHashSet<URL>&) const;
    void getCandidateSubresourceURLs(ListHashSet<URL>&) const;

    WEBCORE_EXPORT ExceptionOr<void> insertBefore(Node& newChild, RefPtr<Node>&& refChild);
    WEBCORE_EXPORT ExceptionOr<void> replaceChild(Node& newChild, Node& oldChild);
    WEBCORE_EXPORT ExceptionOr<void> removeChild(Node& child);
    WEBCORE_EXPORT ExceptionOr<void> appendChild(Node& newChild);

    inline bool hasChildNodes() const;

    enum class CloningOperation {
        SelfOnly,
        SelfWithTemplateContent,
        Everything,
    };
    virtual Ref<Node> cloneNodeInternal(Document&, CloningOperation, CustomElementRegistry*) const = 0;
    virtual SerializedNode serializeNode(CloningOperation) const = 0;
    Ref<Node> cloneNode(bool deep) const;
    WEBCORE_EXPORT ExceptionOr<Ref<Node>> cloneNodeForBindings(bool deep) const;

    virtual const AtomString& localName() const;
    virtual const AtomString& namespaceURI() const;
    virtual const AtomString& prefix() const;
    virtual ExceptionOr<void> setPrefix(const AtomString&);
    WEBCORE_EXPORT ExceptionOr<void> normalize();

    bool isSameNode(Node* other) const { return this == other; }
    WEBCORE_EXPORT bool isEqualNode(Node*) const;
    WEBCORE_EXPORT bool isDefaultNamespace(const AtomString& namespaceURI) const;
    WEBCORE_EXPORT const AtomString& lookupPrefix(const AtomString& namespaceURI) const;
    WEBCORE_EXPORT const AtomString& lookupNamespaceURI(const AtomString& prefix) const;

    WEBCORE_EXPORT String textContent(bool convertBRsToNewlines = false) const;
    WEBCORE_EXPORT ExceptionOr<void> setTextContent(String&&);
    
    Node* lastDescendant() const;
    Node* firstDescendant() const;

    // From the NonDocumentTypeChildNode - https://dom.spec.whatwg.org/#nondocumenttypechildnode
    WEBCORE_EXPORT Element* previousElementSibling() const;
    WEBCORE_EXPORT Element* nextElementSibling() const;

    // From the ChildNode - https://dom.spec.whatwg.org/#childnode
    ExceptionOr<void> before(FixedVector<NodeOrString>&&);
    ExceptionOr<void> after(FixedVector<NodeOrString>&&);
    ExceptionOr<void> replaceWith(FixedVector<NodeOrString>&&);
    WEBCORE_EXPORT ExceptionOr<void> remove();

    // Other methods (not part of DOM)

    bool isElementNode() const { return hasTypeFlag(TypeFlag::IsElement); }
    bool isContainerNode() const { return hasTypeFlag(TypeFlag::IsContainerNode); }
    bool isCharacterDataNode() const { return hasTypeFlag(TypeFlag::IsCharacterData); }
    bool isTextNode() const { return hasTypeFlag(TypeFlag::IsText); }
    bool isHTMLElement() const { return hasTypeFlag(TypeFlag::IsHTMLElement); }
    bool isSVGElement() const { return hasTypeFlag(TypeFlag::IsSVGElement); }
    bool isMathMLElement() const { return hasTypeFlag(TypeFlag::IsMathMLElement); }

    bool isUnknownElement() const { return hasTypeFlag(TypeFlag::IsUnknownElement); }
    bool isHTMLUnknownElement() const { return isHTMLElement() && isUnknownElement(); }
    bool isSVGUnknownElement() const { return isSVGElement() && isUnknownElement(); }
    bool isMathMLUnknownElement() const { return isMathMLElement() && isUnknownElement(); }

    bool isFormControlElement() const { return isElementNode() && hasTypeFlag(TypeFlag::IsShadowRootOrFormControlElement); }

    bool isPseudoElement() const { return isElementNode() && hasTypeFlag(TypeFlag::IsPseudoElementOrSpecialInternalNode); }
    inline bool isBeforePseudoElement() const;
    inline bool isAfterPseudoElement() const;
    inline std::optional<PseudoElementType> pseudoElementType() const;
    inline std::optional<Style::PseudoElementIdentifier> pseudoElementIdentifier() const;

#if ENABLE(VIDEO)
    virtual bool isWebVTTElement() const { return false; }
#endif
    bool isStyledElement() const { return hasTypeFlag(TypeFlag::IsHTMLElement) || hasTypeFlag(TypeFlag::IsSVGElement) || hasTypeFlag(TypeFlag::IsMathMLElement); }
    virtual bool isAttributeNode() const { return false; }
    virtual bool isHTMLFrameOwnerElement() const { return false; }
    virtual bool isPluginElement() const { return false; }

    bool isDocumentNode() const { return nodeType() == DOCUMENT_NODE; }
    bool isTreeScope() const { return isDocumentNode() || isShadowRoot(); }
    bool isDocumentFragment() const { return nodeType() == DOCUMENT_FRAGMENT_NODE; }
    bool isShadowRoot() const { return isDocumentFragment() && hasTypeFlag(TypeFlag::IsShadowRootOrFormControlElement); }
    inline bool isUserAgentShadowRoot() const; // Defined in NodeInlines.h

    bool hasCustomStyleResolveCallbacks() const { return hasTypeFlag(TypeFlag::HasCustomStyleResolveCallbacks); }

    bool hasSyntheticAttrChildNodes() const { return hasEventTargetFlag(EventTargetFlag::HasSyntheticAttrChildNodes); }
    void setHasSyntheticAttrChildNodes(bool flag) { setEventTargetFlag(EventTargetFlag::HasSyntheticAttrChildNodes, flag); }

    bool hasShadowRootContainingSlots() const { return hasEventTargetFlag(EventTargetFlag::HasShadowRootContainingSlots); }
    void setHasShadowRootContainingSlots(bool flag) { setEventTargetFlag(EventTargetFlag::HasShadowRootContainingSlots, flag); }

    bool needsSVGRendererUpdate() const { return hasStateFlag(StateFlag::NeedsSVGRendererUpdate); }
    void setNeedsSVGRendererUpdate(bool flag) { setStateFlag(StateFlag::NeedsSVGRendererUpdate, flag); }

    // If this node is in a shadow tree, returns its shadow host. Otherwise, returns null.
    WEBCORE_EXPORT Element* shadowHost() const;
    RefPtr<Element> protectedShadowHost() const;
    ShadowRoot* containingShadowRoot() const;
    RefPtr<ShadowRoot> protectedContainingShadowRoot() const;
    inline ShadowRoot* shadowRoot() const; // Defined in ElementRareData.h
    inline RefPtr<ShadowRoot> protectedShadowRoot() const; // Defined in ElementRareData.h
    bool isClosedShadowHidden(const Node&) const;

    HTMLSlotElement* assignedSlot() const;
    HTMLSlotElement* assignedSlotForBindings() const;
    HTMLSlotElement* manuallyAssignedSlot() const;
    void setManuallyAssignedSlot(HTMLSlotElement*);

    bool hasEverPaintedImages() const;
    void setHasEverPaintedImages(bool);

    bool isUncustomizedCustomElement() const { return customElementState() == CustomElementState::Uncustomized; }
    bool isCustomElementUpgradeCandidate() const { return customElementState() == CustomElementState::Undefined; }
    bool isDefinedCustomElement() const { return customElementState() == CustomElementState::Custom; }
    bool isFailedCustomElement() const { return customElementState() == CustomElementState::FailedOrPrecustomized && isUnknownElement(); }
    bool isFailedOrPrecustomizedCustomElement() const { return customElementState() == CustomElementState::FailedOrPrecustomized; }
    bool isPrecustomizedCustomElement() const { return customElementState() == CustomElementState::FailedOrPrecustomized && !isUnknownElement(); }
    bool isPrecustomizedOrDefinedCustomElement() const { return isPrecustomizedCustomElement() || isDefinedCustomElement(); }

    bool isInCustomElementReactionQueue() const { return hasStateFlag(StateFlag::IsInCustomElementReactionQueue); }
    void setIsInCustomElementReactionQueue() { setStateFlag(StateFlag::IsInCustomElementReactionQueue); }
    void clearIsInCustomElementReactionQueue() { clearStateFlag(StateFlag::IsInCustomElementReactionQueue); }

    bool usesNullCustomElementRegistry() const { return hasStateFlag(StateFlag::UsesNullCustomElementRegistry); }
    void setUsesNullCustomElementRegistry() const { setStateFlag(StateFlag::UsesNullCustomElementRegistry); }
    void clearUsesNullCustomElementRegistry() const { clearStateFlag(StateFlag::UsesNullCustomElementRegistry); }

    bool usesScopedCustomElementRegistryMap() const { return hasStateFlag(StateFlag::UsesScopedCustomElementRegistryMap); }
    void setUsesScopedCustomElementRegistryMap() { setStateFlag(StateFlag::UsesScopedCustomElementRegistryMap); }
    void clearUsesScopedCustomElementRegistryMap() { clearStateFlag(StateFlag::UsesScopedCustomElementRegistryMap); }

    // Returns null, a child of ShadowRoot, or a legacy shadow root.
    Node* nonBoundaryShadowTreeRootNode();

    // Node's parent or shadow tree host.
    inline ContainerNode* parentOrShadowHostNode() const; // Defined in NodeInlines.h
    inline RefPtr<ContainerNode> protectedParentOrShadowHostNode() const; // Defined in NodeInlines.h
    ContainerNode* parentInComposedTree() const;
    WEBCORE_EXPORT Element* parentElementInComposedTree() const;
    Element* parentOrShadowHostElement() const;
    inline void setParentNode(ContainerNode*);
    inline Node& rootNode() const;
    WEBCORE_EXPORT Node& traverseToRootNode() const;
    Node& shadowIncludingRoot() const;

    struct GetRootNodeOptions {
        bool composed;
    };
    Node& getRootNode(const GetRootNodeOptions&) const;
    
    inline WebCoreOpaqueRoot opaqueRoot() const;
    WebCoreOpaqueRoot traverseToOpaqueRoot() const;

    void queueTaskKeepingThisNodeAlive(TaskSource, Function<void ()>&&);
    void queueTaskToDispatchEvent(TaskSource, Ref<Event>&&);

    // Use when it's guaranteed to that shadowHost is null.
    inline ContainerNode* parentNodeGuaranteedHostFree() const;
    // Returns the parent node, but null if the parent node is a ShadowRoot.
    ContainerNode* nonShadowBoundaryParentNode() const;

    bool selfOrPrecedingNodesAffectDirAuto() const { return hasStateFlag(StateFlag::SelfOrPrecedingNodesAffectDirAuto); }
    void setSelfOrPrecedingNodesAffectDirAuto(bool flag) { setStateFlag(StateFlag::SelfOrPrecedingNodesAffectDirAuto, flag); }

    TextDirection effectiveTextDirection() const;
    void setEffectiveTextDirection(TextDirection);

    bool usesEffectiveTextDirection() const { return rareDataBitfields().usesEffectiveTextDirection; }
    void setUsesEffectiveTextDirection(bool);

    // Returns the enclosing event parent Element (or self) that, when clicked, would trigger a navigation.
    WEBCORE_EXPORT Element* enclosingLinkEventParentOrSelf();

    // These low-level calls give the caller responsibility for maintaining the integrity of the tree.
    void setPreviousSibling(Node* previous) { m_previousSibling = previous; }
    void setNextSibling(Node* next) { m_next = next; }

    virtual bool canContainRangeEndPoint() const { return false; }

    WEBCORE_EXPORT bool isRootEditableElement() const;
    WEBCORE_EXPORT Element* rootEditableElement() const;

    // For <link> and <style> elements.
    virtual bool sheetLoaded() { return true; }
    virtual void notifyLoadedSheetAndAllCriticalSubresources(bool /* error loading subresource */) { }
    virtual void startLoadingDynamicSheet() { ASSERT_NOT_REACHED(); }

    bool isUserActionElement() const { return hasStateFlag(StateFlag::IsUserActionElement); }
    void setUserActionElement(bool flag) { setStateFlag(StateFlag::IsUserActionElement, flag); }

    bool inRenderedDocument() const;
    bool needsStyleRecalc() const { return styleValidity() != Style::Validity::Valid || hasInvalidRenderer(); }
    Style::Validity styleValidity() const { return styleBitfields().styleValidity(); }
    bool hasInvalidRenderer() const { return hasStateFlag(StateFlag::HasInvalidRenderer); }
    bool styleResolutionShouldRecompositeLayer() const { return hasStateFlag(StateFlag::StyleResolutionShouldRecompositeLayer); }
    bool childNeedsStyleRecalc() const { return hasStyleFlag(NodeStyleFlag::DescendantNeedsStyleResolution); }
    bool isEditingText() const { return isTextNode() && hasTypeFlag(TypeFlag::IsPseudoElementOrSpecialInternalNode); }

    bool isDocumentFragmentForInnerOuterHTML() const { return isDocumentFragment() && hasTypeFlag(TypeFlag::IsPseudoElementOrSpecialInternalNode); }

    bool hasHeldBackChildrenChanged() const { return hasStateFlag(StateFlag::HasHeldBackChildrenChanged); }
    void setHasHeldBackChildrenChanged() { setStateFlag(StateFlag::HasHeldBackChildrenChanged); }
    void clearHasHeldBackChildrenChanged() { clearStateFlag(StateFlag::HasHeldBackChildrenChanged); }

    bool hasDidMutateSubtreeAfterSetInnerHTML() const { return hasStateFlag(StateFlag::DidMutateSubtreeAfterSetInnerHTML); }
    void setDidMutateSubtreeAfterSetInnerHTML() { setStateFlag(StateFlag::DidMutateSubtreeAfterSetInnerHTML); }
    void clearDidMutateSubtreeAfterSetInnerHTML() { clearStateFlag(StateFlag::DidMutateSubtreeAfterSetInnerHTML); }
    void setDidMutateSubtreeAfterSetInnerHTMLOnAncestors();

    bool hasWasParsedWithFastPath() const { return hasStateFlag(StateFlag::WasParsedWithFastPath); }
    void setWasParsedWithFastPath() { setStateFlag(StateFlag::WasParsedWithFastPath); }
    void clearWasParsedWithFastPath() { clearStateFlag(StateFlag::WasParsedWithFastPath); }

    void setChildNeedsStyleRecalc() { setStyleFlag(NodeStyleFlag::DescendantNeedsStyleResolution); }
    inline void clearChildNeedsStyleRecalc();

    inline void setHasValidStyle();

    WEBCORE_EXPORT bool isContentEditable() const;
    bool isContentRichlyEditable() const;

    WEBCORE_EXPORT void inspect();

    enum class UserSelectAllTreatment : bool { NotEditable, Editable };
    bool hasEditableStyle(UserSelectAllTreatment treatment = UserSelectAllTreatment::NotEditable) const
    {
        return computeEditability(treatment, ShouldUpdateStyle::DoNotUpdate) != Editability::ReadOnly;
    }

    // FIXME: Replace every use of this function by helpers in Editing.h
    bool hasRichlyEditableStyle() const
    {
        return computeEditability(UserSelectAllTreatment::NotEditable, ShouldUpdateStyle::DoNotUpdate) == Editability::CanEditRichly;
    }

    enum class Editability { ReadOnly, CanEditPlainText, CanEditRichly };
    enum class ShouldUpdateStyle { Update, DoNotUpdate };
    WEBCORE_EXPORT Editability computeEditability(UserSelectAllTreatment, ShouldUpdateStyle) const;
    Editability computeEditabilityWithStyle(const RenderStyle*, UserSelectAllTreatment, ShouldUpdateStyle) const;

    WEBCORE_EXPORT LayoutRect absoluteBoundingRect(bool* isReplaced);
    inline IntRect pixelSnappedAbsoluteBoundingRect(bool* isReplaced); // Defined in NodeInlines.h

    WEBCORE_EXPORT unsigned computeNodeIndex() const;

    // Returns the DOM ownerDocument attribute. This method never returns null, except in the case
    // of a Document node.
    WEBCORE_EXPORT Document* ownerDocument() const;

    // Returns the document associated with this node. A document node returns itself.
    inline Document& document() const; // Defined in NodeDocument.h
    inline Ref<Document> protectedDocument() const; // Defined in NodeDocument.h

    TreeScope& treeScope() const
    {
        ASSERT(m_treeScope);
        return *m_treeScope;
    }
    inline Ref<TreeScope> protectedTreeScope() const; // Defined in NodeInlines.h
    inline void setTreeScopeRecursively(TreeScope&);
    static constexpr ptrdiff_t treeScopeMemoryOffset() { return OBJECT_OFFSETOF(Node, m_treeScope); }

    TreeScope& treeScopeForSVGReferences() const;

    // Returns true if this node is associated with a document and is in its associated document's
    // node tree, false otherwise (https://dom.spec.whatwg.org/#connected).
    bool isConnected() const { return hasEventTargetFlag(EventTargetFlag::IsConnected); }
    bool isInUserAgentShadowTree() const;
    bool isInShadowTree() const { return hasEventTargetFlag(EventTargetFlag::IsInShadowTree); }
    bool isInTreeScope() const { return isConnected() || isInShadowTree(); }
    bool hasBeenInUserAgentShadowTree() const { return hasEventTargetFlag(EventTargetFlag::HasBeenInUserAgentShadowTree); }

    // https://dom.spec.whatwg.org/#in-a-document-tree
    bool isInDocumentTree() const { return isConnected() && !isInShadowTree(); }

    bool isDocumentTypeNode() const { return nodeType() == DOCUMENT_TYPE_NODE; }
    virtual bool childTypeAllowed(NodeType) const { return false; }
    inline unsigned countChildNodes() const;
    inline unsigned length() const;
    inline Node* traverseToChildAt(unsigned) const;

    ExceptionOr<void> checkSetPrefix(const AtomString& prefix);

    // https://dom.spec.whatwg.org/#concept-tree-descendant
    WEBCORE_EXPORT bool isDescendantOf(const Node&) const;
    bool isDescendantOf(const Node* other) const { return other && isDescendantOf(*other); }

    // https://dom.spec.whatwg.org/#concept-tree-inclusive-descendant
    ALWAYS_INLINE bool isInclusiveDescendantOf(const Node& other) const { return this == &other || isDescendantOf(other); }
    ALWAYS_INLINE bool isInclusiveDescendantOf(const Node* other) const { return other && isInclusiveDescendantOf(*other); }

    // https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor
    ALWAYS_INLINE bool contains(const Node& other) const { return other.isInclusiveDescendantOf(*this); }
    ALWAYS_INLINE bool contains(const Node* other) const { return other && contains(*other); }

    // https://dom.spec.whatwg.org/#concept-shadow-including-descendant
    WEBCORE_EXPORT bool isShadowIncludingDescendantOf(const Node&) const;
    ALWAYS_INLINE bool isShadowIncludingDescendantOf(const Node* other) const { return other && isShadowIncludingDescendantOf(*other); }

    // https://dom.spec.whatwg.org/#concept-shadow-including-inclusive-ancestor
    ALWAYS_INLINE bool isShadowIncludingInclusiveAncestorOf(const Node& other) const { return this == &other || other.isShadowIncludingDescendantOf(*this); }
    ALWAYS_INLINE bool isShadowIncludingInclusiveAncestorOf(const Node* other) const { return other && isShadowIncludingInclusiveAncestorOf(*other); }

    bool isComposedTreeDescendantOf(const Node&) const;

    // Whether or not a selection can be started in this object
    virtual bool canStartSelection() const;

    virtual bool shouldSelectOnMouseDown() { return false; }

    // Getting points into and out of screen space
    FloatPoint convertToPage(const FloatPoint&) const;
    FloatPoint convertFromPage(const FloatPoint&) const;

    // -----------------------------------------------------------------------------
    // Integration with rendering tree

    RenderObject* renderer() const { return m_renderer; }
    inline CheckedPtr<RenderObject> checkedRenderer() const; // Defined in NodeInlines.h
    void setRenderer(RenderObject*); // Defined in NodeInlines.h

    // Use these two methods with caution.
    inline RenderBox* renderBox() const; // Defined in NodeInlines.h
    inline CheckedPtr<RenderBox> checkedRenderBox() const; // Defined in NodeInlines.h
    inline RenderBoxModelObject* renderBoxModelObject() const; // Defined in NodeInlines.h

    // Wrapper for nodes that don't have a renderer, but still cache the style (like HTMLOptionElement).
    inline const RenderStyle* renderStyle() const; // Defined in NodeRenderStyle.h

    WEBCORE_EXPORT const RenderStyle* computedStyle();
    virtual const RenderStyle* computedStyle(const std::optional<Style::PseudoElementIdentifier>&);

    enum class InsertedIntoAncestorResult {
        Done,
        NeedsPostInsertionCallback,
    };
    struct InsertionType {
        bool connectedToDocument { false };
        bool treeScopeChanged { false };
    };
    // Called *after* this node or its ancestor is inserted into a new parent (may or may not be a part of document) by scripts or parser.
    // insertedInto **MUST NOT** invoke scripts. Return NeedsPostInsertionCallback and implement didFinishInsertingNode instead to run scripts.
    virtual InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode& parentOfInsertedTree);
    virtual void didFinishInsertingNode() { }

    struct RemovalType {
        bool disconnectedFromDocument { false };
        bool treeScopeChanged { false };
    };
    virtual void removedFromAncestor(RemovalType, ContainerNode& oldParentOfRemovedTree);

    virtual String description() const;
    virtual String debugDescription() const;

#if ENABLE(TREE_DEBUGGING)
    void showNode(ASCIILiteral prefix = ""_s) const;
    WEBCORE_EXPORT void showTreeForThis() const;
    void showNodePathForThis() const;
    void showTreeAndMark(const Node* markedNode1, ASCIILiteral markedLabel1, const Node* markedNode2 = nullptr, ASCIILiteral markedLabel2 = { }) const;
    void showTreeForThisAcrossFrame() const;
#endif // ENABLE(TREE_DEBUGGING)

    void invalidateNodeListAndCollectionCachesInAncestors();
    void invalidateNodeListCollectionAndInnerHTMLPrefixCachesInAncestorsForAttribute(const QualifiedName&, const IsMutationBySetInnerHTML = IsMutationBySetInnerHTML::No);
    NodeListsNodeData* nodeLists();
    void clearNodeLists();

    virtual bool willRespondToMouseMoveEvents() const;
    WEBCORE_EXPORT bool willRespondToMouseClickEvents(const RenderStyle* = nullptr) const;
    Editability computeEditabilityForMouseClickEvents(const RenderStyle* = nullptr) const;
    virtual bool willRespondToMouseClickEventsWithEditability(Editability) const;
    virtual bool willRespondToTouchEvents() const;

    WEBCORE_EXPORT unsigned short compareDocumentPosition(Node&);

    enum EventTargetInterfaceType eventTargetInterface() const override;
    ScriptExecutionContext* scriptExecutionContext() const final;
    inline RefPtr<ScriptExecutionContext> protectedScriptExecutionContext() const;

    WEBCORE_EXPORT bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) override;
    bool removeEventListener(const AtomString& eventType, EventListener&, const EventListenerOptions&) override;
    void removeAllEventListeners() override;

    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&) override;

    void dispatchScopedEvent(Event&);

    void dispatchSubtreeModifiedEvent();
    void dispatchDOMActivateEvent(Event& underlyingClickEvent);

    void dispatchWebKitSubmitEvent(Event& underlyingSubmitEvent);

#if ENABLE(TOUCH_EVENTS)
    virtual bool allowsDoubleTapGesture() const { return true; }
#endif

    WEBCORE_EXPORT void dispatchInputEvent();

    // Perform the default action for an event.
    virtual void defaultEventHandler(Event&);

    ALWAYS_INLINE void ref() const;
    ALWAYS_INLINE void deref() const;
    ALWAYS_INLINE bool hasOneRef() const;
    ALWAYS_INLINE unsigned refCount() const;
    void applyRefDuringDestructionCheck() const;

    inline void relaxAdoptionRequirement();

    HashMap<Ref<MutationObserver>, MutationRecordDeliveryOptions> registeredMutationObservers(MutationObserverOptionType, const QualifiedName* attributeName);
    void registerMutationObserver(MutationObserver&, MutationObserverOptions, const MemoryCompactLookupOnlyRobinHoodHashSet<AtomString>& attributeFilter);
    void unregisterMutationObserver(MutationObserverRegistration&);
    void registerTransientMutationObserver(MutationObserverRegistration&);
    void unregisterTransientMutationObserver(MutationObserverRegistration&);
    void notifyMutationObserversNodeWillDetach();

    unsigned connectedSubframeCount() const { return rareDataBitfields().connectedSubframeCount; }
    void incrementConnectedSubframeCount(unsigned amount = 1);
    void decrementConnectedSubframeCount(unsigned amount = 1);
    void updateAncestorConnectedSubframeCountForRemoval() const;
    void updateAncestorConnectedSubframeCountForInsertion() const;

#if ENABLE(JIT)
    static constexpr ptrdiff_t typeFlagsMemoryOffset() { return OBJECT_OFFSETOF(Node, m_typeBitFields); }
    static constexpr ptrdiff_t stateFlagsMemoryOffset() { return OBJECT_OFFSETOF(Node, m_stateFlags); }
    static constexpr ptrdiff_t rareDataMemoryOffset() { return OBJECT_OFFSETOF(Node, m_rareDataWithBitfields); }
#if CPU(ADDRESS64)
    static uint64_t rareDataPointerMask() { return CompactPointerTuple<NodeRareData*, uint16_t>::pointerMask; }
#else
    static uint32_t rareDataPointerMask() { return -1; }
#endif
    static auto flagIsText() { return enumToUnderlyingType(TypeFlag::IsText); }
    static auto flagIsContainer() { return enumToUnderlyingType(TypeFlag::IsContainerNode); }
    static auto flagIsElement() { return enumToUnderlyingType(TypeFlag::IsElement); }
    static auto flagIsHTML() { return enumToUnderlyingType(TypeFlag::IsHTMLElement); }
    static auto flagIsLink() { return enumToUnderlyingType(StateFlag::IsLink); }
    static auto flagIsParsingChildren() { return enumToUnderlyingType(StateFlag::IsParsingChildren); }
#endif // ENABLE(JIT)

#if ASSERT_ENABLED
    bool deletionHasBegun() const { return m_deletionHasBegun; }
#endif

    bool containsSelectionEndPoint() const { return hasStateFlag(StateFlag::ContainsSelectionEndPoint); }
    void setContainsSelectionEndPoint(bool value) { setStateFlag(StateFlag::ContainsSelectionEndPoint, value); }

    WEBCORE_EXPORT NodeIdentifier nodeIdentifier() const;
    WEBCORE_EXPORT static Node* fromIdentifier(NodeIdentifier);

protected:
    enum class TypeFlag : uint16_t {
        IsCharacterData = 1 << 0,
        IsText = 1 << 1,
        IsContainerNode = 1 << 2,
        IsElement = 1 << 3,
        IsHTMLElement = 1 << 4,
        IsSVGElement = 1 << 5,
        IsMathMLElement = 1 << 6,
        IsShadowRootOrFormControlElement = 1 << 7,
        IsUnknownElement = 1 << 8,
        IsPseudoElementOrSpecialInternalNode = 1 << 9,
        HasCustomStyleResolveCallbacks = 1 << 10,
        HasDidMoveToNewDocument = 1 << 11,
    };
    static constexpr auto typeFlagBitCount = 12;

    static uint16_t constructBitFieldsFromNodeTypeAndFlags(NodeType type, OptionSet<TypeFlag> flags) { return (type << typeFlagBitCount) | flags.toRaw(); }
    static NodeType nodeTypeFromBitFields(uint16_t bitFields) { return static_cast<NodeType>((bitFields >> typeFlagBitCount) & 0xf); }
    // Don't bother masking with (1 << typeFlagBitCount) - 1 since OptionSet tolerates the upper 4-bits being used for other purposes.
    bool hasTypeFlag(TypeFlag flag) const { return OptionSet<TypeFlag>::fromRaw(m_typeBitFields).contains(flag); }

    enum class StateFlag : uint32_t {
        IsLink = 1 << 0,
        IsParsingChildren = 1 << 1,
        SelfOrPrecedingNodesAffectDirAuto = 1 << 2,
        EffectiveLangKnownToMatchDocumentElement = 1 << 3,
        IsComputedStyleInvalidFlag = 1 << 4,
        HasInvalidRenderer = 1 << 5,
        StyleResolutionShouldRecompositeLayer = 1 << 6,
        ContainsOnlyASCIIWhitespace = 1 << 7, // Only used on CharacterData.
        ContainsOnlyASCIIWhitespaceIsValid = 1 << 8, // Only used on CharacterData.
        HasHeldBackChildrenChanged = 1 << 9,
        ContainsSelectionEndPoint = 1 << 10,
        HasNodeIdentifier = 1 << 11,
        IsUserActionElement = 1 << 12,
        IsInCustomElementReactionQueue = 1 << 13,
        UsesNullCustomElementRegistry = 1 << 14,
        UsesScopedCustomElementRegistryMap = 1 << 15,
        NeedsSVGRendererUpdate = 1 << 16,
        NeedsUpdateQueryContainerDependentStyle = 1 << 17,
        EverHadSmoothScroll = 1 << 18,
#if ENABLE(FULLSCREEN_API)
        IsFullscreen = 1 << 19,
#endif
        IsShadowRootAttachedEventPending = 1 << 20,
        InLargestContentfulPaintTextContentSet = 1 << 21,
        DidMutateSubtreeAfterSetInnerHTML = 1 << 22,
        WasParsedWithFastPath = 1 << 23
        // 8 bits free.
    };

    enum class TabIndexState : uint8_t {
        NotSet = 0,
        Zero = 1,
        NegativeOne = 2,
        InRareData = 3,
    };

    enum class CustomElementState : uint8_t {
        Uncustomized = 0,
        Undefined = 1,
        Custom = 2,
        FailedOrPrecustomized = 3,
    };

    struct RareDataBitFields {
        uint16_t connectedSubframeCount : 10;
        uint16_t tabIndexState : 2;
        uint16_t customElementState : 2;
        uint16_t usesEffectiveTextDirection : 1;
        uint16_t effectiveTextDirection : 1;
    };

    bool hasStateFlag(StateFlag flag) const { return m_stateFlags.contains(flag); }
    void setStateFlag(StateFlag flag, bool value = true) const { m_stateFlags.set(flag, value); }
    void clearStateFlag(StateFlag flag) const { setStateFlag(flag, false); }

    RareDataBitFields rareDataBitfields() const { return std::bit_cast<RareDataBitFields>(m_rareDataWithBitfields.type()); }
    void setRareDataBitfields(RareDataBitFields bitfields) { m_rareDataWithBitfields.setType(std::bit_cast<uint16_t>(bitfields)); }

    TabIndexState tabIndexState() const { return static_cast<TabIndexState>(rareDataBitfields().tabIndexState); }
    inline void setTabIndexState(TabIndexState);

    CustomElementState customElementState() const { return static_cast<CustomElementState>(rareDataBitfields().customElementState); }
    void setCustomElementState(CustomElementState);

    bool isParsingChildrenFinished() const { return !hasStateFlag(StateFlag::IsParsingChildren); }
    void setIsParsingChildrenFinished() { clearStateFlag(StateFlag::IsParsingChildren); }
    void clearIsParsingChildrenFinished() { setStateFlag(StateFlag::IsParsingChildren); }

    Node(Document&, NodeType, OptionSet<TypeFlag>);

    static constexpr uint32_t s_refCountIncrement = 2;
    static constexpr uint32_t s_refCountMask = ~static_cast<uint32_t>(1);

    enum class NodeStyleFlag : uint16_t {
        DescendantNeedsStyleResolution                          = 1 << 0,
        DirectChildNeedsStyleResolution                         = 1 << 1,

        AffectedByHasWithPositionalPseudoClass                  = 1 << 2,
        ChildrenAffectedByFirstChildRules                       = 1 << 3,
        ChildrenAffectedByLastChildRules                        = 1 << 4,
        AffectsNextSiblingElementStyle                          = 1 << 5,
        StyleIsAffectedByPreviousSibling                        = 1 << 6,
        DescendantsAffectedByPreviousSibling                    = 1 << 7,
        StyleAffectedByEmpty                                    = 1 << 8,
        // We optimize for :first-child and :last-child. The other positional child selectors like nth-child or
        // *-child-of-type, we will just give up and re-evaluate whenever children change at all.
        ChildrenAffectedByForwardPositionalRules                = 1 << 9,
        DescendantsAffectedByForwardPositionalRules             = 1 << 10,
        ChildrenAffectedByBackwardPositionalRules               = 1 << 11,
        DescendantsAffectedByBackwardPositionalRules            = 1 << 12,
    };

    struct StyleBitfields {
    public:
        static StyleBitfields fromRaw(uint16_t packed) { return std::bit_cast<StyleBitfields>(packed); }
        uint16_t toRaw() const { return std::bit_cast<uint16_t>(*this); }

        Style::Validity styleValidity() const { return static_cast<Style::Validity>(m_styleValidity); }
        void setStyleValidity(Style::Validity validity) { m_styleValidity = enumToUnderlyingType(validity); }

        OptionSet<NodeStyleFlag> flags() const { return OptionSet<NodeStyleFlag>::fromRaw(m_flags); }
        void setFlag(NodeStyleFlag flag) { m_flags = (flags() | flag).toRaw(); }
        void clearFlag(NodeStyleFlag flag) { m_flags = (flags() - flag).toRaw(); }
        void clearFlags(OptionSet<NodeStyleFlag> flagsToClear) { m_flags = (flags() - flagsToClear).toRaw(); }
        void clearDescendantsNeedStyleResolution() { m_flags = (flags() - NodeStyleFlag::DescendantNeedsStyleResolution - NodeStyleFlag::DirectChildNeedsStyleResolution).toRaw(); }

    private:
        uint16_t m_styleValidity : 3 { 0 };
        uint16_t m_flags : 13 { 0 };
    };

    StyleBitfields styleBitfields() const { return m_styleBitfields; }
    void setStyleBitfields(StyleBitfields bitfields) { m_styleBitfields = bitfields; }
    ALWAYS_INLINE bool hasStyleFlag(NodeStyleFlag flag) const { return styleBitfields().flags().contains(flag); }
    ALWAYS_INLINE void setStyleFlag(NodeStyleFlag);
    ALWAYS_INLINE void clearStyleFlags(OptionSet<NodeStyleFlag>);

    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const { }
    virtual void addCandidateSubresourceURLs(ListHashSet<URL>&) const { }

    bool hasRareData() const { return !!m_rareDataWithBitfields.pointer(); }
    NodeRareData* rareData() const { return m_rareDataWithBitfields.pointer(); }
    NodeRareData& ensureRareData();
    void clearRareData();

    void setTreeScope(TreeScope&);

    void invalidateStyle(Style::Validity, Style::InvalidationMode = Style::InvalidationMode::Normal);
    void updateAncestorsForStyleRecalc();
    void markAncestorsForInvalidatedStyle();

    template<typename NodeClass>
    static NodeClass& traverseToRootNodeInternal(const NodeClass&);

    // FIXME: Replace all uses of convertNodesOrStringsIntoNode by convertNodesOrStringsIntoNodeVector.
    ExceptionOr<RefPtr<Node>> convertNodesOrStringsIntoNode(FixedVector<NodeOrString>&&);
    ExceptionOr<NodeVector> convertNodesOrStringsIntoNodeVector(FixedVector<NodeOrString>&&);

private:
    WEBCORE_EXPORT void removedLastRef();

    void refEventTarget() final;
    void derefEventTarget() final;

    void trackForDebugging();
    void materializeRareData();

    Vector<std::unique_ptr<MutationObserverRegistration>>* mutationObserverRegistry();
    WeakHashSet<MutationObserverRegistration>* transientMutationObserverRegistry();

    void adjustStyleValidity(Style::Validity, Style::InvalidationMode);

    static unsigned moveShadowTreeToNewDocumentFastCase(ShadowRoot&, Document& oldDocument, Document& newDocument);
    static void moveShadowTreeToNewDocumentSlowCase(ShadowRoot&, Document& oldDocument, Document& newDocument);
    static void moveTreeToNewScope(Node&, TreeScope& oldScope, TreeScope& newScope);
    void moveNodeToNewDocumentFastCase(Document& oldDocument, Document& newDocument);
    void moveNodeToNewDocumentSlowCase(Document& oldDocument, Document& newDocument);

    WEBCORE_EXPORT void notifyInspectorOfRendererChange();

#if ASSERT_ENABLED
    mutable bool m_inRemovedLastRefFunction { false };
    bool m_adoptionIsRequired { true };
    bool m_deletionHasBegun { false };

    friend inline void adopted(Node*);
#endif

    mutable uint32_t m_refCountAndParentBit { s_refCountIncrement };
    const uint16_t m_typeBitFields;
    mutable StyleBitfields m_styleBitfields;
    mutable OptionSet<StateFlag> m_stateFlags;

    CheckedPtr<ContainerNode> m_parentNode;
    TreeScope* m_treeScope { nullptr };
    Node* m_previousSibling { nullptr };
    CheckedPtr<Node> m_next;
    RenderObject* m_renderer { nullptr };
    CompactUniquePtrTuple<NodeRareData, uint16_t> m_rareDataWithBitfields;
};

bool connectedInSameTreeScope(const Node*, const Node*);

enum TreeType { Tree, ShadowIncludingTree, ComposedTree };
template<TreeType = Tree> ContainerNode* parent(const Node&);
template<TreeType = Tree> Node* commonInclusiveAncestor(const Node&, const Node&);
template<TreeType = Tree> std::partial_ordering treeOrder(const Node&, const Node&);

WEBCORE_EXPORT std::partial_ordering treeOrderForTesting(TreeType, const Node&, const Node&);

bool isTouchRelatedEventType(const EventTypeInfo&, const EventTarget&);

#if ASSERT_ENABLED

inline void adopted(Node* node)
{
    if (!node)
        return;
    node->m_adoptionIsRequired = false;
}

#endif // ASSERT_ENABLED

ALWAYS_INLINE void Node::ref() const
{
    ASSERT(isMainThread());
    ASSERT(!m_adoptionIsRequired);
    applyRefDuringDestructionCheck();
    m_refCountAndParentBit += s_refCountIncrement;
}

inline void Node::applyRefDuringDestructionCheck() const
{
#if ASSERT_ENABLED
    if (!deletionHasBegun())
        return;
    WTF::RefCountedBase::logRefDuringDestruction(this);
#endif
}

ALWAYS_INLINE void Node::deref() const
{
    ASSERT(isMainThread());
    ASSERT(!m_adoptionIsRequired);

    ASSERT_WITH_SECURITY_IMPLICATION(refCount());
    auto updatedRefCount = m_refCountAndParentBit - s_refCountIncrement;
    if (!updatedRefCount) {
        ASSERT(!deletionHasBegun());
#if ASSERT_ENABLED
        m_inRemovedLastRefFunction = true;
#endif
        const_cast<Node&>(*this).removedLastRef();
        return;
    }
    m_refCountAndParentBit = updatedRefCount;
}

ALWAYS_INLINE unsigned Node::refCount() const
{
    return m_refCountAndParentBit / s_refCountIncrement;
}

inline ContainerNode* Node::parentNode() const
{
    ASSERT(isMainThreadOrGCThread());
    return m_parentNode.get();
}

ALWAYS_INLINE void Node::setStyleFlag(NodeStyleFlag flag)
{
    auto bitfields = styleBitfields();
    bitfields.setFlag(flag);
    setStyleBitfields(bitfields);
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Node&);

inline void collectChildNodes(Node&, NodeVector&);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showTree(const WebCore::Node*);
void showNodePath(const WebCore::Node*);
#endif

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Node)
    static bool isType(const WebCore::EventTarget& target) { return target.isNode(); }
SPECIALIZE_TYPE_TRAITS_END()
