/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "KURLHash.h"
#include "RenderStyleConstants.h"
#include "ScriptWrappable.h"
#include "TreeShared.h"
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>

#if USE(JSC)
namespace JSC {
class JSGlobalData;
class MarkStack;
}
#endif

namespace WebCore {

class Attribute;
class ClassNodeList;
class ContainerNode;
class Document;
class DynamicNodeList;
class Element;
class Event;
class EventListener;
class FloatPoint;
class Frame;
class IntRect;
class KeyboardEvent;
class NSResolver;
class NamedNodeMap;
class NameNodeList;
class NodeList;
class NodeRareData;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;
class QualifiedName;
class RegisteredEventListener;
class RenderArena;
class RenderBox;
class RenderBoxModelObject;
class RenderObject;
class RenderStyle;
class StringBuilder;
class TagNodeList;

typedef int ExceptionCode;

const int nodeStyleChangeShift = 24;

// SyntheticStyleChange means that we need to go through the entire style change logic even though
// no style property has actually changed. It is used to restructure the tree when, for instance,
// RenderLayers are created or destroyed due to animation changes.
enum StyleChangeType { 
    NoStyleChange = 0, 
    InlineStyleChange = 1 << nodeStyleChangeShift, 
    FullStyleChange = 2 << nodeStyleChangeShift, 
    SyntheticStyleChange = 3 << nodeStyleChangeShift
};

class Node : public EventTarget, public TreeShared<ContainerNode>, public ScriptWrappable {
    friend class Document;
public:
    enum NodeType {
        ELEMENT_NODE = 1,
        ATTRIBUTE_NODE = 2,
        TEXT_NODE = 3,
        CDATA_SECTION_NODE = 4,
        ENTITY_REFERENCE_NODE = 5,
        ENTITY_NODE = 6,
        PROCESSING_INSTRUCTION_NODE = 7,
        COMMENT_NODE = 8,
        DOCUMENT_NODE = 9,
        DOCUMENT_TYPE_NODE = 10,
        DOCUMENT_FRAGMENT_NODE = 11,
        NOTATION_NODE = 12,
        XPATH_NAMESPACE_NODE = 13
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

    static bool isSupported(const String& feature, const String& version);

    static void startIgnoringLeaks();
    static void stopIgnoringLeaks();

    static void dumpStatistics();

    enum StyleChange { NoChange, NoInherit, Inherit, Detach, Force };    
    static StyleChange diff(const RenderStyle*, const RenderStyle*);

    virtual ~Node();

    // DOM methods & attributes for Node

    bool hasTagName(const QualifiedName&) const;
    virtual String nodeName() const = 0;
    virtual String nodeValue() const;
    virtual void setNodeValue(const String&, ExceptionCode&);
    virtual NodeType nodeType() const = 0;
    ContainerNode* parentNode() const { return parent(); }
    Element* parentElement() const;
    Node* previousSibling() const { return m_previous; }
    Node* nextSibling() const { return m_next; }
    PassRefPtr<NodeList> childNodes();
    Node* firstChild() const { return isContainerNode() ? containerFirstChild() : 0; }
    Node* lastChild() const { return isContainerNode() ? containerLastChild() : 0; }
    bool hasAttributes() const;
    NamedNodeMap* attributes() const;

    virtual KURL baseURI() const;
    
    void getSubresourceURLs(ListHashSet<KURL>&) const;

    // These should all actually return a node, but this is only important for language bindings,
    // which will already know and hold a ref on the right node to return. Returning bool allows
    // these methods to be more efficient since they don't need to return a ref
    virtual bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&, bool shouldLazyAttach = false);
    virtual bool removeChild(Node* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&, bool shouldLazyAttach = false);

    void remove(ExceptionCode&);
    bool hasChildNodes() const { return firstChild(); }
    virtual PassRefPtr<Node> cloneNode(bool deep) = 0;
    const AtomicString& localName() const { return virtualLocalName(); }
    const AtomicString& namespaceURI() const { return virtualNamespaceURI(); }
    const AtomicString& prefix() const { return virtualPrefix(); }
    virtual void setPrefix(const AtomicString&, ExceptionCode&);
    void normalize();

