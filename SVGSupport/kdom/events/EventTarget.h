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

#ifndef KDOM_EventTarget_H
#define KDOM_EventTarget_H

#include <kdom/kdom.h>
#include <kdom/ecma/DOMLookup.h>
#include <kdom/events/kdomevents.h>

namespace KDOM
{
	class Event;
	class DOMString;
	class EventListener;
	class DOMImplementation;

	class EventTargetImpl;
	class EventTarget 
	{
	public:
		EventTarget();
		explicit EventTarget(EventTargetImpl *i);
		EventTarget(const EventTarget &other);
		virtual ~EventTarget();

		// Operators
		EventTarget &operator=(const EventTarget &other);
		bool operator==(const EventTarget &other) const;
		bool operator!=(const EventTarget &other) const;

		// 'EventTarget' functions
		void addEventListener(const DOMString &type, const EventListener &listener, bool useCapture);
		void removeEventListener(const DOMString &type, const EventListener &listener, bool useCapture);
		bool dispatchEvent(const Event &evt);

		// Internal
		KDOM_INTERNAL_BASE(EventTarget)

		// This is "the" d-ptr, which is shared thorough kdom's nodes
		EventTargetImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_FORWARDPUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
}

KDOM_DEFINE_PROTOTYPE(EventTargetProto)
KDOM_IMPLEMENT_PROTOFUNC(EventTargetProtoFunc, EventTarget)

#endif

// vim:ts=4:noet
