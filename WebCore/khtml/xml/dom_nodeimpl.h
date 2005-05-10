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
#include "misc/main_thread_malloc.h"
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

using khtml::SharedPtr;

class DocumentImpl;
class ElementImpl;
class EventImpl;
class EventListener;
class NodeListImpl;
class NamedAttrMapImpl;
class RegisteredEventListener;

// The namespace used for XHTML elements
#define XHTML_NAMESPACE "http://www.w3.org/1999/xhtml"

const Q_UINT16 noNamespace = 0;
const Q_UINT16 anyNamespace = 1;
const Q_UINT16 xhtmlNamespace = 2;
const Q_UINT16 anyLocalName = 0;

const Q_UINT32 namespaceMask = 0xFFFF0000U;
const Q_UINT32 localNameMask = 0x0000FFFFU;

inline Q_UINT16 namespacePart(Q_UINT32 i) { return i >> 16; }
inline Q_UINT16 localNamePart(Q_UINT32 i) { return i; }
inline Q_UINT32 makeId(Q_UINT16 n, Q_UINT16 l) { return (n << 16) | l; }

// Can't use makeId here because it results in an "initroutine".
const Q_UINT32 anyQName = anyNamespace << 16 | anyLocalName;

class DocumentPtr : public khtml::Shared<DocumentPtr>
{
public:
    DocumentImpl *document() const { return doc; }
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

    MAIN_THREAD_ALLOCATED;

    // DOM methods & attributes for Node
    virtual DOMString nodeName() const = 0;
    virtual DOMString nodeValue() const;
    virtual void setNodeValue( const DOMString &_nodeValue, int &exceptioncode );
    virtual unsigned short nodeType() const = 0;
    NodeImpl *parentNode() const { return m_parent; }
    NodeImpl *previousSibling() const { return m_previous; }
    NodeImpl *nextSibling() const { return m_next; }
    virtual NodeListImpl *childNodes();
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
    virtual DOMString localName() const;
    virtual DOMString namespaceURI() const;
    virtual DOMString prefix() const;
    virtual void setPrefix(const DOMString &_prefix, int &exceptioncode );
    void normalize ();
    static bool isSupported(const DOMString &feature, const DOMString &version);

    NodeImpl *lastDescendent() const;

    // Other methods (not part of DOM)
    virtual bool isElementNode() const { return false; }
    virtual bool isHTMLElement() const { return false; }
    virtual bool isAttributeNode() const { return false; }
    virtual bool isTextNode() const { return false; }
    virtual bool isDocumentNode() const { return false; }
    virtual bool isXMLElementNode() const { return false; }
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
    
    // used by the parser. Doesn't do as many error checkings as
    // appendChild(), and returns the node into which will be parsed next.
    virtual NodeImpl *addChild(NodeImpl *newChild);
    
    // called by the parser when this element's close tag is reached,
    // signalling that all child tags have been parsed and added.
    // This is only needed for <applet> and <object> elements, which can't lay themselves out
    // until they know all of their nested <param>s. [3603191, 4040848]
    virtual void closeRenderer() { }

    typedef Q_UINT32 Id;
    // id() is used to easily and exactly identify a node. It
    // is optimized for quick comparison and low memory consumption.
    // its value depends on the owner document of the node and is
    // categorized in the following way:
    // 1..ID_LAST_TAG: the node inherits HTMLElementImpl and is
    //                 part of the HTML namespace.
    //                 The HTML namespace is either the global
    //                 one (no namespace) or the XHTML namespace
    //                 depending on the owner document's doctype
    // ID_LAST_TAG+1..0xffff: non-HTML elements in the global namespace
    // others       non-HTML elements in a namespace.
    //                 the upper 16 bit identify the namespace
    //                 the lower 16 bit identify the local part of the
    //                 qualified element name.
    virtual Id id() const { return 0; };
#if APPLE_CHANGES
    Id identifier() const;
#endif
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
    bool focused() const { return m_focused; }
    bool attached() const   { return m_attached; }
    bool changed() const    { return m_changed; }
    bool hasChangedChild() const { return m_hasChangedChild; }
    bool isLink() const { return m_isLink; }
    bool inDocument() const { return m_inDocument; }
    bool styleElement() const { return m_styleElement; }
    bool implicitNode() const { return m_implicit; }
    void setHasID(bool b=true) { m_hasId = b; }
    void setHasClass(bool b=true) { m_hasClass = b; }
    void setHasStyle(bool b=true) { m_hasStyle = b; }
    void setHasChangedChild( bool b = true ) { m_hasChangedChild = b; }
    void setInDocument(bool b=true) { m_inDocument = b; }
    virtual void setFocus(bool b=true) { m_focused = b; }
    virtual void setActive(bool b=true) { m_active = b; }
    void setChanged(bool b=true);