    bool isSameNode(Node* other) const { return this == other; }
    bool isEqualNode(Node*) const;
    bool isDefaultNamespace(const AtomicString& namespaceURI) const;
    String lookupPrefix(const AtomicString& namespaceURI) const;
    String lookupNamespaceURI(const String& prefix) const;
    String lookupNamespacePrefix(const AtomicString& namespaceURI, const Element* originalElement) const;
    
    String textContent(bool convertBRsToNewlines = false) const;
    void setTextContent(const String&, ExceptionCode&);
    
    Node* lastDescendant() const;
    Node* firstDescendant() const;
    
    // Other methods (not part of DOM)

    bool isElementNode() const { return getFlag(IsElementFlag); }
    bool isContainerNode() const { return getFlag(IsContainerFlag); }
    bool isTextNode() const { return getFlag(IsTextFlag); }

    bool isHTMLElement() const { return getFlag(IsHTMLFlag); }

    bool isSVGElement() const { return getFlag(IsSVGFlag); }

#if ENABLE(WML)
    virtual bool isWMLElement() const { return false; }
#else
    static bool isWMLElement() { return false; }
#endif

#if ENABLE(MATHML)
    virtual bool isMathMLElement() const { return false; }
#else
    static bool isMathMLElement() { return false; }
#endif


    virtual bool isMediaControlElement() const { return false; }
    bool isStyledElement() const { return getFlag(IsStyledElementFlag); }
    virtual bool isFrameOwnerElement() const { return false; }
    virtual bool isAttributeNode() const { return false; }
    bool isCommentNode() const { return getFlag(IsCommentFlag); }
    virtual bool isCharacterDataNode() const { return false; }
    bool isDocumentNode() const;
    virtual bool isShadowNode() const { return false; }
    virtual ContainerNode* shadowParentNode() { return 0; }
    Node* shadowAncestorNode();
    Node* shadowTreeRootNode();
    bool isInShadowTree();

    // The node's parent for the purpose of event capture and bubbling.
    virtual ContainerNode* eventParentNode();

    // Returns the enclosing event parent node (or self) that, when clicked, would trigger a navigation.
    Node* enclosingLinkEventParentOrSelf();

    // Node ancestors when concerned about event flow
    void eventAncestors(Vector<RefPtr<ContainerNode> > &ancestors);

    bool isBlockFlow() const;
    bool isBlockFlowOrBlockTable() const;
    
    // These low-level calls give the caller responsibility for maintaining the integrity of the tree.
    void setPreviousSibling(Node* previous) { m_previous = previous; }
    void setNextSibling(Node* next) { m_next = next; }

    // FIXME: These two functions belong in editing -- "atomic node" is an editing concept.
    Node* previousNodeConsideringAtomicNodes() const;
    Node* nextNodeConsideringAtomicNodes() const;
    
    /** (Not part of the official DOM)
     * Returns the next leaf node.
     *
     * Using this function delivers leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
     * @return next leaf node or 0 if there are no more.
     */
    Node* nextLeafNode() const;

    /** (Not part of the official DOM)
     * Returns the previous leaf node.
     *
     * Using this function delivers leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
     * @return previous leaf node or 0 if there are no more.
     */
    Node* previousLeafNode() const;

    bool isEditableBlock() const;
    
    // enclosingBlockFlowElement() is deprecated.  Use enclosingBlock instead.
    Element* enclosingBlockFlowElement() const;
    
    Element* enclosingInlineElement() const;
    Element* rootEditableElement() const;
    
    bool inSameContainingBlockFlowElement(Node*);

    // FIXME: All callers of this function are almost certainly wrong!
    virtual void deprecatedParserAddChild(PassRefPtr<Node>);

    // Called by the parser when this element's close tag is reached,
    // signaling that all child tags have been parsed and added.
    // This is needed for <applet> and <object> elements, which can't lay themselves out
    // until they know all of their nested <param>s. [Radar 3603191, 4040848].
    // Also used for script elements and some SVG elements for similar purposes,
    // but making parsing a special case in this respect should be avoided if possible.
    virtual void finishParsingChildren() { }
    virtual void beginParsingChildren() { }

    // Called by the frame right before dispatching an unloadEvent. [Radar 4532113]
    // This is needed for HTMLInputElements to tell the frame that it is done editing 
    // (sends textFieldDidEndEditing notification)
    virtual void aboutToUnload() { }

