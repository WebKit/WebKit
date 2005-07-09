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

#include "Ecma.h"
#include "Event.h"
#include "Document.h"
#include "DocumentImpl.h"
#include "DOMException.h"
#include "DocumentEvent.h"
#include "DOMExceptionImpl.h"
#include "DocumentEventImpl.h"

#include "UIEvent.h"
#include "UIEventImpl.h"

#include <kdom/data/EventsConstants.h>
#include "DocumentEvent.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DocumentEvent::s_hashTable 2
 dummy			DocumentEventConstants::Dummy		DontDelete|ReadOnly
@end
@begin DocumentEventProto::s_hashTable 2
 createEvent	DocumentEventConstants::CreateEvent	DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("DocumentEvent", DocumentEventProto, DocumentEventProtoFunc)

Value DocumentEvent::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
};

Value DocumentEventProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	DocumentEvent obj(cast(exec, static_cast<KJS::ObjectImp *>(thisObj.imp())));
	Q_ASSERT(obj.d != 0);

	KDOM_ENTER_SAFE

	switch(id)
	{
		case DocumentEventConstants::CreateEvent:
		{
			DOMString eventType = toDOMString(exec, args[0]);
			return getDOMEvent(exec, obj.createEvent(eventType));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

DocumentEvent DocumentEvent::null;

DocumentEvent::DocumentEvent() : d(0)
{
}

DocumentEvent::DocumentEvent(DocumentEventImpl *i) : d(i)
{
}

DocumentEvent::DocumentEvent(const DocumentEvent &other) : d(0)
{
	(*this) = other;
}

DocumentEvent::~DocumentEvent()
{
}

DocumentEvent &DocumentEvent::operator=(const DocumentEvent &other)
{
	if(d != other.d)
		d = other.d;

	return *this;
}

DocumentEvent &DocumentEvent::operator=(DocumentEventImpl *other)
{
	if(d != other)
		d = other;

	return *this;
}

Event DocumentEvent::createEvent(const DOMString &eventType)
{
	if(!d)
		return Event::null;

	return Event(d->createEvent(eventType));
}
		
// vim:ts=4:noet
