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

#include "EventTarget.h"
#include "ExceptionOr.h"
#include "LayoutRect.h"
#include "RenderStyleConstants.h"
#include "StyleValidity.h"
#include "TaskSource.h"
#include "TreeScope.h"
#include <wtf/CompactPointerTuple.h>
#include <wtf/CompactUniquePtrTuple.h>
#include <wtf/FixedVector.h>
#include <wtf/Forward.h>
#include <wtf/IsoMalloc.h>
#include <wtf/ListHashSet.h>
#include <wtf/MainThread.h>
#include <wtf/OptionSet.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/URLHash.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class FloatPoint;
class HTMLQualifiedName;
class HTMLSlotElement;
class MathMLQualifiedName;
class MutationObserver;
class MutationObserverRegistration;
class NamedNodeMap;
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
class WebCoreOpaqueRoot;

enum class MutationObserverOptionType : uint8_t;
using MutationObserverOptions = OptionSet<MutationObserverOptionType>;
using MutationRecordDeliveryOptions = OptionSet<MutationObserverOptionType>;

using NodeOrString = std::variant<RefPtr<Node>, String>;

class Node : public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(Node);

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

    WEBCORE_EXPORT static void startIgnoringLeaks();
    WEBCORE_EXPORT static void stopIgnoringLeaks();

    static void dumpStatistics();

    virtual ~Node();
    void willBeDeletedFrom(Document&);

    // DOM methods & attributes for Node

    bool hasTagName(const HTMLQualifiedName&) const;
    bool hasTagName(const MathMLQualifiedName&) const;
    inline bool hasTagName(const SVGQualifiedName&) const;
    virtual String nodeName() const = 0;
    virtual String nodeValue() const;
    virtual void setNodeValue(const String&);
    virtual NodeType nodeType() const = 0;
    virtual size_t approximateMemoryCost() const { return sizeof(*this); }
    ContainerNode* parentNode() const;
    static ptrdiff_t parentNodeMemoryOffset() { return OBJECT_OFFSETOF(Node, m_parentNode); }
    inline Element* parentElement() const;
    Node* previousSibling() const { return m_previous; }
    static ptrdiff_t previousSiblingMemoryOffset() { return OBJECT_OFFSETOF(Node, m_previous); }
    Node* nextSibling() const { return m_next; }
    static ptrdiff_t nextSiblingMemoryOffset() { return OBJECT_OFFSETOF(Node, m_next); }
    WEBCORE_EXPORT RefPtr<NodeList> childNodes();
    Node* firstChild() const;
    Node* lastChild() const;
    inline bool hasAttributes() const;
    inline NamedNodeMap* attributes() const;
    Node* pseudoAwareNextSibling() const;
    Node* pseudoAwarePreviousSibling() const;
    Node* pseudoAwareFirstChild() const;
    Node* pseudoAwareLastChild() const;

    WEBCORE_EXPORT const URL& baseURI() const;
    
    void getSubresourceURLs(ListHashSet<URL>&) const;

    WEBCORE_EXPORT ExceptionOr<void> insertBefore(Node& newChild, Node* refChild);
    WEBCORE_EXPORT ExceptionOr<void> replaceChild(Node& newChild, Node& oldChild);
    WEBCORE_EXPORT ExceptionOr<void> removeChild(Node& child);
    WEBCORE_EXPORT ExceptionOr<void> appendChild(Node& newChild);

    bool hasChildNodes() const { return firstChild(); }

    enum class CloningOperation {
        OnlySelf,
        SelfWithTemplateContent,
        Everything,
    };
    virtual Ref<Node> cloneNodeInternal(Document&, CloningOperation) = 0;
    Ref<Node> cloneNode(bool deep) { return cloneNodeInternal(document(), deep ? CloningOperation::Everything : CloningOperation::OnlySelf); }
    WEBCORE_EXPORT ExceptionOr<Ref<Node>> cloneNodeForBindings(bool deep);

    virtual const AtomString& localName() const;
    virtual const AtomString& namespaceURI() const;
    virtual const AtomString& prefix() const;
    virtual ExceptionOr<void> setPrefix(const AtomString&);
    WEBCORE_EXPORT void normalize();

    bool isSameNode(Node* other) const { return this == other; }
    WEBCORE_EXPORT bool isEqualNode(Node*) const;
    WEBCORE_EXPORT bool isDefaultNamespace(const AtomString& namespaceURI) const;
    WEBCORE_EXPORT const AtomString& lookupPrefix(const AtomString& namespaceURI) const;
    WEBCORE_EXPORT const AtomString& lookupNamespaceURI(const AtomString& prefix) const;

    WEBCORE_EXPORT String textContent(bool convertBRsToNewlines = false) const;
    WEBCORE_EXPORT void setTextContent(String&&);
    
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

    bool isElementNode() const { return hasNodeFlag(NodeFlag::IsElement); }
    bool isContainerNode() const { return hasNodeFlag(NodeFlag::IsContainerNode); }
    bool isTextNode() const { return hasNodeFlag(NodeFlag::IsText); }
    bool isHTMLElement() const { return hasNodeFlag(NodeFlag::IsHTMLElement); }
    bool isSVGElement() const { return hasNodeFlag(NodeFlag::IsSVGElement); }
    bool isMathMLElement() const { return hasNodeFlag(NodeFlag::IsMathMLElement); }

    bool isUnknownElement() const { return hasNodeFlag(NodeFlag::IsUnknownElement); }
    bool isHTMLUnknownElement() const { return isHTMLElement() && isUnknownElement(); }
    bool isSVGUnknownElement() const { return isSVGElement() && isUnknownElement(); }
    bool isMathMLUnknownElement() const { return isMathMLElement() && isUnknownElement(); }

    bool isPseudoElement() const { return pseudoId() != PseudoId::None; }
    bool isBeforePseudoElement() const { return pseudoId() == PseudoId::Before; }
    bool isAfterPseudoElement() const { return pseudoId() == PseudoId::After; }
    PseudoId pseudoId() const { return (isElementNode() && hasCustomStyleResolveCallbacks()) ? customPseudoId() : PseudoId::None; }

