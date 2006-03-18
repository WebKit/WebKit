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

#include "config.h"
#include "EventTargetNodeImpl.h"

#include "DocumentImpl.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "MouseEvent.h"
#include "WheelEvent.h"
#include "dom2_eventsimpl.h"
#include "kjs_proxy.h"
#include "htmlnames.h"
#include <qptrlist.h>
#include <qtextstream.h>

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

EventTargetNodeImpl::EventTargetNodeImpl(DocumentImpl *doc)
    : NodeImpl(doc)
    , m_regdListeners(0)
{
}

EventTargetNodeImpl::~EventTargetNodeImpl()
{
    if (m_regdListeners && !m_regdListeners->isEmpty() && !inDocument())
        getDocument()->unregisterDisconnectedNodeWithEventListeners(this);
    delete m_regdListeners;
}

void EventTargetNodeImpl::insertedIntoDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        getDocument()->unregisterDisconnectedNodeWithEventListeners(this);
    
    NodeImpl::insertedIntoDocument();
}

void EventTargetNodeImpl::removedFromDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        getDocument()->registerDisconnectedNodeWithEventListeners(this);

    NodeImpl::removedFromDocument();
}

void EventTargetNodeImpl::addEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener, const bool useCapture)
{
    if (!getDocument()->attached())
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
    
    // Remove existing identical listener set with identical arguments.
    // The DOM2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(eventType, listener.get(), useCapture);
    
    // adding the first one
    if (m_regdListeners->isEmpty() && !inDocument())
        getDocument()->registerDisconnectedNodeWithEventListeners(this);
    
    m_regdListeners->append(new RegisteredEventListener(eventType, listener.get(), useCapture));
}

void EventTargetNodeImpl::removeEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture)
{
    if (!m_regdListeners) // nothing to remove
        return;
    
    RegisteredEventListener rl(eventType, listener, useCapture);
    
    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (*(it.current()) == rl) {
            m_regdListeners->removeRef(it.current());
            // removed last
            if (m_regdListeners->isEmpty() && !inDocument())
                getDocument()->unregisterDisconnectedNodeWithEventListeners(this);
            return;
        }
}

void EventTargetNodeImpl::removeAllEventListeners()
{
    delete m_regdListeners;
    m_regdListeners = 0;
}

void EventTargetNodeImpl::handleLocalEvents(EventImpl *evt, bool useCapture)
{
    if (!m_regdListeners)
        return;
    
    if (disabled() && evt->isMouseEvent())
        return;
    
    QPtrList<RegisteredEventListener> listenersCopy = *m_regdListeners;
    QPtrListIterator<RegisteredEventListener> it(listenersCopy);
    for (; it.current(); ++it)
        if (it.current()->eventType() == evt->type() && it.current()->useCapture() == useCapture)
            it.current()->listener()->handleEvent(evt, false);
}

bool EventTargetNodeImpl::dispatchGenericEvent(PassRefPtr<EventImpl> e, ExceptionCode&, bool tempEvent)
{
    RefPtr<EventImpl> evt(e);
    assert(!eventDispatchForbidden());
    assert(evt->target());
    
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
    void* data = preDispatchEventHandler(evt.get());
    
    // trigger any capturing event handlers on our way down
    evt->setEventPhase(EventImpl::CAPTURING_PHASE);
    
    it.toFirst();
    // Handle window events for capture phase
    if (it.current()->isDocumentNode() && !evt->propagationStopped()) {
        static_cast<DocumentImpl*>(it.current())->handleWindowEvent(evt.get(), true);
    }  
    
    for (; it.current() && it.current() != this && !evt->propagationStopped(); ++it) {
        evt->setCurrentTarget(it.current());
        EventTargetNodeCast(it.current())->handleLocalEvents(evt.get(), true);
    }
    
    // dispatch to the actual target node
    it.toLast();
    if (!evt->propagationStopped()) {
        evt->setEventPhase(EventImpl::AT_TARGET);
        evt->setCurrentTarget(it.current());
        
        if (!evt->propagationStopped())
            EventTargetNodeCast(it.current())->handleLocalEvents(evt.get(), false);
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
        evt->setEventPhase(EventImpl::BUBBLING_PHASE);
        for (; it.current() && !evt->propagationStopped() && !evt->getCancelBubble(); --it) {
            evt->setCurrentTarget(it.current());
            EventTargetNodeCast(it.current())->handleLocalEvents(evt.get(), false);
        }
        // Handle window events for bubbling phase
        it.toFirst();
        if (it.current()->isDocumentNode() && !evt->propagationStopped() && !evt->getCancelBubble()) {
            evt->setCurrentTarget(it.current());
            static_cast<DocumentImpl*>(it.current())->handleWindowEvent(evt.get(), false);
        } 
    } 
    
    evt->setCurrentTarget(0);
    evt->setEventPhase(0); // I guess this is correct, the spec does not seem to say
                           // anything about the default event handler phase.
    
    
    // Now call the post dispatch.
    postDispatchEventHandler(evt.get(), data);
    
    // now we call all default event handlers (this is not part of DOM - it is internal to khtml)
    
    it.toLast();
    if (evt->bubbles())
        for (; it.current() && !evt->defaultPrevented() && !evt->defaultHandled(); --it)
            EventTargetNodeCast(it.current())->defaultEventHandler(evt.get());
    else if (!evt->defaultPrevented() && !evt->defaultHandled())
        EventTargetNodeCast(it.current())->defaultEventHandler(evt.get());
    
    // deref all nodes in chain
    it.toFirst();
    for (; it.current(); ++it)
        it.current()->deref(); // this may delete us
    
    DocumentImpl::updateDocumentsRendering();
    
    // If tempEvent is true, this means that the DOM implementation
    // will not be storing a reference to the event, i.e.  there is no
    // way to retrieve it from javascript if a script does not already
    // have a reference to it in a variable.  So there is no need for
    // the interpreter to keep the event in it's cache
    Frame *frame = getDocument()->frame();
    if (tempEvent && frame && frame->jScript())
        frame->jScript()->finishedWithEvent(evt.get());
    
    return !evt->defaultPrevented(); // ### what if defaultPrevented was called before dispatchEvent?
}

