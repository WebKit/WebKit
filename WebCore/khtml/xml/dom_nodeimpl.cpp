/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 */

#include "xml/dom_nodeimpl.h"
#include "dom/dom_exception.h"
#include "misc/htmlattrs.h"
#include "misc/htmltags.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_nodeimpl.h"
#include "css/cssstyleselector.h"

#include <kglobal.h>
#include <kdebug.h>

#include "rendering/render_text.h"

#include "ecma/kjs_binding.h"
#include "ecma/kjs_proxy.h"
#include "khtmlview.h"
#include "khtml_part.h"


using namespace DOM;
using namespace khtml;

const Q_UINT32 NodeImpl::IdNSMask    = 0xffff0000;
const Q_UINT32 NodeImpl::IdLocalMask = 0x0000ffff;

NodeImpl::NodeImpl(DocumentPtr *doc)
    : document(doc),
      m_previous(0),
      m_next(0),
      m_render(0),
      m_regdListeners( 0 ),
      m_tabIndex( 0 ),
      m_hasId( false ),
      m_hasClass( false ),
      m_hasStyle( false ),
      m_attached(false),
      m_changed( false ),
      m_hasChangedChild( false ),
      m_inDocument( false ),
      m_hasAnchor( false ),
      m_specified( false ),
      m_focused( false ),
      m_active( false ),
      m_styleElement( false ),
      m_implicit( false ),
      m_rendererNeedsClose( false )
{
    if (document)
        document->ref();
}

void NodeImpl::setDocument(DocumentPtr *doc)
{
    if (inDocument())
	return;
    
    if (doc)
	doc->ref();
    
    if (document)
	document->deref();

    document = doc;
}

NodeImpl::~NodeImpl()
{
    if (m_render)
        detach();
    delete m_regdListeners;
    if (document)
        document->deref();
    if (m_previous)
        m_previous->setNextSibling(0);
    if (m_next)
        m_next->setPreviousSibling(0);
}

DOMString NodeImpl::nodeValue() const
{
  return DOMString();
}

void NodeImpl::setNodeValue( const DOMString &/*_nodeValue*/, int &exceptioncode )
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // be default nodeValue is null, so setting it has no effect
}

DOMString NodeImpl::nodeName() const
{
  return DOMString();
}

unsigned short NodeImpl::nodeType() const
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

void NodeImpl::normalize ()
{
    // ### normalize attributes? (when we store attributes using child nodes)
    int exceptioncode = 0;
    NodeImpl *child = firstChild();

    // Recursively go through the subtree beneath us, normalizing all nodes. In the case
    // where there are two adjacent text nodes, they are merged together
    while (child) {
        NodeImpl *nextChild = child->nextSibling();

        if (nextChild && child->nodeType() == Node::TEXT_NODE && nextChild->nodeType() == Node::TEXT_NODE) {
            // Current child and the next one are both text nodes... merge them
            TextImpl *currentText = static_cast<TextImpl*>(child);
            TextImpl *nextText = static_cast<TextImpl*>(nextChild);

            currentText->appendData(nextText->data(),exceptioncode);
            if (exceptioncode)
                return;

            removeChild(nextChild,exceptioncode);
            if (exceptioncode)
                return;
        }
        else {
            child->normalize();
            child = nextChild;
        }
    }
}

DOMString NodeImpl::prefix() const
{
    // For nodes other than elements and attributes, the prefix is always null
    return DOMString();
}

void NodeImpl::setPrefix(const DOMString &/*_prefix*/, int &exceptioncode )
{
    // The spec says that for nodes other than elements and attributes, prefix is always null.
    // It does not say what to do when the user tries to set the prefix on another type of
    // node, however mozilla throws a NAMESPACE_ERR exception
    exceptioncode = DOMException::NAMESPACE_ERR;
}

DOMString NodeImpl::localName() const
{
    return DOMString();
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

static QString escapeHTML( const QString& in )
{
    QString s;
    for ( unsigned int i = 0; i < in.length(); ++i ) {
        switch( in[i].latin1() ) {
        case '&':
            s += "&amp;";
            break;
        case '<':
            s += "&lt;";
            break;
        case '>':
            s += "&gt;";
            break;
        default:
            s += in[i];
        }
    }

    return s;
}

QString NodeImpl::recursive_toHTML(bool start) const
{
    QString me = "";

    // Copy who I am into the htmlText string
    if ( nodeType() == Node::TEXT_NODE )
        me = escapeHTML( nodeValue().string() );
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
            const ElementImpl *el = static_cast<const ElementImpl *>(this);
            NamedNodeMap attrs = el->attributes();
            unsigned long lmap = attrs.length();
            for( unsigned int j=0; j<lmap; j++ )
                me += " " + attrs.item(j).nodeName().string() + "=\"" + attrs.item(j).nodeValue().string() + "\"";
        }
        // print ending bracket of start tag
        if( firstChild() == 0 ) {    // if element has no endtag
	    if (isHTMLElement()) {
		me +=">";
	    } else {
                me +="/>";
	    }
	} else                        // if element has endtag
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
    if (b && !attached()) // changed compared to what?
        return;

    m_changed = b;
    if ( b ) {
	NodeImpl *p = parentNode();
	while ( p ) {
	    p->setHasChangedChild( true );
	    p = p->parentNode();
	}
        getDocument()->setDocumentChanged(true);
    }
}

