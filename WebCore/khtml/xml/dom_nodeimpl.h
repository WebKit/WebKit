/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef _DOM_NodeImpl_h_
#define _DOM_NodeImpl_h_

#include "dom/dom_string.h"
#include "dom/dom_node.h"
#include "misc/helper.h"
#include "misc/shared.h"
#include "dom_atomicstring.h"

class QPainter;
template <class type> class QPtrList;
template <class type> class QPtrDict;
class KHTMLView;
class RenderArena;
class QRect;
class QMouseEvent;
class QKeyEvent;
class QTextStream;
class QStringList;
class QWheelEvent;

namespace khtml {
    class RenderObject;
    class RenderStyle;
};

namespace DOM {

class AtomicString;
class DocumentImpl;
class ElementImpl;
class EventImpl;
class EventListener;
class NodeListImpl;
class NamedAttrMapImpl;
class QualifiedName;
class RegisteredEventListener;

class DocumentPtr : public khtml::Shared<DocumentPtr>
{
public:
    DocumentImpl *document() const { return doc; }
    static DocumentPtr *nullDocumentPtr();
    
private:
    DocumentPtr() { doc = 0; }
    friend class DocumentImpl;
    friend class DOMImplementationImpl;

    DocumentImpl *doc;
};

// this class implements nodes, which can have a parent but no children:
class NodeImpl : public khtml::TreeShared<NodeImpl>
{
    friend class DocumentImpl;
public:
    NodeImpl(DocumentPtr *doc);
    virtual ~NodeImpl();

    // DOM methods & attributes for Node
    virtual bool hasTagName(const QualifiedName& tagName) const { return false; }
    virtual DOMString nodeName() const = 0;
    virtual DOMString nodeValue() const;
    virtual void setNodeValue( const DOMString &_nodeValue, int &exceptioncode );
    virtual unsigned short nodeType() const = 0;
    NodeImpl *parentNode() const { return parent(); }
    NodeImpl *previousSibling() const { return m_previous; }
    NodeImpl *nextSibling() const { return m_next; }
    virtual SharedPtr<NodeListImpl> childNodes();
    virtual NodeImpl *firstChild() const;
    virtual NodeImpl *lastChild() const;
    virtual bool hasAttributes() const;
    virtual NamedAttrMapImpl *attributes() const;
    virtual DocumentImpl *ownerDocument() const;
    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual void remove(int &exceptioncode);
    virtual bool hasChildNodes (  ) const;
    virtual NodeImpl *cloneNode ( bool deep ) = 0;
    virtual const AtomicString& localName() const;
    virtual const AtomicString& namespaceURI() const;
    virtual const AtomicString& prefix() const;
    virtual void setPrefix(const AtomicString &_prefix, int &exceptioncode);
    void normalize ();
    static bool isSupported(const DOMString &feature, const DOMString &version);

    bool isSameNode(NodeImpl *other) { return this == other; }
    bool isEqualNode(NodeImpl *other) const;
    
    NodeImpl *lastDescendant() const;

    // Other methods (not part of DOM)
    virtual bool isElementNode() const { return false; }
    virtual bool isHTMLElement() const { return false; }
    virtual bool isStyledElement() const { return false; }
    virtual bool isAttributeNode() const { return false; }
    virtual bool isTextNode() const { return false; }
    virtual bool isCommentNode() const { return false; }
    virtual bool isDocumentNode() const { return false; }
    bool isBlockFlow() const;
    bool isBlockFlowOrTable() const;
    
    // Used by <form> elements to indicate a malformed state of some kind, typically
    // used to keep from applying the bottom margin of the form.
    virtual bool isMalformed() { return false; }
    virtual void setMalformed(bool malformed) {};
    
    virtual bool containsOnlyWhitespace() const { return false; }
    
    // helper functions not being part of the DOM
    // Attention: they assume that the caller did the consistency checking!
    void setPreviousSibling(NodeImpl *previous) { m_previous = previous; }
    void setNextSibling(NodeImpl *next) { m_next = next; }

    virtual void setFirstChild(NodeImpl *child);
    virtual void setLastChild(NodeImpl *child);

    bool isAtomicNode() const;
    NodeImpl *previousNodeConsideringAtomicNodes() const;
    NodeImpl *nextNodeConsideringAtomicNodes() const;
    
