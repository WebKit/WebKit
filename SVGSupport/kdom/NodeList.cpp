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

#include "Node.h"
#include "Ecma.h"
#include "NodeImpl.h"
#include "NodeList.h"
#include "NodeListImpl.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"

#include "DOMConstants.h"
#include "NodeList.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin NodeList::s_hashTable 2
 length		NodeListConstants::Length	DontDelete
@end
@begin NodeListProto::s_hashTable 2
 item		NodeListConstants::Item		DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("NodeList", NodeListProto, NodeListProtoFunc)

Value NodeList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case NodeListConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

Value NodeListProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(NodeList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case NodeListConstants::Item:
		{
			unsigned long index = args[0].toUInt32(exec);
			return getDOMNode(exec, obj.item(index));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
			break;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

NodeList NodeList::null;

NodeList::NodeList() : d(0)
{
}

NodeList::NodeList(NodeListImpl *i) : d(i)
{
	if(d)
		d->ref();
}

NodeList::NodeList(const NodeList &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(NodeList)

Node NodeList::item(unsigned long index) const
{
	if(!d)
		return Node::null;

	return d->item(index);
}

unsigned long NodeList::length() const
{
	if(!d)
		return 0;

	return d->length();
}

// vim:ts=4:noet
