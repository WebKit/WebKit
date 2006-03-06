/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef DOM_NodeImpl_h_
#define DOM_NodeImpl_h_

#include "DocPtr.h"
#include "Shared.h"
#include "PlatformString.h"
#include <kxmlcore/HashSet.h>
#include <kxmlcore/PassRefPtr.h>

class QStringList;
class QTextStream;
class RenderArena;

template <typename T> class QPtrList;

namespace WebCore {

class AtomicString;
class ContainerNodeImpl;
class DocumentImpl;
class ElementImpl;
class EventImpl;
class EventListener;
class IntRect;
class KeyEvent;
class MouseEvent;
class NamedAttrMapImpl;
class NodeListImpl;
class QualifiedName;
class RegisteredEventListener;
class RenderObject;
class RenderStyle;
class WheelEvent;

typedef int ExceptionCode;

// this class implements nodes, which can have a parent but no children:
class NodeImpl : public TreeShared<NodeImpl>
{
    friend class DocumentImpl;
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
        NOTATION_NODE = 12
    };

    NodeImpl(DocumentImpl*);
    virtual ~NodeImpl();

    // DOM methods & attributes for Node

    virtual bool hasTagName(const QualifiedName&) const { return false; }
    virtual DOMString nodeName() const = 0;
    virtual DOMString nodeValue() const;
    virtual void setNodeValue(const DOMString&, ExceptionCode&);
    virtual NodeType nodeType() const = 0;
    NodeImpl* parentNode() const { return parent(); }
    NodeImpl* previousSibling() const { return m_previous; }
    NodeImpl* nextSibling() const { return m_next; }
    virtual PassRefPtr<NodeListImpl> childNodes();
    virtual NodeImpl* firstChild() const;
    virtual NodeImpl* lastChild() const;
    virtual bool hasAttributes() const;
    virtual NamedAttrMapImpl* attributes() const;
    virtual DocumentImpl* ownerDocument() const;

    // These should all actually return a node, but this is only important for language bindings,
    // which will already know and hold a ref on the right node to return. Returning bool allows
    // these methods to be more efficient since they don't need to return a ref
    virtual bool insertBefore(PassRefPtr<NodeImpl> newChild, NodeImpl* refChild, ExceptionCode&);
    virtual bool replaceChild(PassRefPtr<NodeImpl> newChild, NodeImpl* oldChild, ExceptionCode&);
    virtual bool removeChild(NodeImpl* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<NodeImpl> newChild, ExceptionCode&);

    virtual void remove(ExceptionCode&);
    virtual bool hasChildNodes() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep) = 0;
    virtual const AtomicString& localName() const;
    virtual const AtomicString& namespaceURI() const;
    virtual const AtomicString& prefix() const;
    virtual void setPrefix(const AtomicString&, ExceptionCode&);
    void normalize();
    static bool isSupported(const DOMString& feature, const DOMString& version);

    bool isSameNode(NodeImpl* other) const { return this == other; }
    bool isEqualNode(NodeImpl*) const;
    bool isDefaultNamespace(const DOMString& namespaceURI) const;
    DOMString lookupPrefix(const DOMString& namespaceURI) const;
    DOMString lookupNamespaceURI(const DOMString& prefix) const;
    DOMString lookupNamespacePrefix(const DOMString& namespaceURI, const ElementImpl* originalElement) const;
    
    DOMString textContent() const;
    void setTextContent(const DOMString&, ExceptionCode&);
    
    NodeImpl* lastDescendant() const;

    // Other methods (not part of DOM)

    virtual bool isElementNode() const { return false; }
    virtual bool isHTMLElement() const { return false; }
#if SVG_SUPPORT
    virtual bool isSVGElement() const { return false; }
