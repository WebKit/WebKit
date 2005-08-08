/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include <qdatetime.h>

#include "Ecma.h"
#include "Node.h"
#include "Event.h"
#include "NodeImpl.h"
#include "EventImpl.h"
#include "EventTarget.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"

#include <kdom/data/EventsConstants.h>
#include "Event.lut.h"
#include "EventTargetImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin Event::s_hashTable 9
 type			EventConstants::Type			DontDelete|ReadOnly
 target			EventConstants::Target			DontDelete|ReadOnly
 currentTarget	EventConstants::CurrentTarget	DontDelete|ReadOnly
 eventPhase		EventConstants::EventPhase		DontDelete|ReadOnly
 bubbles		EventConstants::Bubbles			DontDelete|ReadOnly
 cancelable		EventConstants::Cancelable		DontDelete|ReadOnly
 timeStamp		EventConstants::TimeStamp		DontDelete|ReadOnly
@end
@begin EventProto::s_hashTable 5
 stopPropagation	EventConstants::StopPropagation	DontDelete|Function 0
 preventDefault		EventConstants::PreventDefault	DontDelete|Function 0
 initEvent			EventConstants::InitEvent		DontDelete|Function 3
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("Event", EventProto, EventProtoFunc)

ValueImp *Event::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case EventConstants::Type:
			return getDOMString(type());
		case EventConstants::Target:
		case EventConstants::CurrentTarget:
		{
			EventTarget obj;
			if(token == EventConstants::Target)
				obj = target();
			else
				obj = currentTarget();

			// We should return a 'KDOM::EventTarget' object here, according to the
			// specification, but real life expects it to be a 'KDOM::Node' at least!
			if(obj != EventTarget::null)
			{
				EventTargetImpl *objPtr = obj.handle();
				NodeImpl *nodePtr = dynamic_cast<NodeImpl *>(objPtr);
				if(nodePtr)
					return getDOMNode(exec, Node(nodePtr));
				else if(objPtr)
					return obj.cache(exec);
			}
			
			return NULL;
		}
		case EventConstants::EventPhase:
			return Number(eventPhase());
		case EventConstants::Bubbles:
			return KJS::Boolean(bubbles());
		case EventConstants::Cancelable:
			return KJS::Boolean(cancelable());
		 case EventConstants::TimeStamp:
		 	return Number(static_cast<long unsigned int>(timeStamp()));
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

ValueImp *EventProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(Event)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case EventConstants::StopPropagation:
		{
			obj.stopPropagation();
			return Undefined();
		}
		case EventConstants::PreventDefault:
		{
			obj.preventDefault();
			return Undefined();
		}
		case EventConstants::InitEvent:
		{
			DOMString eventTypeArg = toDOMString(exec, args[0]);
			bool canBubbleArg = args[1]->toBoolean(exec);
			bool cancelableArg = args[2]->toBoolean(exec);
			obj.initEvent(eventTypeArg, canBubbleArg, cancelableArg);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

Event Event::null;

Event::Event() : d(0)
{
}

Event::Event(EventImpl *i) : d(i)
{
	if(d)
		d->ref();
}

Event::Event(const Event &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(Event)

DOMString Event::type() const
{
	if(!d)
		return DOMString();

	return d->type();
}

EventTarget Event::target() const
{
	if(!d)
		return EventTarget::null;

	return EventTarget(d->target());
}

EventTarget Event::currentTarget() const
{
	if(!d)
		return EventTarget::null;

	return EventTarget(d->currentTarget());
}

unsigned short Event::eventPhase() const
{
	if(!d)
		return 0;
		
	return d->eventPhase();
}

bool Event::bubbles() const
{
	if(!d)
		return false;

	return d->bubbles();
}

bool Event::cancelable() const
{
	if(!d)
		return false;
		
	return d->cancelable();
}

DOMTimeStamp Event::timeStamp() const
{
	if(!d)
		return 0;

	return d->timeStamp();
}

void Event::stopPropagation()
{
	if(d)
		d->stopPropagation();
}

void Event::preventDefault()
{
	if(d)
		d->preventDefault();
}

void Event::initEvent(const DOMString &eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
	if(!d) // Only way to support initEvent()....
	{
		d = new EventImpl(TypeGenericEvent);
		d->ref();
	}

	d->initEvent(eventTypeArg, canBubbleArg, cancelableArg);
}

// vim:ts=4:noet
