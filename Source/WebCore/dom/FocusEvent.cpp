/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FocusEvent.h"

#include "EventDispatcher.h"
#include "EventNames.h"

namespace WebCore {

FocusEvent::FocusEvent()
{
}

FocusEvent::~FocusEvent()
{
}

void FocusEvent::initFocusEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<AbstractView> view, int detail, PassRefPtr<EventTarget> relatedTarget)
{
    if (dispatched())
        return;
    
    initUIEvent(type, canBubble, cancelable, view, detail);
    
    m_relatedTarget = relatedTarget;
}

const AtomicString& FocusEvent::interfaceName() const
{
    return eventNames().interfaceForFocusEvent;
}

FocusEvent::FocusEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<AbstractView> view, int detail, PassRefPtr<EventTarget> relatedTarget)
    : UIEvent(type, canBubble, cancelable, view, detail)
    , m_relatedTarget(relatedTarget)
{
}



PassRefPtr<FocusInEventDispatchMediator> FocusInEventDispatchMediator::create(PassRefPtr<FocusEvent> event, PassRefPtr<Node> oldFocusedNode)
{
    return adoptRef(new FocusInEventDispatchMediator(event, oldFocusedNode));
}

FocusInEventDispatchMediator::FocusInEventDispatchMediator(PassRefPtr<FocusEvent> event, PassRefPtr<Node> oldFocusedNode)
    : EventDispatchMediator(event)
    , m_oldFocusedNode(oldFocusedNode)
{
}

bool FocusInEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    if (dispatcher->node()->disabled()) // Don't even send DOM events for disabled controls.
        return true;
    
    if (event()->type().isEmpty())
        return false; // Shouldn't happen.
    
    RefPtr<EventTarget> relatedTarget = dispatcher->adjustRelatedTarget(event(), m_oldFocusedNode);
    event()->setRelatedTarget(relatedTarget);
     
    return EventDispatchMediator::dispatchEvent(dispatcher);   
}

FocusEvent* FocusInEventDispatchMediator::event() const
{
    return static_cast<FocusEvent*>(EventDispatchMediator::event());
}



PassRefPtr<FocusOutEventDispatchMediator> FocusOutEventDispatchMediator::create(PassRefPtr<FocusEvent> event, PassRefPtr<Node> newFocusedNode)
{
    return adoptRef(new FocusOutEventDispatchMediator(event, newFocusedNode));
}

FocusOutEventDispatchMediator::FocusOutEventDispatchMediator(PassRefPtr<FocusEvent> event, PassRefPtr<Node> newFocusedNode)
    : EventDispatchMediator(event)
    , m_newFocusedNode(newFocusedNode)
{
}

bool FocusOutEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    if (dispatcher->node()->disabled()) // Don't even send DOM events for disabled controls.
        return true;
    
    if (event()->type().isEmpty())
        return false; // Shouldn't happen.
    
    RefPtr<EventTarget> relatedTarget = dispatcher->adjustRelatedTarget(event(), m_newFocusedNode);
    event()->setRelatedTarget(relatedTarget);
     
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

FocusEvent* FocusOutEventDispatchMediator::event() const
{
    return static_cast<FocusEvent*>(EventDispatchMediator::event());
}



PassRefPtr<FocusEventDispatchMediator> FocusEventDispatchMediator::create(PassRefPtr<Node> oldFocusedNode)
{
    return adoptRef(new FocusEventDispatchMediator(oldFocusedNode));
}

FocusEventDispatchMediator::FocusEventDispatchMediator(PassRefPtr<Node> oldFocusedNode)
    : EventDispatchMediator(Event::create(eventNames().focusEvent, false, false))
    , m_oldFocusedNode(oldFocusedNode)
{
}

bool FocusEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    dispatcher->adjustRelatedTarget(event(), m_oldFocusedNode);
    return EventDispatchMediator::dispatchEvent(dispatcher);
}



PassRefPtr<BlurEventDispatchMediator> BlurEventDispatchMediator::create(PassRefPtr<Node> newFocusedNode)
{
    return adoptRef(new BlurEventDispatchMediator(newFocusedNode));
}

BlurEventDispatchMediator::BlurEventDispatchMediator(PassRefPtr<Node> newFocusedNode)
    : EventDispatchMediator(Event::create(eventNames().blurEvent, false, false))
    , m_newFocusedNode(newFocusedNode)
{
}

bool BlurEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    dispatcher->adjustRelatedTarget(event(), m_newFocusedNode);
    return EventDispatchMediator::dispatchEvent(dispatcher);
}


} // namespace WebCore
