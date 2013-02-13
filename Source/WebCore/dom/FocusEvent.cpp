/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FocusEvent.h"

#include "Event.h"
#include "EventDispatcher.h"
#include "EventNames.h"
#include "Node.h"

namespace WebCore {

FocusEventInit::FocusEventInit()
    : relatedTarget(0)
{
}

const AtomicString& FocusEvent::interfaceName() const
{
    return eventNames().interfaceForFocusEvent;
}

bool FocusEvent::isFocusEvent() const
{
    return true;
}

FocusEvent::FocusEvent()
{
}

FocusEvent::FocusEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<AbstractView> view, int detail, PassRefPtr<EventTarget> relatedTarget)
    : UIEvent(type, canBubble, cancelable, view, detail)
    , m_relatedTarget(relatedTarget)
{
}

FocusEvent::FocusEvent(const AtomicString& type, const FocusEventInit& initializer)
    : UIEvent(type, initializer)
    , m_relatedTarget(initializer.relatedTarget)
{
}

PassRefPtr<FocusEventDispatchMediator> FocusEventDispatchMediator::create(PassRefPtr<FocusEvent> focusEvent, PassRefPtr<Node> oldFocusedNode)
{
    return adoptRef(new FocusEventDispatchMediator(focusEvent, oldFocusedNode));
}

FocusEventDispatchMediator::FocusEventDispatchMediator(PassRefPtr<FocusEvent> focusEvent, PassRefPtr<Node> oldFocusedNode)
    : EventDispatchMediator(focusEvent)
    , m_oldFocusedNode(oldFocusedNode)
{
}

bool FocusEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    dispatcher->adjustRelatedTarget(event(), m_oldFocusedNode);
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

PassRefPtr<BlurEventDispatchMediator> BlurEventDispatchMediator::create(PassRefPtr<FocusEvent> focusEvent, PassRefPtr<Node> newFocusedNode)
{
    return adoptRef(new BlurEventDispatchMediator(focusEvent, newFocusedNode));
}

BlurEventDispatchMediator::BlurEventDispatchMediator(PassRefPtr<FocusEvent> focusEvent, PassRefPtr<Node> newFocusedNode)
    : EventDispatchMediator(focusEvent)
    , m_newFocusedNode(newFocusedNode)
{
}

bool BlurEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    dispatcher->adjustRelatedTarget(event(), m_newFocusedNode);
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

PassRefPtr<FocusInEventDispatchMediator> FocusInEventDispatchMediator::create(PassRefPtr<FocusEvent> focusEvent, PassRefPtr<Node> oldFocusedNode)
{
    return adoptRef(new FocusInEventDispatchMediator(focusEvent, oldFocusedNode));
}

FocusInEventDispatchMediator::FocusInEventDispatchMediator(PassRefPtr<FocusEvent> focusEvent, PassRefPtr<Node> oldFocusedNode)
    : EventDispatchMediator(focusEvent)
    , m_oldFocusedNode(oldFocusedNode)
{
}

bool FocusInEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    dispatcher->adjustRelatedTarget(event(), m_oldFocusedNode);
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

PassRefPtr<FocusOutEventDispatchMediator> FocusOutEventDispatchMediator::create(PassRefPtr<FocusEvent> focusEvent, PassRefPtr<Node> newFocusedNode)
{
    return adoptRef(new FocusOutEventDispatchMediator(focusEvent, newFocusedNode));
}

FocusOutEventDispatchMediator::FocusOutEventDispatchMediator(PassRefPtr<FocusEvent> focusEvent, PassRefPtr<Node> newFocusedNode)
    : EventDispatchMediator(focusEvent)
    , m_newFocusedNode(newFocusedNode)
{
}

bool FocusOutEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    dispatcher->adjustRelatedTarget(event(), m_newFocusedNode);
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

} // namespace WebCore
