/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
              (C) 2001 Peter Kelly (pmk@post.com)

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

#include "kdom.h"
#include "NodeImpl.h"
#include "kdomtraversal.h"
#include "TreeWalkerImpl.h"
#include "DOMExceptionImpl.h"

using namespace KDOM;

TreeWalkerImpl::TreeWalkerImpl(NodeImpl *rootNode, short whatToShow,
							   NodeFilterImpl *nodeFilter,
							   bool expandEntityReferences)
: TraversalImpl(rootNode, whatToShow, nodeFilter, expandEntityReferences),
m_current(rootNode)
{
	if(currentNode())
		currentNode()->ref();
}

TreeWalkerImpl::~TreeWalkerImpl()
{
	if(currentNode())
		currentNode()->deref();
}

void TreeWalkerImpl::setCurrentNode(NodeImpl *node)
{
	if(!node)
		throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);

	if(node == m_current)
		return;

	NodeImpl *old = m_current;
	m_current = node;
	if(m_current)
		m_current->ref();
	if(old)
		old->deref();
}


NodeImpl *TreeWalkerImpl::parentNode()
{
	NodeImpl *node = findParentNode(currentNode());
	if(node)
		setCurrentNode(node);

	return node;
}

NodeImpl *TreeWalkerImpl::firstChild()
{
	NodeImpl *node = findFirstChild(currentNode());
	if(node)
		setCurrentNode(node);

	return node;
}

NodeImpl *TreeWalkerImpl::lastChild()
{
	NodeImpl *node = findLastChild(currentNode());
	if(node)
		setCurrentNode(node);

	return node;
}

NodeImpl *TreeWalkerImpl::previousSibling()
{
	NodeImpl *node = findPreviousSibling(currentNode());
	if(node)
		setCurrentNode(node);

	return node;
}

NodeImpl *TreeWalkerImpl::nextSibling()
{
	NodeImpl *node = findNextSibling(currentNode());
	if(node)
		setCurrentNode(node);

	return node;
}

NodeImpl *TreeWalkerImpl::previousNode()
{
	NodeImpl *node = findPreviousNode(currentNode());
	if(node)
		setCurrentNode(node);

	return node;
}

NodeImpl *TreeWalkerImpl::nextNode()
{
	NodeImpl *node = findNextNode(currentNode());
	if(node)
		setCurrentNode(node);

	return node;
}

// vim:ts=4:noet