    // For <link> and <style> elements.
    virtual bool sheetLoaded() { return true; }

    bool hasID() const { return getFlag(HasIDFlag); }
    bool hasClass() const { return getFlag(HasClassFlag); }
    bool active() const { return getFlag(IsActiveFlag); }
    bool inActiveChain() const { return getFlag(InActiveChainFlag); }
    bool inDetach() const { return getFlag(InDetachFlag); }
    bool hovered() const { return getFlag(IsHoveredFlag); }
    bool focused() const { return hasRareData() ? rareDataFocused() : false; }
    bool attached() const { return getFlag(IsAttachedFlag); }
    void setAttached() { setFlag(IsAttachedFlag); }
    bool needsStyleRecalc() const { return styleChangeType() != NoStyleChange; }
    StyleChangeType styleChangeType() const { return static_cast<StyleChangeType>(m_nodeFlags & StyleChangeMask); }
    bool childNeedsStyleRecalc() const { return getFlag(ChildNeedsStyleRecalcFlag); }
    bool isLink() const { return getFlag(IsLinkFlag); }

    void setHasID(bool f) { setFlag(f, HasIDFlag); }
    void setHasClass(bool f) { setFlag(f, HasClassFlag); }
    void setChildNeedsStyleRecalc() { setFlag(ChildNeedsStyleRecalcFlag); }
    void clearChildNeedsStyleRecalc() { clearFlag(ChildNeedsStyleRecalcFlag); }
    void setInDocument() { setFlag(InDocumentFlag); }
    void clearInDocument() { clearFlag(InDocumentFlag); }

    void setInActiveChain() { setFlag(InActiveChainFlag); }
    void clearInActiveChain() { clearFlag(InActiveChainFlag); }

    void setNeedsStyleRecalc(StyleChangeType changeType = FullStyleChange);
    void clearNeedsStyleRecalc() { m_nodeFlags &= ~StyleChangeMask; }

    void setIsLink(bool f) { setFlag(f, IsLinkFlag); }
    void setIsLink() { setFlag(IsLinkFlag); }
    void clearIsLink() { clearFlag(IsLinkFlag); }

    enum ShouldSetAttached {
        SetAttached,
        DoNotSetAttached
    };
    void lazyAttach(ShouldSetAttached = SetAttached);

    virtual void setFocus(bool b = true);
    virtual void setActive(bool f = true, bool /*pause*/ = false) { setFlag(f, IsActiveFlag); }
    virtual void setHovered(bool f = true) { setFlag(f, IsHoveredFlag); }

    virtual short tabIndex() const;

    // Whether this kind of node can receive focus by default. Most nodes are
    // not focusable but some elements, such as form controls and links are.
    virtual bool supportsFocus() const;
    // Whether the node can actually be focused.
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;

    virtual bool isContentEditable() const;
    virtual bool isContentRichlyEditable() const;
    virtual bool shouldUseInputMethod() const;
    virtual IntRect getRect() const;
    IntRect renderRect(bool* isReplaced);

    // Returns true if the node has a non-empty bounding box in layout.
    // This does not 100% guarantee the user can see it, but is pretty close.
    // Note: This method only works properly after layout has occurred.
    bool hasNonEmptyBoundingBox() const;

    virtual void recalcStyle(StyleChange = NoChange) { }

    unsigned nodeIndex() const;

    // Returns the DOM ownerDocument attribute. This method never returns NULL, except in the case 
    // of (1) a Document node or (2) a DocumentType node that is not used with any Document yet. 
    virtual Document* ownerDocument() const;

    // Returns the document associated with this node. This method never returns NULL, except in the case 
    // of a DocumentType node that is not used with any Document yet. A Document node returns itself.
    Document* document() const
    {
        ASSERT(this);
        ASSERT(m_document || (nodeType() == DOCUMENT_TYPE_NODE && !inDocument()));
        return m_document;
    }

    // Do not use this method to change the document of a node until after the node has been
    // removed from its previous document.
    void setDocument(Document*);

    // Returns true if this node is associated with a document and is in its associated document's
    // node tree, false otherwise.
    bool inDocument() const 
    { 
        ASSERT(m_document || !getFlag(InDocumentFlag));
        return getFlag(InDocumentFlag);
    }