#if ENABLE(VIDEO)
    virtual bool isWebVTTElement() const { return false; }
#endif
    bool isStyledElement() const { return hasNodeFlag(NodeFlag::IsHTMLElement) || hasNodeFlag(NodeFlag::IsSVGElement) || hasNodeFlag(NodeFlag::IsMathMLElement); }
    virtual bool isAttributeNode() const { return false; }
    bool isCharacterDataNode() const { return hasNodeFlag(NodeFlag::IsCharacterData); }
    virtual bool isFrameOwnerElement() const { return false; }
    virtual bool isPluginElement() const { return false; }

    bool isDocumentNode() const { return hasNodeFlag(NodeFlag::IsDocumentNode); }
    bool isTreeScope() const { return hasNodeFlag(NodeFlag::IsDocumentNode) || hasNodeFlag(NodeFlag::IsShadowRoot); }
    bool isDocumentFragment() const { return hasNodeFlag(NodeFlag::IsDocumentFragment); }
    bool isShadowRoot() const { return hasNodeFlag(NodeFlag::IsShadowRoot); }
    bool isUserAgentShadowRoot() const; // Defined in ShadowRoot.h

    bool hasCustomStyleResolveCallbacks() const { return hasNodeFlag(NodeFlag::HasCustomStyleResolveCallbacks); }

    bool hasSyntheticAttrChildNodes() const { return hasNodeFlag(NodeFlag::HasSyntheticAttrChildNodes); }
    void setHasSyntheticAttrChildNodes(bool flag) { setNodeFlag(NodeFlag::HasSyntheticAttrChildNodes, flag); }

    bool hasShadowRootContainingSlots() const { return hasNodeFlag(NodeFlag::HasShadowRootContainingSlots); }
    void setHasShadowRootContainingSlots(bool flag) { setNodeFlag(NodeFlag::HasShadowRootContainingSlots, flag); }

    bool needsSVGRendererUpdate() const { return hasNodeFlag(NodeFlag::NeedsSVGRendererUpdate); }
    void setNeedsSVGRendererUpdate(bool flag) { setNodeFlag(NodeFlag::NeedsSVGRendererUpdate, flag); }

    // If this node is in a shadow tree, returns its shadow host. Otherwise, returns null.
    WEBCORE_EXPORT Element* shadowHost() const;
    ShadowRoot* containingShadowRoot() const;
    inline ShadowRoot* shadowRoot() const; // Defined in ElementRareData.h
    bool isClosedShadowHidden(const Node&) const;

    HTMLSlotElement* assignedSlot() const;
    HTMLSlotElement* assignedSlotForBindings() const;
    HTMLSlotElement* manuallyAssignedSlot() const;
    void setManuallyAssignedSlot(HTMLSlotElement*);

    bool isUncustomizedCustomElement() const { return customElementState() == CustomElementState::Uncustomized; }
    bool isCustomElementUpgradeCandidate() const { return customElementState() == CustomElementState::Undefined; }
    bool isDefinedCustomElement() const { return customElementState() == CustomElementState::Custom; }
    bool isFailedCustomElement() const { return customElementState() == CustomElementState::FailedOrPrecustomized && isUnknownElement(); }
    bool isFailedOrPrecustomizedCustomElement() const { return customElementState() == CustomElementState::FailedOrPrecustomized; }
    bool isPrecustomizedCustomElement() const { return customElementState() == CustomElementState::FailedOrPrecustomized && !isUnknownElement(); }
    bool isPrecustomizedOrDefinedCustomElement() const { return isPrecustomizedCustomElement() || isDefinedCustomElement(); }

    // Returns null, a child of ShadowRoot, or a legacy shadow root.
    Node* nonBoundaryShadowTreeRootNode();

    // Node's parent or shadow tree host.
    ContainerNode* parentOrShadowHostNode() const; // Defined in ShadowRoot.h
    ContainerNode* parentInComposedTree() const;
    Element* parentElementInComposedTree() const;
    Element* parentOrShadowHostElement() const;
    void setParentNode(ContainerNode*);
    Node& rootNode() const;
    WEBCORE_EXPORT Node& traverseToRootNode() const;
    Node& shadowIncludingRoot() const;

    struct GetRootNodeOptions {
        bool composed;
    };
    Node& getRootNode(const GetRootNodeOptions&) const;
    
    inline WebCoreOpaqueRoot opaqueRoot() const; // Defined in DocumentInlines.h.
    WebCoreOpaqueRoot traverseToOpaqueRoot() const;

    void queueTaskKeepingThisNodeAlive(TaskSource, Function<void ()>&&);
    void queueTaskToDispatchEvent(TaskSource, Ref<Event>&&);

    // Use when it's guaranteed to that shadowHost is null.
    ContainerNode* parentNodeGuaranteedHostFree() const;
    // Returns the parent node, but null if the parent node is a ShadowRoot.
    ContainerNode* nonShadowBoundaryParentNode() const;

    bool selfOrPrecedingNodesAffectDirAuto() const { return hasNodeFlag(NodeFlag::SelfOrPrecedingNodesAffectDirAuto); }
    void setSelfOrPrecedingNodesAffectDirAuto(bool flag) { setNodeFlag(NodeFlag::SelfOrPrecedingNodesAffectDirAuto, flag); }

    TextDirection effectiveTextDirection() const;
    void setEffectiveTextDirection(TextDirection);

    bool usesEffectiveTextDirection() const { return rareDataBitfields().usesEffectiveTextDirection; }
    void setUsesEffectiveTextDirection(bool);

    // Returns the enclosing event parent Element (or self) that, when clicked, would trigger a navigation.
    WEBCORE_EXPORT Element* enclosingLinkEventParentOrSelf();

    // These low-level calls give the caller responsibility for maintaining the integrity of the tree.
    void setPreviousSibling(Node* previous) { m_previous = previous; }
    void setNextSibling(Node* next) { m_next = next; }

    virtual bool canContainRangeEndPoint() const { return false; }

    WEBCORE_EXPORT bool isRootEditableElement() const;
    WEBCORE_EXPORT Element* rootEditableElement() const;

    // Called by the parser when this element's close tag is reached,
    // signaling that all child tags have been parsed and added.
    // This is needed for <applet> and <object> elements, which can't lay themselves out
    // until they know all of their nested <param>s. [Radar 3603191, 4040848].
    // Also used for script elements and some SVG elements for similar purposes,
    // but making parsing a special case in this respect should be avoided if possible.
    virtual void finishParsingChildren() { }
    virtual void beginParsingChildren() { }

    // For <link> and <style> elements.
    virtual bool sheetLoaded() { return true; }
    virtual void notifyLoadedSheetAndAllCriticalSubresources(bool /* error loading subresource */) { }
    virtual void startLoadingDynamicSheet() { ASSERT_NOT_REACHED(); }

    bool isUserActionElement() const { return hasNodeFlag(NodeFlag::IsUserActionElement); }
    void setUserActionElement(bool flag) { setNodeFlag(NodeFlag::IsUserActionElement, flag); }

    bool inRenderedDocument() const;
    bool needsStyleRecalc() const { return styleValidity() != Style::Validity::Valid; }
    Style::Validity styleValidity() const { return styleBitfields().styleValidity(); }
    bool styleResolutionShouldRecompositeLayer() const { return hasStyleFlag(NodeStyleFlag::StyleResolutionShouldRecompositeLayer); }
    bool childNeedsStyleRecalc() const { return hasStyleFlag(NodeStyleFlag::DescendantNeedsStyleResolution); }
    bool isEditingText() const { return hasNodeFlag(NodeFlag::IsEditingText); }

    bool isDocumentFragmentForInnerOuterHTML() const { return hasNodeFlag(NodeFlag::IsDocumentFragmentForInnerOuterHTML); }

    void setChildNeedsStyleRecalc() { setStyleFlag(NodeStyleFlag::DescendantNeedsStyleResolution); }
    void clearChildNeedsStyleRecalc();

    void setHasValidStyle();

    bool isLink() const { return hasNodeFlag(NodeFlag::IsLink); }
    void setIsLink(bool flag) { setNodeFlag(NodeFlag::IsLink, flag); }

    bool isInGCReacheableRefMap() const { return hasNodeFlag(NodeFlag::IsInGCReachableRefMap); }
    void setIsInGCReacheableRefMap(bool flag) { setNodeFlag(NodeFlag::IsInGCReachableRefMap, flag); }

    WEBCORE_EXPORT bool isContentEditable() const;
    bool isContentRichlyEditable() const;

    WEBCORE_EXPORT void inspect();

    enum UserSelectAllTreatment {
        UserSelectAllDoesNotAffectEditability,
        UserSelectAllIsAlwaysNonEditable
    };
    bool hasEditableStyle(UserSelectAllTreatment treatment = UserSelectAllIsAlwaysNonEditable) const
    {
        return computeEditability(treatment, ShouldUpdateStyle::DoNotUpdate) != Editability::ReadOnly;
    }

    // FIXME: Replace every use of this function by helpers in Editing.h
    bool hasRichlyEditableStyle() const
    {
        return computeEditability(UserSelectAllIsAlwaysNonEditable, ShouldUpdateStyle::DoNotUpdate) == Editability::CanEditRichly;
    }

    enum class Editability { ReadOnly, CanEditPlainText, CanEditRichly };
    enum class ShouldUpdateStyle { Update, DoNotUpdate };
    WEBCORE_EXPORT Editability computeEditability(UserSelectAllTreatment, ShouldUpdateStyle) const;
    Editability computeEditabilityWithStyle(const RenderStyle*, UserSelectAllTreatment, ShouldUpdateStyle) const;

    WEBCORE_EXPORT LayoutRect renderRect(bool* isReplaced);
    IntRect pixelSnappedRenderRect(bool* isReplaced) { return snappedIntRect(renderRect(isReplaced)); }

    WEBCORE_EXPORT unsigned computeNodeIndex() const;

    // Returns the DOM ownerDocument attribute. This method never returns null, except in the case
    // of a Document node.
    WEBCORE_EXPORT Document* ownerDocument() const;

    // Returns the document associated with this node. A document node returns itself.
    Document& document() const { return treeScope().documentScope(); }

    TreeScope& treeScope() const
    {
        ASSERT(m_treeScope);
        return *m_treeScope;
    }
    void setTreeScopeRecursively(TreeScope&);
    static ptrdiff_t treeScopeMemoryOffset() { return OBJECT_OFFSETOF(Node, m_treeScope); }

    // Returns true if this node is associated with a document and is in its associated document's
    // node tree, false otherwise (https://dom.spec.whatwg.org/#connected).
    bool isConnected() const { return hasNodeFlag(NodeFlag::IsConnected); }
    bool isInUserAgentShadowTree() const;
    bool isInShadowTree() const { return hasNodeFlag(NodeFlag::IsInShadowTree); }
    bool isInTreeScope() const { return hasNodeFlag(NodeFlag::IsConnected) || hasNodeFlag(NodeFlag::IsInShadowTree); }
    bool hasBeenInUserAgentShadowTree() const { return hasNodeFlag(NodeFlag::HasBeenInUserAgentShadowTree); }

    // https://dom.spec.whatwg.org/#in-a-document-tree
    bool isInDocumentTree() const { return isConnected() && !isInShadowTree(); }

    bool isDocumentTypeNode() const { return nodeType() == DOCUMENT_TYPE_NODE; }
    virtual bool childTypeAllowed(NodeType) const { return false; }
    unsigned countChildNodes() const;
    unsigned length() const;
    Node* traverseToChildAt(unsigned) const;

    ExceptionOr<void> checkSetPrefix(const AtomString& prefix);

    WEBCORE_EXPORT bool isDescendantOf(const Node&) const;
    bool isDescendantOf(const Node* other) const { return other && isDescendantOf(*other); }
    WEBCORE_EXPORT bool contains(const Node&) const;
    bool contains(const Node* other) const { return other && contains(*other); }

    WEBCORE_EXPORT bool isDescendantOrShadowDescendantOf(const Node&) const;
    bool isDescendantOrShadowDescendantOf(const Node* other) const { return other && isDescendantOrShadowDescendantOf(*other); } 
    WEBCORE_EXPORT bool containsIncludingShadowDOM(const Node*) const;

    // Whether or not a selection can be started in this object
    virtual bool canStartSelection() const;

    virtual bool shouldSelectOnMouseDown() { return false; }

    // Getting points into and out of screen space
    FloatPoint convertToPage(const FloatPoint&) const;
    FloatPoint convertFromPage(const FloatPoint&) const;

    // -----------------------------------------------------------------------------
    // Integration with rendering tree

    // As renderer() includes a branch you should avoid calling it repeatedly in hot code paths.
    RenderObject* renderer() const { return m_rendererWithStyleFlags.pointer(); }
    void setRenderer(RenderObject*); // Defined in RenderObject.h

    // Use these two methods with caution.
    WEBCORE_EXPORT RenderBox* renderBox() const;
    RenderBoxModelObject* renderBoxModelObject() const;
    
    // Wrapper for nodes that don't have a renderer, but still cache the style (like HTMLOptionElement).
    const RenderStyle* renderStyle() const;

    virtual const RenderStyle* computedStyle(PseudoId pseudoElementSpecifier = PseudoId::None);

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
    void showNode(const char* prefix = "") const;
    void showTreeForThis() const;
    void showNodePathForThis() const;
    void showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2 = nullptr, const char* markedLabel2 = nullptr) const;
    void showTreeForThisAcrossFrame() const;
