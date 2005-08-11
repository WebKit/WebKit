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

#include "kdom.h"
#include "Node.h"
#include "Ecma.h"
#include "Event.h"
#include "Document.h"
#include "EventTarget.h"
#include "EventListener.h"
#include "EventException.h"
#include "RegisteredEventListener.h"

#include "NodeImpl.h"
#include "EventImpl.h"
#include "DocumentImpl.h"
#include "EventTargetImpl.h"
#include "EventListenerImpl.h"
#include "EventExceptionImpl.h"
#include "DOMImplementationImpl.h"

#include <kdom/data/EventsConstants.h>
#include "EventTarget.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin EventTarget::s_hashTable 2
 dummy					EventTargetConstants::Dummy					DontDelete|ReadOnly
@end
@begin EventTargetProto::s_hashTable 5
 addEventListener		EventTargetConstants::AddEventListener		DontDelete|Function 3
 removeEventListener	EventTargetConstants::RemoveEventListener	DontDelete|Function 3
 dispatchEvent			EventTargetConstants::DispatchEvent			DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("EventTarget", EventTargetProto, EventTargetProtoFunc)

ValueImp *EventTarget::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(EventException)
	return Undefined();
}

ValueImp *EventTargetProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(EventTarget)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case EventTargetConstants::AddEventListener:
		case EventTargetConstants::RemoveEventListener:
		{
			DOMString type = toDOMString(exec, args[0]);

			// Some magic to access the document's ecma engine...
			NodeImpl *nodeImpl = dynamic_cast<NodeImpl *>(obj.d);
			if(!nodeImpl)
			{
				kdError() << k_funcinfo << " EventTarget must be inherited by Node! This should never happen." << endl;
				return Undefined();
			}

			// Be careful, the 'node' could be the document itself, which
			// means that node->ownerDocument() will return a null document...
			Ecma *ecmaEngine = nodeImpl->ownerDocument() ? nodeImpl->ownerDocument()->ecmaEngine() : 0;
			if(!ecmaEngine)
			{
				if(nodeImpl->nodeType() == DOCUMENT_NODE)
				{
					// Be really sure everything is okay, in such
					// a frequently used EcmaScript function.....
					DocumentImpl *docPtr = static_cast<DocumentImpl *>(nodeImpl);
					if(docPtr != 0)
						ecmaEngine = docPtr->ecmaEngine();
				}

				if(!ecmaEngine)
				{
					kdError() << k_funcinfo  << " Can't access document's ecma engine! This should never happen." << endl;
					return Undefined();
				}
			}

			bool useCapture = args[2]->toBoolean(exec);

			// createEventListener automatically reuses existing listeners, if possible...
			if(id == EventTargetConstants::AddEventListener)
			{
				EventListenerImpl *listener = ecmaEngine->createEventListener(exec, args[1]);
				listener->ref();

				EventListener listenerObject(listener);
				obj.addEventListener(type, listenerObject, useCapture);

				// TODO: Investigate?
				// listener->deref();
			}
			else
			{
				EventListenerImpl *listener = ecmaEngine->findEventListener(exec, args[1]);
				
				EventListener listenerObject(listener);
				obj.removeEventListener(type, listenerObject, useCapture);
			}

			return Undefined();
		}
		case EventTargetConstants::DispatchEvent:
		{
			Event evt = ecma_cast<Event>(exec, args[0], &toEvent);
			return KJS::Boolean(obj.dispatchEvent(evt));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(EventException)
	return Undefined();
}

EventTarget EventTarget::null;

EventTarget::EventTarget() : d(0)
{
}

EventTarget::EventTarget(EventTargetImpl *i) : d(i)
{
	if(d)
		d->ref();
}

EventTarget::EventTarget(const EventTarget &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(EventTarget)

void EventTarget::addEventListener(const DOMString &type, const EventListener &listener, bool useCapture)
{
	if(d)
		d->addEventListener(type, listener.d, useCapture);
}

void EventTarget::removeEventListener(const DOMString &type, const EventListener &listener, bool useCapture)
{
	if(d)
		d->removeEventListener(type, listener.d, useCapture);
}

bool EventTarget::dispatchEvent(const Event &evt)
{
	if(!d)
		return false;
		
	return d->dispatchEvent(evt.d);
}

// vim:ts=4:noet
