/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef Node_h
#define Node_h

#include "EventTarget.h"
#include "URLHash.h"
#include "LayoutRect.h"
#include "MutationObserver.h"
#include "RenderStyleConstants.h"
#include "TreeScope.h"
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/MainThread.h>
#include <wtf/TypeCasts.h>

// This needs to be here because Document.h also depends on it.
#define DUMP_NODE_STATISTICS 0

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
class NodeOrString;
class NodeRareData;
class QualifiedName;
class RenderBox;
class RenderBoxModelObject;
class RenderObject;
class RenderStyle;
class SVGQualifiedName;
class ShadowRoot;
class TouchEvent;
class UIRequestEvent;

const int nodeStyleChangeShift = 14;

// SyntheticStyleChange means that we need to go through the entire style change logic even though
// no style property has actually changed. It is used to restructure the tree when, for instance,
// RenderLayers are created or destroyed due to animation changes.
enum StyleChangeType { 
    NoStyleChange = 0, 
    InlineStyleChange = 1 << nodeStyleChangeShift, 
    FullStyleChange = 2 << nodeStyleChangeShift, 
    SyntheticStyleChange = 3 << nodeStyleChangeShift,
    ReconstructRenderTree = 4 << nodeStyleChangeShift,
};

class NodeRareDataBase {
public:
    RenderObject* renderer() const { return m_renderer; }
    void setRenderer(RenderObject* renderer) { m_renderer = renderer; }

protected:
    NodeRareDataBase(RenderObject* renderer)
        : m_renderer(renderer)
    { }

private:
    RenderObject* m_renderer;
};

class Node : public EventTarget {
    WTF_MAKE_FAST_ALLOCATED;

    friend class Document;
    friend class TreeScope;
    friend class TreeScopeAdopter;
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

    // Only used by ObjC / GObject bindings.
    static bool isSupportedForBindings(const String& feature, const String& version);

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
    virtual void setNodeValue(const String&, ExceptionCode&);
    virtual NodeType nodeType() const = 0;
    ContainerNode* parentNode() const;
    static ptrdiff_t parentNodeMemoryOffset() { return OBJECT_OFFSETOF(Node, m_parentNode); }
    Element* parentElement() const;
    Node* previousSibling() const { return m_previous; }
    static ptrdiff_t previousSiblingMemoryOffset() { return OBJECT_OFFSETOF(Node, m_previous); }
    Node* nextSibling() const { return m_next; }
    static ptrdiff_t nextSiblingMemoryOffset() { return OBJECT_OFFSETOF(Node, m_next); }
    RefPtr<NodeList> childNodes();
    Node* firstChild() const;
    Node* lastChild() const;
    bool hasAttributes() const;
    NamedNodeMap* attributes() const;
    Node* pseudoAwareNextSibling() const;
    Node* pseudoAwarePreviousSibling() const;
    Node* pseudoAwareFirstChild() const;
    Node* pseudoAwareLastChild() const;

    URL baseURI() const;
    
    void getSubresourceURLs(ListHashSet<URL>&) const;