#endif // ENABLE(TREE_DEBUGGING)

    void invalidateNodeListAndCollectionCachesInAncestors();
    void invalidateNodeListAndCollectionCachesInAncestorsForAttribute(const QualifiedName& attrName);
    NodeListsNodeData* nodeLists();
    void clearNodeLists();

    virtual bool willRespondToMouseMoveEvents() const;
    bool willRespondToMouseClickEvents() const;
    Editability computeEditabilityForMouseClickEvents(const RenderStyle* = nullptr) const;
    virtual bool willRespondToMouseClickEventsWithEditability(Editability) const;
    virtual bool willRespondToTouchEvents() const;

    WEBCORE_EXPORT unsigned short compareDocumentPosition(Node&);

    EventTargetInterface eventTargetInterface() const override;
    ScriptExecutionContext* scriptExecutionContext() const final; // Implemented in Document.h

    WEBCORE_EXPORT bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) override;
    bool removeEventListener(const AtomString& eventType, EventListener&, const EventListenerOptions&) override;

    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&) override;

    void dispatchScopedEvent(Event&);

    void dispatchSubtreeModifiedEvent();
    void dispatchDOMActivateEvent(Event& underlyingClickEvent);

#if ENABLE(TOUCH_EVENTS)
    virtual bool allowsDoubleTapGesture() const { return true; }
