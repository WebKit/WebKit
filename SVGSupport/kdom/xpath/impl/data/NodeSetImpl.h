/*
    Copyright (C) 2004 Richard Moore <rich@kde.org>
    Copyright (C) 2004 Zack Rusin <zack@kde.org>
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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


#ifndef KDOM_XPath_NodeSetImpl_H
#define KDOM_XPath_NodeSetImpl_H

#include <qptrlist.h>

#include <kdom/xpath/impl/data/ValueImpl.h>


namespace KDOM
{
	class NodeImpl;

namespace XPath
{
	class BooleanImpl;
	class NumberImpl;
	class StringImpl;

	/**
	 * Internal implementation of a NodeSet Value.
	 * An unordered collection of nodes without duplicates
	 */
	class NodeSetImpl : public ValueImpl
	{
	public:
		NodeSetImpl();
		~NodeSetImpl();

		/**
		 * The count function returns the number of nodes in the argument
		 * node-set.
		 */
		int count() const;

		/**
		 * Returns the index of the current item in the argument node-set.
		 */
		int position() const;

		/*
		 * a node-set is true if and only if it is non-empty
		 */
		virtual BooleanImpl *toBoolean();

		/**
		 * A node-set is first converted to a string as if by a call to
		 * the string function and then converted in the same way as a
		 * string argument
		 */
		virtual NumberImpl  *toNumber();

		/**
		 * A node-set is converted to a string by returning the string-value of
		 * the node in the node-set that is first in document order. If the
		 * node-set is empty, an empty string is returned.
		 */
		virtual StringImpl  *toString();

		NodeImpl *next();
		NodeImpl *prev();
		NodeImpl *current() const;

		void append(NodeImpl *node);
		void remove(NodeImpl *node);

		bool compare(NodeSetImpl *other);

		void refNodes();
		void derefNodes();

	private:
		QPtrList<NodeImpl> m_storage;
	};
};
};
#endif
// vim:ts=4:noet
