/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#include "dom_nodeimpl.h"

#include "dom_node.h"
#include "dom_exception.h"
#include "htmlattrs.h"

#include "dom_elementimpl.h"
#include "dom_docimpl.h"
#include "dom2_eventsimpl.h"

#include <kdebug.h>

#include "rendering/render_object.h"
#include "rendering/render_text.h"
#include <qrect.h>
#include <qevent.h>
#include <qnamespace.h>

using namespace DOM;
using namespace khtml;

NodeImpl::NodeImpl(DocumentPtr *doc)
    : document(doc),
      m_render(0),
      m_complexText( false ),
      m_hasEvents( false ),
      m_hasId( false ),
      m_hasClass( false ),
      m_hasStyle( false ),
      m_pressed( false ),
      m_mouseInside( false ),
      m_attached( false ),
      m_changed( false ),
      m_specified( false ),
      m_focused( false ),
      m_active( false ),
      m_regdListeners( 0 )
{
    document->ref();
}

NodeImpl::~NodeImpl()
{
    if (document->document())
        document->document()->changedNodes.remove(this);
    if (m_regdListeners)
        delete m_regdListeners;
    document->deref();
}

DOMString NodeImpl::nodeValue() const
{
  return DOMString();
}

void NodeImpl::setNodeValue( const DOMString &, int &exceptioncode )
{
  exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
}

const DOMString NodeImpl::nodeName() const
{
  return DOMString();
}

unsigned short NodeImpl::nodeType() const
{
  return 0;
}

NodeImpl *NodeImpl::parentNode() const
{
  return 0;
}

NodeListImpl *NodeImpl::childNodes()
{
  return new ChildNodeListImpl(this);
}

NodeImpl *NodeImpl::firstChild() const
{
  return 0;
}

NodeImpl *NodeImpl::lastChild() const
{
  return 0;
}

NodeImpl *NodeImpl::previousSibling() const
{
  return 0;
}

NodeImpl *NodeImpl::nextSibling() const
{
  return 0;
}

NamedNodeMapImpl *NodeImpl::attributes()
{
  return 0;
}

void NodeImpl::setPreviousSibling(NodeImpl *)
{
}

void NodeImpl::setNextSibling(NodeImpl *)
{
}

NodeImpl *NodeImpl::insertBefore( NodeImpl *, NodeImpl *, int &exceptioncode )
{
    exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
    return 0;
}

NodeImpl *NodeImpl::replaceChild( NodeImpl *, NodeImpl *, int &exceptioncode )
{
  exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
  return 0;
}

NodeImpl *NodeImpl::removeChild( NodeImpl *, int &exceptioncode )
{
  exceptioncode = DOMException::NOT_FOUND_ERR;
  return 0;
}

NodeImpl *NodeImpl::appendChild( NodeImpl *, int &exceptioncode )
{
  exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
  return 0;
}

bool NodeImpl::hasChildNodes(  ) const
{
  return false;
}

// helper functions not being part of the DOM
void NodeImpl::setParent(NodeImpl *)
{
}

void NodeImpl::setFirstChild(NodeImpl *)
{
}

void NodeImpl::setLastChild(NodeImpl *)
{
}

NodeImpl *NodeImpl::addChild(NodeImpl *)
{
  return 0;
}

QString NodeImpl::toHTML() const
{
    NodeImpl* fc = firstChild();
    if ( fc )
        return fc->recursive_toHTML(true);

    return "";
}

QString NodeImpl::recursive_toHTML(bool start) const
{
    QString me = "";

    // Copy who I am into the htmlText string
    if ( nodeType() == Node::TEXT_NODE )
        me = nodeValue().string();
    else
    {
        // If I am an element, not a text
        NodeImpl* temp = previousSibling();
        if(temp)
        {
            if( !start && (temp->nodeType() != Node::TEXT_NODE && nodeType() != Node::TEXT_NODE ) )
                me = QString("    ") + QChar('<') + nodeName().string();
            else
                me = QChar('<') + nodeName().string();
        }
        else
            me = QChar('<') + nodeName().string();
        // print attributes
        if( nodeType() == Node::ELEMENT_NODE )
        {
            ElementImpl *el = const_cast<ElementImpl*>(static_cast<const ElementImpl *>(this));
            AttrImpl *attr;
            int exceptioncode;
            if(el->namedAttrMap) {
                NamedNodeMapImpl *attrs = el->namedAttrMap;
            unsigned long lmap = attrs->length(exceptioncode);
            for( unsigned int j=0; j<lmap; j++ )
            {
                attr = static_cast<AttrImpl*>(attrs->item(j,exceptioncode));
                me += " " + attr->name().string() + "=\"" + attr->value().string() + "\"";
                }
            }
        }

        // print ending bracket of start tag
        if( firstChild() == 0 )     // if element has no endtag
                me += " />\n";
        else                        // if element has endtag
        {
                NodeImpl* temp = nextSibling();
                if(temp)
                {
                    if( (temp->nodeType() != Node::TEXT_NODE) )
                        me += ">\n";
                    else
                        me += ">";
                }
                else
                    me += ">";
        }
    }

    NodeImpl* n;

    if( (n = firstChild()) )
    {
        // print firstChild
        me += n->recursive_toHTML( );

        // Print my ending tag
        if ( nodeType() != Node::TEXT_NODE )
            me += "</" + nodeName().string() + ">\n";
    }
    // print next sibling
    if( (n = nextSibling()) )
        me += n->recursive_toHTML( );

    return me;
}

void NodeImpl::applyChanges(bool, bool)
{
    setChanged(false);
}

