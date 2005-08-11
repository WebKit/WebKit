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

#ifndef KDOM_MouseEvent_H
#define KDOM_MouseEvent_H

#include <kdom/events/UIEvent.h>

namespace KDOM
{
	class MouseEventImpl;
	class MouseEvent : public UIEvent 
	{
	public:
		MouseEvent();
		explicit MouseEvent(MouseEventImpl *i);
		MouseEvent(const MouseEvent &other);
		MouseEvent(const Event &other);
		virtual ~MouseEvent();

		// Operators
		MouseEvent &operator=(const MouseEvent &other);
		MouseEvent &operator=(const Event &other);

		// 'MouseEvent' functions
		long screenX() const;
		long screenY() const;
		long clientX() const;
		long clientY() const;

		bool ctrlKey() const;
		bool shiftKey() const;
		bool altKey() const;
		bool metaKey() const;

		unsigned short button() const;

		EventTarget relatedTarget() const;

		void initMouseEvent(const DOMString &typeArg, bool canBubbleArg, bool cancelableArg, const AbstractView &viewArg, long detailArg, long screenXArg, long screenYArg, long clientXArg, long clientYArg, bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg, unsigned short buttonArg, const EventTarget &relatedTargetArg);

		// Internal
		KDOM_INTERNAL(MouseEvent)
		
	public: // EcmaScript section
		KDOM_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(MouseEventProto)
KDOM_IMPLEMENT_PROTOFUNC(MouseEventProtoFunc, MouseEvent)

#endif

// vim:ts=4:noet
