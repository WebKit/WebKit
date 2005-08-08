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

#ifndef KDOM_DocumentTraversal_H
#define KDOM_DocumentTraversal_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class NodeFilter;
	class NodeIterator;
	class TreeWalker;
	class Node;
	class DocumentTraversalImpl;

	/**
	 * @short DocumentTraversal contains methods that create iterators and
	 * tree-walkers to traverse a node and its children in document order
	 * (depth first, pre-order traversal, which is equivalent to the order in
	 * which the start tags occur in the text representation of the document).
	 *
	 * In DOMs which support the Traversal feature, DocumentTraversal will
	 * be implemented by the same objects that implement the Document interface.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class DocumentTraversal 
	{
	public:
		DocumentTraversal();
		explicit DocumentTraversal(DocumentTraversalImpl *i);
		DocumentTraversal(const DocumentTraversal &other);
		virtual ~DocumentTraversal();

		// Operators
		DocumentTraversal &operator=(const DocumentTraversal &other);
		DocumentTraversal &operator=(DocumentTraversalImpl *other);

		// 'DocumentTraversal' functions
		/**
		 * Create a new NodeIterator over the subtree rooted at the specified
		 * node.
		 *
		 * @param root The node which will be iterated together with its
		 * children. The iterator is initially positioned just before this
		 * node. The whatToShow flags and the filter, if any, are not
		 * considered when setting this position. The root must not be null.
		 * @param whatToShow This flag specifies which node types may appear
		 * in the logical view of the tree presented by the iterator. See the
		 * description of NodeFilter for the set of possible SHOW_ values.
		 * These flags can be combined using OR.
		 * @param filter The NodeFilter to be used with this TreeWalker, or null
		 * to indicate no filter.
		 * @param entityReferenceExpansion The value of this flag determines
		 * whether entity reference nodes are expanded.
		 *
		 * @returns The newly created NodeIterator.
		 */
		NodeIterator createNodeIterator(const Node &root, short whatToShow, const NodeFilter &filter, bool entityReferenceExpansion) const;

		/**
		 * Create a new TreeWalker over the subtree rooted at the specified
		 * node.
		 *
		 * @param root The node which will serve as the root for the
		 * TreeWalker. The whatToShow flags and the NodeFilter are not
		 * considered when setting this value; any node type will be accepted
		 * as the root. The currentNode of the TreeWalker is initialized to
		 * this node, whether or not it is visible. The root functions as a
		 * stopping point for traversal methods that look upward in the
		 * document structure, such as parentNode and nextNode. The root
		 * must not be null.
		 * @param whatToShow This flag specifies which node types may appear
		 * in the logical view of the tree presented by the tree-walker. See
		 * the description of NodeFilter for the set of possible SHOW_ values.
		 * These flags can be combined using OR.
		 * @param filter The NodeFilter to be used with this TreeWalker, or null
		 * to indicate no filter.
		 * @param entityReferenceExpansion If this flag is false, the contents
		 * of EntityReference nodes are not presented in the logical view.
		 *
		 * @returns The newly created TreeWalker.
		 */
		TreeWalker createTreeWalker(const Node &root, short whatToShow, const NodeFilter &filter, bool entityReferenceExpansion) const;

		// Internal
		KDOM_INTERNAL(DocumentTraversal)

	public:
		DocumentTraversalImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(DocumentTraversalProto)
KDOM_IMPLEMENT_PROTOFUNC(DocumentTraversalProtoFunc, DocumentTraversal)

#endif

// vim:ts=4:noet
