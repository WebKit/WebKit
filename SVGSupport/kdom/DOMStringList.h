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

#ifndef KDOM_StringList_H
#define KDOM_StringList_H

namespace KDOM
{
	class DOMString;
	class DOMStringListImpl;

	/**
	 * @short The DOMStringList interface provides the abstraction
	 * of an ordered collection of DOMString values, without defining
	 * or constraining how this collection is implemented. The items
	 * in the DOMStringList are accessible via an integral index,
	 * starting from 0.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class DOMStringList
	{
	public:
		DOMStringList();
		explicit DOMStringList(DOMStringListImpl *i);
		DOMStringList(const DOMStringList &other);
		virtual ~DOMStringList();

		DOMStringList &operator=(const DOMStringList &other);
		bool operator==(const DOMStringList &other) const;
		bool operator!=(const DOMStringList &other) const;

		// 'DOMStringList' functions
		/**
		 * Returns the indexth item in the collection. If index is
		 * greater than or equal to the number of DOMStrings in the
		 * list, this returns null.
		 *
		 * @param index Index into the collection.
		 *
		 * @returns The DOMString at the indexth position in the
		 * DOMStringList, or null if that is not a valid index.
		 */
		DOMString item(unsigned long index) const;

		/**
		 * The number of DOMStrings in the list. The range of valid child
		 * node indices is 0 to length-1 inclusive.
		 */
		unsigned long length() const;

		/**
		 * Test if a string is part of this DOMStringList.
		 *
		 * @param str The string to look for.
		 *
		 * @returns true if the string has been found, false otherwise.
		 */
		bool contains(const DOMString &str) const;

		// Internal
		KDOM_INTERNAL_BASE(DOMStringList)

	protected:
		DOMStringListImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(DOMStringListProto)
KDOM_IMPLEMENT_PROTOFUNC(DOMStringListProtoFunc, DOMStringList)

#endif

// vim:ts=4:noet
