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

#ifndef KDOM_KeyboardEventImpl_H
#define KDOM_KeyboardEventImpl_H

#include <kdom/events/impl/UIEventImpl.h>

class QKeyEvent;

// Introduced in DOM Level 3
namespace KDOM
{
	class EventTargetImpl;
	class AbstractViewImpl;
	class KeyboardEventImpl : public UIEventImpl
	{
	public:
		KeyboardEventImpl(EventImplType identifier);
		virtual ~KeyboardEventImpl();

		void initKeyboardEvent(DOMStringImpl *typeArg, bool canBubbleArg,
							   bool cancelableArg, AbstractViewImpl *viewArg,
							   DOMStringImpl *keyIdentifierArg,
							   unsigned long keyLocationArg,
							   bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg,
							   bool metaKeyArg);

		void initKeyboardEvent(QKeyEvent *key);

		bool ctrlKey() const;
		bool shiftKey() const;
		bool altKey() const;
		bool metaKey() const;

		DOMStringImpl *keyIdentifier() const {  return m_keyIdentifier; }
		unsigned long keyLocation() const { return m_keyLocation; }
    
		bool getModifierState(DOMStringImpl *keyIdentifierArg) const;
    
		QKeyEvent *qKeyEvent() const { return m_keyEvent; }

		int keyCode() const; // key code for keydown and keyup, character for other events
		int charCode() const;
    
		virtual long which() const;

	private:
		QKeyEvent *m_keyEvent;
		DOMStringImpl *m_keyIdentifier;
		unsigned long m_keyLocation;
		unsigned long m_virtKeyVal;
		bool m_ctrlKey : 1;
		bool m_altKey : 1;
		bool m_shiftKey : 1;
		bool m_metaKey : 1;
		bool m_numPad : 1;
	};
};

#endif

// vim:ts=4:noet
