/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "MutationObserver.h"
#include "RenderStyleConstants.h"
#include "StyleValidity.h"
#include "TreeScope.h"
#include <wtf/CompactPointerTuple.h>
#include <wtf/Forward.h>
#include <wtf/IsoMalloc.h>
#include <wtf/ListHashSet.h>
#include <wtf/MainThread.h>
#include <wtf/URLHash.h>

// This needs to be here because Document.h also depends on it.
#define DUMP_NODE_STATISTICS 0

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

using NodeOrString = Variant<RefPtr<Node>, String>;

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
    bool hasTagName(const SVGQualifiedName&) const;
    virtual String nodeName() const = 0;
    virtual String nodeValue() const;
    virtual ExceptionOr<void> setNodeValue(const String&);
    virtual NodeType nodeType() const = 0;
    virtual size_t approximateMemoryCost() const { return sizeof(*this); }
    ContainerNode* parentNode() const;
    static ptrdiff_t parentNodeMemoryOffset() { return OBJECT_OFFSETOF(Node, m_parentNode); }
    Element* parentElement() const;
    Node* previousSibling() const { return m_previous; }
    static ptrdiff_t previousSiblingMemoryOffset() { return OBJECT_OFFSETOF(Node, m_previous); }
    Node* nextSibling() const { return m_next; }
    static ptrdiff_t nextSiblingMemoryOffset() { return OBJECT_OFFSETOF(Node, m_next); }
    WEBCORE_EXPORT RefPtr<NodeList> childNodes();
    Node* firstChild() const;
    Node* lastChild() const;
    bool hasAttributes() const;
    NamedNodeMap* attributes() const;
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
    WEBCORE_EXPORT ExceptionOr<void> setTextContent(const String&);
    
    Node* lastDescendant() const;
    Node* firstDescendant() const;

    // From the NonDocumentTypeChildNode - https://dom.spec.whatwg.org/#nondocumenttypechildnode
    WEBCORE_EXPORT Element* previousElementSibling() const;
    WEBCORE_EXPORT Element* nextElementSibling() const;

    // From the ChildNode - https://dom.spec.whatwg.org/#childnode
    ExceptionOr<void> before(Vector<NodeOrString>&&);
    ExceptionOr<void> after(Vector<NodeOrString>&&);
    ExceptionOr<void> replaceWith(Vector<NodeOrString>&&);
    WEBCORE_EXPORT ExceptionOr<void> remove();

    // Other methods (not part of DOM)

    bool isElementNode() const { return getFlag(IsElementFlag); }
    bool isContainerNode() const { return getFlag(IsContainerFlag); }
    bool isTextNode() const { return getFlag(IsTextFlag); }
    bool isHTMLElement() const { return getFlag(IsHTMLFlag); }
    bool isSVGElement() const { return getFlag(IsSVGFlag); }
    bool isMathMLElement() const { return getFlag(IsMathMLFlag); }

    bool isPseudoElement() const { return pseudoId() != PseudoId::None; }
    bool isBeforePseudoElement() const { return pseudoId() == PseudoId::Before; }
    bool isAfterPseudoElement() const { return pseudoId() == PseudoId::After; }
    PseudoId pseudoId() const { return (isElementNode() && hasCustomStyleResolveCallbacks()) ? customPseudoId() : PseudoId::None; }

#if ENABLE(VIDEO)
    virtual bool isWebVTTElement() const { return false; }
#endif
    bool isStyledElement() const { return getFlag(IsHTMLFlag) || getFlag(IsSVGFlag) || getFlag(IsMathMLFlag); }
    virtual bool isAttributeNode() const { return false; }
    bool isCharacterDataNode() const { return !isContainerNode() && (isTextNode() || virtualIsCharacterData()); }
    virtual bool isFrameOwnerElement() const { return false; }
    virtual bool isPluginElement() const { return false; }
#if ENABLE(SERVICE_CONTROLS)
    virtual bool isImageControlsRootElement() const { return false; }
    virtual bool isImageControlsButtonElement() const { return false; }
