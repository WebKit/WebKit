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

#include <kdom/ecma/Ecma.h>

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