#endif
    virtual bool isStyledElement() const { return false; }
    virtual bool isAttributeNode() const { return false; }
    virtual bool isTextNode() const { return false; }
    virtual bool isCommentNode() const { return false; }
    virtual bool isDocumentNode() const { return false; }
    bool isBlockFlow() const;
    bool isBlockFlowOrBlockTable() const;
    
    // Used by <form> elements to indicate a malformed state of some kind, typically
    // used to keep from applying the bottom margin of the form.
    virtual bool isMalformed() { return false; }
    virtual void setMalformed(bool malformed) { }
    
    // These low-level calls give the caller responsibility for maintaining the integrity of the tree.
    void setPreviousSibling(NodeImpl* previous) { m_previous = previous; }
    void setNextSibling(NodeImpl* next) { m_next = next; }

    // FIXME: These two functions belong in editing -- "atomic node" is an editing concept.
    NodeImpl* previousNodeConsideringAtomicNodes() const;
    NodeImpl* nextNodeConsideringAtomicNodes() const;
    
    /** (Not part of the official DOM)
     * Returns the next leaf node.
     *
     * Using this function delivers leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
     * @return next leaf node or 0 if there are no more.
     */
    NodeImpl* nextLeafNode() const;

    /** (Not part of the official DOM)
     * Returns the previous leaf node.
     *
     * Using this function delivers leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
     * @return previous leaf node or 0 if there are no more.
     */
    NodeImpl* previousLeafNode() const;

    bool isEditableBlock() const;
    ElementImpl* enclosingBlockFlowElement() const;
    ElementImpl* enclosingBlockFlowOrTableElement() const;
    ElementImpl* enclosingInlineElement() const;
    ElementImpl* rootEditableElement() const;
    
    bool inSameRootEditableElement(NodeImpl*);
    bool inSameContainingBlockFlowElement(NodeImpl*);
    
    // Used by the parser. Checks against the DTD, unlike DOM operations like appendChild().
    // Also does not dispatch DOM mutation events.
    // Returns the appropriate container node for future insertions as you parse, or 0 for failure.
    virtual ContainerNodeImpl* addChild(PassRefPtr<NodeImpl>);

    // Called by the parser when this element's close tag is reached,
    // signalling that all child tags have been parsed and added.
    // This is needed for <applet> and <object> elements, which can't lay themselves out
    // until they know all of their nested <param>s. [Radar 3603191, 4040848].
    // Also used for script elements and some SVG elements for similar purposes,
    // but making parsing a special case in this respect should be avoided if possible.
    virtual void closeRenderer() { }

    // For <link> and <style> elements.
    virtual void sheetLoaded() { }

    bool hasID() const { return m_hasId; }
    bool hasClass() const { return m_hasClass; }
    bool hasStyle() const { return m_hasStyle; }
    bool active() const { return m_active; }
    bool inActiveChain() const { return m_inActiveChain; }
    bool hovered() const { return m_hovered; }
    bool focused() const { return m_focused; }
    bool attached() const { return m_attached; }
    void setAttached(bool b = true) { m_attached = b; }
    bool changed() const { return m_changed; }
    bool hasChangedChild() const { return m_hasChangedChild; }
    bool isLink() const { return m_isLink; }
    // FIXME: Should inDocument also make sure a document exists in case the document
    // has been destroyed before the node is removed from the document?
    bool inDocument() const { return document.get() && m_inDocument; }
    bool styleElement() const { return m_styleElement; }
    bool implicitNode() const { return m_implicit; }
    void setHasID(bool b = true) { m_hasId = b; }
    void setHasClass(bool b = true) { m_hasClass = b; }
    void setHasStyle(bool b = true) { m_hasStyle = b; }
    void setHasChangedChild( bool b = true ) { m_hasChangedChild = b; }
    void setInDocument(bool b = true) { m_inDocument = b; }
    void setInActiveChain(bool b = true) { m_inActiveChain = b; }
    void setChanged(bool b = true);

    virtual void setFocus(bool b = true) { m_focused = b; }
    virtual void setActive(bool b = true, bool pause=false) { m_active = b; }
    virtual void setHovered(bool b = true) { m_hovered = b; }

    unsigned short tabIndex() const { return m_tabIndex; }
    void setTabIndex(unsigned short i) { m_tabIndex = i; }

    /**
     * Whether this node can receive the keyboard focus.
     */
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;
    virtual bool isControl() const { return false; } // Eventually the notion of what is a control will be extensible.
    virtual bool isEnabled() const { return true; }
    virtual bool isChecked() const { return false; }
    virtual bool isIndeterminate() const { return false; }

    virtual bool isContentEditable() const;
    virtual IntRect getRect() const;

    enum StyleChange { NoChange, NoInherit, Inherit, Detach, Force };
    virtual void recalcStyle(StyleChange = NoChange) { }
    StyleChange diff(RenderStyle*, RenderStyle*) const;

    unsigned nodeIndex() const;

    // Returns the document that this node is associated with. This is guaranteed to always be non-null, as opposed to
    // DOM's ownerDocument() which is null for Document nodes (and sometimes DocumentType nodes).
    DocumentImpl* getDocument() const { return document.get(); }

    void setDocument(DocumentImpl*);

    void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    void removeHTMLEventListener(const AtomicString& eventType);
    void setHTMLEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
    EventListener *getHTMLEventListener(const AtomicString& eventType);
    void removeAllEventListeners();

    bool dispatchEvent(PassRefPtr<EventImpl>, ExceptionCode&, bool tempEvent = false);
    bool dispatchGenericEvent(PassRefPtr<EventImpl>, ExceptionCode&, bool tempEvent = false);
    bool dispatchHTMLEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
    void dispatchWindowEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
    bool dispatchMouseEvent(MouseEvent*, const AtomicString& eventType,
        int detail = 0, NodeImpl* relatedTarget = 0);
    bool dispatchSimulatedMouseEvent(const AtomicString& eventType);
    bool dispatchMouseEvent(const AtomicString& eventType, int button, int detail,
        int clientX, int clientY, int screenX, int screenY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        bool isSimulated = false, NodeImpl* relatedTarget = 0);
    bool dispatchUIEvent(const AtomicString& eventType, int detail = 0);
    bool dispatchSubtreeModifiedEvent(bool childrenChanged = true);
    bool dispatchKeyEvent(KeyEvent*);
    void dispatchWheelEvent(WheelEvent*);

    void handleLocalEvents(EventImpl*, bool useCapture);

    // Handlers to do/undo actions on the target node before an event is dispatched to it and after the event
    // has been dispatched.  The data pointer is handed back by the preDispatch and passed to postDispatch.
    virtual void* preDispatchEventHandler(EventImpl*) { return 0; }
    virtual void postDispatchEventHandler(EventImpl*, void* data) { }

    /**
     * Perform the default action for an event e.g. submitting a form
     */
    virtual void defaultEventHandler(EventImpl*);

    /**
     * Used for disabled form elements; if true, prevents mouse events from being dispatched
     * to event listeners, and prevents DOMActivate events from being sent at all.
     */
    virtual bool disabled() const;

    virtual bool isReadOnly();
    virtual bool childTypeAllowed(NodeType) { return false; }
    virtual unsigned childNodeCount() const;
    virtual NodeImpl* childNode(unsigned index) const;

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
    NodeImpl* traverseNextNode(const NodeImpl* stayWithin = 0) const;
    
    /* Like traverseNextNode, but skips children and starts with the next sibling. */
    NodeImpl* traverseNextSibling(const NodeImpl* stayWithin = 0) const;

    /**
     * Does a reverse pre-order traversal to find the node that comes before the current one in document order
     *
     * see @ref traverseNextNode()
     */
    NodeImpl* traversePreviousNode() const;

    /* Like traversePreviousNode, but visits nodes before their children. */
    NodeImpl* traversePreviousNodePostOrder(const NodeImpl *stayWithin = 0) const;

    /**
     * Finds previous or next editable leaf node.
     */
    NodeImpl* previousEditable() const;
    NodeImpl* nextEditable() const;
    NodeImpl* nextEditable(int offset) const;

    RenderObject* renderer() const { return m_renderer; }
    RenderObject* nextRenderer();
    RenderObject* previousRenderer();
    void setRenderer(RenderObject* renderer) { m_renderer = renderer; }
    
    void checkSetPrefix(const AtomicString& prefix, ExceptionCode&);
    bool isAncestor(const NodeImpl*) const;

    // These two methods are mutually exclusive.  The former is used to do strict error-checking
    // when adding children via the public DOM API (e.g., appendChild()).  The latter is called only when parsing, 
    // to sanity-check against the DTD for error recovery.
    void checkAddChild(NodeImpl* newChild, ExceptionCode&); // Error-checking when adding via the DOM API
    virtual bool childAllowed(NodeImpl* newChild);          // Error-checking during parsing that checks the DTD

    // Used to determine whether range offsets use characters or node indices.
    virtual bool offsetInCharacters() const;

    // FIXME: These next 7 functions are mostly editing-specific and should be moved out.
    virtual int maxOffset() const;
    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    virtual int previousOffset(int current) const;
    virtual int nextOffset(int current) const;
    
