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

#include "EventImpl.h"
#include "kdomevents.h"
#include "DOMStringImpl.h"
#include "EventTargetImpl.h"
#include "DOMImplementationImpl.h"

using namespace KDOM;

EventImpl::EventImpl(EventImplType identifier) : Shared(), m_identifier(identifier)
{
    m_createTime = QDateTime::currentDateTime();

    m_target = 0;
    m_currentTarget = 0;

    m_id = UNKNOWN_EVENT;
    m_eventPhase = 0;

    m_type = 0;
    m_bubbles = false;
    m_cancelable = false;
    m_propagationStopped = false;
    m_defaultPrevented = false;
    m_defaultHandled = false;
}

EventImpl::~EventImpl()
{
    if(m_type)
        m_type->deref();
}

DOMStringImpl *EventImpl::type() const
{
    return m_type;
}

EventTargetImpl *EventImpl::target() const
{
    return m_target;
}

EventTargetImpl *EventImpl::currentTarget() const
{
    return m_currentTarget;
}

unsigned short EventImpl::eventPhase() const
{
    return m_eventPhase;
}

bool EventImpl::bubbles() const
{
    return m_bubbles;
}

bool EventImpl::cancelable() const
{
    return m_cancelable;
}

DOMTimeStamp EventImpl::timeStamp() const
{
    QDateTime epoch(QDate(1970, 1, 1), QTime(0, 0));
    return epoch.secsTo(m_createTime) * 1000 + m_createTime.time().msec();
}

void EventImpl::stopPropagation()
{
    m_propagationStopped = true;
}

void EventImpl::preventDefault()
{
    if(m_cancelable)
        m_defaultPrevented = true;
}

void EventImpl::initEvent(DOMStringImpl *eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
    KDOM_SAFE_SET(m_type, eventTypeArg);
    m_id = DOMImplementationImpl::self()->typeToId(m_type);
    m_bubbles = canBubbleArg;
    m_cancelable = cancelableArg;
}

void EventImpl::setTarget(EventTargetImpl *target)
{
    m_target = target;
}

void EventImpl::setCurrentTarget(EventTargetImpl *currentTarget)
{
    m_currentTarget = currentTarget;
}

void EventImpl::setEventPhase(unsigned short eventPhase)
{
    m_eventPhase = eventPhase;
}

int EventImpl::id() const
{
    return m_id;
}

EventImplType EventImpl::identifier() const
{
    return m_identifier;
}

bool EventImpl::defaultPrevented() const
{
    return m_defaultPrevented;
}

bool EventImpl::propagationStopped() const
{
    return m_propagationStopped;
}

// vim:ts=4:noet