    // These should all actually return a node, but this is only important for language bindings,
    // which will already know and hold a ref on the right node to return. Returning bool allows
    // these methods to be more efficient since they don't need to return a ref
    WEBCORE_EXPORT bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&);
    bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&);
    WEBCORE_EXPORT bool removeChild(Node* child, ExceptionCode&);
    WEBCORE_EXPORT bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&);

    bool hasChildNodes() const { return firstChild(); }

    enum class CloningOperation {
        OnlySelf,
        SelfWithTemplateContent,
        Everything,
    };
    virtual Ref<Node> cloneNodeInternal(Document&, CloningOperation) = 0;
    Ref<Node> cloneNode(bool deep) { return cloneNodeInternal(document(), deep ? CloningOperation::Everything : CloningOperation::OnlySelf); }
    RefPtr<Node> cloneNodeForBindings(bool deep, ExceptionCode&);

    virtual const AtomicString& localName() const;
    virtual const AtomicString& namespaceURI() const;
    virtual const AtomicString& prefix() const;
    virtual void setPrefix(const AtomicString&, ExceptionCode&);
    void normalize();

    bool isSameNode(Node* other) const { return this == other; }
    bool isEqualNode(Node*) const;
    bool isDefaultNamespace(const AtomicString& namespaceURI) const;
    String lookupPrefix(const AtomicString& namespaceURI) const;
    String lookupNamespaceURI(const String& prefix) const;
    String lookupNamespacePrefix(const AtomicString& namespaceURI, const Element* originalElement) const;
    
    WEBCORE_EXPORT String textContent(bool convertBRsToNewlines = false) const;
    WEBCORE_EXPORT void setTextContent(const String&, ExceptionCode&);
    
    Node* lastDescendant() const;
    Node* firstDescendant() const;

    // From the NonDocumentTypeChildNode - https://dom.spec.whatwg.org/#nondocumenttypechildnode
    Element* previousElementSibling() const;
    Element* nextElementSibling() const;

    // From the ChildNode - https://dom.spec.whatwg.org/#childnode
    void before(Vector<NodeOrString>&&, ExceptionCode&);
    void after(Vector<NodeOrString>&&, ExceptionCode&);
    void replaceWith(Vector<NodeOrString>&&, ExceptionCode&);
    WEBCORE_EXPORT void remove(ExceptionCode&);

    // Other methods (not part of DOM)

    bool isElementNode() const { return getFlag(IsElementFlag); }
    bool isContainerNode() const { return getFlag(IsContainerFlag); }
    bool isTextNode() const { return getFlag(IsTextFlag); }
    bool isHTMLElement() const { return getFlag(IsHTMLFlag); }
    bool isSVGElement() const { return getFlag(IsSVGFlag); }
    bool isMathMLElement() const { return getFlag(IsMathMLFlag); }

    bool isPseudoElement() const { return pseudoId() != NOPSEUDO; }
    bool isBeforePseudoElement() const { return pseudoId() == BEFORE; }
    bool isAfterPseudoElement() const { return pseudoId() == AFTER; }
    PseudoId pseudoId() const { return (isElementNode() && hasCustomStyleResolveCallbacks()) ? customPseudoId() : NOPSEUDO; }

    virtual bool isMediaControlElement() const { return false; }
    virtual bool isMediaControls() const { return false; }
#if ENABLE(VIDEO_TRACK)
    virtual bool isWebVTTElement() const { return false; }
#endif
    bool isStyledElement() const { return getFlag(IsStyledElementFlag); }
    virtual bool isAttributeNode() const { return false; }
    virtual bool isCharacterDataNode() const { return false; }
    virtual bool isFrameOwnerElement() const { return false; }
    virtual bool isPluginElement() const { return false; }
#if ENABLE(SERVICE_CONTROLS)
    virtual bool isImageControlsRootElement() const { return false; }
    virtual bool isImageControlsButtonElement() const { return false; }
#endif

    bool isDocumentNode() const;
    bool isTreeScope() const;
    bool isDocumentFragment() const { return getFlag(IsDocumentFragmentFlag); }
    bool isShadowRoot() const { return isDocumentFragment() && isTreeScope(); }

    bool isNamedFlowContentNode() const { return getFlag(IsNamedFlowContentNodeFlag); }
    bool hasCustomStyleResolveCallbacks() const { return getFlag(HasCustomStyleResolveCallbacksFlag); }

    bool hasSyntheticAttrChildNodes() const { return getFlag(HasSyntheticAttrChildNodesFlag); }
    void setHasSyntheticAttrChildNodes(bool flag) { setFlag(flag, HasSyntheticAttrChildNodesFlag); }

    // If this node is in a shadow tree, returns its shadow host. Otherwise, returns null.
    WEBCORE_EXPORT Element* shadowHost() const;
    // If this node is in a shadow tree, returns its shadow host. Otherwise, returns this.
    // Deprecated. Should use shadowHost() and check the return value.
    WEBCORE_EXPORT Node* deprecatedShadowAncestorNode() const;
    ShadowRoot* containingShadowRoot() const;
    ShadowRoot* shadowRoot() const;

