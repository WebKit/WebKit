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

#include <qdict.h>

#include "Ecma.h"
#include "Node.h"
#include "DOMString.h"
#include "DOMException.h"
#include "NamedNodeMap.h"

#include "DOMConstants.h"
#include "NamedNodeMap.lut.h"
#include "NamedNodeMapImpl.h"
using namespace KDOM;
using namespace KJS;

/*
@begin NamedNodeMap::s_hashTable 2
 length				NamedNodeMapConstants::Length				DontDelete|ReadOnly
@end
@begin NamedNodeMapProto::s_hashTable 7
 getNamedItem		NamedNodeMapConstants::GetNamedItem			DontDelete|Function 1
 setNamedItem		NamedNodeMapConstants::SetNamedItem			DontDelete|Function 1
 removeNamedItem	NamedNodeMapConstants::RemoveNamedItem		DontDelete|Function 1
 item				NamedNodeMapConstants::Item					DontDelete|Function 1
 getNamedItemNS		NamedNodeMapConstants::GetNamedItemNS		DontDelete|Function 2
 setNamedItemNS		NamedNodeMapConstants::SetNamedItemNS		DontDelete|Function 1
 removeNamedItemNS	NamedNodeMapConstants::RemoveNamedItemNS	DontDelete|Function 2
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("NamedNodeMap", NamedNodeMapProto, NamedNodeMapProtoFunc)

ValueImp *NamedNodeMap::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case NamedNodeMapConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

ValueImp *NamedNodeMapProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(NamedNodeMap)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case NamedNodeMapConstants::GetNamedItem:
		{
			DOMString name = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.getNamedItem(name));
		}
		case NamedNodeMapConstants::SetNamedItem:
		{
			Node n = ecma_cast<Node>(exec, args[0], &toNode);
			return getDOMNode(exec, obj.setNamedItem(n));
		}
		case NamedNodeMapConstants::RemoveNamedItem:
		{
			DOMString name = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.removeNamedItem(name));
		}
		case NamedNodeMapConstants::Item:
		{
			int index = args[0]->toInt32(exec);
			return getDOMNode(exec, obj.item(index));
		}
		case NamedNodeMapConstants::GetNamedItemNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			return getDOMNode(exec, obj.getNamedItemNS(namespaceURI, localName));
		}
		case NamedNodeMapConstants::SetNamedItemNS:
		{
			Node node = ecma_cast<Node>(exec, args[0], &toNode);
			return getDOMNode(exec, obj.setNamedItemNS(node));
		}
		case NamedNodeMapConstants::RemoveNamedItemNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			return getDOMNode(exec, obj.removeNamedItemNS(namespaceURI, localName));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

NamedNodeMap NamedNodeMap::null;

NamedNodeMap::NamedNodeMap() : d(0)
{
}

NamedNodeMap::NamedNodeMap(NamedNodeMapImpl *i) : d(i)
{
	if(d)
		d->ref();
}

NamedNodeMap::NamedNodeMap(const NamedNodeMap &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(NamedNodeMap)

Node NamedNodeMap::getNamedItem(const DOMString &name) const
{
	if(!d)
		return Node::null;

	return Node(d->getNamedItem(name.implementation()));
}

Node NamedNodeMap::setNamedItem(const Node &arg)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return Node(d->setNamedItem(static_cast<NodeImpl *>(arg.handle())));
}

Node NamedNodeMap::removeNamedItem(const DOMString &name)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return Node(d->removeNamedItem(name.implementation()));
}

Node NamedNodeMap::item(unsigned long index) const
{
	if(!d)
		return Node::null;

	return Node(d->item(index));
}

unsigned long NamedNodeMap::length() const
{
	if(!d)
		return 0;

	return d->length();
}

Node NamedNodeMap::getNamedItemNS(const DOMString &namespaceURI, const DOMString &localName) const
{
	if(!d)
		return 0;

	return Node(d->getNamedItemNS(namespaceURI.implementation(), localName.implementation()));
}

Node NamedNodeMap::setNamedItemNS(const Node &arg)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return Node(d->setNamedItemNS(static_cast<NodeImpl *>(arg.handle())));
}

Node NamedNodeMap::removeNamedItemNS(const DOMString &namespaceURI, const DOMString &localName)
{
	if(!d)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	return Node(d->removeNamedItemNS(namespaceURI.implementation(), localName.implementation()));
}

// vim:ts=4:noet