#endif

    bool isDocumentNode() const { return getFlag(IsDocumentNodeFlag); }
    bool isTreeScope() const { return getFlag(IsDocumentNodeFlag) || getFlag(IsShadowRootFlag); }
    bool isDocumentFragment() const { return getFlag(IsContainerFlag) && !(getFlag(IsElementFlag) || getFlag(IsDocumentNodeFlag)); }
    bool isShadowRoot() const { return getFlag(IsShadowRootFlag); }

    bool hasCustomStyleResolveCallbacks() const { return getFlag(HasCustomStyleResolveCallbacksFlag); }

    bool hasSyntheticAttrChildNodes() const { return getFlag(HasSyntheticAttrChildNodesFlag); }
    void setHasSyntheticAttrChildNodes(bool flag) { setFlag(flag, HasSyntheticAttrChildNodesFlag); }

    // If this node is in a shadow tree, returns its shadow host. Otherwise, returns null.
    WEBCORE_EXPORT Element* shadowHost() const;
    ShadowRoot* containingShadowRoot() const;
    ShadowRoot* shadowRoot() const;
    bool isClosedShadowHidden(const Node&) const;

    HTMLSlotElement* assignedSlot() const;
    HTMLSlotElement* assignedSlotForBindings() const;

    bool isUndefinedCustomElement() const { return isElementNode() && getFlag(IsEditingTextOrUndefinedCustomElementFlag); }
    bool isCustomElementUpgradeCandidate() const { return getFlag(IsCustomElement) && getFlag(IsEditingTextOrUndefinedCustomElementFlag); }
    bool isDefinedCustomElement() const { return getFlag(IsCustomElement) && !getFlag(IsEditingTextOrUndefinedCustomElementFlag); }
    bool isFailedCustomElement() const { return isElementNode() && !getFlag(IsCustomElement) && getFlag(IsEditingTextOrUndefinedCustomElementFlag); }

    // Returns null, a child of ShadowRoot, or a legacy shadow root.
    Node* nonBoundaryShadowTreeRootNode();

    // Node's parent or shadow tree host.
    ContainerNode* parentOrShadowHostNode() const;
    ContainerNode* parentInComposedTree() const;
    Element* parentElementInComposedTree() const;
    Element* parentOrShadowHostElement() const;
    void setParentNode(ContainerNode*);
    Node& rootNode() const;
    Node& traverseToRootNode() const;
    Node& shadowIncludingRoot() const;

    struct GetRootNodeOptions {
        bool composed;
    };
    Node& getRootNode(const GetRootNodeOptions&) const;
    
    void* opaqueRoot() const;

    // Use when it's guaranteed to that shadowHost is null.
    ContainerNode* parentNodeGuaranteedHostFree() const;
    // Returns the parent node, but null if the parent node is a ShadowRoot.
    ContainerNode* nonShadowBoundaryParentNode() const;

    bool selfOrAncestorHasDirAutoAttribute() const { return getFlag(SelfOrAncestorHasDirAutoFlag); }
    void setSelfOrAncestorHasDirAutoAttribute(bool flag) { setFlag(flag, SelfOrAncestorHasDirAutoFlag); }

    // Returns the enclosing event parent Element (or self) that, when clicked, would trigger a navigation.
    Element* enclosingLinkEventParentOrSelf();

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

    bool isUserActionElement() const { return getFlag(IsUserActionElement); }
    void setUserActionElement(bool flag) { setFlag(flag, IsUserActionElement); }

    bool inRenderedDocument() const;
    bool needsStyleRecalc() const { return styleValidity() != Style::Validity::Valid; }
    Style::Validity styleValidity() const;
    bool styleResolutionShouldRecompositeLayer() const;
    bool childNeedsStyleRecalc() const { return getFlag(ChildNeedsStyleRecalcFlag); }
    bool styleIsAffectedByPreviousSibling() const { return getFlag(StyleIsAffectedByPreviousSibling); }
    bool isEditingText() const { return getFlag(IsTextFlag) && getFlag(IsEditingTextOrUndefinedCustomElementFlag); }

    void setChildNeedsStyleRecalc() { setFlag(ChildNeedsStyleRecalcFlag); }
    void clearChildNeedsStyleRecalc() { m_nodeFlags &= ~(ChildNeedsStyleRecalcFlag | DirectChildNeedsStyleRecalcFlag); }

    void setHasValidStyle();

    bool isLink() const { return getFlag(IsLinkFlag); }
    void setIsLink(bool flag) { setFlag(flag, IsLinkFlag); }

    bool hasEventTargetData() const { return getFlag(HasEventTargetDataFlag); }
    void setHasEventTargetData(bool flag) { setFlag(flag, HasEventTargetDataFlag); }

    WEBCORE_EXPORT bool isContentEditable();
    bool isContentRichlyEditable();

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
    bool isConnected() const { return getFlag(IsConnectedFlag); }
    bool isInUserAgentShadowTree() const;
    bool isInShadowTree() const { return getFlag(IsInShadowTreeFlag); }
    bool isInTreeScope() const { return getFlag(static_cast<NodeFlags>(IsConnectedFlag | IsInShadowTreeFlag)); }

    bool isDocumentTypeNode() const { return nodeType() == DOCUMENT_TYPE_NODE; }
    virtual bool childTypeAllowed(NodeType) const { return false; }
    unsigned countChildNodes() const;
    unsigned length() const;
    Node* traverseToChildAt(unsigned) const;

    ExceptionOr<void> checkSetPrefix(const AtomString& prefix);

    WEBCORE_EXPORT bool isDescendantOf(const Node&) const;
    bool isDescendantOf(const Node* other) const { return other && isDescendantOf(*other); }

    bool isDescendantOrShadowDescendantOf(const Node*) const;
    WEBCORE_EXPORT bool contains(const Node*) const;
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

    virtual String debugDescription() const;