#ifndef NDEBUG
    virtual void dump(QTextStream*, QString indent = "") const;
#endif

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
    virtual RenderStyle* styleForRenderer(RenderObject* parent);
    virtual bool rendererIsNeeded(RenderStyle*);
#if SVG_SUPPORT
    virtual bool childShouldCreateRenderer(NodeImpl*) const { return true; }
#endif
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    // -----------------------------------------------------------------------------
    // Methods for maintaining the state of the element between history navigation

    /**
     * Indicates whether or not this type of node maintains it's state. If so, the state of the node will be stored when
     * the user goes to a different page using the state() method, and restored using the restoreState() method if the
     * user returns (e.g. using the back button). This is used to ensure that user-changeable elements such as form
     * controls maintain their contents when the user returns to a previous page in the history.
     */
    virtual bool maintainsState();

    /**
     * Returns the state of this node represented as a string. This string will be passed to restoreState() if the user
     * returns to the page.
     *
     * @return State information about the node represented as a string
     */
    virtual QString state();

    /**
     * Sets the state of the element based on strings previously returned by state(). This is used to initialize form
     * controls with their old values when the user returns to the page in their history.  The receiver
     * should remove the string from the list that it uses for its restore.
     *
     * @param states The strings previously returned by nodes' state methods.
     */
    virtual void restoreState(QStringList& stateList);

    // -----------------------------------------------------------------------------
    // Notification of document stucture changes

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
    virtual void insertedIntoTree(bool deep) { }
    virtual void removedFromTree(bool deep) { }

    /**
     * Notifies the node that it's list of children have changed (either by adding or removing child nodes), or a child
     * node that is of the type CDATA_SECTION_NODE, TEXT_NODE or COMMENT_NODE has changed its value.
     */
    virtual void childrenChanged();

    virtual DOMString toString() const = 0;