    /** (Not part of the official DOM)
     * Returns the next leaf node.
     *
     * Using this function delivers leaf nodes as if the whole DOM tree
     * were a linear chain of its leaf nodes.
     * @return next leaf node or 0 if there are no more.
     */
    NodeImpl *nextLeafNode() const;

    /** (Not part of the official DOM)
     * Returns the previous leaf node.
     *
     * Using this function delivers leaf nodes as if the whole DOM tree
     * were a linear chain of its leaf nodes.
     * @return previous leaf node or 0 if there are no more.
     */
    NodeImpl *previousLeafNode() const;

    bool isEditableBlock() const;
    ElementImpl *enclosingBlockFlowElement() const;
    ElementImpl *enclosingBlockFlowOrTableElement() const;
    ElementImpl *enclosingInlineElement() const;
    ElementImpl *rootEditableElement() const;
    
    bool inSameRootEditableElement(NodeImpl *);
    bool inSameContainingBlockFlowElement(NodeImpl *);
    
    // used by the parser. Checks against the DTD, unlike DOM operations like
    // appendChild(), and returns the new container node for future insertions as you parse.
    virtual NodeImpl *addChild(NodeImpl *newChild);
    
    // called by the parser when this element's close tag is reached,
    // signalling that all child tags have been parsed and added.
    // This is only needed for <applet> and <object> elements, which can't lay themselves out
    // until they know all of their nested <param>s. [3603191, 4040848]
    virtual void closeRenderer() {}

    enum MouseEventType {
        MousePress,
        MouseRelease,
        MouseClick,
        MouseDblClick,
        MouseMove
    };

    struct MouseEvent
    {
        MouseEvent( int _button, MouseEventType _type,
                    const DOMString &_url = DOMString(), const DOMString& _target = DOMString(),
                    NodeImpl *_innerNode = 0)
            {
                button = _button; type = _type;
                url = _url; target = _target;
                innerNode.reset(_innerNode);
            }

        int button;
        MouseEventType type;
        DOMString url; // url under mouse or empty
        DOMString target;
        SharedPtr<NodeImpl> innerNode;
    };

    // for LINK and STYLE
    virtual void sheetLoaded() {}

    bool hasID() const      { return m_hasId; }
    bool hasClass() const   { return m_hasClass; }
    bool hasStyle() const   { return m_hasStyle; }
    bool active() const     { return m_active; }
    bool inActiveChain() const { return m_inActiveChain; }
    bool hovered() const { return m_hovered; }
    bool focused() const { return m_focused; }
    bool attached() const   { return m_attached; }
    bool changed() const    { return m_changed; }
    bool hasChangedChild() const { return m_hasChangedChild; }
    bool isLink() const { return m_isLink; }
    // inDocument should also make sure a document exists in case the document has been destroyed before the node is removed from the document.
    bool inDocument() const { return document->document() && m_inDocument; }
    bool styleElement() const { return m_styleElement; }
    bool implicitNode() const { return m_implicit; }
    void setHasID(bool b=true) { m_hasId = b; }
    void setHasClass(bool b=true) { m_hasClass = b; }
    void setHasStyle(bool b=true) { m_hasStyle = b; }
    void setHasChangedChild( bool b = true ) { m_hasChangedChild = b; }
    void setInDocument(bool b=true) { m_inDocument = b; }
    virtual void setFocus(bool b=true) { m_focused = b; }
    virtual void setActive(bool b=true, bool pause=false) { m_active = b; }
    void setInActiveChain(bool b=true) { m_inActiveChain = b; }
    virtual void setHovered(bool b=true) { m_hovered = b; }
    void setChanged(bool b=true);

    unsigned short tabIndex() const { return m_tabIndex; }
    void setTabIndex(unsigned short _tabIndex) { m_tabIndex = _tabIndex; }

    /**
        * whether this node can receive the keyboard focus.
     */
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;
    virtual bool isControl() const { return false; } // Eventually the notion of what is a control will be extensible.
    virtual bool isEnabled() const { return true; }
    virtual bool isChecked() const { return false; }
    virtual bool isIndeterminate() const { return false; }