#if ENABLE(TREE_DEBUGGING)
    virtual void formatForDebugger(char* buffer, unsigned length) const;

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

    virtual bool willRespondToMouseMoveEvents();
    virtual bool willRespondToMouseClickEvents();
    virtual bool willRespondToMouseWheelEvents();

    WEBCORE_EXPORT unsigned short compareDocumentPosition(Node&);

    EventTargetInterface eventTargetInterface() const override;
    ScriptExecutionContext* scriptExecutionContext() const final; // Implemented in Document.h

    WEBCORE_EXPORT bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) override;
    bool removeEventListener(const AtomString& eventType, EventListener&, const ListenerOptions&) override;

    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&) override;

    void dispatchScopedEvent(Event&);

    virtual void handleLocalEvents(Event&, EventInvokePhase);

    void dispatchSubtreeModifiedEvent();
    void dispatchDOMActivateEvent(Event& underlyingClickEvent);

#if ENABLE(TOUCH_EVENTS)
    virtual bool allowsDoubleTapGesture() const { return true; }
#endif

    bool dispatchBeforeLoadEvent(const String& sourceURL);

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

    EventTargetData* eventTargetData() final;
    EventTargetData* eventTargetDataConcurrently() final;
    EventTargetData& ensureEventTargetData() final;

    HashMap<Ref<MutationObserver>, MutationRecordDeliveryOptions> registeredMutationObservers(MutationObserver::MutationType, const QualifiedName* attributeName);
    void registerMutationObserver(MutationObserver&, MutationObserverOptions, const HashSet<AtomString>& attributeFilter);
    void unregisterMutationObserver(MutationObserverRegistration&);
    void registerTransientMutationObserver(MutationObserverRegistration&);
    void unregisterTransientMutationObserver(MutationObserverRegistration&);
    void notifyMutationObserversNodeWillDetach();

    unsigned connectedSubframeCount() const;
    void incrementConnectedSubframeCount(unsigned amount = 1);
    void decrementConnectedSubframeCount(unsigned amount = 1);
    void updateAncestorConnectedSubframeCountForRemoval() const;
    void updateAncestorConnectedSubframeCountForInsertion() const;

#if ENABLE(JIT)
    static ptrdiff_t nodeFlagsMemoryOffset() { return OBJECT_OFFSETOF(Node, m_nodeFlags); }
    static ptrdiff_t rareDataMemoryOffset() { return OBJECT_OFFSETOF(Node, m_rareData); }
    static int32_t flagIsText() { return IsTextFlag; }
    static int32_t flagIsContainer() { return IsContainerFlag; }
    static int32_t flagIsElement() { return IsElementFlag; }
    static int32_t flagIsShadowRoot() { return IsShadowRootFlag; }
    static int32_t flagIsHTML() { return IsHTMLFlag; }
    static int32_t flagIsLink() { return IsLinkFlag; }
    static int32_t flagHasFocusWithin() { return HasFocusWithin; }
    static int32_t flagIsParsingChildrenFinished() { return IsParsingChildrenFinishedFlag; }
    static int32_t flagChildrenAffectedByFirstChildRulesFlag() { return ChildrenAffectedByFirstChildRulesFlag; }
    static int32_t flagChildrenAffectedByLastChildRulesFlag() { return ChildrenAffectedByLastChildRulesFlag; }

    static int32_t flagAffectsNextSiblingElementStyle() { return AffectsNextSiblingElementStyle; }
    static int32_t flagStyleIsAffectedByPreviousSibling() { return StyleIsAffectedByPreviousSibling; }
#endif // ENABLE(JIT)