    bool isReadOnlyNode() const { return nodeType() == ENTITY_REFERENCE_NODE; }
    virtual bool childTypeAllowed(NodeType) { return false; }
    unsigned childNodeCount() const { return isContainerNode() ? containerChildNodeCount() : 0; }
    Node* childNode(unsigned index) const { return isContainerNode() ? containerChildNode(index) : 0; }

    /**
     * Does a pre-order traversal of the tree to find the node next node after this one. This uses the same order that
     * the tags appear in the source file.
     *
     * @param stayWithin If not null, the traversal will stop once the specified node is reached. This can be used to
     * restrict traversal to a particular sub-tree.
     *
     * @return The next node, in document order
     *
     * see @ref traversePreviousNode()
     */
    Node* traverseNextNode(const Node* stayWithin = 0) const;
    
    // Like traverseNextNode, but skips children and starts with the next sibling.
    Node* traverseNextSibling(const Node* stayWithin = 0) const;

    /**
     * Does a reverse pre-order traversal to find the node that comes before the current one in document order
     *
     * see @ref traverseNextNode()
     */
    Node* traversePreviousNode(const Node * stayWithin = 0) const;

    // Like traverseNextNode, but visits parents after their children.
    Node* traverseNextNodePostOrder() const;

    // Like traversePreviousNode, but visits parents before their children.
    Node* traversePreviousNodePostOrder(const Node *stayWithin = 0) const;
    Node* traversePreviousSiblingPostOrder(const Node *stayWithin = 0) const;

    /**
     * Finds previous or next editable leaf node.
     */
    Node* previousEditable() const;
    Node* nextEditable() const;

    RenderObject* renderer() const { return m_renderer; }
    RenderObject* nextRenderer();
    RenderObject* previousRenderer();
    void setRenderer(RenderObject* renderer) { m_renderer = renderer; }
    
    // Use these two methods with caution.
    RenderBox* renderBox() const;
    RenderBoxModelObject* renderBoxModelObject() const;

    void checkSetPrefix(const AtomicString& prefix, ExceptionCode&);
    bool isDescendantOf(const Node*) const;
    bool contains(const Node*) const;

    // This method is used to do strict error-checking when adding children via
    // the public DOM API (e.g., appendChild()).
    void checkAddChild(Node* newChild, ExceptionCode&); // Error-checking when adding via the DOM API

    void checkReplaceChild(Node* newChild, Node* oldChild, ExceptionCode&);
    virtual bool canReplaceChild(Node* newChild, Node* oldChild);
    
    // Used to determine whether range offsets use characters or node indices.
    virtual bool offsetInCharacters() const;
    // Number of DOM 16-bit units contained in node. Note that rendered text length can be different - e.g. because of
    // css-transform:capitalize breaking up precomposed characters and ligatures.
    virtual int maxCharacterOffset() const;
    
    // FIXME: We should try to find a better location for these methods.
    virtual bool canSelectAll() const { return false; }
    virtual void selectAll() { }

    // Whether or not a selection can be started in this object
    virtual bool canStartSelection() const;

    // Getting points into and out of screen space
    FloatPoint convertToPage(const FloatPoint& p) const;
    FloatPoint convertFromPage(const FloatPoint& p) const;

    // -----------------------------------------------------------------------------
    // Integration with rendering tree

    /**
     * Attaches this node to the rendering tree. This calculates the style to be applied to the node and creates an
     * appropriate RenderObject which will be inserted into the tree (except when the style has display: none). This
     * makes the node visible in the FrameView.
     */
    virtual void attach();

    /**
     * Detaches the node from the rendering tree, making it invisible in the rendered view. This method will remove
     * the node's rendering object from the rendering tree and delete it.
     */
    virtual void detach();

    virtual void willRemove();
    void createRendererIfNeeded();
    PassRefPtr<RenderStyle> styleForRenderer();
    virtual bool rendererIsNeeded(RenderStyle*);
#if ENABLE(SVG) || ENABLE(XHTMLMP)
    virtual bool childShouldCreateRenderer(Node*) const { return true; }
#endif
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    
    // Wrapper for nodes that don't have a renderer, but still cache the style (like HTMLOptionElement).
    RenderStyle* renderStyle() const;
    virtual void setRenderStyle(PassRefPtr<RenderStyle>);

