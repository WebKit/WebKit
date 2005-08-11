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

#ifndef KDOM_NodeList_H
#define KDOM_NodeList_H

namespace KDOM
{
	class Node;
	class NodeListImpl;

	/**
	 * @short The NodeList interface provides the abstraction of an ordered collection
	 * of nodes, without defining or constraining how this collection is implemented.
	 *
	 * NodeList objects in the DOM are live.
	 * The items in the NodeList are accessible via an integral index, starting from 0.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class NodeList
	{
	public:
		NodeList();
		explicit NodeList(NodeListImpl *i);
		NodeList(const NodeList &other);
		virtual ~NodeList();

		// Operators
		NodeList &operator=(const NodeList &other);
		bool operator==(const NodeList &other) const;
		bool operator!=(const NodeList &other) const;

		// 'NodeList' functions
		/**
		 * Returns the indexth item in the collection. If index is greater than
		 * or equal to the number of nodes in the list, this returns null.
		 *
		 * @param index Index into the collection.
		 *
		 * @returns The node at the indexth position in the NodeList, or null if
		 * that is not a valid index.
		 */
		Node item(unsigned long index) const;

		/**
		 * The number of nodes in the list. The range of valid child node indices
		 * is 0 to length-1 inclusive.
		 */
		unsigned long length() const;

		// Internal
		KDOM_INTERNAL_BASE(NodeList)

	protected:
		NodeListImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(NodeListProto)
KDOM_IMPLEMENT_PROTOFUNC(NodeListProtoFunc, NodeList)

#endif

// vim:ts=4:noet