bool EventTargetNodeImpl::dispatchEvent(PassRefPtr<EventImpl> e, ExceptionCode& ec, bool tempEvent)
{
    RefPtr<EventImpl> evt(e);
    assert(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) { 
        ec = UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }
    evt->setTarget(this);
    
    RefPtr<FrameView> view = getDocument()->view();
    
    return dispatchGenericEvent(evt.release(), ec, tempEvent);
}

bool EventTargetNodeImpl::dispatchSubtreeModifiedEvent(bool sendChildrenChanged)
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
    ExceptionCode ec = 0;
    return dispatchEvent(new MutationEventImpl(DOMSubtreeModifiedEvent,
                                               true,false,0,DOMString(),DOMString(),DOMString(),0),ec,true);
}

void EventTargetNodeImpl::dispatchWindowEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
{
    assert(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    RefPtr<EventImpl> evt = new EventImpl(eventType, canBubbleArg, cancelableArg);
    RefPtr<DocumentImpl> doc = getDocument();
    evt->setTarget(doc.get());
    doc->handleWindowEvent(evt.get(), false);
    
    if (eventType == loadEvent) {
        // For onload events, send a separate load event to the enclosing frame only.
        // This is a DOM extension and is independent of bubbling/capturing rules of
        // the DOM.
        ElementImpl* ownerElement = doc->ownerElement();
        if (ownerElement) {
            RefPtr<EventImpl> ownerEvent = new EventImpl(eventType, false, cancelableArg);
            ownerEvent->setTarget(ownerElement);
            ownerElement->dispatchGenericEvent(ownerEvent.release(), ec, true);
        }
    }
}

bool EventTargetNodeImpl::dispatchUIEvent(const AtomicString &eventType, int detail)
{
    assert(!eventDispatchForbidden());
    assert(eventType == DOMFocusInEvent || eventType == DOMFocusOutEvent || eventType == DOMActivateEvent);
    
    bool cancelable = eventType == DOMActivateEvent;
    
    ExceptionCode ec = 0;
    UIEventImpl* evt = new UIEventImpl(eventType, true, cancelable, getDocument()->defaultView(), detail);
    return dispatchEvent(evt, ec, true);
}

bool EventTargetNodeImpl::dispatchKeyEvent(const KeyEvent& key)
{
    assert(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    RefPtr<KeyboardEventImpl> keyboardEventImpl = new KeyboardEventImpl(key, getDocument()->defaultView());
    bool r = dispatchEvent(keyboardEventImpl,ec,true);
    
    // we want to return false if default is prevented (already taken care of)
    // or if the element is default-handled by the DOM. Otherwise we let it just
    // let it get handled by AppKit 
    if (keyboardEventImpl->defaultHandled())
        r = false;
    
    return r;
}

bool EventTargetNodeImpl::dispatchMouseEvent(const MouseEvent& _mouse, const AtomicString& eventType,
                                             int detail, NodeImpl* relatedTarget)
{
    assert(!eventDispatchForbidden());
    
    int clientX = 0;
    int clientY = 0;
    if (FrameView *view = getDocument()->view())
        view->viewportToContents(_mouse.x(), _mouse.y(), clientX, clientY);
    
    return dispatchMouseEvent(eventType, _mouse.button(), detail,
                              clientX, clientY, _mouse.globalX(), _mouse.globalY(),
                              _mouse.ctrlKey(), _mouse.altKey(), _mouse.shiftKey(), _mouse.metaKey(),
                              false, relatedTarget);
}

bool EventTargetNodeImpl::dispatchSimulatedMouseEvent(const AtomicString &eventType)
{
    assert(!eventDispatchForbidden());
    // Like Gecko, we just pass 0 for everything when we make a fake mouse event.
    // Internet Explorer instead gives the current mouse position and state.
    return dispatchMouseEvent(eventType, 0, 0, 0, 0, 0, 0, false, false, false, false, true);
}

bool EventTargetNodeImpl::dispatchMouseEvent(const AtomicString& eventType, int button, int detail,
                                             int clientX, int clientY, int screenX, int screenY,
                                             bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, 
                                             bool isSimulated, NodeImpl* relatedTarget)
{
    assert(!eventDispatchForbidden());
    if (disabled()) // Don't even send DOM events for disabled controls..
        return true;
    
    if (eventType.isEmpty())
        return false; // Shouldn't happen.
    
    // Dispatching the first event can easily result in this node being destroyed.
    // Since we dispatch up to three events here, we need to make sure we're referenced
    // so the pointer will be good for the two subsequent ones.
    RefPtr<NodeImpl> protect(this);
    
    bool cancelable = eventType != mousemoveEvent;
    
    ExceptionCode ec = 0;
    
    bool swallowEvent = false;
    
    RefPtr<EventImpl> me = new MouseEventImpl(eventType, true, cancelable, getDocument()->defaultView(),
                                              detail, screenX, screenY, clientX, clientY,
                                              ctrlKey, altKey, shiftKey, metaKey, button,
                                              relatedTarget, 0, isSimulated);
    
    dispatchEvent(me, ec, true);
    bool defaultHandled = me->defaultHandled();
    bool defaultPrevented = me->defaultPrevented();
    if (defaultHandled || defaultPrevented)
        swallowEvent = true;
    
    // Special case: If it's a double click event, we also send the KHTML_DBLCLICK event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute.  This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    if (eventType == clickEvent && detail == 2) {
        me = new MouseEventImpl(dblclickEvent, true, cancelable, getDocument()->defaultView(),
                                detail, screenX, screenY, clientX, clientY,
                                ctrlKey, altKey, shiftKey, metaKey, button,
                                relatedTarget, 0, isSimulated);
        if (defaultHandled)
            me->setDefaultHandled();
        dispatchEvent(me, ec, true);
        if (me->defaultHandled() || me->defaultPrevented())
            swallowEvent = true;
    }
    
    // Also send a DOMActivate event, which causes things like form submissions to occur.
    if (eventType == clickEvent && !defaultPrevented)
        dispatchUIEvent(DOMActivateEvent, detail);
    
    return swallowEvent;
}

void EventTargetNodeImpl::dispatchWheelEvent(WheelEvent& e)
{
    assert(!eventDispatchForbidden());
    if (e.delta() == 0)
        return;
    
    FrameView *view = getDocument()->view();
    if (!view)
        return;
    
    int x;
    int y;
    view->viewportToContents(e.x(), e.y(), x, y);
    
    RefPtr<WheelEventImpl> we = new WheelEventImpl(e.isHorizontal(), e.delta(),
                                                   getDocument()->defaultView(), e.globalX(), e.globalY(), x, y,
                                                   e.ctrlKey(), e.altKey(), e.shiftKey(), e.metaKey());
    
    ExceptionCode ec = 0;
    if (!dispatchEvent(we, ec, true))
        e.accept();
}

bool EventTargetNodeImpl::dispatchHTMLEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
{
    assert(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(new EventImpl(eventType, canBubbleArg, cancelableArg), ec, true);
}

void EventTargetNodeImpl::removeHTMLEventListener(const AtomicString &eventType)
{
    if (!m_regdListeners) // nothing to remove
        return;
    
    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (it.current()->eventType() == eventType && it.current()->listener()->isHTMLEventListener()) {
            m_regdListeners->removeRef(it.current());
            // removed last
            if (m_regdListeners->isEmpty() && !inDocument())
                getDocument()->unregisterDisconnectedNodeWithEventListeners(this);
            return;
        }
}

void EventTargetNodeImpl::setHTMLEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener)
{
    // In case we are the only one holding a reference to it, we don't want removeHTMLEventListener to destroy it.
    removeHTMLEventListener(eventType);
    if (listener)
        addEventListener(eventType, listener.get(), false);
}

EventListener *EventTargetNodeImpl::getHTMLEventListener(const AtomicString &eventType)
{
    if (!m_regdListeners)
        return 0;
    
    QPtrListIterator<RegisteredEventListener> it(*m_regdListeners);
    for (; it.current(); ++it)
        if (it.current()->eventType() == eventType && it.current()->listener()->isHTMLEventListener())
            return it.current()->listener();
    return 0;
}

bool EventTargetNodeImpl::disabled() const
{
    return false;
}

void EventTargetNodeImpl::defaultEventHandler(EventImpl *evt)
{
}

#ifndef NDEBUG
void EventTargetNodeImpl::dump(QTextStream *stream, QString ind) const
{
    if (m_regdListeners)
        *stream << " #regdListeners=" << m_regdListeners->count(); // ### more detail
    
    NodeImpl::dump(stream,ind);
}
#endif

} // namespace WebCore
