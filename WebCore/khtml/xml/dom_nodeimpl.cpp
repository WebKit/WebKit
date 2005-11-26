/**
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
 */

#include "config.h"
#include "xml/dom_nodeimpl.h"

#include "dom/dom_exception.h"
#include "dom/dom2_events.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_position.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/EventNames.h"
#include "css/csshelper.h"
#include "css/cssstyleselector.h"
#include "editing/html_interchange.h"
#include "editing/SelectionController.h"

#include <kglobal.h>
#include <kdebug.h>

#include "rendering/render_object.h"
#include "rendering/render_text.h"
#include "render_theme.h"

#include "ecma/kjs_binding.h"
#include "ecma/kjs_proxy.h"
#include "khtmlview.h"
#include "khtml_part.h"

// FIXME: Should not have to include this.  Cut the HTML dependency!
#include "htmlnames.h"

#ifndef KHTML_NO_XBL
#include "xbl/xbl_binding_manager.h"
#endif

#include <kxmlcore/Assertions.h>
#include "KWQLogging.h"

using namespace khtml;

namespace DOM {

using namespace EventNames;
using namespace HTMLNames;

/**
 * NodeList which lists all Nodes in a document with a given tag name
 */
class TagNodeListImpl : public NodeListImpl
{
public:
    TagNodeListImpl(NodeImpl *n, const AtomicString& namespaceURI, const AtomicString& localName);

    // DOM methods overridden from  parent classes
    virtual unsigned length() const;
    virtual NodeImpl *item (unsigned index) const;

    // Other methods (not part of DOM)

protected:
    virtual bool nodeMatches(NodeImpl *testNode) const;

    AtomicString m_namespaceURI;
    AtomicString m_localName;
};

#ifndef NDEBUG
struct NodeImplCounter { 
    static int count; 
    ~NodeImplCounter() { /* if (count != 0) fprintf(stderr, "LEAK: %d NodeImpl\n", count); */ }
};
int NodeImplCounter::count = 0;
static NodeImplCounter nodeImplCounter;

static int gEventDispatchForbidden;
#define forbidEventDispatch() gEventDispatchForbidden += 1
#define allowEventDispatch() assert(gEventDispatchForbidden > 0); gEventDispatchForbidden -= 1
#define eventDispatchForbidden() (gEventDispatchForbidden > 0)
#else

#define forbidEventDispatch()
#define allowEventDispatch()
#define eventDispatchForbidden()
#endif NDEBUG

NodeImpl::NodeImpl(DocumentImpl *doc)
    : document(doc),
      m_previous(0),
      m_next(0),
      m_render(0),
      m_regdListeners( 0 ),
      m_nodeLists( 0 ),
      m_tabIndex( 0 ),
      m_hasId( false ),
      m_hasClass( false ),
      m_hasStyle( false ),
      m_attached(false),
      m_changed( false ),
      m_hasChangedChild( false ),
      m_inDocument( false ),
      m_isLink( false ),
      m_specified( false ),
      m_focused( false ),
      m_active( false ),
      m_hovered(false),
      m_styleElement( false ),
      m_implicit( false )
{
#ifndef NDEBUG
    ++NodeImplCounter::count;
#endif
}

void NodeImpl::setDocument(DocumentImpl *doc)
{
    if (inDocument())
	return;
    
    document = doc;
}

NodeImpl::~NodeImpl()
{
#ifndef NDEBUG
    --NodeImplCounter::count;
#endif
    if (m_render)
        detach();
    if (m_regdListeners && !m_regdListeners->isEmpty() && getDocument() && !inDocument())
        getDocument()->unregisterDisconnectedNodeWithEventListeners(this);
    delete m_regdListeners;
    delete m_nodeLists;
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

SharedPtr<NodeListImpl> NodeImpl::childNodes()
{
    return SharedPtr<NodeListImpl>(new ChildNodeListImpl(this));
}

NodeImpl *NodeImpl::firstChild() const
{
  return 0;
}

NodeImpl *NodeImpl::lastChild() const
{
  return 0;
}

NodeImpl *NodeImpl::lastDescendant() const
{
    NodeImpl *n = const_cast<NodeImpl *>(this);
    while (n && n->lastChild())
        n = n->lastChild();
    return n;
}

NodeImpl *NodeImpl::insertBefore( NodeImpl *newChild, NodeImpl *, int &exceptioncode )
{
    newChild->ref();
    newChild->deref();
    exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
    return 0;
}

NodeImpl *NodeImpl::replaceChild( NodeImpl *newChild, NodeImpl *, int &exceptioncode )
{
  newChild->ref();
  newChild->deref();
  exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
  return 0;
}

NodeImpl *NodeImpl::removeChild( NodeImpl *, int &exceptioncode )
{
  exceptioncode = DOMException::NOT_FOUND_ERR;
  return 0;
}

NodeImpl *NodeImpl::appendChild( NodeImpl *newChild, int &exceptioncode )
{
    newChild->ref();
    newChild->deref();
    exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
    return 0;
}

void NodeImpl::remove(int &exceptioncode)
{
    ref();
    if (NodeImpl *p = parentNode())
        p->removeChild(this, exceptioncode);
    else
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
    deref();
}

bool NodeImpl::hasChildNodes(  ) const
{
  return false;
}

void NodeImpl::normalize ()
{
    int exceptioncode = 0;
    NodeImpl *child = firstChild();

    if (isElementNode()) {
        // Normalize any attribute children we might have 
        ElementImpl *element = static_cast<ElementImpl *>(this);
        NamedAttrMapImpl *attrMap = element->attributes();
        
        if (attrMap) {
            unsigned numAttrs = attrMap->length();
            
            for (unsigned i = 0; i < numAttrs; i++) {
                AttributeImpl *attribute = attrMap->attributeItem(i);
                AttrImpl *attr = attribute->attrImpl();
                
                if (attr)
                    attr->normalize();
            }
        }
    }
    
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

            nextChild->remove(exceptioncode);
            if (exceptioncode)
                return;
        }
        else {
            child->normalize();
            child = nextChild;
        }
    }
    
    // Check if we have a single empty text node left and remove it if so
    child = firstChild();
    if (child && !child->nextSibling() && child->isTextNode()) {
        TextImpl *text = static_cast<TextImpl*>(child);
        if (text->data().isEmpty())
            child->remove(exceptioncode);
    }
}

const AtomicString& NodeImpl::prefix() const
{
    // For nodes other than elements and attributes, the prefix is always null
    return nullAtom;
}

void NodeImpl::setPrefix(const AtomicString &/*_prefix*/, int &exceptioncode )
{
    // The spec says that for nodes other than elements and attributes, prefix is always null.
    // It does not say what to do when the user tries to set the prefix on another type of
    // node, however mozilla throws a NAMESPACE_ERR exception
    exceptioncode = DOMException::NAMESPACE_ERR;
}

const AtomicString& NodeImpl::localName() const
{
    return emptyAtom;
}

