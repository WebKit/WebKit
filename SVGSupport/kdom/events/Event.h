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

#ifndef KDOM_Event_H
#define KDOM_Event_H

#include <kdom/kdom.h>
#include <kdom/Shared.h>
#include <kdom/ecma/DOMLookup.h>
#include <kdom/events/kdomevents.h>

namespace KDOM
{
	class EventTarget;
	class DOMImplementation;

	class EventImpl;
	class EventTargetImpl;
	class Event 
	{
	public:
		Event();
		explicit Event(EventImpl *i);
		Event(const Event &other);
		virtual ~Event();

		// Operators
		Event &operator=(const Event &other);
		bool operator==(const Event &other) const;
		bool operator!=(const Event &other) const;

		// 'Event' functions
		DOMString type() const;

		EventTarget target() const;
		EventTarget currentTarget() const;

		unsigned short eventPhase() const;

		bool bubbles() const;
		bool cancelable() const;

		DOMTimeStamp timeStamp() const;

		void stopPropagation();
		void preventDefault();
		void initEvent(const DOMString &eventTypeArg, bool canBubbleArg, bool cancelableArg);

		// Internal
		KDOM_INTERNAL_BASE(Event)
		
	public:
		EventImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_CAST

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};

	KDOM_DEFINE_CAST(Event)

// template for easing assigning of Events to derived classes.
// EVENT_TYPE is used to make sure the type is correct, otherwise
// the d pointer is set to 0. SUPER indicates the operator= to call
// when the eventtype is correct.
#define KDOM_EVENT_DERIVED_ASSIGN_OP(T, SUPER, EVENT_TYPE) \
T &T::operator=(const KDOM::Event &other) \
{ \
	KDOM::EventImpl *ohandle = static_cast<KDOM::EventImpl *>(other.handle()); \
	if(d != ohandle) { \
		if(!ohandle || ohandle->identifier() != EVENT_TYPE) { \
			if(d) d->deref(); \
			d = 0; \
		} \
		else \
			KDOM::SUPER::operator=(other); \
	} \
	return *this; \
}

};

KDOM_DEFINE_PROTOTYPE(EventProto)
KDOM_IMPLEMENT_PROTOFUNC(EventProtoFunc, Event)

#endif

// vim:ts=4:noet