#if ENABLE(SHADOW_DOM)
    HTMLSlotElement* assignedSlot() const;
#endif

#if ENABLE(CUSTOM_ELEMENTS)
    bool isCustomElement() const { return getFlag(IsCustomElement); }
    void setIsCustomElement() { setFlag(IsCustomElement); }

    bool isUnresolvedCustomElement() const { return isElementNode() && getFlag(IsEditingTextOrUnresolvedCustomElementFlag); }
    void setIsUnresolvedCustomElement() { setFlag(IsEditingTextOrUnresolvedCustomElementFlag); }
    void setCustomElementIsResolved();
#endif

    // Returns null, a child of ShadowRoot, or a legacy shadow root.
    Node* nonBoundaryShadowTreeRootNode();

    // Node's parent or shadow tree host.
    ContainerNode* parentOrShadowHostNode() const;
    Element* parentOrShadowHostElement() const;
    void setParentNode(ContainerNode*);
    Node* rootNode() const;

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

    bool isRootEditableElement() const;
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
    bool needsStyleRecalc() const { return styleChangeType() != NoStyleChange; }
    StyleChangeType styleChangeType() const { return static_cast<StyleChangeType>(m_nodeFlags & StyleChangeMask); }
    bool childNeedsStyleRecalc() const { return getFlag(ChildNeedsStyleRecalcFlag); }
    bool styleIsAffectedByPreviousSibling() const { return getFlag(StyleIsAffectedByPreviousSibling); }
    bool isEditingText() const { return getFlag(IsTextFlag) && getFlag(IsEditingTextOrUnresolvedCustomElementFlag); }

    void setChildNeedsStyleRecalc() { setFlag(ChildNeedsStyleRecalcFlag); }
    void clearChildNeedsStyleRecalc() { m_nodeFlags &= ~(ChildNeedsStyleRecalcFlag | DirectChildNeedsStyleRecalcFlag); }

    WEBCORE_EXPORT void setNeedsStyleRecalc(StyleChangeType = FullStyleChange);
    void clearNeedsStyleRecalc() { m_nodeFlags &= ~StyleChangeMask; }

    bool isLink() const { return getFlag(IsLinkFlag); }
    void setIsLink(bool flag) { setFlag(flag, IsLinkFlag); }

    void setIsNamedFlowContentNode() { setFlag(IsNamedFlowContentNodeFlag); }
    void clearIsNamedFlowContentNode() { clearFlag(IsNamedFlowContentNodeFlag); }

    bool hasEventTargetData() const { return getFlag(HasEventTargetDataFlag); }
    void setHasEventTargetData(bool flag) { setFlag(flag, HasEventTargetDataFlag); }

    enum UserSelectAllTreatment {
        UserSelectAllDoesNotAffectEditability,
        UserSelectAllIsAlwaysNonEditable
    };
    WEBCORE_EXPORT bool isContentEditable();
    bool isContentRichlyEditable();

    WEBCORE_EXPORT void inspect();

    bool hasEditableStyle(UserSelectAllTreatment treatment = UserSelectAllIsAlwaysNonEditable) const
    {
        return computeEditability(treatment, ShouldUpdateStyle::DoNotUpdate) != Editability::ReadOnly;
    }
    // FIXME: Replace every use of this function by helpers in htmlediting.h
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

    // Returns the document associated with this node.
    // A Document node returns itself.
    Document& document() const
    {
        return treeScope().documentScope();
    }

    TreeScope& treeScope() const
    {
        ASSERT(m_treeScope);
        return *m_treeScope;
    }
    static ptrdiff_t treeScopeMemoryOffset() { return OBJECT_OFFSETOF(Node, m_treeScope); }

    // Returns true if this node is associated with a document and is in its associated document's
    // node tree, false otherwise.
    bool inDocument() const 
    { 
        return getFlag(InDocumentFlag);
    }
    bool isInUserAgentShadowTree() const;
    bool isInShadowTree() const { return getFlag(IsInShadowTreeFlag); }
    bool isInTreeScope() const { return getFlag(static_cast<NodeFlags>(InDocumentFlag | IsInShadowTreeFlag)); }

    bool isDocumentTypeNode() const { return nodeType() == DOCUMENT_TYPE_NODE; }
    virtual bool childTypeAllowed(NodeType) const { return false; }
    unsigned countChildNodes() const;
    Node* traverseToChildAt(unsigned) const;

    void checkSetPrefix(const AtomicString& prefix, ExceptionCode&);

    WEBCORE_EXPORT bool isDescendantOf(const Node*) const;
    bool isDescendantOrShadowDescendantOf(const Node*) const;
    bool contains(const Node*) const;
    bool containsIncludingShadowDOM(const Node*) const;
    bool containsIncludingHostElements(const Node*) const;

    // Used to determine whether range offsets use characters or node indices.
    virtual bool offsetInCharacters() const;
    // Number of DOM 16-bit units contained in node. Note that rendered text length can be different - e.g. because of
    // css-transform:capitalize breaking up precomposed characters and ligatures.
    virtual int maxCharacterOffset() const;

    // Whether or not a selection can be started in this object
    virtual bool canStartSelection() const;

    virtual bool shouldSelectOnMouseDown() { return false; }

    // Getting points into and out of screen space
    FloatPoint convertToPage(const FloatPoint&) const;
    FloatPoint convertFromPage(const FloatPoint&) const;

    // -----------------------------------------------------------------------------
    // Integration with rendering tree

    // As renderer() includes a branch you should avoid calling it repeatedly in hot code paths.
    RenderObject* renderer() const { return hasRareData() ? m_data.m_rareData->renderer() : m_data.m_renderer; };
    void setRenderer(RenderObject* renderer)
    {
        if (hasRareData())
            m_data.m_rareData->setRenderer(renderer);
        else
            m_data.m_renderer = renderer;
    }

    // Use these two methods with caution.
    WEBCORE_EXPORT RenderBox* renderBox() const;
    RenderBoxModelObject* renderBoxModelObject() const;
    
    // Wrapper for nodes that don't have a renderer, but still cache the style (like HTMLOptionElement).
    RenderStyle* renderStyle() const;

    virtual RenderStyle* computedStyle(PseudoId pseudoElementSpecifier = NOPSEUDO);

    // -----------------------------------------------------------------------------
    // Notification of document structure changes (see ContainerNode.h for more notification methods)
    //
    // At first, WebKit notifies the node that it has been inserted into the document. This is called during document parsing, and also
    // when a node is added through the DOM methods insertBefore(), appendChild() or replaceChild(). The call happens _after_ the node has been added to the tree.
    // This is similar to the DOMNodeInsertedIntoDocument DOM event, but does not require the overhead of event
    // dispatching.
    //
    // WebKit notifies this callback regardless if the subtree of the node is a document tree or a floating subtree.
    // Implementation can determine the type of subtree by seeing insertionPoint->inDocument().
    // For a performance reason, notifications are delivered only to ContainerNode subclasses if the insertionPoint is out of document.
    //
    // There is another callback named finishedInsertingSubtree(), which is called after all descendants are notified.
    // Only a few subclasses actually need this. To utilize this, the node should return InsertionShouldCallFinishedInsertingSubtree
    // from insrtedInto().
    //
    enum InsertionNotificationRequest {
        InsertionDone,
        InsertionShouldCallFinishedInsertingSubtree
    };

    virtual InsertionNotificationRequest insertedInto(ContainerNode& insertionPoint);
    virtual void finishedInsertingSubtree() { }

    // Notifies the node that it is no longer part of the tree.
    //
    // This is a dual of insertedInto(), and is similar to the DOMNodeRemovedFromDocument DOM event, but does not require the overhead of event
    // dispatching, and is called _after_ the node is removed from the tree.
    //
    virtual void removedFrom(ContainerNode& insertionPoint);