const AtomicString& NodeImpl::namespaceURI() const
{
    return nullAtom;
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

bool NodeImpl::isContentEditable() const
{
    return parent() && parent()->isContentEditable();
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

unsigned NodeImpl::nodeIndex() const
{
    NodeImpl *_tempNode = previousSibling();
    unsigned count=0;
    for( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
}

void NodeImpl::addEventListener(const AtomicString &eventType, EventListener *listener, const bool useCapture)
{
    if (getDocument() && !getDocument()->attached())
        return;

    DocumentImpl::ListenerType type = static_cast<DocumentImpl::ListenerType>(0);
    if (eventType == DOMSubtreeModifiedEvent)
        type = DocumentImpl::DOMSUBTREEMODIFIED_LISTENER;
    else if (eventType == DOMNodeInsertedEvent)
        type = DocumentImpl::DOMNODEINSERTED_LISTENER;
    else if (eventType == DOMNodeRemovedEvent)
        type = DocumentImpl::DOMNODEREMOVED_LISTENER;
    else if (eventType == DOMNodeRemovedFromDocumentEvent)
        type = DocumentImpl::DOMNODEREMOVEDFROMDOCUMENT_LISTENER;
    else if (eventType == DOMNodeInsertedIntoDocumentEvent)
        type = DocumentImpl::DOMNODEINSERTEDINTODOCUMENT_LISTENER;
    else if (eventType == DOMAttrModifiedEvent)
        type = DocumentImpl::DOMATTRMODIFIED_LISTENER;
    else if (eventType == DOMCharacterDataModifiedEvent)
        type = DocumentImpl::DOMCHARACTERDATAMODIFIED_LISTENER;
    if (type)
        getDocument()->addListenerType(type);

    if (!m_regdListeners) {
        m_regdListeners = new QPtrList<RegisteredEventListener>;
	m_regdListeners->setAutoDelete(true);
    }

    listener->ref();

    // Remove existing identical listener set with identical arguments.
    // The DOM2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(eventType, listener, useCapture);

    // adding the first one
    if (m_regdListeners->isEmpty() && getDocument() && !inDocument())
        getDocument()->registerDisconnectedNodeWithEventListeners(this);
        
    m_regdListeners->append(new RegisteredEventListener(eventType, listener, useCapture));

    listener->deref();
}

void NodeImpl::removeEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture)
{
    if (!m_regdListeners) // nothing to remove
        return;

    RegisteredEventListener rl(eventType, listener, useCapture);

    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (*(it.current()) == rl) {
            m_regdListeners->removeRef(it.current());
            // removed last
            if (m_regdListeners->isEmpty() && getDocument() && !inDocument())
                getDocument()->unregisterDisconnectedNodeWithEventListeners(this);
            return;
        }
}

void NodeImpl::removeAllEventListeners()
{
    delete m_regdListeners;
    m_regdListeners = 0;
}

void NodeImpl::removeHTMLEventListener(const AtomicString &eventType)
{
    if (!m_regdListeners) // nothing to remove
        return;

    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (it.current()->eventType() == eventType && it.current()->listener()->eventListenerType() == "_khtml_HTMLEventListener") {
            m_regdListeners->removeRef(it.current());
            // removed last
            if (m_regdListeners->isEmpty() && getDocument() && !inDocument())
                getDocument()->unregisterDisconnectedNodeWithEventListeners(this);
            return;
        }
}

void NodeImpl::setHTMLEventListener(const AtomicString &eventType, EventListener *listener)
{
    // In case we are the only one holding a reference to it, we don't want removeHTMLEventListener to destroy it.
    if (listener)
        listener->ref();
    removeHTMLEventListener(eventType);
    if (listener) {
        addEventListener(eventType, listener, false);
        listener->deref();
    }
}

EventListener *NodeImpl::getHTMLEventListener(const AtomicString &eventType)
{
    if (!m_regdListeners)
        return 0;

    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (it.current()->eventType() == eventType && it.current()->listener()->eventListenerType() == "_khtml_HTMLEventListener")
            return it.current()->listener();
    return 0;
}


bool NodeImpl::dispatchEvent(EventImpl *evt, int &exceptioncode, bool tempEvent)
{
    assert(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) { 
        exceptioncode = EventException::_EXCEPTION_OFFSET + EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }

    evt->ref();

    evt->setTarget(this);

    // We've had at least one report of a crash on a page where document is nil here.
    // Unfortunately that page no longer exists, but we'll make this code robust against
    // that anyway.
    // FIXME: Much code in this class assumes document is non-nil; it would be better to
    // ensure that document can never be nil.
    KHTMLPart *part = nil;
    KHTMLView *view = nil;
    
    if (DocumentImpl *doc = getDocument()) {
        part = doc->part();
        view = doc->view();
        // Since event handling code could cause this object to be deleted, grab a reference to the view now
        if (view)
            view->ref();
    }    

    bool ret = dispatchGenericEvent( evt, exceptioncode );

    // If tempEvent is true, this means that the DOM implementation will not be storing a reference to the event, i.e.
    // there is no way to retrieve it from javascript if a script does not already have a reference to it in a variable.
    // So there is no need for the interpreter to keep the event in it's cache
    if (tempEvent && part && part->jScript())
        part->jScript()->finishedWithEvent(evt);

    if (view)
        view->deref();

    evt->deref();

    return ret;
}

bool NodeImpl::dispatchGenericEvent( EventImpl *evt, int &/*exceptioncode */)
{
    assert(!eventDispatchForbidden());
    assert(evt->target());

    evt->ref();

    // ### check that type specified

    // work out what nodes to send event to
    QPtrList<NodeImpl> nodeChain;
    NodeImpl *n;
    for (n = this; n; n = n->parentNode()) {
        n->ref();
        nodeChain.prepend(n);
    }

    
    QPtrListIterator<NodeImpl> it(nodeChain);
    
    // Before we begin dispatching events, give the target node a chance to do some work prior
    // to the DOM event handlers getting a crack.
    void* data = preDispatchEventHandler(evt);

    // trigger any capturing event handlers on our way down
    evt->setEventPhase(Event::CAPTURING_PHASE);
    it.toFirst();
    for (; it.current() && it.current() != this && !evt->propagationStopped(); ++it) {
        evt->setCurrentTarget(it.current());
        it.current()->handleLocalEvents(evt,true);
    }

    // dispatch to the actual target node
    it.toLast();
    if (!evt->propagationStopped()) {
        evt->setEventPhase(Event::AT_TARGET);
        evt->setCurrentTarget(it.current());

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


    // Now call the post dispatch.
    postDispatchEventHandler(evt, data);
    
    if (evt->bubbles()) {
	// now we call all default event handlers (this is not part of DOM - it is internal to khtml)

	it.toLast();
	for (; it.current() && !evt->defaultPrevented() && !evt->defaultHandled(); --it)
	    it.current()->defaultEventHandler(evt);
    }
    
    // deref all nodes in chain
    it.toFirst();
    for (; it.current(); ++it)
        it.current()->deref(); // this may delete us

    DocumentImpl::updateDocumentsRendering();

    bool defaultPrevented = evt->defaultPrevented();

    evt->deref();

    return !defaultPrevented; // ### what if defaultPrevented was called before dispatchEvent?
}

bool NodeImpl::dispatchHTMLEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
{
    assert(!eventDispatchForbidden());
    int exceptioncode = 0;
    return dispatchEvent(new EventImpl(eventType, canBubbleArg, cancelableArg), exceptioncode, true);
}

bool NodeImpl::dispatchWindowEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
{
    assert(!eventDispatchForbidden());
    int exceptioncode = 0;
    SharedPtr<EventImpl> evt = new EventImpl(eventType, canBubbleArg, cancelableArg);
    SharedPtr<DocumentImpl> doc = getDocument();
    evt->setTarget(doc.get());
    bool r = dispatchGenericEvent(evt.get(), exceptioncode);
    if (!evt->defaultPrevented() && doc)
 	doc->defaultEventHandler(evt.get());

    if (eventType == loadEvent && !evt->propagationStopped() && doc) {
        // For onload events, send them to the enclosing frame only.
        // This is a DOM extension and is independent of bubbling/capturing rules of
        // the DOM.  You send the event only to the enclosing frame.  It does not
        // bubble through the parent document.
        ElementImpl* elt = doc->ownerElement();
        if (elt && (elt->getDocument()->domain().isNull() ||
                    elt->getDocument()->domain() == doc->domain())) {
            // We also do a security check, since we don't want to allow the enclosing
            // iframe to see loads of child documents in other domains.
            evt->setCurrentTarget(elt);

            // Capturing first.
            elt->handleLocalEvents(evt.get(), true);

            // Bubbling second.
            if (!evt->propagationStopped())
                elt->handleLocalEvents(evt.get(), false);
            r = !evt->defaultPrevented();
        }
    }

    return r;
}

bool NodeImpl::dispatchMouseEvent(QMouseEvent *_mouse, const AtomicString &overrideType, int overrideDetail)
{
    assert(!eventDispatchForbidden());
    int detail = overrideDetail; // defaults to 0
    AtomicString eventType;
    if (!overrideType.isEmpty()) {
        eventType = overrideType;
    } else {
        switch (_mouse->type()) {
            case QEvent::MouseButtonPress:
                eventType = mousedownEvent;
                break;
            case QEvent::MouseButtonRelease:
                eventType = mouseupEvent;
                break;
            case QEvent::MouseButtonDblClick:
                eventType = clickEvent;
                detail = _mouse->clickCount();
                break;
            case QEvent::MouseMove:
                eventType = mousemoveEvent;
                break;
            default:
                break;
        }
    }

    int clientX = 0;
    int clientY = 0;
    if (KHTMLView *view = getDocument()->view())
        view->viewportToContents(_mouse->x(), _mouse->y(), clientX, clientY);
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
    bool metaKey = (_mouse->state() & Qt::MetaButton);
    
    return dispatchMouseEvent(eventType, button, detail,
        clientX, clientY, screenX, screenY,
        ctrlKey, altKey, shiftKey, metaKey);
}

bool NodeImpl::dispatchSimulatedMouseEvent(const AtomicString &eventType)
{
    assert(!eventDispatchForbidden());
    // Like Gecko, we just pass 0 for everything when we make a fake mouse event.
    // Internet Explorer instead gives the current mouse position and state.
    return dispatchMouseEvent(eventType, 0, 0, 0, 0, 0, 0, false, false, false, false);
}

bool NodeImpl::dispatchMouseEvent(const AtomicString &eventType, int button, int detail,
    int clientX, int clientY, int screenX, int screenY,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    assert(!eventDispatchForbidden());
    if (disabled()) // Don't even send DOM events for disabled controls..
        return true;

    if (eventType.isEmpty())
        return false; // Shouldn't happen.

    // Dispatching the first event can easily result in this node being destroyed.
    // Since we dispatch up to three events here, we need to make sure we're referenced
    // so the pointer will be good for the two subsequent ones.
    SharedPtr<NodeImpl> protect(this);

    bool cancelable = eventType != mousemoveEvent;
    
    int exceptioncode = 0;

    bool swallowEvent = false;

    EventImpl *me = new MouseEventImpl(eventType,true,cancelable,getDocument()->defaultView(),
                   detail,screenX,screenY,clientX,clientY,ctrlKey,altKey,shiftKey,metaKey,
                   button,0);
    me->ref();
    dispatchEvent(me, exceptioncode, true);
    bool defaultHandled = me->defaultHandled();
    bool defaultPrevented = me->defaultPrevented();
    if (defaultHandled || defaultPrevented)
        swallowEvent = true;
    me->deref();

    // Special case: If it's a double click event, we also send the KHTML_DBLCLICK event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute.  This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    if (eventType == clickEvent && detail == 2) {
        me = new MouseEventImpl(khtmlDblclickEvent,
                                true,cancelable,getDocument()->defaultView(),
                                detail,screenX,screenY,clientX,clientY,
                                ctrlKey,altKey,shiftKey,metaKey,
                                button,0);
        me->ref();
        if (defaultHandled)
            me->setDefaultHandled();
        dispatchEvent(me,exceptioncode,true);
        if (me->defaultHandled() || me->defaultPrevented())
            swallowEvent = true;
        me->deref();
    }

    // Also send a DOMActivate event, which causes things like form submissions to occur.
    if (eventType == clickEvent && !defaultPrevented)
        dispatchUIEvent(DOMActivateEvent, detail);

    return swallowEvent;
}

bool NodeImpl::dispatchUIEvent(const AtomicString &eventType, int detail)
{
    assert(!eventDispatchForbidden());
    assert (!( (eventType != DOMFocusInEvent &&
                eventType != DOMFocusOutEvent &&
                eventType != DOMActivateEvent)));

    if (!getDocument())
        return false;

    bool cancelable = false;
    if (eventType == DOMActivateEvent)
        cancelable = true;

    int exceptioncode = 0;

    UIEventImpl *evt = new UIEventImpl(eventType, true, cancelable, getDocument()->defaultView(), detail);
    return dispatchEvent(evt,exceptioncode,true);
}

void NodeImpl::registerNodeList(NodeListImpl *list)
{
    if (!m_nodeLists) {
        m_nodeLists = new QPtrDict<NodeListImpl>;
    }

    m_nodeLists->insert(list, list);
}

void NodeImpl::unregisterNodeList(NodeListImpl *list)
{
    if (!m_nodeLists)
        return;

    m_nodeLists->remove(list);
}

void NodeImpl::notifyLocalNodeListsAttributeChanged()
{
    if (!m_nodeLists)
        return;

    QPtrDictIterator<NodeListImpl> i(*m_nodeLists);

    while (NodeListImpl *list = i.current()) {
        list->rootNodeAttributeChanged();
        ++i;
    }
}

void NodeImpl::notifyNodeListsAttributeChanged()
{
    for (NodeImpl *n = this; n; n = n->parentNode()) {
        n->notifyLocalNodeListsAttributeChanged();
    }
}

void NodeImpl::notifyLocalNodeListsChildrenChanged()
{
    if (!m_nodeLists)
        return;

    QPtrDictIterator<NodeListImpl> i(*m_nodeLists);

    while (NodeListImpl *list = i.current()) {
        list->rootNodeChildrenChanged();
        ++i;
    }
}

void NodeImpl::notifyNodeListsChildrenChanged()
{
    for (NodeImpl *n = this; n; n = n->parentNode()) {
        n->notifyLocalNodeListsChildrenChanged();
    }
}

bool NodeImpl::dispatchSubtreeModifiedEvent(bool sendChildrenChanged)
{
    assert(!eventDispatchForbidden());

    // FIXME: Pull this whole if clause out of this function.
    if (sendChildrenChanged) {
        notifyNodeListsChildrenChanged();
        childrenChanged();
    } else
        notifyNodeListsAttributeChanged(); // FIXME: Can do better some day. Really only care about the name attribute changing.

    if (!getDocument()->hasListenerType(DocumentImpl::DOMSUBTREEMODIFIED_LISTENER))
	return false;
    int exceptioncode = 0;
    return dispatchEvent(new MutationEventImpl(DOMSubtreeModifiedEvent,
			 true,false,0,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
}

bool NodeImpl::dispatchKeyEvent(QKeyEvent *key)
{
    assert(!eventDispatchForbidden());
    int exceptioncode = 0;
    //kdDebug(6010) << "DOM::NodeImpl: dispatching keyboard event" << endl;
    KeyboardEventImpl *keyboardEventImpl = new KeyboardEventImpl(key, getDocument()->defaultView());
    keyboardEventImpl->ref();
    bool r = dispatchEvent(keyboardEventImpl,exceptioncode,true);

    // we want to return false if default is prevented (already taken care of)
    // or if the element is default-handled by the DOM. Otherwise we let it just
    // let it get handled by AppKit 
    if (keyboardEventImpl->defaultHandled())
      r = false;

    keyboardEventImpl->deref();
    return r;

}

void NodeImpl::dispatchWheelEvent(QWheelEvent *e)
{
    assert(!eventDispatchForbidden());
    if (e->delta() == 0)
        return;

    DocumentImpl *doc = getDocument();
    if (!doc)
        return;

    KHTMLView *view = getDocument()->view();
    if (!view)
        return;

    int x;
    int y;
    view->viewportToContents(e->x(), e->y(), x, y);

    int state = e->state();

    WheelEventImpl *we = new WheelEventImpl(e->orientation() == Qt::Horizontal, e->delta(),
        getDocument()->defaultView(), e->globalX(), e->globalY(), x, y,
        state & Qt::ControlButton, state & Qt::AltButton, state & Qt::ShiftButton, state & Qt::MetaButton);

    int exceptionCode = 0;
    if (!dispatchEvent(we, exceptionCode, true))
        e->accept();
}

void NodeImpl::handleLocalEvents(EventImpl *evt, bool useCapture)
{
    if (!m_regdListeners)
        return;

    if (disabled() && evt->isMouseEvent())
        return;

    QPtrList<RegisteredEventListener> listenersCopy = *m_regdListeners;
    QPtrListIterator<RegisteredEventListener> it(listenersCopy);
    for (; it.current(); ++it)
        if (it.current()->eventType() == evt->type() && it.current()->useCapture() == useCapture)
            it.current()->listener()->handleEventImpl(evt, false);
}

void NodeImpl::defaultEventHandler(EventImpl *evt)
{
}

unsigned NodeImpl::childNodeCount() const
{
    return 0;
}

NodeImpl *NodeImpl::childNode(unsigned /*index*/)
{
    return 0;
}

NodeImpl *NodeImpl::traverseNextNode(const NodeImpl *stayWithin) const
{
    if (firstChild()) {
        assert(!stayWithin || firstChild()->isAncestor(stayWithin));
	return firstChild();
    }
    if (this == stayWithin)
        return 0;
    if (nextSibling()) {
        assert(!stayWithin || nextSibling()->isAncestor(stayWithin));
	return nextSibling();
    }
    const NodeImpl *n = this;
    while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n) {
        assert(!stayWithin || !n->nextSibling() || n->nextSibling()->isAncestor(stayWithin));
        return n->nextSibling();
    }
    return 0;
}

NodeImpl *NodeImpl::traverseNextSibling(const NodeImpl *stayWithin) const
{
    if (this == stayWithin)
        return 0;
    if (nextSibling()) {
        assert(!stayWithin || nextSibling()->isAncestor(stayWithin));
	return nextSibling();
    }
    const NodeImpl *n = this;
    while (n && !n->nextSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n) {
        assert(!stayWithin || !n->nextSibling() || n->nextSibling()->isAncestor(stayWithin));
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

NodeImpl *NodeImpl::traversePreviousNodePostOrder(const NodeImpl *stayWithin) const
{
    if (lastChild()) {
        assert(!stayWithin || lastChild()->isAncestor(stayWithin));
	return lastChild();
    }
    if (this == stayWithin)
        return 0;
    if (previousSibling()) {
        assert(!stayWithin || previousSibling()->isAncestor(stayWithin));
	return previousSibling();
    }
    const NodeImpl *n = this;
    while (n && !n->previousSibling() && (!stayWithin || n->parentNode() != stayWithin))
        n = n->parentNode();
    if (n) {
        assert(!stayWithin || !n->previousSibling() || n->previousSibling()->isAncestor(stayWithin));
        return n->previousSibling();
    }
    return 0;
}

void NodeImpl::checkSetPrefix(const AtomicString &_prefix, int &exceptioncode)
{
    // Perform error checking as required by spec for setting Node.prefix. Used by
    // ElementImpl::setPrefix() and AttrImpl::setPrefix()

    // FIXME: Implement support for INVALID_CHARACTER_ERR: Raised if the specified prefix contains an illegal character.
    
    // NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // FIXME: Implement NAMESPACE_ERR: - Raised if the specified prefix is malformed
    // We have to comment this out, since it's used for attributes and tag names, and we've only
    // switched one over.
    /*
    // - if the namespaceURI of this node is null,
    // - if the specified prefix is "xml" and the namespaceURI of this node is different from
    //   "http://www.w3.org/XML/1998/namespace",
    // - if this node is an attribute and the specified prefix is "xmlns" and
    //   the namespaceURI of this node is different from "http://www.w3.org/2000/xmlns/",
    // - or if this node is an attribute and the qualifiedName of this node is "xmlns" [Namespaces].
    if ((namespacePart(id()) == noNamespace && id() > ID_LAST_TAG) ||
        (_prefix == "xml" && DOMString(getDocument()->namespaceURI(id())) != "http://www.w3.org/XML/1998/namespace")) {
        exceptioncode = DOMException::NAMESPACE_ERR;
        return;
    }*/
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
    if (newChild == this || isAncestor(newChild)) {
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return;
    }
    
    if (newChild->nodeType() != Node::DOCUMENT_FRAGMENT_NODE) {
        if (!childTypeAllowed(newChild->nodeType())) {
            exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
            return;
        }
    }
    else {
        for (NodeImpl *n = newChild->firstChild(); n; n = n->nextSibling()) {
            if (!childTypeAllowed(n->nodeType())) {
                exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
                return;
            }
        }
    }
    
    // change the document pointer of newChild and all of its children to be the new document
    if (shouldAdoptChild) {
        for (NodeImpl* node = newChild; node; node = node->traverseNextNode(newChild)) {
            KJS::ScriptInterpreter::updateDOMNodeDocument(node, node->getDocument(), getDocument());
            node->setDocument(getDocument());
        }
    }
}

bool NodeImpl::isAncestor(const NodeImpl *other) const
{
    // Return true if other is an ancestor of this, otherwise false
    if (!other)
        return false;
    for (const NodeImpl *n = parentNode(); n; n = n->parentNode()) {
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
	*stream << ind << child->nodeName().qstring().ascii() << ": ";
        child->dump(stream,ind+"  ");
        child = child->nextSibling();
    }
}
#endif

void NodeImpl::attach()
{
    assert(!attached());
    assert(!m_render || (m_render->style() && m_render->parent()));
    getDocument()->incDOMTreeVersion();
    m_attached = true;
}

void NodeImpl::willRemove()
{
}

void NodeImpl::detach()
{
//    assert(m_attached);

    if (m_render)
        m_render->destroy();
    m_render = 0;

    DocumentImpl *doc = getDocument();
    if (doc)
        doc->incDOMTreeVersion();
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
    if (m_regdListeners && !m_regdListeners->isEmpty() && getDocument())
        getDocument()->unregisterDisconnectedNodeWithEventListeners(this);

    setInDocument(true);
    
    insertedIntoTree(false);
}

void NodeImpl::removedFromDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty() && getDocument())
        getDocument()->registerDisconnectedNodeWithEventListeners(this);

    setInDocument(false);
    
    removedFromTree(false);
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

NodeImpl *NodeImpl::previousEditable() const
{
    NodeImpl *node = previousLeafNode();
    while (node) {
        if (node->isContentEditable())
            return node;
        node = node->previousLeafNode();
    }
    return 0;
}

NodeImpl *NodeImpl::nextEditable() const
{
    NodeImpl *node = nextLeafNode();
    while (node) {
        if (node->isContentEditable())
            return node;
        node = node->nextLeafNode();
    }
    return 0;
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
    // Avoid an O(n^2) problem with this function by not checking for nextRenderer() when the parent element hasn't even 
    // been attached yet.
    if (parent() && !parent()->attached())
        return 0;

    for (NodeImpl *n = nextSibling(); n; n = n->nextSibling()) {
        if (n->renderer())
            return n->renderer();
    }
    return 0;
}

// FIXME: This code is used by editing.  Seems like it could move over there and not pollute NodeImpl.
bool NodeImpl::isAtomicNode() const
{
    return !hasChildNodes() || (renderer() && renderer()->isWidget());
}

NodeImpl *NodeImpl::previousNodeConsideringAtomicNodes() const
{
    if (previousSibling()) {
        NodeImpl *n = previousSibling();
        while (!n->isAtomicNode() && n->lastChild())
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

NodeImpl *NodeImpl::nextNodeConsideringAtomicNodes() const
{
    if (!isAtomicNode() && firstChild())
	return firstChild();
    if (nextSibling())
	return nextSibling();
    const NodeImpl *n = this;
    while (n && !n->nextSibling())
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return 0;
}

NodeImpl *NodeImpl::previousLeafNode() const
{
    NodeImpl *node = previousNodeConsideringAtomicNodes();
    while (node) {
        if (node->isAtomicNode())
            return node;
        node = node->previousNodeConsideringAtomicNodes();
    }
    return 0;
}

NodeImpl *NodeImpl::nextLeafNode() const
{
    NodeImpl *node = nextNodeConsideringAtomicNodes();
    while (node) {
        if (node->isAtomicNode())
            return node;
        node = node->nextNodeConsideringAtomicNodes();
    }
    return 0;
}

void NodeImpl::createRendererIfNeeded()
{

    if (!getDocument()->shouldCreateRenderers())
        return;
        
    assert(!attached());
    assert(!m_render);
    
    NodeImpl *parent = parentNode();    
    assert(parent);
        
    RenderObject *parentRenderer = parent->renderer();
    if (parentRenderer && parentRenderer->canHaveChildren()
#if SVG_SUPPORT
        && parent->childShouldCreateRenderer(this)
#endif
        ) {
        RenderStyle *style = styleForRenderer(parentRenderer);
        style->ref();
#ifndef KHTML_NO_XBL
        bool resolveStyle = false;
        if (getDocument()->bindingManager()->loadBindings(this, style->bindingURIs(), true, &resolveStyle) && 
            rendererIsNeeded(style)) {
            if (resolveStyle) {
                style->deref();
                style = styleForRenderer(parentRenderer);
            }
#else
        if (rendererIsNeeded(style)) {
#endif
            m_render = createRenderer(getDocument()->renderArena(), style);
            if (m_render) {
                m_render->setStyle(style);
                parentRenderer->addChild(m_render, nextRenderer());
            }
#ifndef KHTML_NO_XBL
        } // avoid confusing the change log code parser by having two close braces to match the two open braces above
#else
        }
#endif
        style->deref(getDocument()->renderArena());
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

int NodeImpl::maxOffset() const
{
    return 1;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks questions about HTML in the core DOM class
// is obviously misplaced.
// method for editing madness, which allows BR,1 as a position, though that is incorrect
int NodeImpl::maxDeepOffset() const
{
    if (isTextNode())
        return static_cast<const TextImpl*>(this)->length();
        
    if (hasTagName(brTag) || (renderer() && renderer()->isReplaced()))
        return 1;

    return childNodeCount();
}

int NodeImpl::caretMinOffset() const
{
    return renderer() ? renderer()->caretMinOffset() : 0;
}

int NodeImpl::caretMaxOffset() const
{
    return renderer() ? renderer()->caretMaxOffset() : 1;
}

unsigned NodeImpl::caretMaxRenderedOffset() const
{
    return renderer() ? renderer()->caretMaxRenderedOffset() : 1;
}

int NodeImpl::previousOffset (int current) const
{
    return renderer() ? renderer()->previousOffset(current) : current - 1;
}

int NodeImpl::nextOffset (int current) const
{
    return renderer() ? renderer()->nextOffset(current) : current + 1;
}

bool NodeImpl::isBlockFlow() const
{
    return renderer() && renderer()->isBlockFlow();
}

bool NodeImpl::isBlockFlowOrTable() const
{
    return renderer() && (renderer()->isBlockFlow() || renderer()->isTable());
}

bool NodeImpl::isEditableBlock() const
{
    return isContentEditable() && isBlockFlow();
}

ElementImpl *NodeImpl::enclosingBlockFlowOrTableElement() const
{
    NodeImpl *n = const_cast<NodeImpl *>(this);
    if (isBlockFlowOrTable())
        return static_cast<ElementImpl *>(n);

    while (1) {
        n = n->parentNode();
        if (!n)
            break;
        if (n->isBlockFlowOrTable() || n->hasTagName(bodyTag))
            return static_cast<ElementImpl *>(n);
    }
    return 0;
}

ElementImpl *NodeImpl::enclosingBlockFlowElement() const
{
    NodeImpl *n = const_cast<NodeImpl *>(this);
    if (isBlockFlow())
        return static_cast<ElementImpl *>(n);

    while (1) {
        n = n->parentNode();
        if (!n)
            break;
        if (n->isBlockFlow() || n->hasTagName(bodyTag))
            return static_cast<ElementImpl *>(n);
    }
    return 0;
}

ElementImpl *NodeImpl::enclosingInlineElement() const
{
    NodeImpl *n = const_cast<NodeImpl *>(this);
    NodeImpl *p;

    while (1) {
        p = n->parentNode();
        if (!p || p->isBlockFlow() || p->hasTagName(bodyTag))
            return static_cast<ElementImpl *>(n);
        // Also stop if any previous sibling is a block
        for (NodeImpl *sibling = n->previousSibling(); sibling; sibling = sibling->previousSibling()) {
            if (sibling->isBlockFlow())
                return static_cast<ElementImpl *>(n);
        }
        n = p;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

ElementImpl *NodeImpl::rootEditableElement() const
{
    if (!isContentEditable())
        return 0;

    NodeImpl *n = const_cast<NodeImpl *>(this);
    if (n->hasTagName(bodyTag))
        return static_cast<ElementImpl *>(n);

    NodeImpl *result = n->isEditableBlock() ? n : 0;
    while (1) {
        n = n->parentNode();
        if (!n || !n->isContentEditable())
            break;
        if (n->hasTagName(bodyTag)) {
            result = n;
            break;
        }
        if (n->isBlockFlow())
            result = n;
    }
    return static_cast<ElementImpl *>(result);
}

bool NodeImpl::inSameRootEditableElement(NodeImpl *n)
{
    return n ? rootEditableElement() == n->rootEditableElement() : false;
}

bool NodeImpl::inSameContainingBlockFlowElement(NodeImpl *n)
{
    return n ? enclosingBlockFlowElement() == n->enclosingBlockFlowElement() : false;
}

// FIXME: End of obviously misplaced HTML editing functions.  Try to move these out of NodeImpl.

SharedPtr<NodeListImpl> NodeImpl::getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName)
{
    if (namespaceURI.isNull() || localName.isNull())
        return SharedPtr<NodeListImpl>(); // FIXME: Who cares about this additional check?
    
    DOMString name = localName;
    if (getDocument()->isHTMLDocument())
        name = localName.lower();
    return SharedPtr<NodeListImpl>(new TagNodeListImpl(this, AtomicString(namespaceURI), AtomicString(name)));
}

bool NodeImpl::isSupported(const DOMString &feature, const DOMString &version)
{
    return DOMImplementationImpl::instance()->hasFeature(feature, version);
}

DocumentImpl *NodeImpl::ownerDocument() const
{
    DocumentImpl *doc = getDocument();
    return doc == this ? 0 : doc;
}

bool NodeImpl::hasAttributes() const
{
    return false;
}

NamedAttrMapImpl *NodeImpl::attributes() const
{
    return 0;
}

bool NodeImpl::isEqualNode(NodeImpl *other) const
{
    if (!other)
        return false;
    
    if (nodeType() != other->nodeType())
        return false;
    
    if (nodeName() != other->nodeName())
        return false;
    
    if (localName() != other->localName())
        return false;
    
    if (namespaceURI() != other->namespaceURI())
        return false;
    
    if (prefix() != other->prefix())
        return false;
    
    if (nodeValue() != other->nodeValue())
        return false;
    
    NamedAttrMapImpl *attrs = attributes();
    NamedAttrMapImpl *otherAttrs = other->attributes();
    
    if (!attrs && otherAttrs)
        return false;
    
    if (attrs && !attrs->mapsEquivalent(otherAttrs))
        return false;
    
    NodeImpl *child = firstChild();
    NodeImpl *otherChild = other->firstChild();
    
    while (child) {
        if (!child->isEqualNode(otherChild))
            return false;
        
        child = child->nextSibling();
        otherChild = otherChild->nextSibling();
    }
    
    if (otherChild)
        return false;
    
    // FIXME: For DocumentType nodes we should check equality on
    // the entities and notations NamedNodeMaps as well.
    
    return true;
}

bool NodeImpl::isDefaultNamespace(const DOMString &namespaceURI) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#isDefaultNamespaceAlgo
    
    switch (nodeType()) {
        case Node::ELEMENT_NODE: {
            const ElementImpl *elem = static_cast<const ElementImpl *>(this);
            
            if (elem->prefix().isNull())
                return elem->namespaceURI() == namespaceURI;

            if (elem->hasAttributes()) {
                NamedAttrMapImpl *attrs = elem->attributes();
                
                for (unsigned i = 0; i < attrs->length(); i++) {
                    AttributeImpl *attr = attrs->attributeItem(i);
                    
                    if (attr->localName() == "xmlns")
                        return attr->value() == namespaceURI;
                }
            }
            
            if (ElementImpl *ancestor = getAncestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);

            return false;
        }
        case Node::DOCUMENT_NODE:
            return static_cast <const DocumentImpl *>(this)->documentElement()->isDefaultNamespace(namespaceURI);
        case Node::ENTITY_NODE:
        case Node::NOTATION_NODE:
        case Node::DOCUMENT_TYPE_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
            return false;
        case Node::ATTRIBUTE_NODE: {
            const AttrImpl *attr = static_cast<const AttrImpl *>(this);
            
            if (attr->ownerElement())
                return attr->ownerElement()->isDefaultNamespace(namespaceURI);
            
            return false;
        }
        default:
            if (ElementImpl *ancestor = getAncestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);
            
            return false;
    }
}

DOMString NodeImpl::lookupPrefix(const DOMString &namespaceURI) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespacePrefixAlgo
    
    if (namespaceURI.isEmpty())
        return DOMString();
    
    switch (nodeType()) {
        case Node::ELEMENT_NODE:
            return lookupNamespacePrefix(namespaceURI, static_cast<const ElementImpl *>(this));
        case Node::DOCUMENT_NODE:
            return static_cast<const DocumentImpl *>(this)->documentElement()->lookupPrefix(namespaceURI);
        case Node::ENTITY_NODE:
        case Node::NOTATION_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
        case Node::DOCUMENT_TYPE_NODE:
            return DOMString();
        case Node::ATTRIBUTE_NODE: {
            const AttrImpl *attr = static_cast<const AttrImpl *>(this);
            
            if (attr->ownerElement())
                return attr->ownerElement()->lookupPrefix(namespaceURI);
            
            return DOMString();
        }
        default:
            if (ElementImpl *ancestor = getAncestorElement())
                return ancestor->lookupPrefix(namespaceURI);
            
            return DOMString();
    }
}

DOMString NodeImpl::lookupNamespaceURI(const DOMString &prefix) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespaceURIAlgo
    
    if (!prefix.isNull() && prefix.isEmpty())
        return DOMString();
    
    switch (nodeType()) {
        case Node::ELEMENT_NODE: {
            const ElementImpl *elem = static_cast<const ElementImpl *>(this);
            
            if (!elem->namespaceURI().isNull() && elem->prefix() == prefix)
                return elem->namespaceURI();
            
            if (elem->hasAttributes()) {
                NamedAttrMapImpl *attrs = elem->attributes();
                
                for (unsigned i = 0; i < attrs->length(); i++) {
                    AttributeImpl *attr = attrs->attributeItem(i);
                    
                    if (attr->prefix() == "xmlns" && attr->localName() == prefix) {
                        if (!attr->value().isEmpty())
                            return attr->value();
                        
                        return DOMString();
                    } else if (attr->localName() == "xmlns" && prefix.isNull()) {
                        if (!attr->value().isEmpty())
                            return attr->value();
                        
                        return DOMString();
                    }
                }
            }
            
            if (ElementImpl *ancestor = getAncestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            
            return DOMString();
        }
        case Node::DOCUMENT_NODE:
            return static_cast<const DocumentImpl *>(this)->documentElement()->lookupNamespaceURI(prefix);
        case Node::ENTITY_NODE:
        case Node::NOTATION_NODE:
        case Node::DOCUMENT_TYPE_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
            return DOMString();
        case Node::ATTRIBUTE_NODE: {
            const AttrImpl *attr = static_cast<const AttrImpl *>(this);
            
            if (attr->ownerElement())
                return attr->ownerElement()->lookupNamespaceURI(prefix);
            else
                return DOMString();
        }
        default:
            if (ElementImpl *ancestor = getAncestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            
            return DOMString();
    }
}

DOMString NodeImpl::lookupNamespacePrefix(const DOMString &_namespaceURI, const ElementImpl *originalElement) const
{
    if (_namespaceURI.isNull())
        return DOMString();
            
    if (originalElement->lookupNamespaceURI(prefix()) == _namespaceURI)
        return prefix();
    
    if (hasAttributes()) {
        NamedAttrMapImpl *attrs = attributes();
        
        for (unsigned i = 0; i < attrs->length(); i++) {
            AttributeImpl *attr = attrs->attributeItem(i);
            
            if (attr->prefix() == "xmlns" &&
                attr->value() == _namespaceURI &&
                originalElement->lookupNamespaceURI(attr->localName()) == _namespaceURI)
                return attr->localName();
        }
    }
    
    if (ElementImpl *ancestor = getAncestorElement())
        return ancestor->lookupNamespacePrefix(_namespaceURI, originalElement);
    
    return DOMString();
}

DOMString NodeImpl::textContent() const
{
    switch (nodeType()) {
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        case Node::COMMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
            return nodeValue();
        
        case Node::ELEMENT_NODE:
        case Node::ATTRIBUTE_NODE:
        case Node::ENTITY_NODE:
        case Node::ENTITY_REFERENCE_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE: {
            DOMString s = "";
        
            for (NodeImpl *child = firstChild(); child; child = child->nextSibling()) {
                if (child->nodeType() == Node::COMMENT_NODE || 
                    child->nodeType() == Node::PROCESSING_INSTRUCTION_NODE)
                    continue;
            
                s += child->textContent();
            }
        
            return s;
        }
        
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_TYPE_NODE:
        case Node::NOTATION_NODE:
        default:
            return DOMString();            
    }
}

void NodeImpl::setTextContent(const DOMString &text, int &exception)
{           
    switch (nodeType()) {
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        case Node::COMMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
            setNodeValue(text, exception);
            break;
        case Node::ELEMENT_NODE:
        case Node::ATTRIBUTE_NODE:
        case Node::ENTITY_NODE:
        case Node::ENTITY_REFERENCE_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE: {
            ContainerNodeImpl *container = static_cast<ContainerNodeImpl *>(this);
            
            container->removeChildren();
            
            if (!text.isEmpty())
                appendChild(getDocument()->createTextNode(text), exception);
            break;
        }
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_TYPE_NODE:
        case Node::NOTATION_NODE:
        default:
            // Do nothing
            break;
    }
}

ElementImpl *NodeImpl::getAncestorElement() const
{
    // In theory, there can be EntityReference nodes between elements
    // but this is currently not supported.
    for (NodeImpl *n = parentNode(); n; n = n->parentNode()) {
        if (n->isElementNode())
            return static_cast<ElementImpl *>(n);
    }
    
    return 0;
}

#ifndef NDEBUG

static void appendAttributeDesc(const NodeImpl *node, QString &string, const QualifiedName& name, QString attrDesc)
{
    if (node->isElementNode()) {
        DOMString attr = static_cast<const ElementImpl *>(node)->getAttribute(name);
        if (!attr.isEmpty()) {
            string += attrDesc;
            string += attr.qstring();
        }
    }
}

void NodeImpl::showNode(const char *prefix) const
{
    if (!prefix)
        prefix = "";
    if (isTextNode()) {
        QString value = nodeValue().qstring();
        value.replace('\\', "\\\\");
        value.replace('\n', "\\n");
        fprintf(stderr, "%s%s\t%p \"%s\"\n", prefix, nodeName().qstring().local8Bit().data(), this, value.local8Bit().data());
    } else {
        QString attrs = "";
        appendAttributeDesc(this, attrs, classAttr, " CLASS=");
        appendAttributeDesc(this, attrs, styleAttr, " STYLE=");
        fprintf(stderr, "%s%s\t%p%s\n", prefix, nodeName().qstring().local8Bit().data(), this, attrs.ascii());
    }
}

void NodeImpl::showTree() const
{
    showTreeAndMark((NodeImpl *)this, "*", NULL, NULL);
}

void NodeImpl::showTreeAndMark(NodeImpl * markedNode1, const char * markedLabel1, NodeImpl * markedNode2, const char * markedLabel2) const
{
    NodeImpl *rootNode;
    NodeImpl *node = (NodeImpl *)this;
    while (node->parentNode() && !node->hasTagName(bodyTag))
        node = node->parentNode();
    rootNode = node;
	
    for (node = rootNode; node; node = node->traverseNextNode()) {
        NodeImpl *tmpNode;
		
        if (node == markedNode1)
            fprintf(stderr, "%s", markedLabel1);
        if (node == markedNode2)
            fprintf(stderr, "%s", markedLabel2);
			
        for (tmpNode = node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentNode())
            fprintf(stderr, "\t");
        node->showNode(0);
    }
}

void NodeImpl::formatForDebugger(char *buffer, unsigned length) const
{
    DOMString result;
    DOMString s;
    
    s = nodeName();
    if (s.length() == 0)
        result += "<none>";
    else
        result += s;
          
    strncpy(buffer, result.qstring().latin1(), length - 1);
}

#endif

//-------------------------------------------------------------------------

ContainerNodeImpl::ContainerNodeImpl(DocumentImpl *doc)
    : NodeImpl(doc)
{
    _first = _last = 0;
}

void ContainerNodeImpl::removeAllChildren()
{
    //kdDebug( 6020 ) << "ContainerNodeImpl destructor" << endl;

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
        } else if (n->inDocument())
            n->removedFromDocument();
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
        _first = 0;
        _last = 0;
    }
}

ContainerNodeImpl::~ContainerNodeImpl()
{
    removeAllChildren();
}


NodeImpl *ContainerNodeImpl::firstChild() const
{
    return _first;
}

NodeImpl *ContainerNodeImpl::lastChild() const
{
    return _last;
}

NodeImpl *ContainerNodeImpl::insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode )
{
    exceptioncode = 0;

    // insertBefore(...,null) is equivalent to appendChild()
    if(!refChild)
        return appendChild(newChild, exceptioncode);

    SharedPtr<NodeImpl> protectNewChild(newChild); // make sure the new child is ref'd and deref'd so we don't leak it

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
        return (newChild->hasOneRef() && !newChild->parent()) ? 0 : newChild;

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
        if (newParent)
            newParent->removeChild( child, exceptioncode );
        if (exceptioncode)
            return 0;

        // Add child in the correct position
        forbidEventDispatch();
        if (prev)
            prev->setNextSibling(child);
        else
            _first = child;
        refChild->setPreviousSibling(child);
        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(refChild);
        allowEventDispatch();

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

NodeImpl *ContainerNodeImpl::replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode )
{
    exceptioncode = 0;

    SharedPtr<NodeImpl> protectNewChild(newChild); // make sure the new child is ref'd and deref'd so we don't leak it

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
        if (newParent)
            newParent->removeChild( child, exceptioncode );
        if (exceptioncode)
            return 0;

        // Add child in the correct position
        forbidEventDispatch();
        if (prev) prev->setNextSibling(child);
        if (next) next->setPreviousSibling(child);
        if(!prev) _first = child;
        if(!next) _last = child;
        child->setParent(this);
        child->setPreviousSibling(prev);
        child->setNextSibling(next);
        allowEventDispatch();

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

void ContainerNodeImpl::willRemove()
{
    for (NodeImpl *n = _first; n != 0; n = n->nextSibling()) {
        n->willRemove();
    }
}

int ContainerNodeImpl::willRemoveChild(NodeImpl *child)
{
    int exceptionCode = 0;

    // fire removed from document mutation events.
    dispatchChildRemovalEvents(child, exceptionCode);
    if (exceptionCode)
        return exceptionCode;

    if (child->attached())
        child->willRemove();
    
    return 0;
}

NodeImpl *ContainerNodeImpl::removeChild ( NodeImpl *oldChild, int &exceptioncode )
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
    
    // update auxiliary doc info (e.g. iterators) to note that node is being removed
    // FIX: This looks redundant with same call from dispatchChildRemovalEvents in willRemoveChild
//  getDocument()->notifyBeforeNodeRemoval(oldChild); // ### use events instead

    // dispatch pre-removal mutation events
    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVED_LISTENER)) {
	oldChild->dispatchEvent(new MutationEventImpl(DOMNodeRemovedEvent,
			     true,false,this,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
	if (exceptioncode)
	    return 0;
    }

    exceptioncode = willRemoveChild(oldChild);
    if (exceptioncode)
        return 0;

    forbidEventDispatch();

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

    allowEventDispatch();

    getDocument()->setDocumentChanged(true);

    // Dispatch post-removal mutation events
    dispatchSubtreeModifiedEvent();

    if (oldChild->inDocument())
        oldChild->removedFromDocument();
    else
        oldChild->removedFromTree(true);

    return oldChild;
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNodeImpl::removeChildren()
{
    NodeImpl *n;
    
    if (!_first)
        return;

    // do any prep work needed before actually starting to detach
    // and remove... e.g. stop loading frames, fire unload events
    for (n = _first; n != 0; n = n->nextSibling())
        willRemoveChild(n);
    
    forbidEventDispatch();
    while ((n = _first) != 0) {
        NodeImpl *next = n->nextSibling();
        
        n->ref();

        if (n->attached())
            n->detach();
        n->setPreviousSibling(0);
        n->setNextSibling(0);
        n->setParent(0);
        
        if (n->inDocument())
            n->removedFromDocument();

        n->deref();

        _first = next;
    }
    _last = 0;
    allowEventDispatch();
    
    // Dispatch a single post-removal mutation event denoting a modified subtree.
    dispatchSubtreeModifiedEvent();
}


NodeImpl *ContainerNodeImpl::appendChild ( NodeImpl *newChild, int &exceptioncode )
{
    exceptioncode = 0;

    SharedPtr<NodeImpl> protectNewChild(newChild); // make sure the new child is ref'd and deref'd so we don't leak it

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
        forbidEventDispatch();
        child->setParent(this);
        if (_last) {
            child->setPreviousSibling(_last);
            _last->setNextSibling(child);
            _last = child;
        } else
            _first = _last = child;
        allowEventDispatch();

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

bool ContainerNodeImpl::hasChildNodes (  ) const
{
    return _first != 0;
}

// not part of the DOM
void ContainerNodeImpl::setFirstChild(NodeImpl *child)
{
    _first = child;
}

void ContainerNodeImpl::setLastChild(NodeImpl *child)
{
    _last = child;
}

// check for same source document:
bool ContainerNodeImpl::checkSameDocument( NodeImpl *newChild, int &exceptioncode )
{
    exceptioncode = 0;
    DocumentImpl *ownerDocThis = getDocument();
    // FIXME: Doh! This next line isn't getting newChild, so it's never going to work!
    DocumentImpl *ownerDocNew = getDocument();
    if(ownerDocThis != ownerDocNew) {
        kdDebug(6010)<< "not same document, newChild = " << newChild << "document = " << getDocument() << endl;
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return true;
    }
    return false;
}

// check for being child:
bool ContainerNodeImpl::checkIsChild( NodeImpl *oldChild, int &exceptioncode )
{
    if(!oldChild || oldChild->parentNode() != this) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return true;
    }
    return false;
}

NodeImpl *ContainerNodeImpl::addChild(NodeImpl *newChild)
{
    // do not add applyChanges here! This function is only used during parsing

    SharedPtr<NodeImpl> protectNewChild(newChild); // make sure the new child is ref'd and deref'd so we don't leak it

    // short check for consistency with DTD
    if (getDocument()->isHTMLDocument() && !childAllowed(newChild))
        return 0;

    // just add it...
    forbidEventDispatch();
    newChild->setParent(this);
    if(_last) {
        newChild->setPreviousSibling(_last);
        _last->setNextSibling(newChild);
        _last = newChild;
    } else
        _first = _last = newChild;
    allowEventDispatch();

    if (inDocument())
        newChild->insertedIntoDocument();
    childrenChanged();
    
    if(newChild->nodeType() == Node::ELEMENT_NODE)
        return newChild;
    return this;
}

void ContainerNodeImpl::attach()
{
    NodeImpl *child = _first;
    while(child != 0)
    {
        child->attach();
        child = child->nextSibling();
    }
    NodeImpl::attach();
}

void ContainerNodeImpl::detach()
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

void ContainerNodeImpl::insertedIntoDocument()
{
    NodeImpl::insertedIntoDocument();
    for (NodeImpl *child = _first; child; child = child->nextSibling())
        child->insertedIntoDocument();
}

void ContainerNodeImpl::removedFromDocument()
{
    NodeImpl::removedFromDocument();
    for (NodeImpl *child = _first; child; child = child->nextSibling())
        child->removedFromDocument();
}

void ContainerNodeImpl::insertedIntoTree(bool deep)
{
    NodeImpl::insertedIntoTree(deep);
    if (deep) {
        for (NodeImpl *child = _first; child; child = child->nextSibling())
            child->insertedIntoTree(deep);
    }
}

void ContainerNodeImpl::removedFromTree(bool deep)
{
    NodeImpl::removedFromTree(deep);
    if (deep) {
        for (NodeImpl *child = _first; child; child = child->nextSibling())
            child->removedFromTree(deep);
    }
}

void ContainerNodeImpl::cloneChildNodes(NodeImpl *clone)
{
    int exceptioncode = 0;
    NodeImpl *n;
    for(n = firstChild(); n && !exceptioncode; n = n->nextSibling())
    {
        clone->appendChild(n->cloneNode(true),exceptioncode);
    }
}

bool ContainerNodeImpl::getUpperLeftCorner(int &xPos, int &yPos) const
{
    if (!m_render)
        return false;
    RenderObject *o = m_render;
    RenderObject *p = o;

    xPos = yPos = 0;
    if (!o->isInline() || o->isReplaced()) {
        o->absolutePosition( xPos, yPos );
        return true;
    }

    // find the next text/image child, to get a position
    while(o) {
        p = o;
        if(o->firstChild())
            o = o->firstChild();
        else if(o->nextSibling())
            o = o->nextSibling();
        else {
            o = o->parent();
            if (o) 
                o = o->nextSibling();
            if (!o)
                break;
        }

        if (!o->isInline() || o->isReplaced()) {
            o->absolutePosition( xPos, yPos );
            return true;
        }

        if (p->element() && p->element() == this && o->isText() && !o->isBR() && !static_cast<RenderText*>(o)->firstTextBox()) {
                // do nothing - skip unrendered whitespace that is a child or next sibling of the anchor
        }
        else if((o->isText() && !o->isBR()) || o->isReplaced()) {
            o->container()->absolutePosition( xPos, yPos );
            if (o->isText() && static_cast<RenderText *>(o)->firstTextBox()) {
                xPos += static_cast<RenderText *>(o)->minXPos();
                yPos += static_cast<RenderText *>(o)->firstTextBox()->root()->topOverflow();
            }
            else {
                xPos += o->xPos();
                yPos += o->yPos();
            }
            return true;
        }
    }
    
    // If the target doesn't have any children or siblings that could be used to calculate the scroll position, we must be
    // at the end of the document.  Scroll to the bottom.
    if (!o) {
        yPos += getDocument()->view()->contentsHeight();
        return true;
    }
    return false;
}

bool ContainerNodeImpl::getLowerRightCorner(int &xPos, int &yPos) const
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

QRect ContainerNodeImpl::getRect() const
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
    if (xEnd < xPos)
        xEnd = xPos;
    if (yEnd < yPos)
        yEnd = yPos;
        
    return QRect(xPos, yPos, xEnd - xPos, yEnd - yPos);
}

void ContainerNodeImpl::setFocus(bool received)
{
    if (m_focused == received) return;

    NodeImpl::setFocus(received);

    // note that we need to recalc the style
    setChanged();
}

void ContainerNodeImpl::setActive(bool down, bool pause)
{
    if (down == active()) return;

    NodeImpl::setActive(down);

    // note that we need to recalc the style
    // FIXME: Move to ElementImpl
    if (m_render) {
        bool reactsToPress = m_render->style()->affectedByActiveRules();
        if (reactsToPress)
            setChanged();
        if (renderer() && renderer()->style()->hasAppearance()) {
            if (theme()->stateChanged(renderer(), PressedState))
                reactsToPress = true;
        }
        if (reactsToPress && pause) {
            // The delay here is subtle.  It relies on an assumption, namely that the amount of time it takes
            // to repaint the "down" state of the control is about the same time as it would take to repaint the
            // "up" state.  Once you assume this, you can just delay for 100ms - that time (assuming that after you
            // leave this method, it will be about that long before the flush of the up state happens again).
            QTime startTime;
            startTime.restart();

            // Do an immediate repaint.
            m_render->repaint(true);
            
            int remainingTime = 100 - startTime.elapsed();
            
            // Now pause for a small amount of time (1/10th of a second from before we repainted in the pressed state)
            if (remainingTime > 0)
                usleep(remainingTime * 1000);
        }
    }
}

void ContainerNodeImpl::setHovered(bool over)
{
    if (over == hovered()) return;

    NodeImpl::setHovered(over);

    // note that we need to recalc the style
    // FIXME: Move to ElementImpl
    if (m_render) {
        if (m_render->style()->affectedByHoverRules())
            setChanged();
        if (renderer() && renderer()->style()->hasAppearance())
            theme()->stateChanged(renderer(), HoverState);
    }
}

unsigned ContainerNodeImpl::childNodeCount() const
{
    unsigned count = 0;
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        count++;
    return count;
}

NodeImpl *ContainerNodeImpl::childNode(unsigned index)
{
    unsigned i;
    NodeImpl *n = firstChild();
    for (i = 0; n != 0 && i < index; i++)
        n = n->nextSibling();
    return n;
}

void ContainerNodeImpl::dispatchChildInsertedEvents( NodeImpl *child, int &exceptioncode )
{
    assert(!eventDispatchForbidden());
    if (inDocument())
        child->insertedIntoDocument();
    else
        child->insertedIntoTree(true);

    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEINSERTED_LISTENER)) {
        child->dispatchEvent(new MutationEventImpl(DOMNodeInsertedEvent,
                                                   true,false,this,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
        if (exceptioncode)
            return;
    }

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    bool hasInsertedListeners = getDocument()->hasListenerType(DocumentImpl::DOMNODEINSERTEDINTODOCUMENT_LISTENER);

    if (hasInsertedListeners && inDocument()) {
        for (NodeImpl *c = child; c; c = c->traverseNextNode(child)) {
            c->dispatchEvent(new MutationEventImpl(DOMNodeInsertedIntoDocumentEvent,
                                                   false,false,0,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
            if (exceptioncode)
                return;
        }
    }
}

// base class for nodes that may have children
void ContainerNodeImpl::dispatchChildRemovalEvents( NodeImpl *child, int &exceptioncode )
{
    // update auxiliary doc info (e.g. iterators) to note that node is being removed
    getDocument()->notifyBeforeNodeRemoval(child); // ### use events instead

    // dispatch pre-removal mutation events
    if (getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVED_LISTENER)) {
	child->dispatchEvent(new MutationEventImpl(DOMNodeRemovedEvent,
			     true,false,this,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
	if (exceptioncode)
	    return;
    }

    bool hasRemovalListeners = getDocument()->hasListenerType(DocumentImpl::DOMNODEREMOVEDFROMDOCUMENT_LISTENER);

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (inDocument()) {
	for (NodeImpl *c = child; c; c = c->traverseNextNode(child)) {
	    if (hasRemovalListeners) {
		c->dispatchEvent(new MutationEventImpl(DOMNodeRemovedFromDocumentEvent,
				 false,false,0,DOMString(),DOMString(),DOMString(),0),exceptioncode,true);
		if (exceptioncode)
		    return;
	    }
	}
    }
}

// ---------------------------------------------------------------------------


NodeListImpl::NodeListImpl(NodeImpl *_rootNode)
    : rootNode(_rootNode),
      isLengthCacheValid(false),
      isItemCacheValid(false)
{
    rootNode->ref();
    rootNode->registerNodeList(this);
}    

NodeListImpl::~NodeListImpl()
{
    rootNode->unregisterNodeList(this);
    rootNode->deref();
}

unsigned NodeListImpl::recursiveLength( NodeImpl *start ) const
{
    if (!start)
	start = rootNode;

    if (isLengthCacheValid && start == rootNode) {
        return cachedLength;
    }

    unsigned len = 0;

    for(NodeImpl *n = start->firstChild(); n != 0; n = n->nextSibling()) {
        if ( n->nodeType() == Node::ELEMENT_NODE ) {
            if (nodeMatches(n))
                len++;
            len+= recursiveLength(n);
        }
    }

    if (start == rootNode) {
        cachedLength = len;
        isLengthCacheValid = true;
    }

    return len;
}

NodeImpl *NodeListImpl::recursiveItem ( unsigned offset, NodeImpl *start) const
{
    int remainingOffset = offset;
    if (!start) {
        start = rootNode->firstChild();
        if (isItemCacheValid) {
            if (offset == lastItemOffset) {
                return lastItem;
            } else if (offset > lastItemOffset) {
                start = lastItem;
                remainingOffset -= lastItemOffset;
            }
        }
    }

    NodeImpl *n = start;

    while (n) {
        if ( n->nodeType() == Node::ELEMENT_NODE ) {
            if (nodeMatches(n)) {
                if (!remainingOffset) {
                    lastItem = n;
                    lastItemOffset = offset;
                    isItemCacheValid = 1;
                    return n;
                }
                remainingOffset--;
            }
        }

        n = n->traverseNextNode(rootNode);
    }

    return 0; // no matching node in this subtree
}

NodeImpl *NodeListImpl::itemById (const DOMString& elementId) const
{
    if (rootNode->isDocumentNode() || rootNode->inDocument()) {
        NodeImpl *node = rootNode->getDocument()->getElementById(elementId);

        if (node == NULL || !nodeMatches(node))
            return 0;

        for (NodeImpl *p = node->parentNode(); p; p = p->parentNode()) {
            if (p == rootNode)
                return node;
        }

        return 0;
    }

    unsigned l = length();

    for ( unsigned i = 0; i < l; i++ ) {
        NodeImpl *node = item(i);
        
        if ( static_cast<ElementImpl *>(node)->getIDAttribute() == elementId ) {
            return node;
        }
    }

    return 0;
}


void NodeListImpl::rootNodeChildrenChanged()
{
    isLengthCacheValid = false;
    isItemCacheValid = false;     
}

ChildNodeListImpl::ChildNodeListImpl( NodeImpl *n )
    : NodeListImpl(n)
{
}

unsigned ChildNodeListImpl::length() const
{
    unsigned len = 0;
    NodeImpl *n;
    for(n = rootNode->firstChild(); n != 0; n = n->nextSibling())
        len++;

    return len;
}

NodeImpl *ChildNodeListImpl::item ( unsigned index ) const
{
    unsigned int pos = 0;
    NodeImpl *n = rootNode->firstChild();

    while( n != 0 && pos < index )
    {
        n = n->nextSibling();
        pos++;
    }

    return n;
}

bool ChildNodeListImpl::nodeMatches(NodeImpl *testNode) const
{
    return testNode->parentNode() == rootNode;
}

TagNodeListImpl::TagNodeListImpl(NodeImpl *n, const AtomicString& namespaceURI, const AtomicString& localName)
    : NodeListImpl(n), 
      m_namespaceURI(namespaceURI), 
      m_localName(localName)
{
}

unsigned TagNodeListImpl::length() const
{
    return recursiveLength();
}

NodeImpl *TagNodeListImpl::item(unsigned index) const
{
    return recursiveItem(index);
}

bool TagNodeListImpl::nodeMatches(NodeImpl *testNode) const
{
    if (!testNode->isElementNode())
        return false;

    if (m_namespaceURI != starAtom && m_namespaceURI != testNode->namespaceURI())
        return false;
    
    return m_localName == starAtom || m_localName == testNode->localName();
}

NameNodeListImpl::NameNodeListImpl(NodeImpl *n, const DOMString &t )
  : NodeListImpl(n), nodeName(t)
{
}

unsigned NameNodeListImpl::length() const
{
    return recursiveLength();
}

NodeImpl *NameNodeListImpl::item ( unsigned index ) const
{
    return recursiveItem( index );
}

bool NameNodeListImpl::nodeMatches(NodeImpl *testNode) const
{
    return static_cast<ElementImpl *>(testNode)->getAttribute(nameAttr) == nodeName;
}

}
