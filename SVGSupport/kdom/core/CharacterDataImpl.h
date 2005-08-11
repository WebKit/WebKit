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

#ifndef KDOM_CharacterDataImpl_H
#define KDOM_CharacterDataImpl_H

#include <kdom/impl/NodeImpl.h>

namespace KDOM
{
	class CharacterData;
	class CharacterDataImpl : public NodeImpl
	{
	public:
		CharacterDataImpl(DocumentImpl *doc);
		virtual ~CharacterDataImpl();

		void checkCharDataOperation(CharacterDataImpl *node, const unsigned long offset);

		// 'CharacterData' functions
		DOMStringImpl *data() const;
		void setData(DOMStringImpl *data);

		virtual DOMString nodeValue() const;
		virtual void setNodeValue(const DOMString &nodeValue);

		virtual DOMString textContent() const; // DOM3

		DOMStringImpl *substringData(unsigned long offset, unsigned long count);

		void appendData(DOMStringImpl *arg);
		void insertData(unsigned long offset, DOMStringImpl *arg);
		void deleteData(unsigned long offset, unsigned long count);
		void replaceData(unsigned long offset, unsigned long count, DOMStringImpl *arg);

		unsigned long length() const;

		virtual void normalize();
		
		virtual bool containsOnlyWhitespace() const;

		// Internal
		void dispatchModifiedEvent(const DOMString &prevValue);

	protected:
		DOMStringImpl *str;
	};
};

#endif

// vim:ts=4:noet