#endif

    WEBCORE_EXPORT void dispatchInputEvent();

    // Perform the default action for an event.
    virtual void defaultEventHandler(Event&);

    void ref() const;
    void deref() const;
    bool hasOneRef() const;
    unsigned refCount() const;

#if ASSERT_ENABLED
    bool m_deletionHasBegun { false };
    mutable bool m_inRemovedLastRefFunction { false };
    bool m_adoptionIsRequired { true };
#endif

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
    static ptrdiff_t nodeFlagsMemoryOffset() { return OBJECT_OFFSETOF(Node, m_nodeFlags); }
    static ptrdiff_t rareDataMemoryOffset() { return OBJECT_OFFSETOF(Node, m_rareDataWithBitfields); }
#if CPU(ADDRESS64)
    static uint64_t rareDataPointerMask() { return CompactPointerTuple<NodeRareData*, uint16_t>::pointerMask; }
#else
    static uint32_t rareDataPointerMask() { return -1; }
#endif
    static int32_t flagIsText() { return static_cast<int32_t>(NodeFlag::IsText); }
    static int32_t flagIsContainer() { return static_cast<int32_t>(NodeFlag::IsContainerNode); }
    static int32_t flagIsElement() { return static_cast<int32_t>(NodeFlag::IsElement); }
    static int32_t flagIsShadowRoot() { return static_cast<int32_t>(NodeFlag::IsShadowRoot); }
    static int32_t flagIsHTML() { return static_cast<int32_t>(NodeFlag::IsHTMLElement); }
    static int32_t flagIsLink() { return static_cast<int32_t>(NodeFlag::IsLink); }
    static int32_t flagIsParsingChildrenFinished() { return static_cast<int32_t>(NodeFlag::IsParsingChildrenFinished); }
