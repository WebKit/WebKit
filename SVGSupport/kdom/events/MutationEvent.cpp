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

#include "Node.h"
#include "Ecma.h"
#include "NodeImpl.h"
#include "DOMException.h"
#include "MutationEvent.h"
#include "EventTargetImpl.h"
#include "DOMExceptionImpl.h"
#include "MutationEventImpl.h"

#include <kdom/data/EventsConstants.h>
#include "MutationEvent.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin MutationEvent::s_hashTable 7
 relatedNode		MutationEventConstants::RelatedNode			DontDelete|ReadOnly
 prevValue			MutationEventConstants::PrevValue			DontDelete|ReadOnly
 newValue			MutationEventConstants::NewValue			DontDelete|ReadOnly
 attrName			MutationEventConstants::AttrName			DontDelete|ReadOnly
 attrChange			MutationEventConstants::AttrChange			DontDelete|ReadOnly
@end
@begin MutationEventProto::s_hashTable 2
 initMutationEvent	MutationEventConstants::InitMutationEvent	DontDelete|Function 8
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("MutationEvent", MutationEventProto, MutationEventProtoFunc)

Value MutationEvent::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case MutationEventConstants::RelatedNode:
			return getDOMNode(exec, relatedNode());
		case MutationEventConstants::PrevValue:
			return getDOMString(prevValue());
		case MutationEventConstants::NewValue:
			return getDOMString(newValue());
		case MutationEventConstants::AttrName:
			return getDOMString(attrName());
		case MutationEventConstants::AttrChange:
			return Number(attrChange());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

Value MutationEventProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(MutationEvent)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case MutationEventConstants::InitMutationEvent:
		{
			DOMString typeArg = toDOMString(exec, args[0]);
			bool canBubbleArg = args[1].toBoolean(exec);
			bool cancelableArg = args[2].toBoolean(exec);
			Node relatedNodeArg = ecma_cast<Node>(exec, args[3], &toNode);
			DOMString prevValueArg = toDOMString(exec, args[4]);
			DOMString newValueArg = toDOMString(exec, args[5]);
			DOMString attrNameArg = toDOMString(exec, args[6]);
			unsigned short attrChangeArg = args[7].toUInt16(exec);

			obj.initMutationEvent(typeArg, canBubbleArg, cancelableArg, relatedNodeArg, prevValueArg, newValueArg, attrNameArg, attrChangeArg);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<MutationEventImpl *>(d))

MutationEvent MutationEvent::null;

MutationEvent::MutationEvent() : Event()
{
}

MutationEvent::MutationEvent(MutationEventImpl *i) : Event(i)
{
}

MutationEvent::MutationEvent(const MutationEvent &other) : Event()
{
	(*this) = other;
}

MutationEvent::MutationEvent(const Event &other) : Event()
{
	(*this) = other;
}

MutationEvent::~MutationEvent()
{
}

MutationEvent &MutationEvent::operator=(const MutationEvent &other)
{
	Event::operator=(other);
	return (*this);
}

KDOM_EVENT_DERIVED_ASSIGN_OP(MutationEvent, Event, TypeMutationEvent)

Node MutationEvent::relatedNode() const
{
	if(!d)
		return Node::null;

	return Node(impl->relatedNode());
}

DOMString MutationEvent::prevValue() const
{
	if(!d)
		return DOMString();

	return impl->prevValue();
}

DOMString MutationEvent::newValue() const
{
	if(!d)
		return DOMString();
	
	return impl->newValue();
}

DOMString MutationEvent::attrName() const
{
	if(!d)
		return DOMString();
	
	return impl->attrName();
}

unsigned short MutationEvent::attrChange() const
{
	if(!d)
		return 0;

	return impl->attrChange();
}

void MutationEvent::initMutationEvent(const DOMString &typeArg, bool canBubbleArg, bool cancelableArg, const Node &relatedNodeArg, const DOMString &prevValueArg, const DOMString &newValueArg, const DOMString &attrNameArg, unsigned short attrChangeArg)
{
	if(!d) // Only way to support initEvent()....
	{
		d = new MutationEventImpl(TypeMutationEvent);
		d->ref();
	}

	impl->initMutationEvent(typeArg, canBubbleArg, cancelableArg,
							static_cast<NodeImpl *>(relatedNodeArg.d),
							prevValueArg, newValueArg, attrNameArg, attrChangeArg);
}

// vim:ts=4:noet
