/*
	Copyright (C) 2004 Richard Moore <rich@kde.org>
	Copyright (C) 2004 Zack Rusin <zack@kde.org>
	Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

	This file is part of the KDE project

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or(at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public License
	along with this library; see the file COPYING.LIB.  If not, write to
	the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
	Boston, MA 02111-1307, USA.
*/

#include <qptrlist.h>
#include <qstring.h>

#include "DOMString.h"

#include "BooleanImpl.h"
#include "NodeImpl.h"
#include "NodeSetImpl.h"
#include "NumberImpl.h"
#include "StringImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

NodeSetImpl::NodeSetImpl()
	: ValueImpl(ValueImpl::ValueNodeSet)
{
}

NodeSetImpl::~NodeSetImpl()
{
	derefNodes();
}

int NodeSetImpl::count() const
{
	return m_storage.count();
}

int NodeSetImpl::position() const
{
#ifdef APPLE_COMPILE_HACK
	return 0; // FIXME: TOTAL HACK
#else
	return m_storage.at() + 1; // XPath positions start from 1
#endif
}

BooleanImpl *NodeSetImpl::toBoolean()
{
	return new BooleanImpl(m_storage.isEmpty() ? false : true);
}

NumberImpl *NodeSetImpl::toNumber()
{
	const QString n = m_storage.getFirst()->nodeName().string();
	const double d = n.toDouble();

	return new NumberImpl(d);
}

void NodeSetImpl::refNodes()
{
	QPtrListIterator<NodeImpl> itr(m_storage);

	while(itr.current())
	{
		itr.current()->ref();
		++itr;
	}
}

void NodeSetImpl::derefNodes()
{
	QPtrListIterator<NodeImpl> itr(m_storage);

	while(itr.current())
	{
		itr.current()->deref();
		++itr;
	}
}

StringImpl *NodeSetImpl::toString()
{
	DOMString nodeName;

	if(m_storage.getFirst())
		nodeName = m_storage.getFirst()->nodeName();
	return new StringImpl(nodeName);
}


NodeImpl *NodeSetImpl::next()
{
	return m_storage.next();
}

NodeImpl *NodeSetImpl::prev()
{
	return m_storage.prev();
}

NodeImpl *NodeSetImpl::current() const
{
	return m_storage.current();
}

void NodeSetImpl::append(NodeImpl *node)
{
	node->ref();
	m_storage.append(node);
}

void NodeSetImpl::remove(NodeImpl *node)
{
	if(m_storage.remove(node))
		node->deref();
}

// vim:ts=4:noet
