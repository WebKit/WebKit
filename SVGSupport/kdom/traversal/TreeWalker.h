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

#ifndef KDOM_TreeWalker_H
#define KDOM_TreeWalker_H

#include <kdom/ecma/DOMLookup.h>
#include <kdom/traversal/kdomtraversal.h>

namespace KDOM
{
	class Node;
	class NodeFilter;
	class TreeWalkerImpl;

	class TreeWalker
	{
	public:
		TreeWalker();
		explicit TreeWalker(TreeWalkerImpl *p);
		TreeWalker(const TreeWalker &other);
		virtual ~TreeWalker();

		// Operators
		TreeWalker &operator=(const TreeWalker &other);
		bool operator==(const TreeWalker &other) const;
		bool operator!=(const TreeWalker &other) const;

		Node root() const;
		short whatToShow() const;
		NodeFilter filter() const;
		bool expandEntityReferences() const;
		void setCurrentNode(const Node &currentNode);
		Node currentNode() const;

		Node parentNode();
		Node firstChild();
		Node lastChild();
		Node previousSibling();
		Node nextSibling();
		Node previousNode();
		Node nextNode();

		// Internal
		KDOM_INTERNAL_BASE(TreeWalker)

	private:
		TreeWalkerImpl *d;

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);		
	};
};

KDOM_DEFINE_PROTOTYPE(TreeWalkerProto)
KDOM_IMPLEMENT_PROTOFUNC(TreeWalkerProtoFunc, TreeWalker)

#endif

// vim:ts=4:noet