    RenderStyle* computedStyle(PseudoId pseudoElementSpecifier = NOPSEUDO) { return virtualComputedStyle(pseudoElementSpecifier); }

    // -----------------------------------------------------------------------------
    // Notification of document structure changes

    /**
     * Notifies the node that it has been inserted into the document. This is called during document parsing, and also
     * when a node is added through the DOM methods insertBefore(), appendChild() or replaceChild(). Note that this only
     * happens when the node becomes part of the document tree, i.e. only when the document is actually an ancestor of
     * the node. The call happens _after_ the node has been added to the tree.
     *
     * This is similar to the DOMNodeInsertedIntoDocument DOM event, but does not require the overhead of event
     * dispatching.
     */
    virtual void insertedIntoDocument();

    /**
     * Notifies the node that it is no longer part of the document tree, i.e. when the document is no longer an ancestor
     * node.
     *
     * This is similar to the DOMNodeRemovedFromDocument DOM event, but does not require the overhead of event
     * dispatching, and is called _after_ the node is removed from the tree.
     */
    virtual void removedFromDocument();

    // These functions are called whenever you are connected or disconnected from a tree.  That tree may be the main
    // document tree, or it could be another disconnected tree.  Override these functions to do any work that depends
    // on connectedness to some ancestor (e.g., an ancestor <form> for example).
    virtual void insertedIntoTree(bool /*deep*/) { }
    virtual void removedFromTree(bool /*deep*/) { }

    /**
     * Notifies the node that it's list of children have changed (either by adding or removing child nodes), or a child
     * node that is of the type CDATA_SECTION_NODE, TEXT_NODE or COMMENT_NODE has changed its value.
     */
    virtual void childrenChanged(bool /*changedByParser*/ = false, Node* /*beforeChange*/ = 0, Node* /*afterChange*/ = 0, int /*childCountDelta*/ = 0) { }

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const;

    void showNode(const char* prefix = "") const;
    void showTreeForThis() const;
    void showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2 = 0, const char* markedLabel2 = 0) const;
#endif

    void registerDynamicNodeList(DynamicNodeList*);
    void unregisterDynamicNodeList(DynamicNodeList*);
    void notifyNodeListsChildrenChanged();
    void notifyLocalNodeListsChildrenChanged();
    void notifyNodeListsAttributeChanged();
    void notifyLocalNodeListsAttributeChanged();
    void notifyLocalNodeListsLabelChanged();
    void removeCachedClassNodeList(ClassNodeList*, const String&);
    void removeCachedNameNodeList(NameNodeList*, const String&);
    void removeCachedTagNodeList(TagNodeList*, const QualifiedName&);
    void removeCachedLabelsNodeList(DynamicNodeList*);
    
    PassRefPtr<NodeList> getElementsByTagName(const AtomicString&);
    PassRefPtr<NodeList> getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName);
    PassRefPtr<NodeList> getElementsByName(const String& elementName);
    PassRefPtr<NodeList> getElementsByClassName(const String& classNames);

    PassRefPtr<Element> querySelector(const String& selectors, ExceptionCode&);
    PassRefPtr<NodeList> querySelectorAll(const String& selectors, ExceptionCode&);

    unsigned short compareDocumentPosition(Node*);

    virtual Node* toNode() { return this; }

    virtual ScriptExecutionContext* scriptExecutionContext() const;

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);

    // Handlers to do/undo actions on the target node before an event is dispatched to it and after the event
    // has been dispatched.  The data pointer is handed back by the preDispatch and passed to postDispatch.
    virtual void* preDispatchEventHandler(Event*) { return 0; }
    virtual void postDispatchEventHandler(Event*, void* /*dataFromPreDispatch*/) { }

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtr<Event>);

    bool dispatchGenericEvent(PassRefPtr<Event>);
    virtual void handleLocalEvents(Event*);

    void dispatchSubtreeModifiedEvent();
    void dispatchUIEvent(const AtomicString& eventType, int detail, PassRefPtr<Event> underlyingEvent);
    bool dispatchKeyEvent(const PlatformKeyboardEvent&);
    void dispatchWheelEvent(PlatformWheelEvent&);
    bool dispatchMouseEvent(const PlatformMouseEvent&, const AtomicString& eventType,
        int clickCount = 0, Node* relatedTarget = 0);
    bool dispatchMouseEvent(const AtomicString& eventType, int button, int clickCount,
        int pageX, int pageY, int screenX, int screenY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        bool isSimulated, Node* relatedTarget, PassRefPtr<Event> underlyingEvent);
    void dispatchSimulatedMouseEvent(const AtomicString& eventType, PassRefPtr<Event> underlyingEvent);
    void dispatchSimulatedClick(PassRefPtr<Event> underlyingEvent, bool sendMouseEvents = false, bool showPressedLook = true);

    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();

    /**
     * Perform the default action for an event e.g. submitting a form
     */
    virtual void defaultEventHandler(Event*);

    /**
     * Used for disabled form elements; if true, prevents mouse events from being dispatched
     * to event listeners, and prevents DOMActivate events from being sent at all.
     */
    virtual bool disabled() const;

    using TreeShared<ContainerNode>::ref;
    using TreeShared<ContainerNode>::deref;

    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