#endif // ENABLE(JIT)

protected:
    enum class NodeFlag : uint32_t {
        IsCharacterData = 1 << 0,
        IsText = 1 << 1,
        IsContainerNode = 1 << 2,
        IsElement = 1 << 3,
        IsHTMLElement = 1 << 4,
        IsSVGElement = 1 << 5,
        IsMathMLElement = 1 << 6,
        IsDocumentNode = 1 << 7,
        IsDocumentFragment = 1 << 8,
        IsShadowRoot = 1 << 9,
        IsConnected = 1 << 10,
        IsInShadowTree = 1 << 11,
        IsUnknownElement = 1 << 12,

        // These bits are used by derived classes, pulled up here so they can
        // be stored in the same memory word as the Node bits above.
        IsDocumentFragmentForInnerOuterHTML = 1 << 13, // DocumentFragment
        IsEditingText = 1 << 14, // Text
        IsLink = 1 << 15, // Element
        IsUserActionElement = 1 << 16,
        IsParsingChildrenFinished = 1 << 17,
        HasSyntheticAttrChildNodes = 1 << 18,
        SelfOrPrecedingNodesAffectDirAuto = 1 << 19,

        HasCustomStyleResolveCallbacks = 1 << 20,
        HasPendingResources = 1 << 21,
        IsInGCReachableRefMap = 1 << 22,
        IsComputedStyleInvalidFlag = 1 << 23,
        HasShadowRootContainingSlots = 1 << 24,
        IsInTopLayer = 1 << 25,
        NeedsSVGRendererUpdate = 1 << 26,
        NeedsUpdateQueryContainerDependentStyle = 1 << 27,
        HasBeenInUserAgentShadowTree = 1 << 28,
#if ENABLE(FULLSCREEN_API)
        IsFullscreen = 1 << 29,
        IsIFrameFullscreen = 1 << 30,
#endif
        HasFormAssociatedCustomElementInterface = 1U << 31,
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

    bool hasNodeFlag(NodeFlag flag) const { return m_nodeFlags.contains(flag); }
    void setNodeFlag(NodeFlag flag, bool value = true) const { m_nodeFlags.set(flag, value); }
    void clearNodeFlag(NodeFlag flag) const { m_nodeFlags.remove(flag); }

    RareDataBitFields rareDataBitfields() const { return bitwise_cast<RareDataBitFields>(m_rareDataWithBitfields.type()); }
    void setRareDataBitfields(RareDataBitFields bitfields) { m_rareDataWithBitfields.setType(bitwise_cast<uint16_t>(bitfields)); }

    TabIndexState tabIndexState() const { return static_cast<TabIndexState>(rareDataBitfields().tabIndexState); }
    void setTabIndexState(TabIndexState);

    CustomElementState customElementState() const { return static_cast<CustomElementState>(rareDataBitfields().customElementState); }
    void setCustomElementState(CustomElementState);

    bool isParsingChildrenFinished() const { return hasNodeFlag(NodeFlag::IsParsingChildrenFinished); }
    void setIsParsingChildrenFinished() { setNodeFlag(NodeFlag::IsParsingChildrenFinished); }
    void clearIsParsingChildrenFinished() { clearNodeFlag(NodeFlag::IsParsingChildrenFinished); }

    constexpr static auto DefaultNodeFlags = OptionSet<NodeFlag>(NodeFlag::IsParsingChildrenFinished);
    constexpr static auto CreateOther = DefaultNodeFlags;
    constexpr static auto CreateCharacterData = DefaultNodeFlags | NodeFlag::IsCharacterData;
    constexpr static auto CreateText = CreateCharacterData | NodeFlag::IsText;
    constexpr static auto CreateContainer = DefaultNodeFlags | NodeFlag::IsContainerNode;
    constexpr static auto CreateElement = CreateContainer | NodeFlag::IsElement;
    constexpr static auto CreatePseudoElement = CreateElement | NodeFlag::IsConnected;
    constexpr static auto CreateDocumentFragment = CreateContainer | NodeFlag::IsDocumentFragment;
    constexpr static auto CreateShadowRoot = CreateDocumentFragment | NodeFlag::IsShadowRoot | NodeFlag::IsInShadowTree;
    constexpr static auto CreateHTMLElement = CreateElement | NodeFlag::IsHTMLElement;
    constexpr static auto CreateSVGElement = CreateElement | NodeFlag::IsSVGElement | NodeFlag::HasCustomStyleResolveCallbacks;
    constexpr static auto CreateMathMLElement = CreateElement | NodeFlag::IsMathMLElement;
    constexpr static auto CreateDocument = CreateContainer | NodeFlag::IsDocumentNode | NodeFlag::IsConnected;
    constexpr static auto CreateEditingText = CreateText | NodeFlag::IsEditingText;
    using ConstructionType = OptionSet<NodeFlag>;
    Node(Document&, ConstructionType);

    static constexpr uint32_t s_refCountIncrement = 2;
    static constexpr uint32_t s_refCountMask = ~static_cast<uint32_t>(1);

    enum class NodeStyleFlag : uint16_t {
        DescendantNeedsStyleResolution                          = 1 << 0,
        DirectChildNeedsStyleResolution                         = 1 << 1,
        StyleResolutionShouldRecompositeLayer                   = 1 << 2,

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
        ChildrenAffectedByPropertyBasedBackwardPositionalRules  = 1 << 13,
    };

    struct StyleBitfields {
    public:
        static StyleBitfields fromRaw(uint16_t packed) { return bitwise_cast<StyleBitfields>(packed); }
        uint16_t toRaw() const { return bitwise_cast<uint16_t>(*this); }

        Style::Validity styleValidity() const { return static_cast<Style::Validity>(m_styleValidity); }
        void setStyleValidity(Style::Validity validity) { m_styleValidity = static_cast<uint8_t>(validity); }

        OptionSet<NodeStyleFlag> flags() const { return OptionSet<NodeStyleFlag>::fromRaw(m_flags); }
        void setFlag(NodeStyleFlag flag) { m_flags = (flags() | flag).toRaw(); }
        void clearFlag(NodeStyleFlag flag) { m_flags = (flags() - flag).toRaw(); }
        void clearFlags(OptionSet<NodeStyleFlag> flagsToClear) { m_flags = (flags() - flagsToClear).toRaw(); }
        void clearDescendantsNeedStyleResolution() { m_flags = (flags() - NodeStyleFlag::DescendantNeedsStyleResolution - NodeStyleFlag::DirectChildNeedsStyleResolution).toRaw(); }

    private:
        uint16_t m_styleValidity : 2;
        uint16_t m_flags : 14;
    };

    StyleBitfields styleBitfields() const { return StyleBitfields::fromRaw(m_rendererWithStyleFlags.type()); }
    void setStyleBitfields(StyleBitfields bitfields) { m_rendererWithStyleFlags.setType(bitfields.toRaw()); }
    ALWAYS_INLINE bool hasStyleFlag(NodeStyleFlag flag) const { return styleBitfields().flags().contains(flag); }
    ALWAYS_INLINE void setStyleFlag(NodeStyleFlag);
    ALWAYS_INLINE void clearStyleFlags(OptionSet<NodeStyleFlag>);

    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const { }

    bool hasRareData() const { return !!m_rareDataWithBitfields.pointer(); }
    NodeRareData* rareData() const { return m_rareDataWithBitfields.pointer(); }
    NodeRareData& ensureRareData();
    void clearRareData();

    void setHasCustomStyleResolveCallbacks() { setNodeFlag(NodeFlag::HasCustomStyleResolveCallbacks); }

    void setTreeScope(TreeScope& scope) { m_treeScope = &scope; }

    void invalidateStyle(Style::Validity, Style::InvalidationMode = Style::InvalidationMode::Normal);
    void updateAncestorsForStyleRecalc();
    void markAncestorsForInvalidatedStyle();

    ExceptionOr<RefPtr<Node>> convertNodesOrStringsIntoNode(FixedVector<NodeOrString>&&);

private:
    virtual PseudoId customPseudoId() const
    {
        ASSERT(hasCustomStyleResolveCallbacks());
        return PseudoId::None;
    }

    WEBCORE_EXPORT void removedLastRef();

    void refEventTarget() final;
    void derefEventTarget() final;

    void trackForDebugging();
    void materializeRareData();

    Vector<std::unique_ptr<MutationObserverRegistration>>* mutationObserverRegistry();
    HashSet<MutationObserverRegistration*>* transientMutationObserverRegistry();

    void adjustStyleValidity(Style::Validity, Style::InvalidationMode);

    static void moveShadowTreeToNewDocument(ShadowRoot&, Document& oldDocument, Document& newDocument);
    static void moveTreeToNewScope(Node&, TreeScope& oldScope, TreeScope& newScope);
    void moveNodeToNewDocument(Document& oldDocument, Document& newDocument);

    WEBCORE_EXPORT void notifyInspectorOfRendererChange();
    
    mutable uint32_t m_refCountAndParentBit { s_refCountIncrement };
    mutable OptionSet<NodeFlag> m_nodeFlags;

    ContainerNode* m_parentNode { nullptr };
    TreeScope* m_treeScope { nullptr };
    Node* m_previous { nullptr };
    Node* m_next { nullptr };
    CompactPointerTuple<RenderObject*, uint16_t> m_rendererWithStyleFlags;
    CompactUniquePtrTuple<NodeRareData, uint16_t> m_rareDataWithBitfields;
};

