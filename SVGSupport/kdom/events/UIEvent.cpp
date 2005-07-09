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
#include "UIEvent.h"
#include "UIEventImpl.h"
#include "KeyboardEventImpl.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"

#include <kdom/data/EventsConstants.h>
#include "UIEvent.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin UIEvent::s_hashTable 5
 view			UIEventConstants::View		DontDelete|ReadOnly
 detail			UIEventConstants::Detail	DontDelete|ReadOnly
 keyCode		UIEventConstants::KeyCode	DontDelete|ReadOnly
@end
@begin UIEventProto::s_hashTable 2
 initUIEvent	UIEventConstants::InitUIEvent	DontDelete|Function 5
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("UIEvent", UIEventProto, UIEventProtoFunc)

Value UIEvent::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case UIEventConstants::View: 
			return safe_cache<AbstractView>(exec, view());
		case UIEventConstants::Detail:
			return Number(detail());
		case UIEventConstants::KeyCode:
			return Number(keyCode());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

Value UIEventProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(UIEvent)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case UIEventConstants::InitUIEvent:
		{
			DOMString typeArg = toDOMString(exec, args[0]);
			bool canBubbleArg = args[1].toBoolean(exec);
			bool cancelableArg = args[2].toBoolean(exec);
			AbstractView viewArg = ecma_cast<AbstractView>(exec, args[3], &toAbstractView);
			long detailArg = args[4].toInt32(exec);
			obj.initUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<UIEventImpl *>(d))

UIEvent UIEvent::null;

UIEvent::UIEvent() : Event()
{
}

UIEvent::UIEvent(UIEventImpl *i) : Event(i)
{
}

UIEvent::UIEvent(const UIEvent &other) : Event()
{
	(*this) = other;
}

UIEvent::UIEvent(const Event &other) : Event()
{
	(*this) = other;
}

UIEvent::~UIEvent()
{
}

UIEvent &UIEvent::operator=(const UIEvent &other)
{
	Event::operator=(other);
	return (*this);
}

KDOM_EVENT_DERIVED_ASSIGN_OP(UIEvent, Event, TypeUIEvent)

AbstractView UIEvent::view() const
{
	if(!d)
		return AbstractView::null;

	return AbstractView(impl->view());
}

long UIEvent::detail() const
{
	if(!d)
		return 0;
		
	return impl->detail();
}

int UIEvent::keyCode() const
{
	if(!d)
		return 0;

	if(impl->identifier() == TypeKeyboardEvent)
	        return static_cast<KeyboardEventImpl*>(impl)->keyCode();

	return impl->detail();
}

void UIEvent::initUIEvent(const DOMString &typeArg, bool canBubbleArg, bool cancelableArg, const AbstractView &viewArg, long detailArg)
{
	if(!d) // Only way to support initEvent()....
	{
		d = new UIEventImpl(TypeUIEvent);
		d->ref();
	}

	impl->initUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg.d, detailArg);
}

// vim:ts=4:noet
