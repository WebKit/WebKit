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

#ifndef KDOM_CharacterData_H
#define KDOM_CharacterData_H

#include <kdom/Node.h>

namespace KDOM
{
	class DOMString;
	class CharacterDataImpl;

	/**
	 * @short The CharacterData interface extends Node with a set of
	 * attributes and methods for accessing character data in the DOM.
	 *
	 * For clarity this set is defined here rather than on each object
	 * that uses these attributes and methods. No DOM objects correspond
	 * directly to CharacterData, though Text and others do inherit the
	 * interface from it. All offsets in this interface start from 0.
	 *
	 * As explained in the DOMString interface, text strings in the DOM
	 * are represented in UTF-16, i.e. as a sequence of 16-bit units.
	 * In the following, the term 16-bit units is used whenever necessary
	 * to indicate that indexing on CharacterData is done in 16-bit units. 
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class CharacterData : public Node 
	{
	public:
		CharacterData();
		explicit CharacterData(CharacterDataImpl *p);
		CharacterData(const CharacterData &other);
		CharacterData(const Node &other);
		virtual ~CharacterData();

		// Operators
		CharacterData &operator=(const CharacterData &other);
		CharacterData &operator=(const Node &other);

		// 'CharacterData' functions
		/**
		 * The character data of the node that implements this interface.
		 * The DOM implementation may not put arbitrary limits on the amount
		 * of data that may be stored in a CharacterData node. However,
		 * implementation limits may mean that the entirety of a node's data
		 * may not fit into a single DOMString. In such cases, the user may
		 * call substringData to retrieve the data in appropriately sized pieces.
		 */
		DOMString data() const;
		void setData(const DOMString &data);

		/**
		 * he number of 16-bit units that are available through data and the
		 * substringData method below. This may have the value zero, i.e.,
		 * CharacterData nodes may be empty.
		 */
		unsigned long length() const;

		/**
		 * Extracts a range of data from the node.
		 *
		 * @param offset Start offset of substring to extract.
		 * @param count The number of 16-bit units to extract.
		 *
		 * @returns The specified substring. If the sum of offset and count
		 * exceeds the length, then all 16-bit units to the end of the data
		 * are returned.
		 */
		DOMString substringData(unsigned long offset, unsigned long count);

		/**
		 * Append the string to the end of the character data of the node.
		 * Upon success, data provides access to the concatenation of data
		 * and the DOMString specified.
		 *
		 * @param arg The DOMString to append.
		 */
		void appendData(const DOMString &arg);

		/**
		 * Insert a string at the specified 16-bit unit offset.
		 *
		 * @param offset The character offset at which to insert.
		 * @param arg The DOMString to append.
		 */
		void insertData(unsigned long offset, const DOMString &arg);

		/**
		 * Remove a range of 16-bit units from the node. Upon success,
		 * data and length reflect the change.
		 *
		 * @param offset The offset from which to start removing.
		 * @param count The number of 16-bit units to delete. If the sum of
		 * offset and count exceeds length then all 16-bit units from offset
		 * to the end of the data are deleted.
		 */
		void deleteData(unsigned long offset, unsigned long count);

		/**
		 * Replace the characters starting at the specified 16-bit unit
		 * offset with the specified string.
		 *
		 * @param offset The offset from which to start replacing.
		 * @param count The number of 16-bit units to replace. If the sum
		 * of offset and count exceeds length, then all 16-bit units to
		 * the end of the data are replaced; (i.e., the effect is the same
		 * as a remove method call with the same range, followed by an
		 * append method invocation).
		 * @param arg The DOMString with which the range must be replaced.
		 */
		void replaceData(unsigned long offset, unsigned long count, const DOMString &arg);

		// Internal
		KDOM_INTERNAL(CharacterData)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};
};

KDOM_DEFINE_PROTOTYPE(CharacterDataProto)
KDOM_IMPLEMENT_PROTOFUNC(CharacterDataProtoFunc, CharacterData)

#endif

// vim:ts=4:noet