bool connectedInSameTreeScope(const Node*, const Node*);

// Designed to be used the same way as C++20 std::partial_ordering class.
// FIXME: Consider putting this in a separate header.
// FIXME: Once we can require C++20, replace with std::partial_ordering.
class PartialOrdering {
public:
    static const PartialOrdering less;
    static const PartialOrdering equivalent;
    static const PartialOrdering greater;
    static const PartialOrdering unordered;

    friend constexpr bool is_eq(PartialOrdering);
    friend constexpr bool is_lt(PartialOrdering);
    friend constexpr bool is_gt(PartialOrdering);

private:
    enum class Type : uint8_t { Less, Equivalent, Greater, Unordered };
    constexpr PartialOrdering(Type type) : m_type { type } { }
    Type m_type;
};
constexpr bool is_eq(PartialOrdering);
constexpr bool is_lt(PartialOrdering);
constexpr bool is_gt(PartialOrdering);
constexpr bool is_neq(PartialOrdering);
constexpr bool is_lteq(PartialOrdering);
constexpr bool is_gteq(PartialOrdering);

enum TreeType { Tree, ShadowIncludingTree, ComposedTree };
template<TreeType = Tree> ContainerNode* parent(const Node&);
template<TreeType = Tree> Node* commonInclusiveAncestor(const Node&, const Node&);
template<TreeType = Tree> PartialOrdering treeOrder(const Node&, const Node&);