#if ENABLE(TREE_DEBUGGING)
    virtual void formatForDebugger(char* buffer, unsigned length) const;

    void showNode(const char* prefix = "") const;
    void showTreeForThis() const;
    void showNodePathForThis() const;
    void showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2 = nullptr, const char* markedLabel2 = nullptr) const;
    void showTreeForThisAcrossFrame() const;
#endif // ENABLE(TREE_DEBUGGING)

    void invalidateNodeListAndCollectionCachesInAncestors(const QualifiedName* attrName = nullptr, Element* attributeOwnerElement = nullptr);
    NodeListsNodeData* nodeLists();
    void clearNodeLists();

    virtual bool willRespondToMouseMoveEvents();
    virtual bool willRespondToMouseClickEvents();
    virtual bool willRespondToMouseWheelEvents();

    WEBCORE_EXPORT unsigned short compareDocumentPosition(Node*);

    Node* toNode() override;

    EventTargetInterface eventTargetInterface() const override;
    ScriptExecutionContext* scriptExecutionContext() const final; // Implemented in Document.h

    bool addEventListener(const AtomicString& eventType, RefPtr<EventListener>&&, bool useCapture) override;
    bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) override;

    using EventTarget::dispatchEvent;
    bool dispatchEvent(Event&) override;

    void dispatchScopedEvent(Event&);

    virtual void handleLocalEvents(Event&);

    void dispatchSubtreeModifiedEvent();
    bool dispatchDOMActivateEvent(int detail, PassRefPtr<Event> underlyingEvent);