void NodeImpl::getCursor(int offset, int &_x, int &_y, int &height)
{
    if(m_render) m_render->cursorPos(offset, _x, _y, height);
    else _x = _y = height = -1;
}

QRect NodeImpl::getRect() const
{
    int _x, _y;
    if(m_render && m_render->absolutePosition(_x, _y))
        return QRect( _x, _y, m_render->width(), m_render->height() );
    return QRect();
}

void NodeImpl::setChanged(bool b)
{
    if (!attached()) // changed compared to what?
        return;

    if (b && !changed() && ownerDocument())
        ownerDocument()->changedNodes.append(this);
    else if (!b && changed() && ownerDocument())
        ownerDocument()->changedNodes.remove(this);
    m_changed = b;
}

DOMString NodeImpl::namespaceURI() const
{
    return DOMString();
}


void NodeImpl::printTree(int indent)
{
    QString ind;
    QString s;
    ind.fill(' ', indent);

    // ### find out why this isn't working
    if(isElementNode())
    {
        s = ind + "<" + nodeName().string();

        ElementImpl *el = const_cast<ElementImpl*>(static_cast<const ElementImpl *>(this));
        AttrImpl *attr;
        int exceptioncode;
        NamedNodeMapImpl *attrs = el->attributes();
        unsigned long lmap = attrs->length(exceptioncode);
        for( unsigned int j=0; j<lmap; j++ )
        {
            attr = static_cast<AttrImpl*>(attrs->item(j,exceptioncode));
            s += " " + attr->name().string() + "=\"" + attr->value().string() + "\"";
        }
        if(!firstChild())
            s += " />";
        else
            s += ">";
    }
    else
        s = ind + "'" + nodeValue().string() + "'";

    kdDebug() << s << endl;

    NodeImpl *child = firstChild();
    while( child )
    {
        child->printTree(indent+2);
        child = child->nextSibling();
    }
    if(isElementNode() && firstChild())
        kdDebug() << ind << "</" << nodeName().string() << ">" << endl;
}

unsigned long NodeImpl::nodeIndex() const
{
    return 0;
}

void NodeImpl::addEventListener(int id, EventListener *listener, const bool useCapture)
{
    switch (id) {
	case EventImpl::DOMSUBTREEMODIFIED_EVENT:
	    getDocument()->addListenerType(DocumentImpl::DOMSUBTREEMODIFIED_LISTENER);
	    break;
	case EventImpl::DOMNODEINSERTED_EVENT:
	    getDocument()->addListenerType(DocumentImpl::DOMNODEINSERTED_LISTENER);
	    break;
	case EventImpl::DOMNODEREMOVED_EVENT:
	    getDocument()->addListenerType(DocumentImpl::DOMNODEREMOVED_LISTENER);
	    break;
        case EventImpl::DOMNODEREMOVEDFROMDOCUMENT_EVENT:
	    getDocument()->addListenerType(DocumentImpl::DOMNODEREMOVEDFROMDOCUMENT_LISTENER);
	    break;
        case EventImpl::DOMNODEINSERTEDINTODOCUMENT_EVENT:
	    getDocument()->addListenerType(DocumentImpl::DOMNODEINSERTEDINTODOCUMENT_LISTENER);
	    break;
        case EventImpl::DOMATTRMODIFIED_EVENT:
	    getDocument()->addListenerType(DocumentImpl::DOMATTRMODIFIED_LISTENER);
	    break;
        case EventImpl::DOMCHARACTERDATAMODIFIED_EVENT:
	    getDocument()->addListenerType(DocumentImpl::DOMCHARACTERDATAMODIFIED_LISTENER);
	    break;
	default:
	    break;
    }

    RegisteredEventListener *rl = new RegisteredEventListener(static_cast<EventImpl::EventId>(id),listener,useCapture);
    if (!m_regdListeners) {
        m_regdListeners = new QList<RegisteredEventListener>;
	m_regdListeners->setAutoDelete(true);
    }

    // remove existing ones of the same type - ### is this correct (or do we ignore the new one?)
    removeEventListener(id,listener,useCapture);

    m_regdListeners->append(rl);
    listener->ref();
}

void NodeImpl::addEventListener(const DOMString &type, EventListener *listener,
                                  const bool useCapture, int &/*exceptioncode*/)
{
    addEventListener(EventImpl::typeToId(type),listener,useCapture);
}

void NodeImpl::removeEventListener(int id, EventListener *listener, bool useCapture)
{
    if (!m_regdListeners) // nothing to remove
        return;

    RegisteredEventListener rl(static_cast<EventImpl::EventId>(id),listener,useCapture);

    QListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (*(it.current()) == rl) {
            m_regdListeners->removeRef(it.current());
            listener->deref();
            return;
        }
}

void NodeImpl::removeEventListener(const DOMString &type, EventListener *listener,
                                     bool useCapture,int &exceptioncode)
{
    removeEventListener(EventImpl::typeToId(type),listener,useCapture,exceptioncode);
}

void NodeImpl::removeHTMLEventListener(int id)
{
    if (!m_regdListeners) // nothing to remove
        return;

    QListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (it.current()->id == id &&
            it.current()->listener->eventListenerType() == "_khtml_HTMLEventListener") {
            m_regdListeners->removeRef(it.current());
            return;
        }
}

void NodeImpl::setHTMLEventListener(int id, EventListener *listener)
{
    removeHTMLEventListener(id);
    if (listener)
	addEventListener(id,listener,false);
}

