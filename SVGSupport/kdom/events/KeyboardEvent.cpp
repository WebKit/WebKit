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

#include "Node.h"
#include "Ecma.h"
#include "KeyboardEvent.h"
#include "EventTarget.h"
#include "DOMException.h"
#include "KeyboardEventImpl.h"
#include "DOMExceptionImpl.h"

#include <kdom/data/EventsConstants.h>
#include "KeyboardEvent.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin KeyboardEvent::s_hashTable 7
 keyIdentifier	KeyboardEventConstants::KeyIdentifier	DontDelete|ReadOnly
 keyLocation	KeyboardEventConstants::KeyLocation		DontDelete|ReadOnly
 ctrlKey		KeyboardEventConstants::CtrlKey			DontDelete|ReadOnly
 shiftKey		KeyboardEventConstants::ShiftKey		DontDelete|ReadOnly
 altKey			KeyboardEventConstants::AltKey			DontDelete|ReadOnly
 metaKey		KeyboardEventConstants::MetaKey			DontDelete|ReadOnly
@end
@begin KeyboardEventProto::s_hashTable 3
 getModifierState		KeyboardEventConstants::GetModifierState	DontDelete|Function 1
 initKeyboardEvent	KeyboardEventConstants::InitKeyboardEvent	DontDelete|Function 15
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("KeyboardEvent", KeyboardEventProto, KeyboardEventProtoFunc)

ValueImp *KeyboardEvent::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case KeyboardEventConstants::CtrlKey:
			return KJS::Boolean(ctrlKey());
		case KeyboardEventConstants::ShiftKey:
			return KJS::Boolean(shiftKey());
		case KeyboardEventConstants::AltKey:
			return KJS::Boolean(altKey());
		case KeyboardEventConstants::MetaKey:
			return KJS::Boolean(metaKey());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

ValueImp *KeyboardEventProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(KeyboardEvent)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case KeyboardEventConstants::GetModifierState:
		{
			DOMString keyIdentifierArg = toDOMString(exec, args[0]);
			return KJS::Boolean(obj.getModifierState(keyIdentifierArg));
		}
		case KeyboardEventConstants::InitKeyboardEvent:
		{
			DOMString typeArg = toDOMString(exec, args[0]);
			bool canBubbleArg = args[1]->toBoolean(exec);
			bool cancelableArg = args[2]->toBoolean(exec);
			AbstractView viewArg = ecma_cast<AbstractView>(exec, args[3], &toAbstractView);
			DOMString keyIdentifierArg = toDOMString(exec, args[4]);
			long keyLocationArg = args[5]->toInt32(exec);
			bool ctrlKeyArg = args[6]->toBoolean(exec);
			bool altKeyArg = args[7]->toBoolean(exec);
			bool shiftKeyArg = args[8]->toBoolean(exec);
			bool metaKeyArg = args[9]->toBoolean(exec);

			obj.initKeyboardEvent(typeArg, canBubbleArg, cancelableArg, viewArg, keyIdentifierArg, keyLocationArg, ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<KeyboardEventImpl *>(d))

KeyboardEvent KeyboardEvent::null;

KeyboardEvent::KeyboardEvent() : UIEvent()
{
}

KeyboardEvent::KeyboardEvent(KeyboardEventImpl *i) : UIEvent(i)
{
}

KeyboardEvent::KeyboardEvent(const KeyboardEvent &other) : UIEvent()
{
	(*this) = other;
}

KeyboardEvent::KeyboardEvent(const Event &other) : UIEvent()
{
	(*this) = other;
}

KeyboardEvent::~KeyboardEvent()
{
}

KeyboardEvent &KeyboardEvent::operator=(const KeyboardEvent &other)
{
	UIEvent::operator=(other);
	return (*this);
}

KDOM_EVENT_DERIVED_ASSIGN_OP(KeyboardEvent, UIEvent, TypeKeyboardEvent)

DOMString KeyboardEvent::keyIdentifier() const
{
	if(!d)
		return 0;

	return DOMString(impl->keyIdentifier());
}

unsigned long KeyboardEvent::keyLocation() const
{
	if(!d)
		return 0;

	return impl->keyLocation();
}

bool KeyboardEvent::ctrlKey() const
{
	if(!d)
		return false;

	return impl->ctrlKey();
}

bool KeyboardEvent::shiftKey() const
{
	if(!d)
		return false;

	return impl->shiftKey();
}

bool KeyboardEvent::altKey() const
{
	if(!d)
		return false;

	return impl->altKey();
}

bool KeyboardEvent::metaKey() const
{
	if(!d)
		return false;

	return impl->metaKey();
}

bool KeyboardEvent::getModifierState(const DOMString &keyIdentifierArg) const
{
	if(!d)
		return false;

	return impl->getModifierState(keyIdentifierArg);
}

void KeyboardEvent::initKeyboardEvent(const DOMString &typeArg, bool canBubbleArg,
								bool cancelableArg, const AbstractView &viewArg,
								const DOMString &keyIdentifierArg,
								unsigned long keyLocationArg, bool ctrlKeyArg,
								bool shiftKeyArg, bool altKeyArg, bool metaKeyArg)
{
	if(!d) // Only way to support initEvent()....
	{
		d = new KeyboardEventImpl(TypeKeyboardEvent);
		d->ref();
	}

	impl->initKeyboardEvent(typeArg, canBubbleArg, cancelableArg, viewArg.d,
						 keyIdentifierArg, keyLocationArg, ctrlKeyArg,
						 altKeyArg, shiftKeyArg, metaKeyArg);
}

// vim:ts=4:noet