WEBCORE_EXPORT PartialOrdering treeOrderForTesting(TreeType, const Node&, const Node&);

#if ASSERT_ENABLED

inline void adopted(Node* node)
{
    if (!node)
        return;
    ASSERT(!node->m_deletionHasBegun);
    ASSERT(!node->m_inRemovedLastRefFunction);
    node->m_adoptionIsRequired = false;
}

#endif // ASSERT_ENABLED

ALWAYS_INLINE void Node::ref() const
{
    ASSERT(isMainThread());
    ASSERT(!m_deletionHasBegun);
    ASSERT(!m_inRemovedLastRefFunction);
    ASSERT(!m_adoptionIsRequired);
    m_refCountAndParentBit += s_refCountIncrement;
}

ALWAYS_INLINE void Node::deref() const
{
    ASSERT(isMainThread());
    ASSERT(refCount());
    ASSERT(!m_deletionHasBegun);
    ASSERT(!m_inRemovedLastRefFunction);
    ASSERT(!m_adoptionIsRequired);
    auto updatedRefCount = m_refCountAndParentBit - s_refCountIncrement;
    if (!updatedRefCount) {
        // Don't update m_refCountAndParentBit to avoid double destruction through use of Ref<T>/RefPtr<T>.
        // (This is a security mitigation in case of programmer error. It will ASSERT in debug builds.)
#if ASSERT_ENABLED
        m_inRemovedLastRefFunction = true;
#endif
        const_cast<Node&>(*this).removedLastRef();
        return;
    }
    m_refCountAndParentBit = updatedRefCount;
}