EventListener *NodeImpl::getHTMLEventListener(int id)
{
    if (!m_regdListeners) // nothing to remove
        return 0;

    QListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (it.current()->id == id &&
            it.current()->listener->eventListenerType() == "_khtml_HTMLEventListener") {
            return it.current()->listener;
        }
    return 0;
}


bool NodeImpl::dispatchEvent(EventImpl *evt, int &exceptioncode)
{
    evt->setTarget(this);
    return dispatchGenericEvent( evt, exceptioncode );
}

bool NodeImpl::dispatchGenericEvent( EventImpl *evt, int &/*exceptioncode */)
{
    // ### check that type specified

    // work out what nodes to send event to
    QList<NodeImpl> nodeChain;
    NodeImpl *n;
    for (n = this; n; n = n->parentNode()) {
        n->ref();
        nodeChain.prepend(n);
    }

    // trigger any capturing event handlers on our way down
    evt->setEventPhase(Event::CAPTURING_PHASE);
    QListIterator<NodeImpl> it(nodeChain);
    for (; it.current() && it.current() != this && !evt->propagationStopped(); ++it) {
        evt->setCurrentTarget(it.current());
        it.current()->handleLocalEvents(evt,true);
    }

    // dispatch to the actual target node
    it.toLast();
    if (!evt->propagationStopped()) {
        evt->setEventPhase(Event::AT_TARGET);
        evt->setCurrentTarget(it.current());
        it.current()->handleLocalEvents(evt,false);
    }
    --it;

    // ok, now bubble up again (only non-capturing event handlers will be called)
    // ### recalculate the node chain here? (e.g. if target node moved in document by previous event handlers)
    if (evt->bubbles()) {
        evt->setEventPhase(Event::BUBBLING_PHASE);
        for (; it.current() && !evt->propagationStopped(); --it) {
            evt->setCurrentTarget(it.current());
            it.current()->handleLocalEvents(evt,false);
        }
    }

    evt->setCurrentTarget(0);
    evt->setEventPhase(0); // I guess this is correct, the spec does not seem to say

    if (evt->bubbles()) {
        // now we call all default event handlers (this is not part of DOM - it is internal to khtml)
        it.toLast();
        for (; it.current() && !evt->propagationStopped() && !evt->defaultPrevented() && !evt->defaultHandled(); --it)
            it.current()->defaultEventHandler(evt);
    }

    // copy this over into a local variable, as the following deref() calls might cause this to be deleted.
    DocumentPtr *doc = document;
    doc->ref();

    // deref all nodes in chain
    it.toFirst();
    for (; it.current(); ++it)
        it.current()->deref(); // this may delete us

    if (doc->document())
        doc->document()->updateRendering();
    doc->deref();

    return !evt->defaultPrevented(); // ### what if defaultPrevented was called before dispatchEvent?
}

bool NodeImpl::dispatchHTMLEvent(int _id, bool canBubbleArg, bool cancelableArg)
{
    int exceptioncode;
    EventImpl *evt = new EventImpl(static_cast<EventImpl::EventId>(_id),canBubbleArg,cancelableArg);
    evt->ref();
    bool r = dispatchEvent(evt,exceptioncode);
    evt->deref();
    return r;
}

bool NodeImpl::dispatchWindowEvent(int _id, bool canBubbleArg, bool cancelableArg)
{
    int exceptioncode;
    EventImpl *evt = new EventImpl(static_cast<EventImpl::EventId>(_id),canBubbleArg,cancelableArg);
    evt->setTarget( 0 );
    evt->ref();
    DocumentPtr *doc = document;
    doc->ref();
    bool r = dispatchGenericEvent( evt, exceptioncode );
    if (!evt->defaultPrevented())
	doc->document()->defaultEventHandler(evt);
    doc->deref();
    evt->deref();
    return r;
}

bool NodeImpl::dispatchMouseEvent(QMouseEvent *_mouse, int overrideId, int overrideDetail)
{
    bool cancelable = true;
    int detail = overrideDetail; // defaults to 0
    EventImpl::EventId evtId = EventImpl::UNKNOWN_EVENT;
    if (overrideId) {
        evtId = static_cast<EventImpl::EventId>(overrideId);
    }
    else {
        switch (_mouse->type()) {
            case QEvent::MouseButtonPress:
                evtId = EventImpl::MOUSEDOWN_EVENT;
                break;
            case QEvent::MouseButtonRelease:
                evtId = EventImpl::MOUSEUP_EVENT;
                break;
            case QEvent::MouseButtonDblClick:
                evtId = EventImpl::CLICK_EVENT;
                detail = 1; // ### support for multiple double clicks
                break;
            case QEvent::MouseMove:
                evtId = EventImpl::MOUSEMOVE_EVENT;
                cancelable = false;
                break;
            default:
                break;
        }
    }
    if (evtId == EventImpl::UNKNOWN_EVENT)
        return false; // shouldn't happen


    int exceptioncode;

//    int clientX, clientY;
//    viewportToContents(_mouse->x(), _mouse->y(), clientX, clientY);
    int clientX = _mouse->x(); // ### adjust to be relative to view
    int clientY = _mouse->y(); // ### adjust to be relative to view

    int screenX = _mouse->globalX();
    int screenY = _mouse->globalY();

    int button = -1;
    switch (_mouse->button()) {
        case Qt::LeftButton:
            button = 0;
            break;
        case Qt::MidButton:
            button = 1;
            break;
        case Qt::RightButton:
            button = 2;
            break;
        default:
            break;
    }
    bool ctrlKey = (_mouse->state() & Qt::ControlButton);
    bool altKey = (_mouse->state() & Qt::AltButton);
    bool shiftKey = (_mouse->state() & Qt::ShiftButton);
    bool metaKey = false; // ### qt support?

    EventImpl *evt = new MouseEventImpl(evtId,true,cancelable,getDocument()->defaultView(),
                   detail,screenX,screenY,clientX,clientY,ctrlKey,altKey,shiftKey,metaKey,
                   button,0);
    evt->ref();
    bool r = dispatchEvent(evt,exceptioncode);
    evt->deref();
    return r;

}