#if ENABLE(TOUCH_EVENTS)
    virtual bool allowsDoubleTapGesture() const { return true; }
#endif

#if ENABLE(TOUCH_EVENTS) && !PLATFORM(IOS)
    bool dispatchTouchEvent(TouchEvent&);
#endif
#if ENABLE(INDIE_UI)
    bool dispatchUIRequestEvent(UIRequestEvent&);
#endif

    bool dispatchBeforeLoadEvent(const String& sourceURL);

    virtual void dispatchInputEvent();

    // Perform the default action for an event.
    virtual void defaultEventHandler(Event*);

    void ref();
    void deref();
    bool hasOneRef() const;
    int refCount() const;

#ifndef NDEBUG
    bool m_deletionHasBegun { false };
    bool m_inRemovedLastRefFunction { false };
    bool m_adoptionIsRequired { true };
#endif

    EventTargetData* eventTargetData() final;
    EventTargetData& ensureEventTargetData() final;

    void getRegisteredMutationObserversOfType(HashMap<MutationObserver*, MutationRecordDeliveryOptions>&, MutationObserver::MutationType, const QualifiedName* attributeName);
    void registerMutationObserver(MutationObserver*, MutationObserverOptions, const HashSet<AtomicString>& attributeFilter);
    void unregisterMutationObserver(MutationObserverRegistration*);
    void registerTransientMutationObserver(MutationObserverRegistration*);
    void unregisterTransientMutationObserver(MutationObserverRegistration*);
    void notifyMutationObserversNodeWillDetach();

    WEBCORE_EXPORT void textRects(Vector<IntRect>&) const;

    unsigned connectedSubframeCount() const;
    void incrementConnectedSubframeCount(unsigned amount = 1);
    void decrementConnectedSubframeCount(unsigned amount = 1);
    void updateAncestorConnectedSubframeCountForRemoval() const;
    void updateAncestorConnectedSubframeCountForInsertion() const;