bool NodeImpl::isInline() const
{
    if (m_render) return m_render->style()->display() == khtml::INLINE;
    return !isElementNode();
}

bool NodeImpl::isFocusable() const
{
    return false;
}

bool NodeImpl::isKeyboardFocusable() const
{
    return isFocusable();
}

bool NodeImpl::isMouseFocusable() const
{
    return isFocusable();
}

unsigned long NodeImpl::nodeIndex() const
{
    NodeImpl *_tempNode = previousSibling();
    unsigned long count=0;
    for( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
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
        m_regdListeners = new QPtrList<RegisteredEventListener>;
	m_regdListeners->setAutoDelete(true);
    }

    listener->ref();

    // remove existing identical listener set with identical arguments - the DOM2
    // spec says that "duplicate instances are discarded" in this case.
    removeEventListener(id,listener,useCapture);

    m_regdListeners->append(rl);
    listener->deref();
}

void NodeImpl::removeEventListener(int id, EventListener *listener, bool useCapture)
{
    if (!m_regdListeners) // nothing to remove
        return;

    RegisteredEventListener rl(static_cast<EventImpl::EventId>(id),listener,useCapture);

    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (*(it.current()) == rl) {
            m_regdListeners->removeRef(it.current());
            return;
        }
}

void NodeImpl::removeHTMLEventListener(int id)
{
    if (!m_regdListeners) // nothing to remove
        return;

    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (it.current()->id == id &&
            it.current()->listener->eventListenerType() == "_khtml_HTMLEventListener") {
            m_regdListeners->removeRef(it.current());
            return;
        }
}

void NodeImpl::setHTMLEventListener(int id, EventListener *listener)
{
    // in case we already have it, we don't want removeHTMLEventListener to destroy it
    if (listener)
        listener->ref();
    removeHTMLEventListener(id);
    if (listener)
    {
        addEventListener(id,listener,false);
        listener->deref();
    }
}

EventListener *NodeImpl::getHTMLEventListener(int id)
{
    if (!m_regdListeners)
        return 0;

    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (it.current()->id == id &&
            it.current()->listener->eventListenerType() == "_khtml_HTMLEventListener") {
            return it.current()->listener;
        }
    return 0;
}


bool NodeImpl::dispatchEvent(EventImpl *evt, int &exceptioncode, bool tempEvent)
{
    evt->setTarget(this);

    KHTMLPart *part = document->document()->part();

    // Since event handling code could cause this object to be deleted, grab a reference to the view now
    KHTMLView *view = document->document()->view();
    if (view)
        view->ref();

    bool ret = dispatchGenericEvent( evt, exceptioncode );

    // If tempEvent is true, this means that the DOM implementation will not be storing a reference to the event, i.e.
    // there is no way to retrieve it from javascript if a script does not already have a reference to it in a variable.
    // So there is no need for the interpreter to keep the event in it's cache
    if (tempEvent && part && part->jScript())
        part->jScript()->finishedWithEvent(evt);

    if (view)
        view->deref();

    return ret;
}

bool NodeImpl::dispatchGenericEvent( EventImpl *evt, int &/*exceptioncode */)
{
    // ### check that type specified

    // work out what nodes to send event to
    QPtrList<NodeImpl> nodeChain;
    NodeImpl *n;
    for (n = this; n; n = n->parentNode()) {
        n->ref();
        nodeChain.prepend(n);
    }

    // trigger any capturing event handlers on our way down
    evt->setEventPhase(Event::CAPTURING_PHASE);
    QPtrListIterator<NodeImpl> it(nodeChain);
    for (; it.current() && it.current() != this && !evt->propagationStopped(); ++it) {
        evt->setCurrentTarget(it.current());
        it.current()->handleLocalEvents(evt,true);
    }

    // dispatch to the actual target node
    it.toLast();
    if (!evt->propagationStopped()) {
        evt->setEventPhase(Event::AT_TARGET);
        evt->setCurrentTarget(it.current());

	// Capturing first. -dwh
        it.current()->handleLocalEvents(evt,true);

	// Bubbling second. -dwh
	if (!evt->propagationStopped())
	  it.current()->handleLocalEvents(evt,false);
    }
    --it;

    // ok, now bubble up again (only non-capturing event handlers will be called)
    // ### recalculate the node chain here? (e.g. if target node moved in document by previous event handlers)
    // no. the DOM specs says:
    // The chain of EventTargets from the event target to the top of the tree
    // is determined before the initial dispatch of the event.
    // If modifications occur to the tree during event processing,
    // event flow will proceed based on the initial state of the tree.
    //
    // since the initial dispatch is before the capturing phase,
    // there's no need to recalculate the node chain.
    // (tobias)

    if (evt->bubbles()) {
        evt->setEventPhase(Event::BUBBLING_PHASE);
        for (; it.current() && !evt->propagationStopped() && !evt->getCancelBubble(); --it) {
            evt->setCurrentTarget(it.current());
            it.current()->handleLocalEvents(evt,false);
        }
    }

    evt->setCurrentTarget(0);
    evt->setEventPhase(0); // I guess this is correct, the spec does not seem to say
                           // anything about the default event handler phase.


    if (evt->bubbles()) {
	// now we call all default event handlers (this is not part of DOM - it is internal to khtml)

	it.toLast();
	for (; it.current() && !evt->propagationStopped() && !evt->defaultPrevented() && !evt->defaultHandled(); --it)
	    it.current()->defaultEventHandler(evt);
    }
    
    // In the case of a mouse click, also send a DOMActivate event, which causes things like form submissions
    // to occur. Note that this only happens for _real_ mouse clicks (for which we get a KHTML_CLICK_EVENT or
    // KHTML_DBLCLICK_EVENT), not the standard DOM "click" event that could be sent from js code.
    if (!evt->defaultPrevented() && !disabled())
        if (evt->id() == EventImpl::KHTML_CLICK_EVENT)
            dispatchUIEvent(EventImpl::DOMACTIVATE_EVENT, 1);
        else if (evt->id() == EventImpl::KHTML_DBLCLICK_EVENT)
            dispatchUIEvent(EventImpl::DOMACTIVATE_EVENT, 2);

    // deref all nodes in chain
    it.toFirst();
    for (; it.current(); ++it)
        it.current()->deref(); // this may delete us

    DocumentImpl::updateDocumentsRendering();

    return !evt->defaultPrevented(); // ### what if defaultPrevented was called before dispatchEvent?
}

