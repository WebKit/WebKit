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

#ifndef KDOM_DocumentFragment_H
#define KDOM_DocumentFragment_H

#include <kdom/Node.h>

namespace KDOM
{
	class DocumentFragmentImpl;

	/**
	 * @short DocumentFragment is a "lightweight" or "minimal" Document
	 * object.
	 *
	 * It is very common to want to be able to extract a portion of a
	 * document's tree or to create a new fragment of a document.
	 * Imagine implementing a user command like cut or rearranging
	 * a document by moving fragments around. It is desirable to have an
	 * object which can hold such fragments and it is quite natural to
	 * use a Node for this purpose. While it is true that a Document
	 * object could fulfill this role, a Document object can
	 * potentially be a heavyweight object, depending on the underlying
	 * implementation. What is really needed for this is a very
	 * lightweight object. DocumentFragment is such an object.
	 *
	 * Furthermore, various operations -- such as inserting nodes
	 * as children of another Node -- may take DocumentFragment
	 * objects as arguments; this results in all the child nodes
	 * of the DocumentFragment being moved to the child list of this node.
	 *
	 * The children of a DocumentFragment node are zero or more nodes
	 * representing the tops of any sub-trees defining the structure
	 * of the document. DocumentFragment nodes do not need to be
	 * well-formed XML documents (although they do need to follow the
	 * rules imposed upon well-formed XML parsed entities, which can
	 * have multiple top nodes). For example, a DocumentFragment might
	 * have only one child and that child node could be a Text node.
	 * Such a structure model represents neither an HTML document nor
	 * a well-formed XML document.
	 *
	 * When a DocumentFragment is inserted into a Document (or indeed
	 * any other Node that may take children) the children of the
	 * DocumentFragment and not the DocumentFragment itself are
	 * inserted into the Node. This makes the DocumentFragment very
	 * useful when the user wishes to create nodes that are siblings;
	 * the DocumentFragment acts as the parent of these nodes so that
	 * the user can use the standard methods from the Node interface,
	 * such as Node.insertBefore and Node.appendChild. 
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class DocumentFragment : public Node 
	{
	public:
		DocumentFragment();
		explicit DocumentFragment(DocumentFragmentImpl *i);
		DocumentFragment(const DocumentFragment &other);
		DocumentFragment(const Node &other);
		virtual ~DocumentFragment();

		// Operators
		DocumentFragment &operator=(const DocumentFragment &other);
		DocumentFragment &operator=(const Node &other);

		// Internal
		KDOM_INTERNAL(DocumentFragment)
	};
};

#endif

// vim:ts=4:noet