protected:
    enum NodeFlags {
        IsTextFlag = 1,
        IsContainerFlag = 1 << 1,
        IsElementFlag = 1 << 2,
        IsHTMLFlag = 1 << 3,
        IsSVGFlag = 1 << 4,
        IsMathMLFlag = 1 << 5,
        IsDocumentNodeFlag = 1 << 6,
        IsShadowRootFlag = 1 << 7,
        IsConnectedFlag = 1 << 8,
        IsInShadowTreeFlag = 1 << 9,
        // UnusedFlag = 1 << 10,
        HasEventTargetDataFlag = 1 << 11,

        // These bits are used by derived classes, pulled up here so they can
        // be stored in the same memory word as the Node bits above.
        ChildNeedsStyleRecalcFlag = 1 << 12, // ContainerNode
        DirectChildNeedsStyleRecalcFlag = 1 << 13,

        IsEditingTextOrUndefinedCustomElementFlag = 1 << 14, // Text and Element
        IsCustomElement = 1 << 15, // Element
        HasFocusWithin = 1 << 16,
        IsLinkFlag = 1 << 17,
        IsUserActionElement = 1 << 18,
        IsParsingChildrenFinishedFlag = 1 << 19,
        HasSyntheticAttrChildNodesFlag = 1 << 20,
        SelfOrAncestorHasDirAutoFlag = 1 << 21,

        // The following flags are used in style invalidation.
        StyleValidityShift = 22,
        StyleValidityMask = 3 << StyleValidityShift,
        StyleResolutionShouldRecompositeLayerFlag = 1 << 24,

        ChildrenAffectedByFirstChildRulesFlag = 1 << 25,
        ChildrenAffectedByLastChildRulesFlag = 1 << 26,
        // UnusedFlag = 1 << 27,

        AffectsNextSiblingElementStyle = 1 << 28,
        StyleIsAffectedByPreviousSibling = 1 << 29,
        DescendantsAffectedByPreviousSiblingFlag = 1 << 30,

        HasCustomStyleResolveCallbacksFlag = 1 << 31,

        DefaultNodeFlags = IsParsingChildrenFinishedFlag
    };

    bool getFlag(NodeFlags mask) const { return m_nodeFlags & mask; }
    void setFlag(bool f, NodeFlags mask) const { m_nodeFlags = (m_nodeFlags & ~mask) | (-(int32_t)f & mask); } 
    void setFlag(NodeFlags mask) const { m_nodeFlags |= mask; } 
    void clearFlag(NodeFlags mask) const { m_nodeFlags &= ~mask; }

    bool isParsingChildrenFinished() const { return getFlag(IsParsingChildrenFinishedFlag); }
    void setIsParsingChildrenFinished() { setFlag(IsParsingChildrenFinishedFlag); }
    void clearIsParsingChildrenFinished() { clearFlag(IsParsingChildrenFinishedFlag); }

    enum ConstructionType {
        CreateOther = DefaultNodeFlags,
        CreateText = DefaultNodeFlags | IsTextFlag,
        CreateContainer = DefaultNodeFlags | IsContainerFlag, 
        CreateElement = CreateContainer | IsElementFlag, 
        CreatePseudoElement =  CreateElement | IsConnectedFlag,
        CreateShadowRoot = CreateContainer | IsShadowRootFlag | IsInShadowTreeFlag,
        CreateDocumentFragment = CreateContainer,
        CreateHTMLElement = CreateElement | IsHTMLFlag,
        CreateSVGElement = CreateElement | IsSVGFlag | HasCustomStyleResolveCallbacksFlag,
        CreateMathMLElement = CreateElement | IsMathMLFlag,
        CreateDocument = CreateContainer | IsDocumentNodeFlag | IsConnectedFlag,
        CreateEditingText = CreateText | IsEditingTextOrUndefinedCustomElementFlag,
    };
    Node(Document&, ConstructionType);

    static constexpr uint32_t s_refCountIncrement = 2;
    static constexpr uint32_t s_refCountMask = ~static_cast<uint32_t>(1);

    enum class ElementStyleFlag : uint8_t {
        StyleAffectedByEmpty                                    = 1 << 0,
        // Bits for dynamic child matching.
        // We optimize for :first-child and :last-child. The other positional child selectors like nth-child or
        // *-child-of-type, we will just give up and re-evaluate whenever children change at all.
        ChildrenAffectedByForwardPositionalRules                = 1 << 1,
        DescendantsAffectedByForwardPositionalRules             = 1 << 2,
        ChildrenAffectedByBackwardPositionalRules               = 1 << 3,
        DescendantsAffectedByBackwardPositionalRules            = 1 << 4,
        ChildrenAffectedByPropertyBasedBackwardPositionalRules  = 1 << 5,
    };

    bool hasStyleFlag(ElementStyleFlag state) const { return m_rendererWithStyleFlags.type() & static_cast<uint8_t>(state); }
    void setStyleFlag(ElementStyleFlag state) { m_rendererWithStyleFlags.setType(m_rendererWithStyleFlags.type() | static_cast<uint8_t>(state)); }
    void clearStyleFlags() { m_rendererWithStyleFlags.setType(0); }

    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const { }

    bool hasRareData() const { return !!m_rareData; }
    NodeRareData* rareData() const { return m_rareData.get(); }
    NodeRareData& ensureRareData();
    void clearRareData();

    void clearEventTargetData();

    void setHasCustomStyleResolveCallbacks() { setFlag(true, HasCustomStyleResolveCallbacksFlag); }

    void setTreeScope(TreeScope& scope) { m_treeScope = &scope; }

    void invalidateStyle(Style::Validity, Style::InvalidationMode = Style::InvalidationMode::Normal);
    void updateAncestorsForStyleRecalc();

    ExceptionOr<RefPtr<Node>> convertNodesOrStringsIntoNode(Vector<NodeOrString>&&);