bool NodeImpl::dispatchUIEvent(int _id, int detail)
{
    if (_id != EventImpl::DOMFOCUSIN_EVENT &&
        _id != EventImpl::DOMFOCUSOUT_EVENT &&
        _id != EventImpl::DOMACTIVATE_EVENT)
        return false; // shouldn't happen

    bool cancelable = false;
    if (_id == EventImpl::DOMACTIVATE_EVENT)
        cancelable = true;

    int exceptioncode;
    UIEventImpl *evt = new UIEventImpl(static_cast<EventImpl::EventId>(_id),true,
                                       cancelable,getDocument()->defaultView(),detail);
    evt->ref();
    bool r = dispatchEvent(evt,exceptioncode);
    evt->deref();
    return r;
}

bool NodeImpl::dispatchSubtreeModifiedEvent()
{
    if (!getDocument()->hasListenerType(DocumentImpl::DOMSUBTREEMODIFIED_LISTENER))
	return false;
    int exceptioncode;
    return dispatchEvent(new MutationEventImpl(EventImpl::DOMSUBTREEMODIFIED_EVENT,
			 true,false,0,0,0,0,0),exceptioncode);
}

void NodeImpl::handleLocalEvents(EventImpl *evt, bool useCapture)
{
    if (!m_regdListeners)
        return;

    QListIterator<RegisteredEventListener> it(*m_regdListeners);
    Event ev = evt;
    for (; it.current(); ++it) {
        if (it.current()->id == evt->id() && it.current()->useCapture == useCapture)
            it.current()->listener->handleEvent(ev);
    }

}

void NodeImpl::defaultEventHandler(EventImpl */*evt*/)
{
}

unsigned long NodeImpl::childNodeCount()
{
    return 0;
}

NodeImpl *NodeImpl::childNode(unsigned long /*index*/)
{
    return 0;
}

NodeImpl *NodeImpl::traverseNextNode(NodeImpl *stayWithin) {
    if (firstChild())
	return firstChild();
    else if (nextSibling())
	return nextSibling();
    else {
	NodeImpl *n = this;
	while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
	    n = n->parentNode();
	if (n && (!stayWithin || n->parentNode() != stayWithin))
	    return n->nextSibling();
    }
    return 0;
}

RenderObject *NodeImpl::nextRenderer()
{
    return 0;
}

//--------------------------------------------------------------------

NodeWParentImpl::NodeWParentImpl(DocumentPtr *doc) : NodeImpl(doc)
{
    _parent = 0;
    _previous = 0;
    _next = 0;
}

NodeWParentImpl::~NodeWParentImpl()
{
    // previous and next node may still reference this!!!
    // hope this fix is fine...
    if(_previous) _previous->setNextSibling(0);
    if(_next) _next->setPreviousSibling(0);
}

NodeImpl *NodeWParentImpl::parentNode() const
{
    return _parent;
}

NodeImpl *NodeWParentImpl::previousSibling() const
{
    return _previous;
}

NodeImpl *NodeWParentImpl::nextSibling() const
{
    return _next;
}

// not part of the DOM
void NodeWParentImpl::setParent(NodeImpl *n)
{
    _parent = n;
}

bool NodeWParentImpl::deleteMe()
{
    if(!_parent && _ref <= 0) return true;
    return false;
}

void NodeWParentImpl::setPreviousSibling(NodeImpl *n)
{
    _previous = n;
}

void NodeWParentImpl::setNextSibling(NodeImpl *n)
{
    _next = n;
}

bool NodeWParentImpl::checkReadOnly() const
{
    // ####
    return false;
}

