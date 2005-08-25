/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

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
#include "DocumentImpl.h"
#include "NamedNodeMapImpl.h"
#include "DOMImplementationImpl.h"

using namespace KDOM;

NamedNodeMapImpl::NamedNodeMapImpl() : Shared()
{
}

NamedNodeMapImpl::~NamedNodeMapImpl()
{
}

RONamedNodeMapImpl::RONamedNodeMapImpl(DocumentPtr *doc)
: NamedNodeMapImpl(), m_doc(doc)
{
	m_map = new QPtrList<NodeImpl>;
}

RONamedNodeMapImpl::~RONamedNodeMapImpl()
{
	while(!m_map->isEmpty())
		m_map->take(0)->deref();

	delete m_map;
}

bool RONamedNodeMapImpl::isReadOnly() const
{
	return !m_doc->document()->parsing();
}

NodeImpl *RONamedNodeMapImpl::item(unsigned long index) const
{
	if((unsigned int) index >= length())
		return 0;

	return m_map->at(index);
}

unsigned long RONamedNodeMapImpl::length() const
{
	return m_map->count();
}

void RONamedNodeMapImpl::addNode(NodeImpl *n)
{
	n->ref();
	m_map->append(n);
}

void RONamedNodeMapImpl::clone(NamedNodeMapImpl *other)
{
	m_doc->document()->setParsing(true);
	
	unsigned long len = other->length(); 
	for(unsigned long i = 0; i < len; ++i)
		addNode(other->item(i)->cloneNode(true, m_doc));

	m_doc->document()->setParsing(false);
}

NodeImpl *RONamedNodeMapImpl::getNamedItem(DOMStringImpl *name)
{
	// TODO : id mechanism doesnt work,
	// reverting to old system
	QPtrListIterator<NodeImpl> it(*m_map);
	for(; it.current(); ++it)
	{
		if(DOMString(it.current()->nodeName()) == DOMString(name))
			return it.current();
	}
	
	return 0;
}

NodeImpl *RONamedNodeMapImpl::setNamedItem(NodeImpl *arg)
{
	if(!arg)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);
		
	if(!isReadOnly())
	{
		addNode(arg);
		return arg;
	}

	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
	return 0;
}

NodeImpl *RONamedNodeMapImpl::removeNamedItem(DOMStringImpl *)
{
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
	return 0;
}

NodeImpl *RONamedNodeMapImpl::getNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName)
{
	// dont allow ns for entities and notations
	if(!namespaceURI)
		return 0;
		
	return getNamedItem(localName);
}

NodeImpl *RONamedNodeMapImpl::setNamedItemNS(NodeImpl *arg)
{
	if(!arg)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);
		
	// dont allow ns for entities and notations
	//if(arg->namespaceURI().isNull()) return 0;
	return setNamedItem(arg);
}

NodeImpl *RONamedNodeMapImpl::removeNamedItemNS(DOMStringImpl *, DOMStringImpl *localName)
{
	// dont allow ns for entities and notations
	//if(!namespaceURI.isNull()) return 0;
	return removeNamedItem(localName);
}

// vim:ts=4:noet
