/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <kjs/interpreter.h>

#include "EventImpl.h"
#include "DocumentImpl.h"
#include "EventTargetImpl.h"
#include "EventListenerImpl.h"
#include "EventExceptionImpl.h"
#include "DOMImplementationImpl.h"
#include "RegisteredEventListener.h"

using namespace KDOM;

EventTargetImpl::EventTargetImpl() : TreeShared<EventTargetImpl>(), m_eventListeners(0)
{
    m_listenerTypes = 0;
}

EventTargetImpl::~EventTargetImpl()
{
    delete m_eventListeners;
}

void EventTargetImpl::addEventListener(DOMStringImpl *type, EventListenerImpl *listener, bool useCapture)
{
    if(!listener)
        return;

    // If the requested listener is builtin (ie. DOMNODEREMOVED_EVENT)
    // we'll cache the specific enumeration value to save space
    // but in case of a custom event (ie. as used in the dom2 events test suite)
    // for example 'foo' just don't cache it; kdom internally won't fire any
    // 'foo' events, which means it doesn't need to know anything about it...
    NodeImpl *dynObj = const_cast<NodeImpl *>(dynamic_cast<const NodeImpl *>(this));
    if(!dynObj)
    {
        kdFatal() << "Upcast failed! The 'impossible' happened...!" << endl;
        return;
    }

    DocumentImpl *doc = (dynObj->nodeType() == DOCUMENT_NODE) ? static_cast<DocumentImpl *>(dynObj) : dynObj->ownerDocument();
    if(!doc)
    {
        kdError() << "Couldn't register listener type! No document available!" << endl;
        return;
    }

    if(!m_eventListeners)
    {
        m_eventListeners = new Q3PtrList<RegisteredEventListener>();
        m_eventListeners->setAutoDelete(true);
    }
    else
    {
        // If there is an existing listener forget this call...
        RegisteredEventListener compare(type, listener, useCapture); 

        Q3PtrListIterator<RegisteredEventListener> it(*m_eventListeners);
        for(; it.current(); ++it)
        {
            if(*(it.current()) == compare)
                return;
        }
    }

    addListenerType(doc->addListenerType(type));
    m_eventListeners->append(new RegisteredEventListener(type, listener, useCapture));
}

void EventTargetImpl::removeEventListener(DOMStringImpl *type, EventListenerImpl *listener, bool useCapture)
{
    if(!m_eventListeners || !listener)
        return;

    NodeImpl *dynObj = const_cast<NodeImpl *>(dynamic_cast<const NodeImpl *>(this));
    if(!dynObj)
    {
        kdFatal() << "Upcast failed! The 'impossible' happened...!" << endl;
        return;
    }

    DocumentImpl *doc = (dynObj->nodeType() == DOCUMENT_NODE) ? static_cast<DocumentImpl *>(dynObj) : dynObj->ownerDocument();
    if(!doc)
    {
        kdError() << "Couldn't remove listener type! No document available!" << endl;
        return;
    }

    RegisteredEventListener compare(type, listener, useCapture);

    Q3PtrListIterator<RegisteredEventListener> it(*m_eventListeners);
    for(; it.current(); ++it)
    {
        if((*it.current()) == compare)
        {
            m_eventListeners->removeRef(it.current());
            removeListenerType(doc->removeListenerType(type));
            return;
        }
    }
}

bool EventTargetImpl::dispatchEvent(EventImpl *evt)
{
    if(!evt || (evt->id() == UNKNOWN_EVENT && (!evt->type() || evt->type()->isEmpty())))
        throw new EventExceptionImpl(UNSPECIFIED_EVENT_TYPE_ERR);

    evt->setTarget(const_cast<EventTargetImpl *>(this));

    // Find out, where to send to -> collect parent nodes,
    // cast them to EventTargets and add them to list
    Q3PtrList<EventTargetImpl> targetChain;

    NodeImpl *i = dynamic_cast<NodeImpl *>(this);
    if(!i)
    {
        kdError() << k_funcinfo << " EventTarget must be inherited by Node! This should never happen." << endl;
        return false;
    }

    for(NodeImpl *n = i; n != 0; n = n->parentNode())
        targetChain.prepend(n);

    // Trigger any capturing event handlers on our way down
    evt->setEventPhase(CAPTURING_PHASE);

    Q3PtrListIterator<EventTargetImpl> it(targetChain);
    for(; it.current() && it.current() != this && !evt->propagationStopped(); ++it)
    {
        EventTargetImpl *i = it.current();
        evt->setCurrentTarget(i);

        if(i)
            i->handleLocalEvents(evt, true);
    }

    // Dispatch to the actual target node
    it.toLast();
    if(!evt->propagationStopped())
    {
        EventTargetImpl *i = it.current();

        evt->setCurrentTarget(i);
        evt->setEventPhase(AT_TARGET);

        if(i)
            i->handleLocalEvents(evt, false);
    }

    --it;

    // Bubble up again
    if(evt->bubbles())
    {
        evt->setEventPhase(BUBBLING_PHASE);
        for(; it.current() && !evt->propagationStopped(); --it)
        {
            EventTargetImpl *i = it.current();
            evt->setCurrentTarget(i);

            if(i)
                i->handleLocalEvents(evt, false);
        }
    }

    evt->setCurrentTarget(0);
    evt->setEventPhase(0);        // I guess this is correct, the spec does not seem to say
                                // anything about the default event handler phase.
    if(evt->bubbles())
    {
        // now we call all default event handlers (this is not part of DOM - it is internal to khtml)
        it.toLast();
        for(; it.current() && !evt->propagationStopped() && !evt->defaultPrevented() && !evt->defaultHandled(); --it)
            it.current()->defaultEventHandler(evt);
    }

    DocumentImpl *doc = (i->nodeType() == DOCUMENT_NODE) ? static_cast<DocumentImpl *>(i) : i->ownerDocument();
    doc->updateRendering(); // any changes during event handling need to be rendered

/* FIXME
    Ecma *ecmaEngine = doc->ecmaEngine();
    if(ecmaEngine)
        ecmaEngine->finishedWithEvent(evt);
*/
    bool retVal = !evt->defaultPrevented(); // What if defaultPrevented was called before dispatchEvent?
    return retVal;
}

void EventTargetImpl::handleLocalEvents(EventImpl *evt, bool useCapture)
{
    if(!m_eventListeners || !evt)
        return;

    KJS::Interpreter::lock();

    Q3PtrListIterator<RegisteredEventListener> it(*m_eventListeners);
    for(; it.current(); ++it)
    {
        RegisteredEventListener *current = it.current();
        if(DOMString(current->type()) == DOMString(evt->type()) &&
           current->useCapture() == useCapture && current->listener())
        {
            current->listener()->handleEvent(evt);
        }
    }

    KJS::Interpreter::unlock();
}

void EventTargetImpl::addListenerType(int eventId)
{
    m_listenerTypes |= (1 << eventId);
}

void EventTargetImpl::removeListenerType(int eventId)
{
    m_listenerTypes &= ~(1 << eventId);
}

bool EventTargetImpl::hasListenerType(int eventId) const
{
    return m_listenerTypes & (1 << eventId);
}

// vim:ts=4:noet