    virtual bool isContentEditable() const;
    virtual QRect getRect() const;

    enum StyleChange { NoChange, NoInherit, Inherit, Detach, Force };
    virtual void recalcStyle( StyleChange = NoChange ) {}
    StyleChange diff( khtml::RenderStyle *s1, khtml::RenderStyle *s2 ) const;

    unsigned nodeIndex() const;
    // Returns the document that this node is associated with. This is guaranteed to always be non-null, as opposed to
    // DOM's ownerDocument() which is null for Document nodes (and sometimes DocumentType nodes).
    DocumentImpl* getDocument() const { return document->document(); }

    void setDocument(DocumentPtr *doc);

    void addEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture);
    void removeEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture);
    void removeHTMLEventListener(const AtomicString &eventType);
    void setHTMLEventListener(const AtomicString &eventType, EventListener *listener);
    EventListener *getHTMLEventListener(const AtomicString &eventType);
    void removeAllEventListeners();

    bool dispatchEvent(EventImpl *evt, int &exceptioncode, bool tempEvent = false);
    bool dispatchGenericEvent( EventImpl *evt, int &exceptioncode);
    bool dispatchHTMLEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg);
    bool dispatchWindowEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg);
    bool dispatchMouseEvent(QMouseEvent *e, const AtomicString &overrideType, int overrideDetail = 0);
    bool dispatchSimulatedMouseEvent(const AtomicString &eventType);
    bool dispatchMouseEvent(const AtomicString &eventType, int button, int detail,
        int clientX, int clientY, int screenX, int screenY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey);
    bool dispatchUIEvent(const AtomicString &eventType, int detail = 0);
    bool dispatchSubtreeModifiedEvent(bool childrenChanged = true);
    bool dispatchKeyEvent(QKeyEvent *key);
    void dispatchWheelEvent(QWheelEvent *);

    void handleLocalEvents(EventImpl *evt, bool useCapture);

    // Handlers to do/undo actions on the target node before an event is dispatched to it and after the event
    // has been dispatched.  The data pointer is handed back by the preDispatch and passed to postDispatch.
    virtual void* preDispatchEventHandler(EventImpl *evt) { return 0; }
    virtual void postDispatchEventHandler(EventImpl *evt, void* data) {}

    /**
     * Perform the default action for an event e.g. submitting a form
     */
    virtual void defaultEventHandler(EventImpl *evt);

    /**
     * Used for disabled form elements; if true, prevents mouse events from being dispatched
     * to event listeners, and prevents DOMActivate events from being sent at all.
     */
    virtual bool disabled() const;

    virtual bool isReadOnly();
    virtual bool childTypeAllowed( unsigned short /*type*/ ) { return false; }
    virtual unsigned childNodeCount() const;
    virtual NodeImpl *childNode(unsigned index);

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
    NodeImpl *traverseNextNode(const NodeImpl *stayWithin = 0) const;
    
    /* Like traverseNextNode, but skips children and starts with the next sibling. */
    NodeImpl *traverseNextSibling(const NodeImpl *stayWithin = 0) const;

    /**
     * Does a reverse pre-order traversal to find the node that comes before the current one in document order
     *
     * see @ref traverseNextNode()
     */
    NodeImpl *traversePreviousNode() const;

    /* Like traversePreviousNode, but visits nodes before their children. */
    NodeImpl *traversePreviousNodePostOrder(const NodeImpl *stayWithin = 0) const;

    DocumentPtr *docPtr() const { return document; }

    NodeImpl *previousEditable() const;
    NodeImpl *nextEditable() const;
    //bool isEditable() const;

    khtml::RenderObject *renderer() const { return m_render; }
    khtml::RenderObject *nextRenderer();
    khtml::RenderObject *previousRenderer();
    void setRenderer(khtml::RenderObject* renderer) { m_render = renderer; }
    
    void checkSetPrefix(const AtomicString &_prefix, int &exceptioncode);
    bool isAncestor(const NodeImpl *) const;

	// These two methods are mutually exclusive.  The former is used to do strict error-checking
	// when adding children via the public DOM API (e.g., appendChild()).  The latter is called only when parsing, 
	// to sanity-check against the DTD for error recovery.
	void checkAddChild(NodeImpl *newChild, int &exceptioncode);    // Error-checking when adding via the DOM API
	virtual bool childAllowed(NodeImpl *newChild);				   // Error-checking during parsing that checks the DTD
	
    virtual int maxOffset() const;
    int maxDeepOffset() const;
    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;

    virtual int previousOffset (int current) const;
    virtual int nextOffset (int current) const;
    
