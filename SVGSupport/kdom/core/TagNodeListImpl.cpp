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

#include "NodeImpl.h"
#include "TagNodeListImpl.h"

using namespace KDOM;
	
TagNodeListImpl::TagNodeListImpl(NodeImpl *refNode, const DOMString &name, const DOMString &namespaceURI) : NodeListImpl(refNode)
{
	m_name = name;
	m_namespaceURI = namespaceURI;
}

TagNodeListImpl::~TagNodeListImpl()
{
}

NodeImpl *TagNodeListImpl::item(unsigned long index) const
{
	if(!m_refNode)
		return 0;

	return recursiveItem(m_refNode, index);
}

NodeImpl *TagNodeListImpl::recursiveItem(const NodeImpl *refNode, unsigned long &index) const
{
	for(NodeImpl *n = refNode->firstChild(); n != 0; n = n->nextSibling())
	{
		int type = n->nodeType();
		if(type == ELEMENT_NODE || type == ENTITY_REFERENCE_NODE)
		{
			if(type == ELEMENT_NODE && check(n))
			{
				if(!index--)
					return n;
			}

			NodeImpl *search = recursiveItem(n, index);
			if(search)
				return search;
		}
	}

	return 0;
}

unsigned long TagNodeListImpl::length() const
{
	return recursiveLength(m_refNode);
}

unsigned long TagNodeListImpl::recursiveLength(const NodeImpl *refNode) const
{
	unsigned long len = 0;
	for(NodeImpl *n = refNode->firstChild(); n != 0; n = n->nextSibling())
	{
		if(n->nodeType() == ELEMENT_NODE)
		{
			if(check(n))
				len++;

			len += recursiveLength(n);
		}
		else if(n->nodeType() == ENTITY_REFERENCE_NODE)
			len += recursiveLength(n);
	}

	return len;
}

// The caller has to check if n != 0
bool TagNodeListImpl::check(NodeImpl *node) const
{
	if(!m_namespaceURI.isEmpty())
	{
		// The special value "*" matches all namespaces & names.
		return ((m_namespaceURI == "*" || m_namespaceURI == node->namespaceURI()) &&
				(m_name == "*" || node->localName() == m_name));
	}

	if(m_name == "*") // The special value "*" matches all tags.
		return true;

	return node->nodeName() == m_name;
}

// vim:ts=4:noet