unsigned long NodeWParentImpl::nodeIndex() const
{
    NodeImpl *_tempNode = _previous;
    unsigned long count=0;
    for( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
}

bool NodeWParentImpl::isReadOnly()
{
    // Entity & Entity Reference nodes and their descendants are read-only
    NodeImpl *n = this;
    while (n) {
	if (n->nodeType() == Node::ENTITY_NODE ||
	    n->nodeType() == Node::ENTITY_REFERENCE_NODE)
	    return true;
	n = n->parentNode();
    }
    return NodeImpl::isReadOnly();
}

RenderObject * NodeWParentImpl::nextRenderer()
{
    NodeImpl *n = _next;
    for (; n; n = n->nextSibling()) {
	if (n->renderer())
	    return n->renderer();
    }
    return 0;
}

//-------------------------------------------------------------------------

NodeBaseImpl::NodeBaseImpl(DocumentPtr *doc) : NodeWParentImpl(doc)
{
    _first = _last = 0;
    m_style = 0;
}


NodeBaseImpl::~NodeBaseImpl()
{
    //kdDebug( 6020 ) << "NodeBaseImpl destructor" << endl;
    // we have to tell all children, that the parent has died...
    NodeImpl *n;
    NodeImpl *next;

    for( n = _first; n != 0; n = next )
    {
        next = n->nextSibling();
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        if(n->deleteMe())
            delete n;
    }
    if (m_style)
        m_style->deref();
}


NodeImpl *NodeBaseImpl::firstChild() const
{
    return _first;
}

NodeImpl *NodeBaseImpl::lastChild() const
{
    return _last;
}

NodeImpl *NodeBaseImpl::insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode )
{
    exceptioncode = 0;
    if (checkReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }
    if (!newChild || (newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE && !newChild->firstChild())) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    if(!refChild)
        return appendChild(newChild, exceptioncode);

    if (newChild == refChild) // ### HIERARCHY_REUEST_ERR ?
	return 0;

    if( checkSameDocument(newChild, exceptioncode) )
        return 0;
    if( checkNoOwner(newChild, exceptioncode) )
        return 0;
    if( checkIsChild(refChild, exceptioncode) )
        return 0;

    if(newChild->parentNode() == this)
        removeChild(newChild, exceptioncode);
    if( exceptioncode )
        return 0;

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;
    NodeImpl *nextChild;
    NodeImpl *child = isFragment ? newChild->firstChild() : newChild;

    NodeImpl *prev = refChild->previousSibling();
    while (child) {
        nextChild = isFragment ? child->nextSibling() : 0;

        if( checkNoOwner(child, exceptioncode) )
            return 0;
        if(!childAllowed(child)) {
            exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
            return 0;
        }
        // if already in the tree, remove it first!
        NodeImpl *newParent = child->parentNode();
        if(newParent)
            newParent->removeChild( child, exceptioncode );
        if ( exceptioncode )
            return 0;

        // seems ok, lets's insert it.
        if (prev)
            prev->setNextSibling(child);
        else
            _first = child;
        refChild->setPreviousSibling(child);
        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(refChild);
        if (attached() && !child->attached() && ownerDocument() )
            child->attach();

        dispatchChildInsertedEvents(child,exceptioncode);

        prev = child;
        child = nextChild;
    }

    // ### set style in case it's attached
    setChanged(true);
    dispatchSubtreeModifiedEvent();
    return newChild;
}

NodeImpl *NodeBaseImpl::replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode )
{
    exceptioncode = 0;
    if (checkReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }
    if (!newChild || (newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE && !newChild->firstChild())) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }
    if( checkSameDocument(newChild, exceptioncode) )
        return 0;
    if( checkIsChild(oldChild, exceptioncode) )
        return 0;
    if( checkNoOwner(newChild, exceptioncode) )
        return 0;

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;
    NodeImpl *nextChild;
    NodeImpl *child = isFragment ? newChild->firstChild() : newChild;

    // make sure we will be able to insert the first node before we go removing the old one
    if( checkNoOwner(isFragment ? newChild->firstChild() : newChild, exceptioncode) )
        return 0;
    if(!childAllowed(isFragment ? newChild->firstChild() : newChild)) {
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return 0;
    }

    NodeImpl *prev = oldChild->previousSibling();
    NodeImpl *next = oldChild->nextSibling();
    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);
    if(oldChild->attached())
	oldChild->detach();

    while (child) {
        nextChild = isFragment ? child->nextSibling() : 0;

        if( checkNoOwner(child, exceptioncode ) )
            return 0;
        if(!childAllowed(child)) {
            exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
            return 0;
        }

        // if already in the tree, remove it first!
        NodeImpl *newParent = child->parentNode();
        if(newParent)
            newParent->removeChild( child, exceptioncode );
        if ( exceptioncode )
            return 0;

        // seems ok, lets's insert it.
        if (prev) prev->setNextSibling(child);
        if (next) next->setPreviousSibling(child);
        if(!prev) _first = child;
        if(!next) _last = child;

        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(next);
        if (attached() && !child->attached() && ownerDocument() )
            child->attach();

        dispatchChildInsertedEvents(child,exceptioncode);

        prev = child;
        child = nextChild;
    }

    // ### set style in case it's attached
    setChanged(true);
    dispatchSubtreeModifiedEvent();
    return oldChild;
}

NodeImpl *NodeBaseImpl::removeChild ( NodeImpl *oldChild, int &exceptioncode )
{
    exceptioncode = 0;
    if( checkReadOnly() )
        return 0;
    if( checkIsChild(oldChild, exceptioncode) )
        return 0;

    getDocument()->notifyBeforeNodeRemoval(oldChild); // ### use events instead
    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVED_LISTENER)) {
	oldChild->dispatchEvent(new MutationEventImpl(EventImpl::DOMNODEREMOVED_EVENT,
			     true,false,this,0,0,0,0),exceptioncode);
	if (exceptioncode)
	    return 0;
    }

    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
	// dispatch the DOMNOdeRemovedFromDocument event to all descendants
	NodeImpl *p = this;
	while (p->parentNode())
	    p = p->parentNode();
	if (p->nodeType() == Node::DOCUMENT_NODE) {
	    NodeImpl *c;
	    for (c = oldChild; c; c = c->traverseNextNode(oldChild)) {
		c->dispatchEvent(new MutationEventImpl(EventImpl::DOMNODEREMOVEDFROMDOCUMENT_EVENT,
				 false,false,0,0,0,0,0),exceptioncode);
		if (exceptioncode)
		    return 0;
	    }
	}
    }

    NodeImpl *prev, *next;
    prev = oldChild->previousSibling();
    next = oldChild->nextSibling();

    if(next) next->setPreviousSibling(prev);
    if(prev) prev->setNextSibling(next);
    if(_first == oldChild) _first = next;
    if(_last == oldChild) _last = prev;

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);
    if (oldChild->attached())
        oldChild->detach();

    setChanged(true);
    dispatchSubtreeModifiedEvent();
    return oldChild;
}

