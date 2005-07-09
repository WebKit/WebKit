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

#ifndef KDOM_TreeWalkerImpl_H
#define KDOM_TreeWalkerImpl_H

#include <kdom/traversal/impl/TraversalImpl.h>

namespace KDOM
{
	class TreeWalkerImpl : public TraversalImpl
	{
	public:
		TreeWalkerImpl(NodeImpl *rootNode, short whatToShow,
					   NodeFilterImpl *nodeFilter,
					   bool expandEntityReferences);
		virtual ~TreeWalkerImpl();

		NodeImpl *currentNode() const { return m_current; }
		void setCurrentNode(NodeImpl *);

		NodeImpl *parentNode();
		NodeImpl *firstChild();
		NodeImpl *lastChild();
		NodeImpl *previousSibling();
		NodeImpl *nextSibling();
		NodeImpl *previousNode();
		NodeImpl *nextNode();

	private:
		TreeWalkerImpl(const TreeWalkerImpl &);
		TreeWalkerImpl &operator=(const TreeWalkerImpl &);

	private:
		NodeImpl *m_current;
	};
};

#endif

// vim:ts=4:noet
