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

#ifndef KDOM_NodeFilter_H
#define KDOM_NodeFilter_H

#include <kdom/Shared.h>
#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class Node;
	class NodeFilterImpl;

	class NodeFilterCondition : public Shared
	{
	public:
		NodeFilterCondition();
		virtual short acceptNode(const Node &) const;
	};

	/**
	 * @short Filters are objects that know how to "filter out" nodes. If
	 * a NodeIterator or TreeWalker is given a NodeFilter, it applies the
	 * filter before it returns the next node. If the filter says to accept
	 * the node, the traversal logic returns it; otherwise, traversal looks
	 * for the next node and pretends that the node that was rejected was
	 * not there.
	 * The DOM does not provide any filters. NodeFilter is just an interface
	 * that users can implement to provide their own filters.
	 * NodeFilters do not need to know how to traverse from node to node, nor
	 * do they need to know anything about the data structure that is being
	 * traversed. This makes it very easy to write filters, since the only
	 * thing they have to know how to do is evaluate a single node. One
	 * filter may be used with a number of different kinds of traversals,
	 * encouraging code reuse.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class NodeFilter 
	{
	public:
		NodeFilter();
		explicit NodeFilter(NodeFilterImpl *i);
		NodeFilter(const NodeFilter &other);
		virtual ~NodeFilter();

		// Operators
		NodeFilter &operator=(const NodeFilter &other);
		bool operator==(const NodeFilter &other) const;
		bool operator!=(const NodeFilter &other) const;

		// 'NodeFilter' functions
		/**
		 * Test whether a specified node is visible in the logical view of
		 * a TreeWalker or NodeIterator. This function will be called by
		 * the implementation of TreeWalker and NodeIterator; it is not normally
		 * called directly from user code. (Though you could do so if you wanted
		 * to use the same filter to guide your own application logic.)
		 *
		 * @param n The node to check to see if it passes the filter or not.
		 *
		 * @returns a constant to determine whether the node is accepted,
		 * rejected, or skipped, as defined above.
		 */
		short acceptNode(const Node &n) const;

		// Internal
		KDOM_INTERNAL_BASE(NodeFilter)

	protected:
		NodeFilterImpl *d;
	};
};

#endif

// vim:ts=4:noet