void NodeBaseImpl::removeChildren()
{
    NodeImpl *n, *next;
    for( n = _first; n != 0; n = next )
    {
        next = n->nextSibling();
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        if (n->attached())
	    n->detach();
        n->setRenderer( 0 );
        n->setStyle( 0 );
        if(n->deleteMe())
            delete n;
    }
    _first = _last = 0;

}


NodeImpl *NodeBaseImpl::appendChild ( NodeImpl *newChild, int &exceptioncode )
{
//    kdDebug(6010) << "NodeBaseImpl::appendChild( " << newChild << " );" <<endl;
    checkReadOnly();
    if (!newChild || (newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE && !newChild->firstChild())) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }
    if( checkSameDocument(newChild, exceptioncode) )
        return 0;
    if( checkNoOwner(newChild, exceptioncode) )
        return 0;

    if(newChild->parentNode() == this)
        removeChild(newChild, exceptioncode);
    if ( exceptioncode )
        return 0;

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;
    NodeImpl *nextChild;
    NodeImpl *child = isFragment ? newChild->firstChild() : newChild;

    while (child) {
        nextChild = isFragment ? child->nextSibling() : 0;

        if (checkNoOwner(child, exceptioncode) )
            return 0;
        if(!childAllowed(child)) {
            exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
            return 0;
        }

        // if already in the tree, remove it first!
        NodeImpl *oldParent = child->parentNode();
        if(oldParent)
            oldParent->removeChild( child, exceptioncode );
        if ( exceptioncode )
            return 0;

        // lets append it
        child->setParent(this);

        if(_last)
        {
            child->setPreviousSibling(_last);
            _last->setNextSibling(child);
            _last = child;
        }
        else
        {
            _first = _last = child;
        }
        if (attached() && !child->attached() && ownerDocument() )
            child->attach();

        dispatchChildInsertedEvents(child,exceptioncode);

        child = nextChild;
    }

    setChanged(true);
    // ### set style in case it's attached
    dispatchSubtreeModifiedEvent();
    return newChild;
}

bool NodeBaseImpl::hasChildNodes (  ) const
{
    return _first != 0;
}

// not part of the DOM
void NodeBaseImpl::setFirstChild(NodeImpl *child)
{
    _first = child;
}

void NodeBaseImpl::setLastChild(NodeImpl *child)
{
    _last = child;
}

// check for same source document:
bool NodeBaseImpl::checkSameDocument( NodeImpl *newChild, int &exceptioncode )
{
    exceptioncode = 0;
    DocumentImpl *ownerDocThis = static_cast<DocumentImpl*>(nodeType() == Node::DOCUMENT_NODE ? this : ownerDocument());
    DocumentImpl *ownerDocNew = static_cast<DocumentImpl*>(newChild->nodeType() == Node::DOCUMENT_NODE ? newChild : newChild->ownerDocument());
    if(ownerDocThis != ownerDocNew) {
        kdDebug(6010)<< "not same document, newChild = " << newChild << "document = " << ownerDocument() << endl;
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return true;
    }
    return false;
}

// check for being (grand-..)father:
bool NodeBaseImpl::checkNoOwner( NodeImpl *newChild, int &exceptioncode )
{
  //check if newChild is parent of this...
  NodeImpl *n;
  for( n = this; n != (NodeImpl *)ownerDocument() && n!= 0; n = n->parentNode() )
      if(n == newChild) {
          exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
          return true;
      }
  return false;
}

// check for being child:
bool NodeBaseImpl::checkIsChild( NodeImpl *oldChild, int &exceptioncode )
{
    if(!oldChild || oldChild->parentNode() != this) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return true;
    }
    return false;
}

bool NodeBaseImpl::childAllowed( NodeImpl *newChild )
{
    return childTypeAllowed(newChild->nodeType());
}

NodeImpl *NodeBaseImpl::addChild(NodeImpl *newChild)
{
    // do not add applyChanges here! This function is only used during parsing

    // short check for consistency with DTD
    if(!isXMLElementNode() && !newChild->isXMLElementNode() && !childAllowed(newChild))
    {
        //kdDebug( 6020 ) << "AddChild failed! id=" << id() << ", child->id=" << newChild->id() << endl;
        return 0;
    }

    // just add it...
    newChild->setParent(this);

    if(_last)
    {
        newChild->setPreviousSibling(_last);
        _last->setNextSibling(newChild);
        _last = newChild;
    }
    else
    {
        _first = _last = newChild;
    }
    if(newChild->nodeType() == Node::ELEMENT_NODE)
        return newChild;
    return this;
}

void NodeBaseImpl::applyChanges(bool top, bool force)
{

    setChanged(false);

    if (!attached())
	    return;

    int ow = (m_style?m_style->outlineWidth():0);

    if (top)
        recalcStyle();

    // a style change can influence the children, so we just go
    // through them and trigger an appplyChanges there too
    NodeImpl *n = _first;
    while(n) {
        n->applyChanges(false,force || changed());
        n = n->nextSibling();
    }

    if ( !m_render )
        return;
    
    m_render->calcMinMaxWidth();    

    if ( top ) {
        if ( force ) {
            // force a relayout of this part of the document
            m_render->updateSize();
            // force a repaint of this part.
            // ### if updateSize() changes any size, it will already force a
            // repaint, so we might do double work here...
            m_render->repaint();
        }
        else {
            // ### FIX ME
            if (m_style) ow = QMAX(ow, m_style->outlineWidth());
            RenderObject *cb = m_render->containingBlock();
            if (cb && cb != m_render)
                cb->repaintRectangle(-ow, -ow, cb->width()+2*ow, cb->height()+2*ow);
            else
                m_render->repaint();
        }
    }

    setChanged(false);
}

