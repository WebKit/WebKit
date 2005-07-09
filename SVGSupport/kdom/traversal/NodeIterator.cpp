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

#include "NodeIterator.h"
#include "NodeIteratorImpl.h"
#include "Ecma.h"
#include "NodeFilter.h"
#include "Node.h"
#include "NodeImpl.h"
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "TraversalConstants.h"
#include "NodeIterator.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin NodeIterator::s_hashTable 5
 root						NodeIteratorConstants::Root						DontDelete|ReadOnly
 whatToShow					NodeIteratorConstants::WhatToShow				DontDelete|ReadOnly
 filter						NodeIteratorConstants::Filter					DontDelete|ReadOnly
 expandEntityReferences		NodeIteratorConstants::ExpandEntityReferences	DontDelete|ReadOnly
@end
@begin NodeIteratorProto::s_hashTable 5
 nextNode		NodeIteratorConstants::NextNode		DontDelete|Function 0
 previousNode	NodeIteratorConstants::PreviousNode	DontDelete|Function 0
 detach			NodeIteratorConstants::Detach	 	DontDelete|Function 0
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("NodeIterator", NodeIteratorProto, NodeIteratorProtoFunc)

Value NodeIterator::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case NodeIteratorConstants::Root:
			return getDOMNode(exec, root());
		case NodeIteratorConstants::WhatToShow:
			return Number(whatToShow());
	//	case NodeIteratorConstants::Filter:
	//		return getDOMString(filter());
		case NodeIteratorConstants::ExpandEntityReferences:
			return KJS::Boolean(expandEntityReferences());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
};

Value NodeIteratorProtoFunc::call(ExecState *exec, Object &thisObj, const List &)
{
	KDOM_CHECK_THIS(NodeIterator)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case NodeIteratorConstants::NextNode:
			return getDOMNode(exec, obj.nextNode());
		case NodeIteratorConstants::PreviousNode:
			return getDOMNode(exec, obj.previousNode());
		case NodeIteratorConstants::Detach:
			obj.detach();
			return Value();
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

NodeIterator NodeIterator::null;

NodeIterator::NodeIterator() : d(0)
{
}

NodeIterator::NodeIterator(NodeIteratorImpl *i) : d(i)
{
	if(d)
		d->ref();
}

NodeIterator::NodeIterator(const NodeIterator &other) : d(0)
{
	(*this) = other;
}

NodeIterator::~NodeIterator()
{
	if(d)
		d->deref();
}

NodeIterator &NodeIterator::operator=(const NodeIterator &other)
{
	if(d)
		d->deref();

	d = other.d;

	if(d)
		d->ref();

	return (*this);
}

bool NodeIterator::operator==(const NodeIterator &other) const
{
	return d == other.d;
}

bool NodeIterator::operator!=(const NodeIterator &other) const
{
	return !operator==(other);
}

Node NodeIterator::root() const
{
	if(!d)
		return Node::null;

	return Node(d->root());
}

unsigned long NodeIterator::whatToShow() const
{
	if(!d)
		return 0;

	return d->whatToShow();
}

NodeFilter NodeIterator::filter() const
{
	if(!d)
		return NodeFilter::null;

	return NodeFilter(d->filter());
}

bool NodeIterator::expandEntityReferences() const
{
	if(!d)
		return false;

	return d->expandEntityReferences();
}

Node NodeIterator::nextNode()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return Node(d->nextNode());
}

Node NodeIterator::previousNode()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return Node(d->previousNode());
}

void NodeIterator::detach()
{
	if(d)
		d->detach();
}

// vim:ts=4:noet