#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

    // -----------------------------------------------------------------------------
    // Integration with rendering tree

    /**
     * Attaches this node to the rendering tree. This calculates the style to be applied to the node and creates an
     * appropriate RenderObject which will be inserted into the tree (except when the style has display: none). This
     * makes the node visible in the KHTMLView.
     */
    virtual void attach();

    /**
     * Detaches the node from the rendering tree, making it invisible in the rendered view. This method will remove
     * the node's rendering object from the rendering tree and delete it.
     */
    virtual void detach();

    virtual void willRemove();
    void createRendererIfNeeded();
    virtual khtml::RenderStyle *styleForRenderer(khtml::RenderObject *parent);
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);

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
    virtual void restoreState(QStringList &stateList);

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
    virtual void insertedIntoTree(bool deep) {};
    virtual void removedFromTree(bool deep) {};

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
    void showTreeAndMark(NodeImpl * markedNode1, const char * markedLabel1, NodeImpl * markedNode2, const char * markedLabel2) const;
	
#endif

    void registerNodeList(NodeListImpl *list);
    void unregisterNodeList(NodeListImpl *list);
    void notifyNodeListsChildrenChanged();
    void notifyLocalNodeListsChildrenChanged();
    void notifyNodeListsAttributeChanged();
    void notifyLocalNodeListsAttributeChanged();
    
    SharedPtr<NodeListImpl> getElementsByTagName(const DOMString &name) { return getElementsByTagNameNS("*", name); }
    SharedPtr<NodeListImpl> getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName);

private: // members
    DocumentPtr *document;
    NodeImpl *m_previous;
    NodeImpl *m_next;
protected:
    khtml::RenderObject *m_render;
    QPtrList<RegisteredEventListener> *m_regdListeners;
    QPtrDict<NodeListImpl> *m_nodeLists;

    unsigned short m_tabIndex : 15;
    bool m_hasTabIndex  : 1;

    bool m_hasId : 1;
    bool m_hasClass : 1;
    bool m_hasStyle : 1;
    bool m_attached : 1;
    bool m_changed : 1;
    bool m_hasChangedChild : 1;
    bool m_inDocument : 1;

    bool m_isLink : 1;
    bool m_specified : 1; // used in AttrImpl. Accessor functions there
    bool m_focused : 1;
    bool m_active : 1;
    bool m_hovered : 1;
    bool m_inActiveChain : 1;
    bool m_styleElement : 1; // contains stylesheet text
    bool m_implicit : 1; // implicitely generated by the parser

    // 3 bits unused
};

class ContainerNodeImpl : public NodeImpl
{
public:
    ContainerNodeImpl(DocumentPtr *doc);
    virtual ~ContainerNodeImpl();

    // DOM methods overridden from  parent classes
    virtual NodeImpl *firstChild() const;
    virtual NodeImpl *lastChild() const;
    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual bool hasChildNodes (  ) const;

    // Other methods (not part of DOM)
    void willRemove();
    int willRemoveChild(NodeImpl *child);
    void removeChildren();
    void cloneChildNodes(NodeImpl *clone);

    virtual void setFirstChild(NodeImpl *child);
    virtual void setLastChild(NodeImpl *child);
    virtual NodeImpl *addChild(NodeImpl *newChild);
    virtual void attach();
    virtual void detach();

    virtual QRect getRect() const;
    bool getUpperLeftCorner(int &xPos, int &yPos) const;
    bool getLowerRightCorner(int &xPos, int &yPos) const;

    virtual void setFocus(bool=true);
    virtual void setActive(bool active = true, bool pause = false);
    virtual void setHovered(bool=true);
    virtual unsigned childNodeCount() const;
    virtual NodeImpl *childNode(unsigned index);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);

