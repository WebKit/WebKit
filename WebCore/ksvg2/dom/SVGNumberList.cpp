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

#include <kdom/ecma/Ecma.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGException.h"
#include "SVGNumber.h"
#include "SVGElementImpl.h"
#include "SVGExceptionImpl.h"
#include "SVGNumberList.h"
#include "SVGNumberListImpl.h"

#include "SVGConstants.h"
#include "SVGNumberList.lut.h"
using namespace KSVG;

/*
@begin SVGNumberList::s_hashTable 3
 numberOfItems					SVGNumberListConstants::NumberOfItems				DontDelete|ReadOnly
@end
@begin SVGNumberListProto::s_hashTable 9
 clear							SVGNumberListConstants::Clear						DontDelete|Function 0
 initialize						SVGNumberListConstants::Initialize					DontDelete|Function 1
 getItem						SVGNumberListConstants::GetItem						DontDelete|Function 1
 insertItemBefore				SVGNumberListConstants::InsertItemBefore				DontDelete|Function 2
 replaceItem					SVGNumberListConstants::ReplaceItem					DontDelete|Function 2
 removeItem						SVGNumberListConstants::RemoveItem					DontDelete|Function 1
 appendItem						SVGNumberListConstants::AppendItem					DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGNumberList", SVGNumberListProto, SVGNumberListProtoFunc)

Value SVGNumberList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGNumberListConstants::NumberOfItems:
			return Number(numberOfItems());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

Value SVGNumberListProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGNumberList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGNumberListConstants::Clear:
		{
			obj.clear();
			return Undefined();
		}
		case SVGNumberListConstants::Initialize:
		{
			SVGNumber newItem = KDOM::ecma_cast<SVGNumber>(exec, args[0], &toSVGNumber);
			return KDOM::safe_cache<SVGNumber>(exec, obj.initialize(newItem));
		}
		case SVGNumberListConstants::GetItem:
		{
			unsigned long index = args[0].toUInt32(exec);
			return KDOM::safe_cache<SVGNumber>(exec, obj.getItem(index));
		}
		case SVGNumberListConstants::InsertItemBefore:
		{
			SVGNumber newItem = KDOM::ecma_cast<SVGNumber>(exec, args[0], &toSVGNumber);
			unsigned long index = args[1].toUInt32(exec);
			return KDOM::safe_cache<SVGNumber>(exec, obj.insertItemBefore(newItem, index));
		}
		case SVGNumberListConstants::ReplaceItem:
		{
			SVGNumber newItem = KDOM::ecma_cast<SVGNumber>(exec, args[0], &toSVGNumber);
			unsigned long index = args[1].toUInt32(exec);
			return KDOM::safe_cache<SVGNumber>(exec, obj.replaceItem(newItem, index));
		}
		case SVGNumberListConstants::RemoveItem:
		{
			unsigned long index = args[0].toUInt32(exec);
			return KDOM::safe_cache<SVGNumber>(exec, obj.removeItem(index));
		}
		case SVGNumberListConstants::AppendItem:
		{
			SVGNumber newItem = KDOM::ecma_cast<SVGNumber>(exec, args[0], &toSVGNumber);
			return KDOM::safe_cache<SVGNumber>(exec, obj.appendItem(newItem));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGNumberList SVGNumberList::null;

SVGNumberList::SVGNumberList() : impl(0)
{
}

SVGNumberList::SVGNumberList(SVGNumberListImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGNumberList::SVGNumberList(const SVGNumberList &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGNumberList)

unsigned long SVGNumberList::numberOfItems() const
{
	if(!impl)
		return 0;

	return impl->numberOfItems();
}

void SVGNumberList::clear()
{
	if(impl)
		impl->clear();
}

SVGNumber SVGNumberList::initialize(const SVGNumber &newItem)
{
	if(!impl)
		return SVGNumber::null;

	return SVGNumber(impl->initialize(newItem.handle()));
}

SVGNumber SVGNumberList::getItem(unsigned long index)
{
	if(!impl)
		return SVGNumber::null;

	return SVGNumber(impl->getItem(index));
}

SVGNumber SVGNumberList::insertItemBefore(const SVGNumber &newItem, unsigned long index)
{
	if(!impl)
		return SVGNumber::null;

	return SVGNumber(impl->insertItemBefore(newItem.handle(), index));
}

SVGNumber SVGNumberList::replaceItem(const SVGNumber &newItem, unsigned long index)
{
	if(!impl)
		return SVGNumber::null;

	return SVGNumber(impl->replaceItem(newItem.handle(), index));
}

SVGNumber SVGNumberList::removeItem(unsigned long index)
{
	if(!impl)
		return SVGNumber::null;

	return SVGNumber(impl->removeItem(index));
}

SVGNumber SVGNumberList::appendItem(const SVGNumber &newItem)
{
	if(!impl)
		return SVGNumber::null;

	return SVGNumber(impl->appendItem(newItem.handle()));
}

// vim:ts=4:noet