#ifndef NDEBUG
    virtual void formatForDebugger(char *buffer, unsigned length) const;

    void showNode(const char *prefix="") const;
    void showTree() const;
    void showTreeAndMark(NodeImpl* markedNode1, const char* markedLabel1, NodeImpl* markedNode2, const char* markedLabel2) const;
#endif

    void registerNodeList(NodeListImpl*);
    void unregisterNodeList(NodeListImpl*);
    void notifyNodeListsChildrenChanged();
    void notifyLocalNodeListsChildrenChanged();
    void notifyNodeListsAttributeChanged();
    void notifyLocalNodeListsAttributeChanged();
    
    PassRefPtr<NodeListImpl> getElementsByTagName(const DOMString&);
    PassRefPtr<NodeListImpl> getElementsByTagNameNS(const DOMString& namespaceURI, const DOMString& localName);

private: // members
    DocPtr<DocumentImpl> document;
    NodeImpl* m_previous;
    NodeImpl* m_next;
    RenderObject* m_renderer;
protected:
    QPtrList<RegisteredEventListener>* m_regdListeners;
    typedef HashSet<NodeListImpl*> NodeListSet;
    NodeListSet* m_nodeLists;

    unsigned short m_tabIndex : 15;
    bool m_hasTabIndex : 1;

    bool m_hasId : 1;
    bool m_hasClass : 1;
    bool m_hasStyle : 1;
    bool m_attached : 1;
    bool m_changed : 1;
    bool m_hasChangedChild : 1;
    bool m_inDocument : 1;

    bool m_isLink : 1;
    bool m_specified : 1; // used in AttrImpl; accessor functions there
    bool m_focused : 1;
    bool m_active : 1;
    bool m_hovered : 1;
    bool m_inActiveChain : 1;
    bool m_styleElement : 1; // contains stylesheet text
    bool m_implicit : 1; // implicitly generated by the parser

    bool m_inDetach : 1;

private:
    ElementImpl* ancestorElement() const;
};

#ifndef NDEBUG

void showTree(const NodeImpl *node);

extern int gEventDispatchForbidden;
inline void forbidEventDispatch() { ++gEventDispatchForbidden; }
inline void allowEventDispatch() { if (gEventDispatchForbidden > 0) --gEventDispatchForbidden; }
inline bool eventDispatchForbidden() { return gEventDispatchForbidden > 0; }

#else

inline void forbidEventDispatch() { }
inline void allowEventDispatch() { }

#endif NDEBUG

} //namespace

#endif