//protected:
    NodeImpl *_first;
    NodeImpl *_last;

    // helper functions for inserting children:

    // ### this should vanish. do it in dom/ !
    // check for same source document:
    bool checkSameDocument( NodeImpl *newchild, int &exceptioncode );
    // check for being child:
    bool checkIsChild( NodeImpl *oldchild, int &exceptioncode );
    // ###

    // find out if a node is allowed to be our child
    void dispatchChildInsertedEvents( NodeImpl *child, int &exceptioncode );
    void dispatchChildRemovalEvents( NodeImpl *child, int &exceptioncode );
};

// --------------------------------------------------------------------------
class Node;
class NodeImpl;

class NodeListImpl : public khtml::Shared<NodeListImpl>
{
public:
    NodeListImpl( NodeImpl *_rootNode );
    virtual ~NodeListImpl();

    // DOM methods & attributes for NodeList
    virtual unsigned length() const = 0;
    virtual NodeImpl *item ( unsigned index ) const = 0;
    virtual NodeImpl *itemById ( const DOMString & elementId ) const;

    // Other methods (not part of DOM)

    virtual void rootNodeChildrenChanged();
    virtual void rootNodeAttributeChanged() {}

protected:
    // helper functions for searching all ElementImpls in a tree
    unsigned recursiveLength(NodeImpl *start = 0) const;
    NodeImpl *recursiveItem ( unsigned offset, NodeImpl *start = 0 ) const;
    virtual bool nodeMatches( NodeImpl *testNode ) const = 0;

    NodeImpl *rootNode;
    mutable int cachedLength;
    mutable NodeImpl *lastItem;
    mutable unsigned lastItemOffset;
    mutable bool isLengthCacheValid : 1;
    mutable bool isItemCacheValid : 1;
};

class ChildNodeListImpl : public NodeListImpl
{
public:
    ChildNodeListImpl( NodeImpl *n);

    // DOM methods overridden from  parent classes

    virtual unsigned length() const;
    virtual NodeImpl *item ( unsigned index ) const;

protected:
    virtual bool nodeMatches( NodeImpl *testNode ) const;
};

/**
 * NodeList which lists all Nodes in a Element with a given "name=" tag
 */
class NameNodeListImpl : public NodeListImpl
{
public:
    NameNodeListImpl( NodeImpl *doc, const DOMString &t );

    // DOM methods overridden from  parent classes

    virtual unsigned length() const;
    virtual NodeImpl *item ( unsigned index ) const;

    // Other methods (not part of DOM)
    virtual void rootNodeChildrenChanged() {};
    virtual void rootNodeAttributeChanged() { NodeListImpl::rootNodeChildrenChanged(); }

protected:
    virtual bool nodeMatches( NodeImpl *testNode ) const;

    DOMString nodeName;
};


// Generic NamedNodeMap interface
// Other classes implement this for more specific situations e.g. attributes
// of an element
class NamedNodeMapImpl : public khtml::Shared<NamedNodeMapImpl>
{
public:
    NamedNodeMapImpl() {}
    virtual ~NamedNodeMapImpl() {}

    NodeImpl *getNamedItem(const DOMString &name) const { return getNamedItemNS(DOMString(), name); }
    SharedPtr<NodeImpl> removeNamedItem(const DOMString &name, int &exception) { return removeNamedItemNS(DOMString(), name, exception); }

    virtual NodeImpl *getNamedItemNS(const DOMString &namespaceURI, const DOMString &localName) const = 0;
    SharedPtr<NodeImpl> setNamedItemNS(NodeImpl *arg, int &exception) { return setNamedItem(arg, exception); }
    virtual SharedPtr<NodeImpl> removeNamedItemNS(const DOMString &namespaceURI, const DOMString &localName, int &exception) = 0;

    // DOM methods & attributes for NamedNodeMap
    virtual NodeImpl *getNamedItem(const QualifiedName& attrName) const = 0;
    virtual SharedPtr<NodeImpl> removeNamedItem (const QualifiedName& attrName, int &exceptioncode) = 0;
    virtual SharedPtr<NodeImpl> setNamedItem (NodeImpl* arg, int &exceptioncode) = 0;

    virtual NodeImpl *item ( unsigned index ) const = 0;
    virtual unsigned length(  ) const = 0;

    // Other methods (not part of DOM)
    virtual bool isReadOnly() { return false; }
};

}; //namespace
#endif
