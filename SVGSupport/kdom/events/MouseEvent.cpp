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
#include "MouseEvent.h"
#include "EventTarget.h"
#include "DOMException.h"
#include "MouseEventImpl.h"
#include "DOMExceptionImpl.h"

#include <kdom/data/EventsConstants.h>
#include "MouseEvent.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin MouseEvent::s_hashTable 11
 screenX		MouseEventConstants::ScreenX		DontDelete|ReadOnly
 screenY		MouseEventConstants::ScreenY		DontDelete|ReadOnly
 clientX		MouseEventConstants::ClientX		DontDelete|ReadOnly
 clientY		MouseEventConstants::ClientY		DontDelete|ReadOnly
 ctrlKey		MouseEventConstants::CtrlKey		DontDelete|ReadOnly
 shiftKey		MouseEventConstants::ShiftKey		DontDelete|ReadOnly
 altKey			MouseEventConstants::AltKey			DontDelete|ReadOnly
 metaKey		MouseEventConstants::MetaKey		DontDelete|ReadOnly
 button			MouseEventConstants::Button			DontDelete|ReadOnly
 relatedTarget	MouseEventConstants::RelatedTarget	DontDelete|ReadOnly
@end
@begin MouseEventProto::s_hashTable 2
 initMouseEvent	MouseEventConstants::InitMouseEvent	DontDelete|Function 15
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("MouseEvent", MouseEventProto, MouseEventProtoFunc)

Value MouseEvent::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case MouseEventConstants::ScreenX:
			return Number(screenX());
		case MouseEventConstants::ScreenY:
			return Number(screenY());
		case MouseEventConstants::ClientX:
			return Number(clientX());
		case MouseEventConstants::ClientY:
			return Number(clientY());
		case MouseEventConstants::CtrlKey:
			return KJS::Boolean(ctrlKey());
		case MouseEventConstants::ShiftKey:
			return KJS::Boolean(shiftKey());
		case MouseEventConstants::AltKey:
			return KJS::Boolean(altKey());
		case MouseEventConstants::MetaKey:
			return KJS::Boolean(metaKey());
		case MouseEventConstants::Button:
		{
			// Tricky. The DOM (and khtml) use 0 for LMB, 1 for MMB and 2 for RMB
			// but MSIE uses 1=LMB, 2=RMB, 4=MMB, as a bitfield
			int domButton = button();
			int button = domButton==0 ? 1 : domButton==1 ? 4 : domButton==2 ? 2 : 0;
			return Number((unsigned int)button);
		}
		case MouseEventConstants::RelatedTarget:
			return safe_cache<EventTarget>(exec, relatedTarget());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

Value MouseEventProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(MouseEvent)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case MouseEventConstants::InitMouseEvent:
		{
			DOMString typeArg = toDOMString(exec, args[0]);
			bool canBubbleArg = args[1].toBoolean(exec);
			bool cancelableArg = args[2].toBoolean(exec);
			AbstractView viewArg = ecma_cast<AbstractView>(exec, args[3], &toAbstractView);
			long detailArg = args[4].toInt32(exec);
			long screenXArg = args[5].toInt32(exec);
			long screenYArg = args[6].toInt32(exec);
			long clientXArg = args[7].toInt32(exec);
			long clientYArg = args[8].toInt32(exec);
			bool ctrlKeyArg = args[9].toBoolean(exec);
			bool altKeyArg = args[10].toBoolean(exec);
			bool shiftKeyArg = args[11].toBoolean(exec);
			bool metaKeyArg = args[12].toBoolean(exec);
			unsigned short buttonArg = args[13].toUInt16(exec);
			Node relatedTargetArg = ecma_cast<Node>(exec, args[14], &toNode);

			obj.initMouseEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg, screenXArg, screenYArg, clientXArg, clientYArg, ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg, buttonArg, relatedTargetArg);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<MouseEventImpl *>(d))

MouseEvent MouseEvent::null;

MouseEvent::MouseEvent() : UIEvent()
{
}

MouseEvent::MouseEvent(MouseEventImpl *i) : UIEvent(i)
{
}

MouseEvent::MouseEvent(const MouseEvent &other) : UIEvent()
{
	(*this) = other;
}

MouseEvent::MouseEvent(const Event &other) : UIEvent()
{
	(*this) = other;
}

MouseEvent::~MouseEvent()
{
}

MouseEvent &MouseEvent::operator=(const MouseEvent &other)
{
	UIEvent::operator=(other);
	return (*this);
}

KDOM_EVENT_DERIVED_ASSIGN_OP(MouseEvent, UIEvent, TypeMouseEvent)

long MouseEvent::screenX() const
{
	if(!d)
		return 0;

	return impl->screenX();
}

long MouseEvent::screenY() const
{
	if(!d)
		return 0;

	return impl->screenY();
}

long MouseEvent::clientX() const
{
	if(!d)
		return 0;

	return impl->clientX();
}

long MouseEvent::clientY() const
{
	if(!d)
		return 0;

	return impl->clientY();
}

bool MouseEvent::ctrlKey() const
{
	if(!d)
		return false;
	
	return impl->ctrlKey();
}

bool MouseEvent::shiftKey() const
{
	if(!d)
		return false;

	return impl->shiftKey();
}

bool MouseEvent::altKey() const
{
	if(!d)
		return false;

	return impl->altKey();
}

bool MouseEvent::metaKey() const
{
	if(!d)
		return false;

	return impl->metaKey();
}

unsigned short MouseEvent::button() const
{
	if(!d)
		return 0;

	return impl->button();
}

EventTarget MouseEvent::relatedTarget() const
{
	if(!d)
		return EventTarget::null;
		
	return EventTarget(impl->relatedTarget());
}

void MouseEvent::initMouseEvent(const DOMString &typeArg, bool canBubbleArg, bool cancelableArg, const AbstractView &viewArg, long detailArg, long screenXArg, long screenYArg, long clientXArg, long clientYArg, bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg, unsigned short buttonArg, const EventTarget &relatedTargetArg)
{
	if(!d) // Only way to support initEvent()....
	{
		d = new MouseEventImpl(TypeMouseEvent);
		d->ref();
	}

	impl->initMouseEvent(typeArg, canBubbleArg, cancelableArg, viewArg.d,
						 detailArg, screenXArg, screenYArg, clientXArg,
						 clientYArg, ctrlKeyArg, altKeyArg, shiftKeyArg,
						 metaKeyArg, buttonArg, relatedTargetArg.d);
}

// vim:ts=4:noet