bool NodeImpl::dispatchHTMLEvent(int _id, bool canBubbleArg, bool cancelableArg)
{
    int exceptioncode = 0;
    EventImpl *evt = new EventImpl(static_cast<EventImpl::EventId>(_id),canBubbleArg,cancelableArg);
    evt->ref();
    bool r = dispatchEvent(evt,exceptioncode,true);
    evt->deref();
    return r;
}

bool NodeImpl::dispatchWindowEvent(int _id, bool canBubbleArg, bool cancelableArg)
{
    int exceptioncode = 0;
    EventImpl *evt = new EventImpl(static_cast<EventImpl::EventId>(_id),canBubbleArg,cancelableArg);
    evt->setTarget( 0 );
    evt->ref();
    DocumentPtr *doc = document;
    doc->ref();
    bool r = dispatchGenericEvent( evt, exceptioncode );
    if (!evt->defaultPrevented() && doc->document())
	doc->document()->defaultEventHandler(evt);
    
    if (_id == EventImpl::LOAD_EVENT && !evt->propagationStopped() && doc->document()) {
        // For onload events, send them to the enclosing frame only.
        // This is a DOM extension and is independent of bubbling/capturing rules of
        // the DOM.  You send the event only to the enclosing frame.  It does not
        // bubble through the parent document.
        DOM::ElementImpl* elt = doc->document()->ownerElement();
        if (elt && (elt->getDocument()->domain().isNull() ||
                    elt->getDocument()->domain() == doc->document()->domain())) {
            // We also do a security check, since we don't want to allow the enclosing
            // iframe to see loads of child documents in other domains.
            evt->setCurrentTarget(elt);

            // Capturing first.
            elt->handleLocalEvents(evt,true);

            // Bubbling second.
            if (!evt->propagationStopped())
                elt->handleLocalEvents(evt,false);
            r = !evt->defaultPrevented();
        }
    }

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


    int exceptioncode = 0;

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
    bool r = dispatchEvent(evt,exceptioncode,true);
    evt->deref();
    return r;

}

bool NodeImpl::dispatchUIEvent(int _id, int detail)
{
    assert (!( (_id != EventImpl::DOMFOCUSIN_EVENT &&
        _id != EventImpl::DOMFOCUSOUT_EVENT &&
                _id != EventImpl::DOMACTIVATE_EVENT)));

    bool cancelable = false;
    if (_id == EventImpl::DOMACTIVATE_EVENT)
        cancelable = true;

    int exceptioncode = 0;
    UIEventImpl *evt = new UIEventImpl(static_cast<EventImpl::EventId>(_id),true,
                                       cancelable,getDocument()->defaultView(),detail);
    evt->ref();
    bool r = dispatchEvent(evt,exceptioncode,true);
    evt->deref();
    return r;
}

