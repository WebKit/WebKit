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
#include "SVGMatrix.h"
#include "SVGException.h"
#include "SVGTransform.h"
#include "SVGElementImpl.h"
#include "SVGExceptionImpl.h"
#include "SVGTransformList.h"
#include "SVGTransformListImpl.h"


#include "SVGConstants.h"
#include "SVGTransformList.lut.h"
using namespace KSVG;

/*
@begin SVGTransformList::s_hashTable 3
 numberOfItems					SVGTransformListConstants::NumberOfItems				DontDelete|ReadOnly
@end
@begin SVGTransformListProto::s_hashTable 9
 clear							SVGTransformListConstants::Clear						DontDelete|Function 0
 initialize						SVGTransformListConstants::Initialize					DontDelete|Function 1
 getItem						SVGTransformListConstants::GetItem						DontDelete|Function 1
 insertItemBefore				SVGTransformListConstants::InsertItemBefore				DontDelete|Function 2
 replaceItem					SVGTransformListConstants::ReplaceItem					DontDelete|Function 2
 removeItem						SVGTransformListConstants::RemoveItem					DontDelete|Function 1
 appendItem						SVGTransformListConstants::AppendItem					DontDelete|Function 1
 createSVGTransformFromMatrix	SVGTransformListConstants::CreateSVGTransformFromMatrix	DontDelete|Function 1
 consolidate					SVGTransformListConstants::Consolidate					DontDelete|Function 0
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGTransformList", SVGTransformListProto, SVGTransformListProtoFunc)

ValueImp *SVGTransformList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGTransformListConstants::NumberOfItems:
			return Number(numberOfItems());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGTransformListProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGTransformList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGTransformListConstants::Clear:
		{
			obj.clear();
			return Undefined();
		}
		case SVGTransformListConstants::Initialize:
		{
			SVGTransform newItem = KDOM::ecma_cast<SVGTransform>(exec, args[0], &toSVGTransform);
			return KDOM::safe_cache<SVGTransform>(exec, obj.initialize(newItem));
		}
		case SVGTransformListConstants::GetItem:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::safe_cache<SVGTransform>(exec, obj.getItem(index));
		}
		case SVGTransformListConstants::InsertItemBefore:
		{
			SVGTransform newItem = KDOM::ecma_cast<SVGTransform>(exec, args[0], &toSVGTransform);
			unsigned long index = args[1]->toUInt32(exec);
			return KDOM::safe_cache<SVGTransform>(exec, obj.insertItemBefore(newItem, index));
		}
		case SVGTransformListConstants::ReplaceItem:
		{
			SVGTransform newItem = KDOM::ecma_cast<SVGTransform>(exec, args[0], &toSVGTransform);
			unsigned long index = args[1]->toUInt32(exec);
			return KDOM::safe_cache<SVGTransform>(exec, obj.replaceItem(newItem, index));
		}
		case SVGTransformListConstants::RemoveItem:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::safe_cache<SVGTransform>(exec, obj.removeItem(index));
		}
		case SVGTransformListConstants::AppendItem:
		{
			SVGTransform newItem = KDOM::ecma_cast<SVGTransform>(exec, args[0], &toSVGTransform);
			return KDOM::safe_cache<SVGTransform>(exec, obj.appendItem(newItem));
		}
		case SVGTransformListConstants::CreateSVGTransformFromMatrix:
		{
			SVGMatrix newMatrix = KDOM::ecma_cast<SVGMatrix>(exec, args[0], &toSVGMatrix);
			return KDOM::safe_cache<SVGTransform>(exec, obj.createSVGTransformFromMatrix(newMatrix));
		}
		case SVGTransformListConstants::Consolidate:
			return KDOM::safe_cache<SVGTransform>(exec, obj.consolidate());
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGTransformList SVGTransformList::null;

SVGTransformList::SVGTransformList() : impl(0)
{
}

SVGTransformList::SVGTransformList(SVGTransformListImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGTransformList::SVGTransformList(const SVGTransformList &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGTransformList)

unsigned long SVGTransformList::numberOfItems() const
{
	if(!impl)
		return 0;

	return impl->numberOfItems();
}

void SVGTransformList::clear()
{
	if(impl)
		impl->clear();
}

SVGTransform SVGTransformList::initialize(const SVGTransform &newItem)
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->initialize(newItem.handle()));
}

SVGTransform SVGTransformList::getItem(unsigned long index)
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->getItem(index));
}

SVGTransform SVGTransformList::insertItemBefore(const SVGTransform &newItem, unsigned long index)
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->insertItemBefore(newItem.handle(), index));
}

SVGTransform SVGTransformList::replaceItem(const SVGTransform &newItem, unsigned long index)
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->replaceItem(newItem.handle(), index));
}

SVGTransform SVGTransformList::removeItem(unsigned long index)
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->removeItem(index));
}

SVGTransform SVGTransformList::appendItem(const SVGTransform &newItem)
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->appendItem(newItem.handle()));
}

SVGTransform SVGTransformList::createSVGTransformFromMatrix(const SVGMatrix &matrix) const
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->createSVGTransformFromMatrix(matrix.handle()));
}
	
SVGTransform SVGTransformList::consolidate()
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->consolidate());
}

// vim:ts=4:noet
