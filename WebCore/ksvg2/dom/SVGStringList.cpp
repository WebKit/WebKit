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

#include <kdom/ecma/Ecma.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>
#include <kdom/DOMString.h>

#include "ksvg.h"
#include "SVGStringList.h"
#include "SVGHelper.h"
#include "SVGStringListImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGElementImpl.h"

#include "SVGConstants.h"
#include "SVGStringList.lut.h"
using namespace KSVG;

/*
@begin SVGStringList::s_hashTable 3
 numberOfItems		SVGStringListConstants::NumberOfItems		DontDelete|ReadOnly
@end
@begin SVGStringListProto::s_hashTable 9
 clear				SVGStringListConstants::Clear				DontDelete|Function 0
 initialize			SVGStringListConstants::Initialize			DontDelete|Function 1
 getItem			SVGStringListConstants::GetItem				DontDelete|Function 1
 insertItemBefore	SVGStringListConstants::InsertItemBefore	DontDelete|Function 2
 replaceItem		SVGStringListConstants::ReplaceItem			DontDelete|Function 2
 removeItem			SVGStringListConstants::RemoveItem			DontDelete|Function 1
 appendItem			SVGStringListConstants::AppendItem			DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGStringList", SVGStringListProto, SVGStringListProtoFunc)

ValueImp *SVGStringList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGStringListConstants::NumberOfItems:
			return Number(numberOfItems());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGStringListProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGStringList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGStringListConstants::Clear:
		{
			obj.clear();
			return Undefined();
		}
		case SVGStringListConstants::Initialize:
		{
			KDOM::DOMString newItem = KDOM::toDOMString(exec, args[0]);
			return KDOM::getDOMString(obj.initialize(newItem));
		}
		case SVGStringListConstants::GetItem:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::getDOMString(obj.getItem(index));
		}
		case SVGStringListConstants::InsertItemBefore:
		{
			KDOM::DOMString newItem = KDOM::toDOMString(exec, args[0]);
			unsigned long index = args[1]->toUInt32(exec);
			return KDOM::getDOMString(obj.insertItemBefore(newItem, index));
		}
		case SVGStringListConstants::ReplaceItem:
		{
			KDOM::DOMString newItem = KDOM::toDOMString(exec, args[0]);
			unsigned long index = args[1]->toUInt32(exec);
			return KDOM::getDOMString(obj.replaceItem(newItem, index));
		}
		case SVGStringListConstants::RemoveItem:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::getDOMString(obj.removeItem(index));
		}
		case SVGStringListConstants::AppendItem:
		{
			KDOM::DOMString newItem = KDOM::toDOMString(exec, args[0]);
			return KDOM::getDOMString(obj.appendItem(newItem));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGStringList SVGStringList::null;

SVGStringList::SVGStringList() : impl(0)
{
}

SVGStringList::SVGStringList(SVGStringListImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGStringList::SVGStringList(const SVGStringList &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGStringList)

unsigned long SVGStringList::numberOfItems() const
{
	if(!impl)
		return 0;

	return impl->numberOfItems();
}

void SVGStringList::clear()
{
	if(impl)
		impl->clear();
}

KDOM::DOMString SVGStringList::initialize(const KDOM::DOMString &newItem)
{
	if(!impl)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->initialize(newItem.implementation()));
}

KDOM::DOMString SVGStringList::getItem(unsigned long index)
{
	if(!impl)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->getItem(index));
}

KDOM::DOMString SVGStringList::insertItemBefore(const KDOM::DOMString &newItem, unsigned long index)
{
	if(!impl)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->insertItemBefore(newItem.implementation(), index));
}

KDOM::DOMString SVGStringList::replaceItem(const KDOM::DOMString &newItem, unsigned long index)
{
	if(!impl)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->replaceItem(newItem.implementation(), index));
}

KDOM::DOMString SVGStringList::removeItem(unsigned long index)
{
	if(!impl)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->removeItem(index));
}

KDOM::DOMString SVGStringList::appendItem(const KDOM::DOMString &newItem)
{
	if(!impl)
		return KDOM::DOMString();

	return KDOM::DOMString(impl->appendItem(newItem.implementation()));
}

// vim:ts=4:noet