bool NodeImpl::dispatchSubtreeModifiedEvent()
{
    childrenChanged();
    if (!getDocument()->hasListenerType(DocumentImpl::DOMSUBTREEMODIFIED_LISTENER))
	return false;
    int exceptioncode = 0;
    return dispatchEvent(new MutationEventImpl(EventImpl::DOMSUBTREEMODIFIED_EVENT,
			 true,false,0,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
}

bool NodeImpl::dispatchKeyEvent(QKeyEvent *key)
{
    int exceptioncode = 0;
    //kdDebug(6010) << "DOM::NodeImpl: dispatching keyboard event" << endl;
    KeyboardEventImpl *keyboardEventImpl = new KeyboardEventImpl(key, getDocument()->defaultView());
    keyboardEventImpl->ref();
    bool r = dispatchEvent(keyboardEventImpl,exceptioncode,true);

#if APPLE_CHANGES
    // we want to return false if default is prevented (already taken care of)
    // or if the element is default-handled by the DOM. Otherwise we let it just
    // let it get handled by AppKit 
    if (keyboardEventImpl->defaultHandled())
#else
    // the default event handler should accept() the internal QKeyEvent
    // to prevent the view from further evaluating it.
    if (!keyboardEventImpl->defaultPrevented() && !keyboardEventImpl->qKeyEvent->isAccepted())
#endif
      r = false;

    keyboardEventImpl->deref();
    return r;
}

void NodeImpl::handleLocalEvents(EventImpl *evt, bool useCapture)
{
    if (!m_regdListeners)
        return;

    if (disabled() && evt->isMouseEvent())
        return;

    QPtrList<RegisteredEventListener> listenersCopy = *m_regdListeners;
    QPtrListIterator<RegisteredEventListener> it(listenersCopy);
    Event ev = evt;
    for (; it.current(); ++it) {
        if (it.current()->id == evt->id() && it.current()->useCapture == useCapture)
            it.current()->listener->handleEvent(ev, false);
    }

}

void NodeImpl::defaultEventHandler(EventImpl *evt)
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

NodeImpl *NodeImpl::traverseNextNode(NodeImpl *stayWithin) const
{
    if (firstChild())
	return firstChild();
    else if (nextSibling())
	return nextSibling();
    else {
	const NodeImpl *n = this;
	while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
	    n = n->parentNode();
	if (n && (!stayWithin || n->parentNode() != stayWithin))
	    return n->nextSibling();
    }
    return 0;
}

NodeImpl *NodeImpl::traversePreviousNode() const
{
    if (previousSibling()) {
        NodeImpl *n = previousSibling();
        while (n->lastChild())
            n = n->lastChild();
        return n;
    }
    else if (parentNode()) {
        return parentNode();
    }
    else {
        return 0;
    }
}

void NodeImpl::checkSetPrefix(const DOMString &_prefix, int &exceptioncode)
{
    // Perform error checking as required by spec for setting Node.prefix. Used by
    // ElementImpl::setPrefix() and AttrImpl::setPrefix()

    // INVALID_CHARACTER_ERR: Raised if the specified prefix contains an illegal character.
    if (!Element::khtmlValidPrefix(_prefix)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // NAMESPACE_ERR: - Raised if the specified prefix is malformed
    // - if the namespaceURI of this node is null,
    // - if the specified prefix is "xml" and the namespaceURI of this node is different from
    //   "http://www.w3.org/XML/1998/namespace",
    // - if this node is an attribute and the specified prefix is "xmlns" and
    //   the namespaceURI of this node is different from "http://www.w3.org/2000/xmlns/",
    // - or if this node is an attribute and the qualifiedName of this node is "xmlns" [Namespaces].
    if (Element::khtmlMalformedPrefix(_prefix) || (!(id() & IdNSMask) && id() > ID_LAST_TAG) ||
        (_prefix == "xml" && DOMString(getDocument()->namespaceURI(id())) != "http://www.w3.org/XML/1998/namespace")) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return;
    }
}

void NodeImpl::checkAddChild(NodeImpl *newChild, int &exceptioncode)
{
    // Perform error checking as required by spec for adding a new child. Used by
    // appendChild(), replaceChild() and insertBefore()

    // Not mentioned in spec: throw NOT_FOUND_ERR if newChild is null
    if (!newChild) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    bool shouldAdoptChild = false;

    // WRONG_DOCUMENT_ERR: Raised if newChild was created from a different document than the one that
    // created this node.
    // We assume that if newChild is a DocumentFragment, all children are created from the same document
    // as the fragment itself (otherwise they could not have been added as children)
    if (newChild->getDocument() != getDocument()) {
	// but if the child is not in a document yet then loosen the
	// restriction, so that e.g. creating an element with the Option()
	// constructor and then adding it to a different document works,
	// as it does in Mozilla and Mac IE.
	if (!newChild->inDocument()) {
	    shouldAdoptChild = true;
	} else {
	    exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
	    return;
	}
    }

    // HIERARCHY_REQUEST_ERR: Raised if this node is of a type that does not allow children of the type of the
    // newChild node, or if the node to append is one of this node's ancestors.

    // check for ancestor/same node
    if (isAncestor(newChild)) {
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return;
    }

    // check node allowed
    if (newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE) {
        // newChild is a DocumentFragment... check all it's children instead of newChild itself
        NodeImpl *child;
        for (child = newChild->firstChild(); child; child = child->nextSibling()) {
            if (!childAllowed(child)) {
                exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
                return;
            }
        }
    }
    else {
        // newChild is not a DocumentFragment... check if it's allowed directly
        if(!childAllowed(newChild)) {
            exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
            return;
        }
    }

    // only do this once we know there won't be an exception
    if (shouldAdoptChild) {
	KJS::ScriptInterpreter::updateDOMObjectDocument(newChild, newChild->getDocument(), getDocument());
	newChild->setDocument(getDocument()->docPtr());
    }
}

bool NodeImpl::isAncestor( NodeImpl *other )
{
    // Return true if other is the same as this node or an ancestor of it, otherwise false
    NodeImpl *n;
    for (n = this; n; n = n->parentNode()) {
        if (n == other)
            return true;
    }
    return false;
}

bool NodeImpl::childAllowed( NodeImpl *newChild )
{
    return childTypeAllowed(newChild->nodeType());
}

NodeImpl::StyleChange NodeImpl::diff( khtml::RenderStyle *s1, khtml::RenderStyle *s2 ) const
{
    // FIXME: The behavior of this function is just totally wrong.  It doesn't handle
    // explicit inheritance of non-inherited properties and so you end up not re-resolving
    // style in cases where you need to.
    StyleChange ch = NoInherit;
    EDisplay display1 = s1 ? s1->display() : NONE;
    bool fl1 = s1 ? s1->hasPseudoStyle(RenderStyle::FIRST_LETTER) : false;
    EDisplay display2 = s2 ? s2->display() : NONE;
    bool fl2 = s2 ? s2->hasPseudoStyle(RenderStyle::FIRST_LETTER) : false;
    if (display1 != display2 || fl1 != fl2)
        ch = Detach;
    else if ( !s1 || !s2 )
	ch = Inherit;
    else if ( *s1 == *s2 )
 	ch = NoChange;
    else if ( s1->inheritedNotEqual( s2 ) )
	ch = Inherit;
    return ch;
}

#ifndef NDEBUG
void NodeImpl::dump(QTextStream *stream, QString ind) const
{
    // ### implement dump() for all appropriate subclasses

    if (m_hasId) { *stream << " hasId"; }
    if (m_hasClass) { *stream << " hasClass"; }
    if (m_hasStyle) { *stream << " hasStyle"; }
    if (m_specified) { *stream << " specified"; }
    if (m_focused) { *stream << " focused"; }
    if (m_active) { *stream << " active"; }
    if (m_styleElement) { *stream << " styleElement"; }
    if (m_implicit) { *stream << " implicit"; }

    *stream << " tabIndex=" << m_tabIndex;
    if (m_regdListeners)
	*stream << " #regdListeners=" << m_regdListeners->count(); // ### more detail
    *stream << endl;

    NodeImpl *child = firstChild();
    while( child != 0 )
    {
	*stream << ind << child->nodeName().string().ascii() << ": ";
        child->dump(stream,ind+"  ");
        child = child->nextSibling();
    }
}
#endif

void NodeImpl::closeRenderer()
{
    // It's important that we close the renderer, even if it hasn't been
    // created yet. This happens even more because of the FOUC fixes we did
    // at Apple, which prevent renderers from being created until the stylesheets
    // are all loaded. If the renderer is not here to be closed, we set a flag,
    // then close it later when it's attached.
    assert(!m_rendererNeedsClose);
    if (m_render)
        m_render->close();
    else
        m_rendererNeedsClose = true;
}

void NodeImpl::attach()
{
    assert(!attached());
    assert(!m_render || (m_render->style() && m_render->parent()));
    if (m_render && m_rendererNeedsClose) {
        m_render->close();
        m_rendererNeedsClose = false;
    }
    m_attached = true;
}

void NodeImpl::detach()
{
//    assert(m_attached);

    if (m_render)
        m_render->detach();

    m_render = 0;
    m_attached = false;
}

bool NodeImpl::maintainsState()
{
    return false;
}

QString NodeImpl::state()
{
    return QString::null;
}

void NodeImpl::restoreState(QStringList &/*states*/)
{
}

void NodeImpl::insertedIntoDocument()
{
    setInDocument(true);
}

void NodeImpl::removedFromDocument()
{
    setInDocument(false);
}

void NodeImpl::childrenChanged()
{
}

bool NodeImpl::disabled() const
{
    return false;
}

bool NodeImpl::isReadOnly()
{
    // Entity & Entity Reference nodes and their descendants are read-only
    NodeImpl *n = this;
    while (n) {
	if (n->nodeType() == Node::ENTITY_NODE ||
	    n->nodeType() == Node::ENTITY_REFERENCE_NODE)
	    return true;
	n = n->parentNode();
    }
    return false;
}

RenderObject * NodeImpl::previousRenderer()
{
    for (NodeImpl *n = previousSibling(); n; n = n->previousSibling()) {
        if (n->renderer())
            return n->renderer();
    }
    return 0;
}

RenderObject * NodeImpl::nextRenderer()
{
    for (NodeImpl *n = nextSibling(); n; n = n->nextSibling()) {
        if (n->renderer())
            return n->renderer();
    }
    return 0;
}

void NodeImpl::createRendererIfNeeded()
{
#if APPLE_CHANGES
    if (!getDocument()->shouldCreateRenderers())
        return;
#endif
        
    assert(!attached());
    assert(!m_render);
    
    NodeImpl *parent = parentNode();    
    assert(parent);
    
    RenderObject *parentRenderer = parent->renderer();
    if (parentRenderer && parentRenderer->canHaveChildren()) {
        RenderStyle *style = styleForRenderer(parentRenderer);
        style->ref();
        if (rendererIsNeeded(style)) {
            m_render = createRenderer(getDocument()->renderArena(), style);
            m_render->setStyle(style);
            parentRenderer->addChild(m_render, nextRenderer());
        }
        style->deref();
    }
}

RenderStyle *NodeImpl::styleForRenderer(RenderObject *parent)
{
    return parent->style();
}

bool NodeImpl::rendererIsNeeded(RenderStyle *style)
{
    return (getDocument()->documentElement() == this) || (style->display() != NONE);
}

RenderObject *NodeImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    assert(false);
    return 0;
}

//-------------------------------------------------------------------------

NodeBaseImpl::NodeBaseImpl(DocumentPtr *doc)
    : NodeImpl(doc)
{
    _first = _last = 0;
}


NodeBaseImpl::~NodeBaseImpl()
{
    //kdDebug( 6020 ) << "NodeBaseImpl destructor" << endl;

    // Avoid deep recursion when destroying the node tree.
    static bool alreadyInsideDestructor; 
    bool topLevel = !alreadyInsideDestructor;
    if (topLevel)
        alreadyInsideDestructor = true;
    
    // List of nodes to be deleted.
    static NodeImpl *head;
    static NodeImpl *tail;
    
    // We have to tell all children that their parent has died.
    NodeImpl *n;
    NodeImpl *next;

    for( n = _first; n != 0; n = next ) {
        next = n->nextSibling();
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        
	if ( !n->refCount() ) {
            // Add the node to the list of nodes to be deleted.
            // Reuse the nextSibling pointer for this purpose.
            if (tail)
                tail->setNextSibling(n);
            else
                head = n;
            tail = n;
        }
    }
    
    // Only for the top level call, do the actual deleting.
    if (topLevel) {
        while ((n = head) != 0) {
            next = n->nextSibling();
            n->setNextSibling(0);

            head = next;
            if (next == 0)
                tail = 0;
            
            delete n;
        }
        
        alreadyInsideDestructor = false;
    }
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

    // insertBefore(...,null) is equivalent to appendChild()
    if(!refChild)
        return appendChild(newChild, exceptioncode);

    // Make sure adding the new child is ok
    checkAddChild(newChild, exceptioncode);
    if (exceptioncode)
        return 0;

    // NOT_FOUND_ERR: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;

    // If newChild is a DocumentFragment with no children.... there's nothing to do.
    // Just return the document fragment
    if (isFragment && !newChild->firstChild())
        return newChild;

    // Now actually add the child(ren)
    NodeImpl *nextChild;
    NodeImpl *child = isFragment ? newChild->firstChild() : newChild;

    NodeImpl *prev = refChild->previousSibling();
    if ( prev == newChild || refChild == newChild ) // nothing to do
	return newChild;
    
    while (child) {
        nextChild = isFragment ? child->nextSibling() : 0;

        // If child is already present in the tree, first remove it
        NodeImpl *newParent = child->parentNode();
        if(newParent)
            newParent->removeChild( child, exceptioncode );
        if ( exceptioncode )
            return 0;

        // Add child in the correct position
        if (prev)
            prev->setNextSibling(child);
        else
            _first = child;
        refChild->setPreviousSibling(child);
        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(refChild);

        // Add child to the rendering tree
        // ### should we detach() it first if it's already attached?
        if (attached() && !child->attached())
            child->attach();

        // Dispatch the mutation events
        dispatchChildInsertedEvents(child,exceptioncode);

        prev = child;
        child = nextChild;
    }

    getDocument()->setDocumentChanged(true);
    dispatchSubtreeModifiedEvent();
    return newChild;
}

NodeImpl *NodeBaseImpl::replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode )
{
    exceptioncode = 0;

    if ( oldChild == newChild ) // nothing to do
	return oldChild;
    
    // Make sure adding the new child is ok
    checkAddChild(newChild, exceptioncode);
    if (exceptioncode)
        return 0;

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (!oldChild || oldChild->parentNode() != this) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;
    NodeImpl *nextChild;
    NodeImpl *child = isFragment ? newChild->firstChild() : newChild;


    // Remove the old child
    NodeImpl *prev = oldChild->previousSibling();
    NodeImpl *next = oldChild->nextSibling();

    removeChild(oldChild, exceptioncode);
    if (exceptioncode)
        return 0;

    // Add the new child(ren)
    while (child) {
        nextChild = isFragment ? child->nextSibling() : 0;

        // If child is already present in the tree, first remove it
        NodeImpl *newParent = child->parentNode();
        if(newParent)
            newParent->removeChild( child, exceptioncode );
        if (exceptioncode)
            return 0;

        // Add child in the correct position
        if (prev) prev->setNextSibling(child);
        if (next) next->setPreviousSibling(child);
        if(!prev) _first = child;
        if(!next) _last = child;
        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(next);

        // Add child to the rendering tree
        // ### should we detach() it first if it's already attached?
        if (attached() && !child->attached())
            child->attach();

        // Dispatch the mutation events
        dispatchChildInsertedEvents(child,exceptioncode);

        prev = child;
        child = nextChild;
    }

    // ### set style in case it's attached
    getDocument()->setDocumentChanged(true);
    dispatchSubtreeModifiedEvent();
    return oldChild;
}