#if ENABLE(CSS_SELECTOR_JIT)
    static ptrdiff_t nodeFlagsMemoryOffset() { return OBJECT_OFFSETOF(Node, m_nodeFlags); }
    static ptrdiff_t rareDataMemoryOffset() { return OBJECT_OFFSETOF(Node, m_data.m_rareData); }
    static int32_t flagIsText() { return IsTextFlag; }
    static int32_t flagIsElement() { return IsElementFlag; }
    static int32_t flagIsHTML() { return IsHTMLFlag; }
    static int32_t flagIsLink() { return IsLinkFlag; }
    static int32_t flagHasRareData() { return HasRareDataFlag; }
    static int32_t flagIsParsingChildrenFinished() { return IsParsingChildrenFinishedFlag; }
    static int32_t flagChildrenAffectedByFirstChildRulesFlag() { return ChildrenAffectedByFirstChildRulesFlag; }
    static int32_t flagChildrenAffectedByLastChildRulesFlag() { return ChildrenAffectedByLastChildRulesFlag; }

    static int32_t flagAffectsNextSiblingElementStyle() { return AffectsNextSiblingElementStyle; }
    static int32_t flagStyleIsAffectedByPreviousSibling() { return StyleIsAffectedByPreviousSibling; }
#endif // ENABLE(CSS_SELECTOR_JIT)

protected:
    enum NodeFlags {
        IsTextFlag = 1,
        IsContainerFlag = 1 << 1,
        IsElementFlag = 1 << 2,
        IsStyledElementFlag = 1 << 3,
        IsHTMLFlag = 1 << 4,
        IsSVGFlag = 1 << 5,
        ChildNeedsStyleRecalcFlag = 1 << 7,
        InDocumentFlag = 1 << 8,
        IsLinkFlag = 1 << 9,
        IsUserActionElement = 1 << 10,
        HasRareDataFlag = 1 << 11,
        IsDocumentFragmentFlag = 1 << 12,

        // These bits are used by derived classes, pulled up here so they can
        // be stored in the same memory word as the Node bits above.
        IsParsingChildrenFinishedFlag = 1 << 13, // Element

        StyleChangeMask = 1 << nodeStyleChangeShift | 1 << (nodeStyleChangeShift + 1) | 1 << (nodeStyleChangeShift + 2),
        IsEditingTextOrUnresolvedCustomElementFlag = 1 << 17,
        IsNamedFlowContentNodeFlag = 1 << 18,
        HasSyntheticAttrChildNodesFlag = 1 << 19,
        HasCustomStyleResolveCallbacksFlag = 1 << 20,
        HasEventTargetDataFlag = 1 << 21,
        IsCustomElement = 1 << 22,
        IsInShadowTreeFlag = 1 << 23,
        IsMathMLFlag = 1 << 24,

        ChildrenAffectedByFirstChildRulesFlag = 1 << 25,
        ChildrenAffectedByLastChildRulesFlag = 1 << 26,
        ChildrenAffectedByHoverRulesFlag = 1 << 27,

        DirectChildNeedsStyleRecalcFlag = 1 << 28,
        AffectsNextSiblingElementStyle = 1 << 29,
        StyleIsAffectedByPreviousSibling = 1 << 30,

        SelfOrAncestorHasDirAutoFlag = 1 << 31,

        DefaultNodeFlags = IsParsingChildrenFinishedFlag
    };

    bool getFlag(NodeFlags mask) const { return m_nodeFlags & mask; }
    void setFlag(bool f, NodeFlags mask) const { m_nodeFlags = (m_nodeFlags & ~mask) | (-(int32_t)f & mask); } 
    void setFlag(NodeFlags mask) const { m_nodeFlags |= mask; } 
    void clearFlag(NodeFlags mask) const { m_nodeFlags &= ~mask; }

    enum ConstructionType {
        CreateOther = DefaultNodeFlags,
        CreateText = DefaultNodeFlags | IsTextFlag,
        CreateContainer = DefaultNodeFlags | IsContainerFlag, 
        CreateElement = CreateContainer | IsElementFlag, 
        CreatePseudoElement =  CreateElement | InDocumentFlag,
        CreateShadowRoot = CreateContainer | IsDocumentFragmentFlag | IsInShadowTreeFlag,
        CreateDocumentFragment = CreateContainer | IsDocumentFragmentFlag,
        CreateStyledElement = CreateElement | IsStyledElementFlag, 
        CreateHTMLElement = CreateStyledElement | IsHTMLFlag,
        CreateSVGElement = CreateStyledElement | IsSVGFlag | HasCustomStyleResolveCallbacksFlag,
        CreateDocument = CreateContainer | InDocumentFlag,
        CreateEditingText = CreateText | IsEditingTextOrUnresolvedCustomElementFlag,
        CreateMathMLElement = CreateStyledElement | IsMathMLFlag
    };
    Node(Document&, ConstructionType);

    virtual void didMoveToNewDocument(Document* oldDocument);
    
    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const { }

    bool hasRareData() const { return getFlag(HasRareDataFlag); }

    NodeRareData* rareData() const;
    NodeRareData& ensureRareData();
    void clearRareData();

    void clearEventTargetData();

    void setHasCustomStyleResolveCallbacks() { setFlag(true, HasCustomStyleResolveCallbacksFlag); }

    void setTreeScope(TreeScope& scope) { m_treeScope = &scope; }

    void setStyleChange(StyleChangeType changeType) { m_nodeFlags = (m_nodeFlags & ~StyleChangeMask) | changeType; }
    void updateAncestorsForStyleRecalc();