void NodeBaseImpl::attach()
{
    NodeImpl *child = _first;
    while(child != 0)
    {
        child->attach();
        child = child->nextSibling();
    }
    NodeWParentImpl::attach();
}

void NodeBaseImpl::detach()
{
    NodeImpl *child = _first;
    while(child != 0)
    {
        NodeImpl* prev = child;
        child = child->nextSibling();
        prev->detach();
    }
    NodeWParentImpl::detach();
}

void NodeBaseImpl::cloneChildNodes(NodeImpl *clone, int &exceptioncode)
{
    NodeImpl *n;
//    for(n = firstChild(); n != lastChild() && !exceptioncode; n = n->nextSibling())
    for(n = firstChild(); n && !exceptioncode; n = n->nextSibling())
    {
        clone->appendChild(n->cloneNode(true,exceptioncode),exceptioncode);
    }
}

// I don't like this way of implementing the method, but I didn't find any
// other way. Lars
bool NodeBaseImpl::getUpperLeftCorner(int &xPos, int &yPos) const
{
    if (!m_render)
        return false;
    RenderObject *o = m_render;
    xPos = yPos = 0;
    if ( !isInline() )
    {
        o->absolutePosition( xPos, yPos );
        return true;
    }

    // find the next text/image child, to get a position
    while(o) {
        if(o->firstChild())
            o = o->firstChild();
        else if(o->nextSibling())
            o = o->nextSibling();
        else {
            RenderObject *next = 0;
            while(!next) {
                o = o->parent();
                if(!o) return false;
                next = o->nextSibling();
            }
            o = next;
        }
        if((o->isText() && !o->isBR()) || o->isReplaced()) {
            o->container()->absolutePosition( xPos, yPos );
            if (o->isText())
                xPos += static_cast<RenderText *>(o)->minXPos();
            else
                xPos += o->xPos();
            yPos += o->yPos();
            return true;
        }
    }
    return true;
}

bool NodeBaseImpl::getLowerRightCorner(int &xPos, int &yPos) const
{
    if (!m_render)
        return false;

    RenderObject *o = m_render;
    xPos = yPos = 0;
    if (!isInline())
    {
        o->absolutePosition( xPos, yPos );
        xPos += o->width();
        yPos += o->height();
        return true;
    }
    // find the last text/image child, to get a position
    while(o) {
        if(o->lastChild())
            o = o->lastChild();
        else if(o->previousSibling())
            o = o->previousSibling();
        else {
            RenderObject *prev = 0;
            while(!prev) {
                o = o->parent();
                if(!o) return false;
                prev = o->previousSibling();
            }
            o = prev;
        }
        if(o->isText() || o->isReplaced()) {
            o->container()->absolutePosition(xPos, yPos);
            if (o->isText())
                xPos += static_cast<RenderText *>(o)->minXPos() + o->width();
            else
                xPos += o->xPos()+o->width();
            yPos += o->yPos()+o->height();
            return true;
        }
    }
    return true;
}

QRect NodeBaseImpl::getRect() const
{
    int xPos, yPos;
    if (!getUpperLeftCorner(xPos,yPos))
    {
        xPos=0;
        yPos=0;
    }
    int xEnd, yEnd;
    if (!getLowerRightCorner(xEnd,yEnd))
    {
        if (xPos)
            xEnd = xPos;
        if (yPos)
            yEnd = yPos;
    }
    else
    {
        if (xPos==0)
            xPos = xEnd;
        if (yPos==0)
            yPos = yEnd;
    }
    return QRect(xPos, yPos, xEnd - xPos, yEnd - yPos);
}

void NodeBaseImpl::setStyle(khtml::RenderStyle *style)
{
    RenderStyle *oldStyle = m_style;
    m_style = style;
    if (m_style)
        m_style->ref();
    if (oldStyle)
        oldStyle->deref();
}

void NodeBaseImpl::setFocus(bool received)
{
    NodeImpl::setFocus(received);
    for(NodeImpl *it=_first;it;it=it->nextSibling())
        it->setFocus(received);
}

void NodeBaseImpl::setActive(bool down)
{
    NodeImpl::setActive(down);
    for(NodeImpl *it=_first;it;it=it->nextSibling())
        it->setActive(down);
}

unsigned long NodeBaseImpl::childNodeCount()
{
    unsigned long count = 0;
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        count++;
    return count;
}

NodeImpl *NodeBaseImpl::childNode(unsigned long index)
{
    unsigned long i;
    NodeImpl *n = firstChild();
    for (i = 0; i < index; i++)
        n = n->nextSibling();
    return n;
}

void NodeBaseImpl::dispatchChildInsertedEvents( NodeImpl *child, int &exceptioncode )
{
    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEINSERTED_LISTENER)) {
	child->dispatchEvent(new MutationEventImpl(EventImpl::DOMNODEINSERTED_EVENT,
			     true,false,this,0,0,0,0),exceptioncode);
	if (exceptioncode)
	    return;
    }

    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
	// dispatch the DOMNOdeInsertedInfoDocument event to all descendants
	NodeImpl *p = this;
	while (p->parentNode())
	    p = p->parentNode();
	if (p->nodeType() == Node::DOCUMENT_NODE) {
	    NodeImpl *c;
	    for (c = child; c; c = c->traverseNextNode(child)) {
		c->dispatchEvent(new MutationEventImpl(EventImpl::DOMNODEINSERTEDINTODOCUMENT_EVENT,
				 false,false,0,0,0,0,0),exceptioncode);
		if (exceptioncode)
		    return;
	    }
	}
    }
}

