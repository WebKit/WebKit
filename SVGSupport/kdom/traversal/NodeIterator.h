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

#ifndef KDOM_NodeIterator_H
#define KDOM_NodeIterator_H

#include <kdom/Shared.h>
#include <kdom/ecma/DOMLookup.h>
#include <kdom/traversal/kdomtraversal.h>

namespace KDOM
{
	class Node;
	class NodeFilter;
	class NodeIteratorImpl;

	class NodeIterator
	{
	public:
		NodeIterator();
		explicit NodeIterator(NodeIteratorImpl *i);
		NodeIterator(const NodeIterator &other);
		virtual ~NodeIterator();

		// Operators
		NodeIterator &operator=(const NodeIterator &other);
		bool operator==(const NodeIterator &other) const;
		bool operator!=(const NodeIterator &other) const;

		Node root() const;
		unsigned long whatToShow() const;
		NodeFilter filter() const;
		bool expandEntityReferences() const;
		Node nextNode();
		Node previousNode();
		void detach();

		// Internal
		KDOM_INTERNAL_BASE(NodeIterator)

	private:
		NodeIteratorImpl *d;

	public: // EcmaScript section
		KDOM_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(NodeIteratorProto)
KDOM_IMPLEMENT_PROTOFUNC(NodeIteratorProtoFunc, NodeIterator)

#endif

// vim:ts=4:noet
