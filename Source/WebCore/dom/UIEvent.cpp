/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "UIEvent.h"

#include "Console.h"
#include "DOMWindow.h"
#include "EventDispatcher.h"

namespace WebCore {

UIEvent::UIEvent()
    : m_detail(0)
{
}

UIEvent::UIEvent(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, PassRefPtr<AbstractView> viewArg, int detailArg)
    : Event(eventType, canBubbleArg, cancelableArg)
    , m_view(viewArg)
    , m_detail(detailArg)
{
}

UIEvent::~UIEvent()
{
}

void UIEvent::initUIEvent(const AtomicString& typeArg, bool canBubbleArg, bool cancelableArg, PassRefPtr<AbstractView> viewArg, int detailArg)
{
    if (dispatched())
        return;

    initEvent(typeArg, canBubbleArg, cancelableArg);

    m_view = viewArg;
    m_detail = detailArg;
}

bool UIEvent::isUIEvent() const
{
    return true;
}

const AtomicString& UIEvent::interfaceName() const
{
    return eventNames().interfaceForUIEvent;
}

int UIEvent::keyCode() const
{
    return 0;
}

int UIEvent::charCode() const
{
    return 0;
}

int UIEvent::layerX()
{
    return 0;
}

int UIEvent::layerY()
{
    return 0;
}

int UIEvent::pageX() const
{
    return 0;
}

int UIEvent::pageY() const
{
    return 0;
}

int UIEvent::which() const
{
    return 0;
}

PassRefPtr<FocusInEventDispatchMediator> FocusInEventDispatchMediator::create(PassRefPtr<Event> event, PassRefPtr<Node> oldFocusedNode)
{
    return adoptRef(new FocusInEventDispatchMediator(event, oldFocusedNode));
}

FocusInEventDispatchMediator::FocusInEventDispatchMediator(PassRefPtr<Event> event, PassRefPtr<Node> oldFocusedNode)
    : EventDispatchMediator(event)
    , m_oldFocusedNode(oldFocusedNode)
{
}

bool FocusInEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    dispatcher->adjustRelatedTarget(event(), m_oldFocusedNode);
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

PassRefPtr<FocusOutEventDispatchMediator> FocusOutEventDispatchMediator::create(PassRefPtr<Event> event, PassRefPtr<Node> newFocusedNode)
{
    return adoptRef(new FocusOutEventDispatchMediator(event, newFocusedNode));
}

FocusOutEventDispatchMediator::FocusOutEventDispatchMediator(PassRefPtr<Event> event, PassRefPtr<Node> newFocusedNode)
    : EventDispatchMediator(event)
    , m_newFocusedNode(newFocusedNode)
{
}

bool FocusOutEventDispatchMediator::dispatchEvent(EventDispatcher* dispatcher) const
{
    dispatcher->adjustRelatedTarget(event(), m_newFocusedNode);
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

} // namespace WebCore