    unsigned short tabIndex() const { return m_tabIndex; }
    void setTabIndex(unsigned short _tabIndex) { m_tabIndex = _tabIndex; }

    /**
        * whether this node can receive the keyboard focus.
     */
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;
    
    virtual bool isInline() const;
    
    virtual bool isContentEditable() const;
    virtual QRect getRect() const;

    enum StyleChange { NoChange, NoInherit, Inherit, Detach, Force };
    virtual void recalcStyle( StyleChange = NoChange ) {}
    StyleChange diff( khtml::RenderStyle *s1, khtml::RenderStyle *s2 ) const;

    unsigned long nodeIndex() const;
    // Returns the document that this node is associated with. This is guaranteed to always be non-null, as opposed to
    // DOM's ownerDocument() which is null for Document nodes (and sometimes DocumentType nodes).
    DocumentImpl* getDocument() const { return document->document(); }

    void setDocument(DocumentPtr *doc);

    void addEventListener(const DOMString &, EventListener *listener, bool useCapture);
    void removeEventListener(const DOMString &, EventListener *listener, bool useCapture);

    void addEventListener(int id, EventListener *listener, bool useCapture);
    void removeEventListener(int id, EventListener *listener, bool useCapture);
    void removeHTMLEventListener(int id);
    void setHTMLEventListener(int id, EventListener *listener);
    EventListener *getHTMLEventListener(int id);
    void removeAllEventListeners();

    bool dispatchEvent(EventImpl *evt, int &exceptioncode, bool tempEvent = false);
    bool dispatchGenericEvent( EventImpl *evt, int &exceptioncode);
    bool dispatchHTMLEvent(int _id, bool canBubbleArg, bool cancelableArg);
    bool dispatchWindowEvent(int _id, bool canBubbleArg, bool cancelableArg);
    bool dispatchMouseEvent(QMouseEvent *e, int overrideId = 0, int overrideDetail = 0);
    bool dispatchUIEvent(int _id, int detail = 0);
    bool dispatchSubtreeModifiedEvent(bool childrenChanged = true);
    bool dispatchKeyEvent(QKeyEvent *key);
    void dispatchWheelEvent(QWheelEvent *);

    void handleLocalEvents(EventImpl *evt, bool useCapture);

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
    virtual unsigned long childNodeCount() const;
    virtual NodeImpl *childNode(unsigned long index);

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
    
    void checkSetPrefix(const DOMString &_prefix, int &exceptioncode);
    void checkAddChild(NodeImpl *newChild, int &exceptioncode);
    bool isAncestor(const NodeImpl *) const;
    virtual bool childAllowed( NodeImpl *newChild );

    virtual long maxOffset() const;
    long maxDeepOffset() const;
    virtual long caretMinOffset() const;
    virtual long caretMaxOffset() const;
    virtual unsigned long caretMaxRenderedOffset() const;

    virtual long previousOffset (long current) const;
    virtual long nextOffset (long current) const;
    
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

    /**
     * Notifies the node that it's list of children have changed (either by adding or removing child nodes), or a child
     * node that is of the type CDATA_SECTION_NODE, TEXT_NODE or COMMENT_NODE has changed its value.
     */
    virtual void childrenChanged();

    virtual DOMString toString() const = 0;
    
#ifndef NDEBUG
    virtual void formatForDebugger(char *buffer, unsigned length) const;

    void displayNode(const char *prefix="");
    void displayTree();
#endif

    void registerNodeList(NodeListImpl *list);
    void unregisterNodeList(NodeListImpl *list);
    void notifyNodeListsSubtreeModified();
    void notifyLocalNodeListsSubtreeModified();

    SharedPtr<NodeListImpl> getElementsByTagName(const DOMString &name) { return getElementsByTagNameNS(DOMString(), name); }
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
    virtual void setActive(bool=true);
    virtual unsigned long childNodeCount() const;
    virtual NodeImpl *childNode(unsigned long index);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
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

    MAIN_THREAD_ALLOCATED;

    // DOM methods & attributes for NodeList
    virtual unsigned long length() const = 0;
    virtual NodeImpl *item ( unsigned long index ) const = 0;
    virtual NodeImpl *itemById ( const DOMString & elementId ) const;

    // Other methods (not part of DOM)

    void rootNodeSubtreeModified();

protected:
    // helper functions for searching all ElementImpls in a tree
    unsigned long recursiveLength(NodeImpl *start = 0) const;
    NodeImpl *recursiveItem ( unsigned long offset, NodeImpl *start = 0 ) const;
    virtual bool nodeMatches( NodeImpl *testNode ) const = 0;

