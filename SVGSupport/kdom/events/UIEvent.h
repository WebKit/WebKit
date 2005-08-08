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

#ifndef KDOM_UIEvent_H
#define KDOM_UIEvent_H

#include <kdom/events/Event.h>
#include <kdom/views/AbstractView.h>

namespace KDOM
{
	class UIEventImpl;
	class UIEvent : public Event 
	{
	public:
		UIEvent();
		explicit UIEvent(UIEventImpl *i);
		UIEvent(const UIEvent &other);
		UIEvent(const Event &other);
		virtual ~UIEvent();

		// Operators
		UIEvent &operator=(const UIEvent &other);
		UIEvent &operator=(const Event &other);

		// 'UIEvent' functions
		AbstractView view() const;

		long detail() const;

		/**
		 * Non-standard extension to support IE-style keyCode event property.
		 */
		int keyCode() const;

		void initUIEvent(const DOMString &typeArg, bool canBubbleArg, bool cancelableArg, const AbstractView &viewArg, long detailArg);

		// Internal
		KDOM_INTERNAL(UIEvent)

	public: // EcmaScript section
		KDOM_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(UIEventProto)
KDOM_IMPLEMENT_PROTOFUNC(UIEventProtoFunc, UIEvent)

#endif

// vim:ts=4:noet