private:
    virtual PseudoId customPseudoId() const
    {
        ASSERT(hasCustomStyleResolveCallbacks());
        return NOPSEUDO;
    }

    WEBCORE_EXPORT void removedLastRef();

    void refEventTarget() override;
    void derefEventTarget() override;

    Element* ancestorElement() const;

    void trackForDebugging();
    void materializeRareData();

    Vector<std::unique_ptr<MutationObserverRegistration>>* mutationObserverRegistry();
    HashSet<MutationObserverRegistration*>* transientMutationObserverRegistry();

    int m_refCount;
    mutable uint32_t m_nodeFlags;

    ContainerNode* m_parentNode { nullptr };
    TreeScope* m_treeScope { nullptr };
    Node* m_previous { nullptr };
    Node* m_next { nullptr };
    // When a node has rare data we move the renderer into the rare data.
    union DataUnion {
        RenderObject* m_renderer;
        NodeRareDataBase* m_rareData;
    } m_data { nullptr };

protected:
    bool isParsingChildrenFinished() const { return getFlag(IsParsingChildrenFinishedFlag); }
    void setIsParsingChildrenFinished() { setFlag(IsParsingChildrenFinishedFlag); }
    void clearIsParsingChildrenFinished() { clearFlag(IsParsingChildrenFinishedFlag); }
};

#ifndef NDEBUG
inline void adopted(Node* node)
{
    if (!node)
        return;
    ASSERT(!node->m_deletionHasBegun);
    ASSERT(!node->m_inRemovedLastRefFunction);
    node->m_adoptionIsRequired = false;
}
#endif

ALWAYS_INLINE void Node::ref()
{
    ASSERT(isMainThread());
    ASSERT(!m_deletionHasBegun);
    ASSERT(!m_inRemovedLastRefFunction);
    ASSERT(!m_adoptionIsRequired);
    ++m_refCount;
}

ALWAYS_INLINE void Node::deref()
{
    ASSERT(isMainThread());
    ASSERT(m_refCount >= 0);
    ASSERT(!m_deletionHasBegun);
    ASSERT(!m_inRemovedLastRefFunction);
    ASSERT(!m_adoptionIsRequired);
    if (--m_refCount <= 0 && !parentNode()) {
#ifndef NDEBUG
        m_inRemovedLastRefFunction = true;
#endif
        removedLastRef();
    }
}

ALWAYS_INLINE bool Node::hasOneRef() const
{
    ASSERT(!m_deletionHasBegun);
    ASSERT(!m_inRemovedLastRefFunction);
    return m_refCount == 1;
}

ALWAYS_INLINE int Node::refCount() const
{
    return m_refCount;
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

#if ENABLE(CUSTOM_ELEMENTS)

inline void Node::setCustomElementIsResolved()
{
    clearFlag(IsEditingTextOrUnresolvedCustomElementFlag);
    setFlag(IsCustomElement);
}

#endif

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::Node*);
void showNodePath(const WebCore::Node*);
#endif

#endif
