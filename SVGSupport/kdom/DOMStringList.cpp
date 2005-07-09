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

#include "Ecma.h"
#include "DOMString.h"
#include "DOMStringList.h"
#include "DOMStringListImpl.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"

#include "DOMConstants.h"
#include "DOMStringList.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DOMStringList::s_hashTable 2
 length		DOMStringListConstants::Length	DontDelete
@end
@begin DOMStringListProto::s_hashTable 3
 item		DOMStringListConstants::Item		DontDelete|Function 1
 contains	DOMStringListConstants::Contains	DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("DOMStringList", DOMStringListProto, DOMStringListProtoFunc)

Value DOMStringList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case DOMStringListConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

Value DOMStringListProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(DOMStringList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case DOMStringListConstants::Item:
		{
			unsigned long index = args[0].toUInt32(exec);
			return getDOMString(obj.item(index));
		}
		case DOMStringListConstants::Contains:
		{
			DOMString str = toDOMString(exec, args[0]);
			return KJS::Boolean(obj.contains(str));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
			break;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

DOMStringList DOMStringList::null;

DOMStringList::DOMStringList() : d(0)
{
}

DOMStringList::DOMStringList(DOMStringListImpl *i) : d(i)
{
	if(d)
		d->ref();
}

DOMStringList::DOMStringList(const DOMStringList &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(DOMStringList)

DOMString DOMStringList::item(unsigned long index) const
{
	if(!d)
		return DOMString();

	return DOMString(d->item(index));
}

unsigned long DOMStringList::length() const
{
	if(!d)
		return 0;

	return d->length();
}

bool DOMStringList::contains(const DOMString &str) const
{
	if(!d)
		return false;

	unsigned long len = d->length();
	// simple (slow) search
	for(unsigned long i = 0;i < len;i++)
		if(DOMString(item(i)) == str)
			return true;

	return false;
}

// vim:ts=4:noet