#if USE(JSC)
    void markCachedNodeLists(JSC::MarkStack& markStack, JSC::JSGlobalData& globalData)
    {
        // NodeLists may be present.  If so, they need to be marked.
        if (!hasRareData())
            return;

        markCachedNodeListsSlow(markStack, globalData);
    }
#endif

private:
    enum NodeFlags {
        IsTextFlag = 1,
        IsCommentFlag = 1 << 1,
        IsContainerFlag = 1 << 2,
        IsElementFlag = 1 << 3,
        IsStyledElementFlag = 1 << 4,
        IsHTMLFlag = 1 << 5,
        IsSVGFlag = 1 << 6,
        HasIDFlag = 1 << 7,
        HasClassFlag = 1 << 8,
        IsAttachedFlag = 1 << 9,
        ChildNeedsStyleRecalcFlag = 1 << 10,
        InDocumentFlag = 1 << 11,
        IsLinkFlag = 1 << 12,
        IsActiveFlag = 1 << 13,
        IsHoveredFlag = 1 << 14,
        InActiveChainFlag = 1 << 15,
        InDetachFlag = 1 << 16,
        HasRareDataFlag = 1 << 17,

        // These bits are used by derived classes, pulled up here so they can
        // be stored in the same memory word as the Node bits above.
        IsParsingChildrenFinishedFlag = 1 << 18, // Element
        IsStyleAttributeValidFlag = 1 << 19, // StyledElement
        IsSynchronizingStyleAttributeFlag = 1 << 20, // StyledElement
#if ENABLE(SVG)
        AreSVGAttributesValidFlag = 1 << 21, // Element
        IsSynchronizingSVGAttributesFlag = 1 << 22, // SVGElement
        HasSVGRareDataFlag = 1 << 23, // SVGElement
#endif
        StyleChangeMask = 1 << nodeStyleChangeShift | 1 << (nodeStyleChangeShift + 1),

#if ENABLE(SVG)
        DefaultNodeFlags = IsParsingChildrenFinishedFlag | IsStyleAttributeValidFlag | AreSVGAttributesValidFlag
#else
        DefaultNodeFlags = IsParsingChildrenFinishedFlag | IsStyleAttributeValidFlag
#endif
    };

    // 5 bits remaining

    bool getFlag(NodeFlags mask) const { return m_nodeFlags & mask; }
    void setFlag(bool f, NodeFlags mask) const { m_nodeFlags = (m_nodeFlags & ~mask) | (-(int32_t)f & mask); } 
    void setFlag(NodeFlags mask) const { m_nodeFlags |= mask; } 
    void clearFlag(NodeFlags mask) const { m_nodeFlags &= ~mask; } 