ALWAYS_INLINE bool Node::hasOneRef() const
{
    ASSERT(!m_deletionHasBegun);
    ASSERT(!m_inRemovedLastRefFunction);
    return refCount() == 1;
}

ALWAYS_INLINE unsigned Node::refCount() const
{
    return m_refCountAndParentBit / s_refCountIncrement;
}

// Used in Node::addSubresourceAttributeURLs() and in addSubresourceStyleURLs()
inline void addSubresourceURL(ListHashSet<URL>& urls, const URL& url)
{
    if (!url.isNull())
        urls.add(url);
}

inline void Node::setParentNode(ContainerNode* parent)
{
    ASSERT(isMainThread());
    m_parentNode = parent;
    m_refCountAndParentBit = (m_refCountAndParentBit & s_refCountMask) | !!parent;
}

inline ContainerNode* Node::parentNode() const
{
    ASSERT(isMainThreadOrGCThread());
    return m_parentNode;
}

inline ContainerNode* Node::parentNodeGuaranteedHostFree() const
{
    ASSERT(!isShadowRoot());
    return parentNode();
}

ALWAYS_INLINE void Node::setStyleFlag(NodeStyleFlag flag)
{
    auto bitfields = styleBitfields();
    bitfields.setFlag(flag);
    setStyleBitfields(bitfields);
}

ALWAYS_INLINE void Node::clearStyleFlags(OptionSet<NodeStyleFlag> flags)
{
    auto bitfields = styleBitfields();
    bitfields.clearFlags(flags);
    setStyleBitfields(bitfields);
}

inline void Node::clearChildNeedsStyleRecalc()
{
    auto bitfields = styleBitfields();
    bitfields.clearDescendantsNeedStyleResolution();
    setStyleBitfields(bitfields);
}

inline void Node::setHasValidStyle()
{
    auto bitfields = styleBitfields();
    bitfields.setStyleValidity(Style::Validity::Valid);
    bitfields.clearFlag(NodeStyleFlag::StyleResolutionShouldRecompositeLayer);
    setStyleBitfields(bitfields);
    clearNodeFlag(NodeFlag::IsComputedStyleInvalidFlag);
}

inline void Node::setTreeScopeRecursively(TreeScope& newTreeScope)
{
    ASSERT(!isDocumentNode());
    ASSERT(!m_deletionHasBegun);
    if (m_treeScope != &newTreeScope)
        moveTreeToNewScope(*this, *m_treeScope, newTreeScope);
}

inline constexpr PartialOrdering PartialOrdering::less(Type::Less);
inline constexpr PartialOrdering PartialOrdering::equivalent(Type::Equivalent);
inline constexpr PartialOrdering PartialOrdering::greater(Type::Greater);
inline constexpr PartialOrdering PartialOrdering::unordered(Type::Unordered);

constexpr bool is_eq(PartialOrdering ordering)
{
    return ordering.m_type == PartialOrdering::Type::Equivalent;
}

constexpr bool is_lt(PartialOrdering ordering)
{
    return ordering.m_type == PartialOrdering::Type::Less;
}

constexpr bool is_gt(PartialOrdering ordering)
{
    return ordering.m_type == PartialOrdering::Type::Greater;
}

constexpr bool is_neq(PartialOrdering ordering)
{
    return is_lt(ordering) || is_gt(ordering);
}

constexpr bool is_lteq(PartialOrdering ordering)
{
    return is_lt(ordering) || is_eq(ordering);
}

constexpr bool is_gteq(PartialOrdering ordering)
{
    return is_gt(ordering) || is_eq(ordering);
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Node&);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showTree(const WebCore::Node*);
void showNodePath(const WebCore::Node*);
#endif

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Node)
    static bool isType(const WebCore::EventTarget& target) { return target.isNode(); }
SPECIALIZE_TYPE_TRAITS_END()