NodeImpl *NodeBaseImpl::removeChild ( NodeImpl *oldChild, int &exceptioncode )
{
    exceptioncode = 0;

    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return 0;
    }

    // NOT_FOUND_ERR: Raised if oldChild is not a child of this node.
    if (!oldChild || oldChild->parentNode() != this) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    // Dispatch pre-removal mutation events
    getDocument()->notifyBeforeNodeRemoval(oldChild); // ### use events instead
    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVED_LISTENER)) {
	oldChild->dispatchEvent(new MutationEventImpl(EventImpl::DOMNODEREMOVED_EVENT,
			     true,false,this,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
	if (exceptioncode)
	    return 0;
    }

    dispatchChildRemovalEvents(oldChild,exceptioncode);
    if (exceptioncode)
        return 0;

    // Remove from rendering tree
    if (oldChild->attached())
        oldChild->detach();

    // Remove the child
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

    getDocument()->setDocumentChanged(true);

    // Dispatch post-removal mutation events
    dispatchSubtreeModifiedEvent();

    NodeImpl *p = this;
    while (p->parentNode())
	p = p->parentNode();
    if (p->nodeType() == Node::DOCUMENT_NODE) {
	for (NodeImpl *c = oldChild; c; c = c->traverseNextNode(oldChild))
	    c->removedFromDocument();
    }

    return oldChild;
}

void NodeBaseImpl::removeChildren()
{
    NodeImpl *n, *next;
    for( n = _first; n != 0; n = next )
    {
        next = n->nextSibling();
        if (n->attached())
	    n->detach();
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        if( !n->refCount() )
            delete n;
    }
    _first = _last = 0;
}


NodeImpl *NodeBaseImpl::appendChild ( NodeImpl *newChild, int &exceptioncode )
{
    exceptioncode = 0;

    // Make sure adding the new child is ok
    checkAddChild(newChild, exceptioncode);
    if (exceptioncode)
        return 0;
    
    if ( newChild == _last ) // nothing to do
	return newChild;

    bool isFragment = newChild->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;

    // If newChild is a DocumentFragment with no children.... there's nothing to do.
    // Just return the document fragment
    if (isFragment && !newChild->firstChild())
        return newChild;

    // Now actually add the child(ren)
    NodeImpl *nextChild;
    NodeImpl *child = isFragment ? newChild->firstChild() : newChild;

    while (child) {
        nextChild = isFragment ? child->nextSibling() : 0;

        // If child is already present in the tree, first remove it
        NodeImpl *oldParent = child->parentNode();
        if(oldParent) {
            oldParent->removeChild( child, exceptioncode );
            if (exceptioncode)
                return 0;
        }

        // Append child to the end of the list
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

        // Add child to the rendering tree
        // ### should we detach() it first if it's already attached?
        if (attached() && !child->attached())
            child->attach();
          
        // Dispatch the mutation events
        dispatchChildInsertedEvents(child,exceptioncode);

        child = nextChild;
    }

    getDocument()->setDocumentChanged(true);
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
    DocumentImpl *ownerDocThis = getDocument();
    DocumentImpl *ownerDocNew = getDocument();
    if(ownerDocThis != ownerDocNew) {
        kdDebug(6010)<< "not same document, newChild = " << newChild << "document = " << getDocument() << endl;
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return true;
    }
    return false;
}

// check for being (grand-..)father:
// ### remove in favor or isAncestor()
bool NodeBaseImpl::checkNoOwner( NodeImpl *newChild, int &exceptioncode )
{
  //check if newChild is parent of this...
  NodeImpl *n;
  for( n = this; (n != getDocument()) && (n!= 0); n = n->parentNode() )
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

    newChild->insertedIntoDocument();
    childrenChanged();

    if(newChild->nodeType() == Node::ELEMENT_NODE)
        return newChild;
    return this;
}

void NodeBaseImpl::attach()
{
    NodeImpl *child = _first;
    while(child != 0)
    {
        child->attach();
        child = child->nextSibling();
    }
    NodeImpl::attach();
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
    NodeImpl::detach();
}

void NodeBaseImpl::cloneChildNodes(NodeImpl *clone)
{
    int exceptioncode = 0;
    NodeImpl *n;
    for(n = firstChild(); n && !exceptioncode; n = n->nextSibling())
    {
        clone->appendChild(n->cloneNode(true),exceptioncode);
    }
}

NodeListImpl* NodeBaseImpl::getElementsByTagNameNS ( DOMStringImpl* namespaceURI,
                                                     DOMStringImpl* localName )
{
    if (!localName) return 0;

    NodeImpl::Id idMask = NodeImpl::IdNSMask | NodeImpl::IdLocalMask;
    if (localName->l && localName->s[0] == '*')
        idMask &= ~NodeImpl::IdLocalMask;
    if (namespaceURI && namespaceURI->l && namespaceURI->s[0] == '*')
        idMask &= ~NodeImpl::IdNSMask;

    Id id = 0; // 0 means "all items"
    if ( (idMask & NodeImpl::IdLocalMask) || namespaceURI ) // not getElementsByTagName("*")
    {
        id = getDocument()->tagId( namespaceURI, localName, true);
        if ( !id ) // not found -> we want to return an empty list, not "all items"
            id = (Id)-1; // HACK. HEAD has a cleaner implementation of TagNodeListImpl it seems.
    }

    return new TagNodeListImpl( this, id, idMask );
}

// I don't like this way of implementing the method, but I didn't find any
// other way. Lars
bool NodeBaseImpl::getUpperLeftCorner(int &xPos, int &yPos) const
{
    if (!m_render)
        return false;
    RenderObject *o = m_render;
    xPos = yPos = 0;
    if ( !o->isInline() || o->isReplaced() ) {
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
    if (!o->isInline() || o->isReplaced())
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
    if ( xEnd <= xPos || yEnd <= yPos )
        return QRect( QPoint( xPos, yPos ), QSize() );

    return QRect(xPos, yPos, xEnd - xPos, yEnd - yPos);
}

void NodeBaseImpl::setFocus(bool received)
{
    if (m_focused == received) return;

    NodeImpl::setFocus(received);

    // note that we need to recalc the style
    setChanged();
}

void NodeBaseImpl::setActive(bool down)
{
    if (down == active()) return;

    NodeImpl::setActive(down);

    // note that we need to recalc the style
    if (m_render && m_render->style()->affectedByActiveRules())
        setChanged();
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
                                                   true,false,this,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
        if (exceptioncode)
            return;
    }

    // dispatch the DOMNOdeInsertedInfoDocument event to all descendants
    bool hasInsertedListeners = getDocument()->hasListenerType(DocumentImpl::DOMNODEINSERTEDINTODOCUMENT_LISTENER);
    NodeImpl *p = this;
    while (p->parentNode())
        p = p->parentNode();
    if (p->nodeType() == Node::DOCUMENT_NODE) {
        for (NodeImpl *c = child; c; c = c->traverseNextNode(child)) {
            c->insertedIntoDocument();

            if (hasInsertedListeners) {
                c->dispatchEvent(new MutationEventImpl(EventImpl::DOMNODEINSERTEDINTODOCUMENT_EVENT,
                                                       false,false,0,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
                if (exceptioncode)
                    return;
            }
        }
    }
}

void NodeBaseImpl::dispatchChildRemovalEvents( NodeImpl *child, int &exceptioncode )
{
    // Dispatch pre-removal mutation events
    getDocument()->notifyBeforeNodeRemoval(child); // ### use events instead
    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVED_LISTENER)) {
	child->dispatchEvent(new MutationEventImpl(EventImpl::DOMNODEREMOVED_EVENT,
			     true,false,this,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
	if (exceptioncode)
	    return;
    }

    bool hasRemovalListeners = getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVEDFROMDOCUMENT_LISTENER);

    // dispatch the DOMNOdeRemovedFromDocument event to all descendants
    NodeImpl *p = this;
    while (p->parentNode())
	p = p->parentNode();
    if (p->nodeType() == Node::DOCUMENT_NODE) {
	for (NodeImpl *c = child; c; c = c->traverseNextNode(child)) {
	    if (hasRemovalListeners) {
		c->dispatchEvent(new MutationEventImpl(EventImpl::DOMNODEREMOVEDFROMDOCUMENT_EVENT,
				 false,false,0,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
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

bool ChildNodeListImpl::nodeMatches( NodeImpl */*testNode*/ ) const
{
    return true;
}

TagNodeListImpl::TagNodeListImpl(NodeImpl *n, NodeImpl::Id _id, NodeImpl::Id _idMask )
    : refNode(n), m_id(_id & _idMask), m_idMask(_idMask)
{
    refNode->ref();
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
    return ( testNode->isElementNode() &&
             (testNode->id() & m_idMask) == m_id);
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

// ### unused
#if 0
GenericRONamedNodeMapImpl::GenericRONamedNodeMapImpl(DocumentPtr* doc)
    : NamedNodeMapImpl()
{
    m_doc = doc->document();
    m_contents = new QPtrList<NodeImpl>;
}

GenericRONamedNodeMapImpl::~GenericRONamedNodeMapImpl()
{
    while (m_contents->count() > 0)
        m_contents->take(0)->deref();

    delete m_contents;
}

NodeImpl *GenericRONamedNodeMapImpl::getNamedItem ( const DOMString &name, int &/*exceptioncode*/ ) const
{
    QPtrListIterator<NodeImpl> it(*m_contents);
    for (; it.current(); ++it)
        if (it.current()->nodeName() == name)
            return it.current();
    return 0;
}

Node GenericRONamedNodeMapImpl::setNamedItem ( const Node &/*arg*/, int &exceptioncode )
{
    // can't modify this list through standard DOM functions
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this map is readonly
    exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

Node GenericRONamedNodeMapImpl::removeNamedItem ( const DOMString &/*name*/, int &exceptioncode )
{
    // can't modify this list through standard DOM functions
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this map is readonly
    exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

NodeImpl *GenericRONamedNodeMapImpl::item ( unsigned long index ) const
{
    // ### check this when calling from javascript using -1 = 2^sizeof(int)-1
    // (also for other similar methods)
    if (index >= m_contents->count())
        return 0;

    return m_contents->at(index);
}

unsigned long GenericRONamedNodeMapImpl::length(  ) const
{
    return m_contents->count();
}

NodeImpl *GenericRONamedNodeMapImpl::getNamedItemNS( const DOMString &namespaceURI,
                                                     const DOMString &localName,
                                                     int &/*exceptioncode*/ ) const
{
    NodeImpl::Id searchId = m_doc->tagId(namespaceURI.implementation(),
                                                   localName.implementation(), true);

    QPtrListIterator<NodeImpl> it(*m_contents);
    for (; it.current(); ++it)
        if (it.current()->id() == searchId)
            return it.current();

    return 0;
}

NodeImpl *GenericRONamedNodeMapImpl::setNamedItemNS( NodeImpl */*arg*/, int &exceptioncode )
{
    // can't modify this list through standard DOM functions
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this map is readonly
    exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

NodeImpl *GenericRONamedNodeMapImpl::removeNamedItemNS( const DOMString &/*namespaceURI*/,
                                                        const DOMString &/*localName*/,
                                                        int &exceptioncode )
{
    // can't modify this list through standard DOM functions
    exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

void GenericRONamedNodeMapImpl::addNode(NodeImpl *n)
{
    // The spec says that in the case of duplicates we only keep the first one
    int exceptioncode = 0;
    if (getNamedItem(n->nodeName(),exceptioncode))
        return;

    n->ref();
    m_contents->append(n);
}

#endif