    NodeImpl *rootNode;
    mutable int cachedLength;
    mutable NodeImpl *lastItem;
    mutable unsigned long lastItemOffset;
    mutable bool isLengthCacheValid : 1;
    mutable bool isItemCacheValid : 1;
};

class ChildNodeListImpl : public NodeListImpl
{
public:
    ChildNodeListImpl( NodeImpl *n);

    // DOM methods overridden from  parent classes

    virtual unsigned long length() const;
    virtual NodeImpl *item ( unsigned long index ) const;

protected:
    virtual bool nodeMatches( NodeImpl *testNode ) const;
};


/**
 * NodeList which lists all Nodes in a document with a given tag name
 */
class TagNodeListImpl : public NodeListImpl
{
public:
    TagNodeListImpl( NodeImpl *n, NodeImpl::Id tagId, NodeImpl::Id tagIdMask );

    // DOM methods overridden from  parent classes

    virtual unsigned long length() const;
    virtual NodeImpl *item ( unsigned long index ) const;

    // Other methods (not part of DOM)

protected:
    virtual bool nodeMatches( NodeImpl *testNode ) const;

    NodeImpl::Id m_id;
    NodeImpl::Id m_idMask;
};


/**
 * NodeList which lists all Nodes in a Element with a given "name=" tag
 */
class NameNodeListImpl : public NodeListImpl
{
public:
    NameNodeListImpl( NodeImpl *doc, const DOMString &t );

    // DOM methods overridden from  parent classes

    virtual unsigned long length() const;
    virtual NodeImpl *item ( unsigned long index ) const;

    // Other methods (not part of DOM)

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
    NamedNodeMapImpl() { }
    virtual ~NamedNodeMapImpl();

    MAIN_THREAD_ALLOCATED;

    NodeImpl *getNamedItem(const DOMString &name) const { return getNamedItemNS(DOMString(), name); }
    SharedPtr<NodeImpl> removeNamedItem(const DOMString &name, int &exception) { return removeNamedItemNS(DOMString(), name, exception); }

    NodeImpl *getNamedItemNS(const DOMString &namespaceURI, const DOMString &localName) const;
    SharedPtr<NodeImpl> setNamedItemNS(NodeImpl *arg, int &exception) { return setNamedItem(arg, exception); }
    SharedPtr<NodeImpl> removeNamedItemNS(const DOMString &namespaceURI, const DOMString &localName, int &exception);

    // DOM methods & attributes for NamedNodeMap
    virtual NodeImpl *getNamedItem ( NodeImpl::Id id ) const = 0;
    virtual SharedPtr<NodeImpl> removeNamedItem ( NodeImpl::Id id, int &exceptioncode ) = 0;
    virtual SharedPtr<NodeImpl> setNamedItem ( NodeImpl* arg, int &exceptioncode ) = 0;

    virtual NodeImpl *item ( unsigned long index ) const = 0;
    virtual unsigned long length(  ) const = 0;

    // Other methods (not part of DOM)
    virtual NodeImpl::Id mapId(const DOMString& namespaceURI,  const DOMString& localName,  bool readonly) = 0;
    virtual bool isReadOnly() { return false; }
};


// ### fixme
#if 0
// Generic read-only NamedNodeMap implementation
// You can add items using the internal function addItem()
class GenericRONamedNodeMapImpl : public NamedNodeMapImpl
{
public:
    GenericRONamedNodeMapImpl(DocumentPtr* doc);
    virtual ~GenericRONamedNodeMapImpl();

    // DOM methods & attributes for NamedNodeMap

    virtual NodeImpl *getNamedItem ( const DOMString &name, int &exceptioncode ) const;
    virtual Node setNamedItem ( const Node &arg, int &exceptioncode );
    virtual Node removeNamedItem ( const DOMString &name, int &exceptioncode );
    virtual NodeImpl *item ( unsigned long index ) const;
    virtual unsigned long length(  ) const;
    virtual NodeImpl *getNamedItemNS( const DOMString &namespaceURI, const DOMString &localName,
                                      int &exceptioncode ) const;
    virtual NodeImpl *setNamedItemNS( NodeImpl *arg, int &exceptioncode );
    virtual NodeImpl *removeNamedItemNS( const DOMString &namespaceURI, const DOMString &localName,
                                         int &exceptioncode );

    // Other methods (not part of DOM)

    virtual bool isReadOnly() { return true; }

    void addNode(NodeImpl *n);

protected:
    DocumentImpl* m_doc;
    QPtrList<NodeImpl> *m_contents;
};
#endif

}; //namespace
#endif