// ---------------------------------------------------------------------------

NodeImpl *NodeListImpl::item( unsigned long /*index*/ ) const
{
    return 0;
}

unsigned long NodeListImpl::length() const
{
    return 0;
}

unsigned long NodeListImpl::recursiveLength(NodeImpl *start) const
{
    unsigned long len = 0;

    for(NodeImpl *n = start->firstChild(); n != 0; n = n->nextSibling()) {
        if ( n->nodeType() == Node::ELEMENT_NODE ) {
            if (nodeMatches(n))
                len++;
            len+= recursiveLength(n);
        }
    }

    return len;
}

NodeImpl *NodeListImpl::recursiveItem ( NodeImpl *start, unsigned long &offset ) const
{
    for(NodeImpl *n = start->firstChild(); n != 0; n = n->nextSibling()) {
        if ( n->nodeType() == Node::ELEMENT_NODE ) {
            if (nodeMatches(n))
                if (!offset--)
                    return n;

            NodeImpl *depthSearch= recursiveItem(n, offset);
            if (depthSearch)
                return depthSearch;
        }
    }

    return 0; // no matching node in this subtree
}

bool NodeListImpl::nodeMatches( NodeImpl */*testNode*/ ) const
{
  // ###
    return false;
}


ChildNodeListImpl::ChildNodeListImpl( NodeImpl *n )
{
    refNode = n;
    refNode->ref();
}

ChildNodeListImpl::~ChildNodeListImpl()
{
    refNode->deref();
}

unsigned long ChildNodeListImpl::length() const
{
    unsigned long len = 0;
    NodeImpl *n;
    for(n = refNode->firstChild(); n != 0; n = n->nextSibling())
        len++;

    return len;
}

NodeImpl *ChildNodeListImpl::item ( unsigned long index ) const
{
    unsigned int pos = 0;
    NodeImpl *n = refNode->firstChild();

    while( n != 0 && pos < index )
    {
        n = n->nextSibling();
        pos++;
    }

    return n;
}



TagNodeListImpl::TagNodeListImpl(NodeImpl *n, const DOMString &t )
  : tagName(t)
{
    refNode = n;
    refNode->ref();
    allElements = (t == "*");
}

TagNodeListImpl::~TagNodeListImpl()
{
    refNode->deref();
}

unsigned long TagNodeListImpl::length() const
{
    return recursiveLength( refNode );
}

NodeImpl *TagNodeListImpl::item ( unsigned long index ) const
{
    return recursiveItem( refNode, index );
}

bool TagNodeListImpl::nodeMatches( NodeImpl *testNode ) const
{
    return ((allElements && testNode->nodeType() == Node::ELEMENT_NODE) ||
            !strcasecmp(testNode->nodeName(),tagName));
}



NameNodeListImpl::NameNodeListImpl(NodeImpl *n, const DOMString &t )
  : nodeName(t)
{
    refNode= n;
    refNode->ref();
}

NameNodeListImpl::~NameNodeListImpl()
{
    refNode->deref();
}

unsigned long NameNodeListImpl::length() const
{
    return recursiveLength( refNode );
}

NodeImpl *NameNodeListImpl::item ( unsigned long index ) const
{
    return recursiveItem( refNode, index );
}

bool NameNodeListImpl::nodeMatches( NodeImpl *testNode ) const
{
    return static_cast<ElementImpl *>(testNode)->getAttribute(ATTR_NAME) == nodeName;
}

// ---------------------------------------------------------------------------

NamedNodeMapImpl::NamedNodeMapImpl()
{
}

NamedNodeMapImpl::~NamedNodeMapImpl()
{
}

// ----------------------------------------------------------------------------

GenericRONamedNodeMapImpl::GenericRONamedNodeMapImpl() : NamedNodeMapImpl()
{
    // not sure why this doesn't work as a normal object
    m_contents = new QList<NodeImpl>;
}

GenericRONamedNodeMapImpl::~GenericRONamedNodeMapImpl()
{
    while (m_contents->count() > 0)
        m_contents->take(0)->deref();

    delete m_contents;
}

unsigned long GenericRONamedNodeMapImpl::length(int &/*exceptioncode*/) const
{
    return m_contents->count();
}

NodeImpl *GenericRONamedNodeMapImpl::getNamedItem ( const DOMString &name, int &/*exceptioncode*/ ) const
{
    QListIterator<NodeImpl> it(*m_contents);
    for (; it.current(); ++it)
        if (it.current()->nodeName() == name)
            return it.current();
    return 0;
}

Node GenericRONamedNodeMapImpl::setNamedItem ( const Node &/*arg*/, int &exceptioncode )
{
    // can't modify this list through standard DOM functions
    exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

Node GenericRONamedNodeMapImpl::removeNamedItem ( const DOMString &/*name*/, int &exceptioncode )
{
    // can't modify this list through standard DOM functions
    exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

NodeImpl *GenericRONamedNodeMapImpl::item ( unsigned long index, int &/*exceptioncode*/ ) const
{
    // ### check this when calling from javascript using -1 = 2^sizeof(int)-1
    // (also for other similar methods)
    if (index >= m_contents->count())
        return 0;

    return m_contents->at(index);
}

void GenericRONamedNodeMapImpl::addNode(NodeImpl *n)
{
    // The spec says that in the case of duplicates we only keep the first one
    int exceptioncode;
    if (getNamedItem(n->nodeName(),exceptioncode))
        return;

    n->ref();
    m_contents->append(n);
}
// vim:ts=4:sw=4
