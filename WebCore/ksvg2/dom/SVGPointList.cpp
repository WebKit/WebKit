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

#include "ksvg.h"
#include "SVGPointList.h"
#include "SVGHelper.h"
#include "SVGPointListImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGPoint.h"
#include "SVGElementImpl.h"

#include "SVGConstants.h"
#include "SVGPointList.lut.h"
using namespace KSVG;

/*
@begin SVGPointList::s_hashTable 3
 numberOfItems		SVGPointListConstants::NumberOfItems	DontDelete|ReadOnly
@end
@begin SVGPointListProto::s_hashTable 9
 clear				SVGPointListConstants::Clear			DontDelete|Function 0
 initialize			SVGPointListConstants::Initialize		DontDelete|Function 1
 getItem			SVGPointListConstants::GetItem			DontDelete|Function 1
 insertItemBefore	SVGPointListConstants::InsertItemBefore	DontDelete|Function 2
 replaceItem		SVGPointListConstants::ReplaceItem		DontDelete|Function 2
 removeItem			SVGPointListConstants::RemoveItem		DontDelete|Function 1
 appendItem			SVGPointListConstants::AppendItem		DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGPointList", SVGPointListProto, SVGPointListProtoFunc)

ValueImp *SVGPointList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGPointListConstants::NumberOfItems:
			return Number(numberOfItems());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGPointListProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGPointList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGPointListConstants::Clear:
		{
			obj.clear();
			return Undefined();
		}
		case SVGPointListConstants::Initialize:
		{
			SVGPoint newItem = KDOM::ecma_cast<SVGPoint>(exec, args[0], &toSVGPoint);
			return KDOM::safe_cache<SVGPoint>(exec, obj.initialize(newItem));
		}
		case SVGPointListConstants::GetItem:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::safe_cache<SVGPoint>(exec, obj.getItem(index));
		}
		case SVGPointListConstants::InsertItemBefore:
		{
			SVGPoint newItem = KDOM::ecma_cast<SVGPoint>(exec, args[0], &toSVGPoint);
			unsigned long index = args[1]->toUInt32(exec);
			return KDOM::safe_cache<SVGPoint>(exec, obj.insertItemBefore(newItem, index));
		}
		case SVGPointListConstants::ReplaceItem:
		{
			SVGPoint newItem = KDOM::ecma_cast<SVGPoint>(exec, args[0], &toSVGPoint);
			unsigned long index = args[1]->toUInt32(exec);
			return KDOM::safe_cache<SVGPoint>(exec, obj.replaceItem(newItem, index));
		}
		case SVGPointListConstants::RemoveItem:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::safe_cache<SVGPoint>(exec, obj.removeItem(index));
		}
		case SVGPointListConstants::AppendItem:
		{
			SVGPoint newItem = KDOM::ecma_cast<SVGPoint>(exec, args[0], &toSVGPoint);
			return KDOM::safe_cache<SVGPoint>(exec, obj.appendItem(newItem));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGPointList SVGPointList::null;

SVGPointList::SVGPointList() : impl(0)
{
}

SVGPointList::SVGPointList(SVGPointListImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGPointList::SVGPointList(const SVGPointList &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGPointList)

unsigned long SVGPointList::numberOfItems() const
{
	if(!impl)
		return 0;

	return impl->numberOfItems();
}

void SVGPointList::clear()
{
	if(impl)
		impl->clear();
}

SVGPoint SVGPointList::initialize(const SVGPoint &newItem)
{
	if(!impl)
		return SVGPoint::null;

	return SVGPoint(impl->initialize(newItem.handle()));
}

SVGPoint SVGPointList::getItem(unsigned long index)
{
	if(!impl)
		return SVGPoint::null;

	return SVGPoint(impl->getItem(index));
}

SVGPoint SVGPointList::insertItemBefore(const SVGPoint &newItem, unsigned long index)
{
	if(!impl)
		return SVGPoint::null;

	return SVGPoint(impl->insertItemBefore(newItem.handle(), index));
}

SVGPoint SVGPointList::replaceItem(const SVGPoint &newItem, unsigned long index)
{
	if(!impl)
		return SVGPoint::null;

	return SVGPoint(impl->replaceItem(newItem.handle(), index));
}

SVGPoint SVGPointList::removeItem(unsigned long index)
{
	if(!impl)
		return SVGPoint::null;

	return SVGPoint(impl->removeItem(index));
}

SVGPoint SVGPointList::appendItem(const SVGPoint &newItem)
{
	if(!impl)
		return SVGPoint::null;

	return SVGPoint(impl->appendItem(newItem.handle()));
}

// vim:ts=4:noet
