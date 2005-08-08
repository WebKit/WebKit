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

#ifndef KDOM_KeyboardEvent_H
#define KDOM_KeyboardEvent_H

#include <kdom/events/UIEvent.h>

/**
 * Introduced in DOM Level 3
 *
 * The KeyboardEvent interface provides specific contextual information
 * associated with Keyboard events.
 *
*/
namespace KDOM
{
	class KeyboardEventImpl;
	class KeyboardEvent : public UIEvent 
	{
	public:
		KeyboardEvent();
		explicit KeyboardEvent(KeyboardEventImpl *i);
		KeyboardEvent(const KeyboardEvent &other);
		KeyboardEvent(const Event &other);
		virtual ~KeyboardEvent();

		// Operators
		KeyboardEvent &operator=(const KeyboardEvent &other);
		KeyboardEvent &operator=(const Event &other);

		// 'KeyboardEvent' functions
		/**
		 * Holds the identifier of the key.
		 */
		DOMString keyIdentifier() const;

		/**
		 * Contains an indication of the location of they key on the device.
		 */
		unsigned long keyLocation() const;

		/**
		 * true if the control (Ctrl) key modifier is activated.
		 */
		bool ctrlKey() const;

		/**
		 * true if the shift (Shift) key modifier is activated.
		 */
		bool shiftKey() const;

		/**
		 * true if the alternative (Alt) key modifier is activated.
		 */
		bool altKey() const;

		/**
		 * true if the meta (Meta) key modifier is activated. 
		 */
		bool metaKey() const;

		bool getModifierState(const DOMString &keyIdentifierArg) const;

		void initKeyboardEvent(const DOMString &typeArg, bool canBubbleArg,
								bool cancelableArg, const AbstractView &viewArg,
								const DOMString &keyIdentifierArg,
								unsigned long keyLocationArg, bool ctrlKeyArg,
								bool shiftKeyArg, bool altKeyArg, bool metaKeyArg);

		// Internal
		KDOM_INTERNAL(KeyboardEvent)

	public: // EcmaScript section
		KDOM_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(KeyboardEventProto)
KDOM_IMPLEMENT_PROTOFUNC(KeyboardEventProtoFunc, KeyboardEvent)

#endif

// vim:ts=4:noet