protected:
    enum ConstructionType { 
        CreateOther = DefaultNodeFlags,
        CreateText = DefaultNodeFlags | IsTextFlag,
        CreateComment = DefaultNodeFlags | IsCommentFlag,
        CreateContainer = DefaultNodeFlags | IsContainerFlag, 
        CreateElement = CreateContainer | IsElementFlag, 
        CreateStyledElement = CreateElement | IsStyledElementFlag, 
        CreateHTMLElement = CreateStyledElement | IsHTMLFlag, 
        CreateSVGElement = CreateStyledElement | IsSVGFlag, 
    };
    Node(Document*, ConstructionType);

    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();
    
    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const { }
    void setTabIndexExplicitly(short);
    
    bool hasRareData() const { return getFlag(HasRareDataFlag); }

    NodeRareData* rareData() const;
    NodeRareData* ensureRareData();

private:
#if USE(JSC)
    void markCachedNodeListsSlow(JSC::MarkStack&, JSC::JSGlobalData&);
#endif

    void setStyleChange(StyleChangeType);

    // Used to share code between lazyAttach and setNeedsStyleRecalc.
    void markAncestorsWithChildNeedsStyleRecalc();

    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    virtual NodeRareData* createRareData();
    Node* containerChildNode(unsigned index) const;
    unsigned containerChildNodeCount() const;
    Node* containerFirstChild() const;
    Node* containerLastChild() const;
    bool rareDataFocused() const;

    virtual RenderStyle* nonRendererRenderStyle() const;

    virtual const AtomicString& virtualPrefix() const;
    virtual const AtomicString& virtualLocalName() const;
    virtual const AtomicString& virtualNamespaceURI() const;
    virtual RenderStyle* virtualComputedStyle(PseudoId = NOPSEUDO);

    Element* ancestorElement() const;

    void appendTextContent(bool convertBRsToNewlines, StringBuilder&) const;
    void trackForDebugging();

    Document* m_document;
    Node* m_previous;
    Node* m_next;
    RenderObject* m_renderer;
    mutable uint32_t m_nodeFlags;

 protected:
    bool isParsingChildrenFinished() const { return getFlag(IsParsingChildrenFinishedFlag); }
    void setIsParsingChildrenFinished() { setFlag(IsParsingChildrenFinishedFlag); }
    void clearIsParsingChildrenFinished() { clearFlag(IsParsingChildrenFinishedFlag); }
    bool isStyleAttributeValid() const { return getFlag(IsStyleAttributeValidFlag); }
    void setIsStyleAttributeValid(bool f) { setFlag(f, IsStyleAttributeValidFlag); }
    void setIsStyleAttributeValid() const { setFlag(IsStyleAttributeValidFlag); }
    void clearIsStyleAttributeValid() { clearFlag(IsStyleAttributeValidFlag); }
    bool isSynchronizingStyleAttribute() const { return getFlag(IsSynchronizingStyleAttributeFlag); }
    void setIsSynchronizingStyleAttribute(bool f) { setFlag(f, IsSynchronizingStyleAttributeFlag); }
    void setIsSynchronizingStyleAttribute() const { setFlag(IsSynchronizingStyleAttributeFlag); }
    void clearIsSynchronizingStyleAttribute() const { clearFlag(IsSynchronizingStyleAttributeFlag); }

#if ENABLE(SVG)
    bool areSVGAttributesValid() const { return getFlag(AreSVGAttributesValidFlag); }
    void setAreSVGAttributesValid() const { setFlag(AreSVGAttributesValidFlag); }
    void clearAreSVGAttributesValid() { clearFlag(AreSVGAttributesValidFlag); }
    bool isSynchronizingSVGAttributes() const { return getFlag(IsSynchronizingSVGAttributesFlag); }
    void setIsSynchronizingSVGAttributes() const { setFlag(IsSynchronizingSVGAttributesFlag); }
    void clearIsSynchronizingSVGAttributes() const { clearFlag(IsSynchronizingSVGAttributesFlag); }
    bool hasRareSVGData() const { return getFlag(HasSVGRareDataFlag); }
    void setHasRareSVGData() { setFlag(HasSVGRareDataFlag); }
    void clearHasRareSVGData() { clearFlag(HasSVGRareDataFlag); }
#endif
};

// Used in Node::addSubresourceAttributeURLs() and in addSubresourceStyleURLs()
inline void addSubresourceURL(ListHashSet<KURL>& urls, const KURL& url)
{
    if (!url.isNull())
        urls.add(url);
}

} //namespace

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::Node*);
#endif

#endif