private:
    virtual PseudoId customPseudoId() const
    {
        ASSERT(hasCustomStyleResolveCallbacks());
        return PseudoId::None;
    }

    virtual bool virtualIsCharacterData() const { return false; }

    WEBCORE_EXPORT void removedLastRef();

    void refEventTarget() final;
    void derefEventTarget() final;
    bool isNode() const final;

    void trackForDebugging();
    void materializeRareData();

    Vector<std::unique_ptr<MutationObserverRegistration>>* mutationObserverRegistry();
    HashSet<MutationObserverRegistration*>* transientMutationObserverRegistry();

    void adjustStyleValidity(Style::Validity, Style::InvalidationMode);

    void* opaqueRootSlow() const;

    static void moveShadowTreeToNewDocument(ShadowRoot&, Document& oldDocument, Document& newDocument);
    static void moveTreeToNewScope(Node&, TreeScope& oldScope, TreeScope& newScope);
    void moveNodeToNewDocument(Document& oldDocument, Document& newDocument);

    struct NodeRareDataDeleter {
        void operator()(NodeRareData*) const;
    };

    mutable uint32_t m_refCountAndParentBit { s_refCountIncrement };
    mutable uint32_t m_nodeFlags;

    ContainerNode* m_parentNode { nullptr };
    TreeScope* m_treeScope { nullptr };
    Node* m_previous { nullptr };
    Node* m_next { nullptr };
    CompactPointerTuple<RenderObject*, uint8_t> m_rendererWithStyleFlags;
    std::unique_ptr<NodeRareData, NodeRareDataDeleter> m_rareData;
};

WEBCORE_EXPORT RefPtr<Node> commonInclusiveAncestor(Node&, Node&);

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

inline void* Node::opaqueRoot() const
{
    // FIXME: Possible race?
    // https://bugs.webkit.org/show_bug.cgi?id=165713
    if (isConnected())
        return &document();
    return opaqueRootSlow();
}

inline ContainerNode* Node::parentNodeGuaranteedHostFree() const
{
    ASSERT(!isShadowRoot());
    return parentNode();
}

inline Style::Validity Node::styleValidity() const
{
    return static_cast<Style::Validity>((m_nodeFlags & StyleValidityMask) >> StyleValidityShift);
}

inline bool Node::styleResolutionShouldRecompositeLayer() const
{
    return getFlag(StyleResolutionShouldRecompositeLayerFlag);
}

inline void Node::setHasValidStyle()
{
    m_nodeFlags &= ~StyleValidityMask;
    clearFlag(StyleResolutionShouldRecompositeLayerFlag);
}

inline void Node::setTreeScopeRecursively(TreeScope& newTreeScope)
{
    ASSERT(!isDocumentNode());
    ASSERT(!m_deletionHasBegun);
    if (m_treeScope != &newTreeScope)
        moveTreeToNewScope(*this, *m_treeScope, newTreeScope);
}

bool areNodesConnectedInSameTreeScope(const Node*, const Node*);

WTF::TextStream& operator<<(WTF::TextStream&, const Node&);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showTree(const WebCore::Node*);
void showNodePath(const WebCore::Node*);
#endif

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Node)
    static bool isType(const WebCore::EventTarget& target) { return target.isNode(); }
SPECIALIZE_TYPE_TRAITS_END()
