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
#include "SVGHelper.h"
#include "SVGMatrix.h"
#include "SVGException.h"
#include "SVGLength.h"
#include "SVGElementImpl.h"
#include "SVGExceptionImpl.h"
#include "SVGLengthList.h"
#include "SVGLengthListImpl.h"


#include "SVGConstants.h"
#include "SVGLengthList.lut.h"
using namespace KSVG;

/*
@begin SVGLengthList::s_hashTable 3
 numberOfItems					SVGLengthListConstants::NumberOfItems				DontDelete|ReadOnly
@end
@begin SVGLengthListProto::s_hashTable 9
 clear					SVGLengthListConstants::Clear				DontDelete|Function 0
 initialize				SVGLengthListConstants::Initialize			DontDelete|Function 1
 getItem				SVGLengthListConstants::GetItem				DontDelete|Function 1
 insertItemBefore		SVGLengthListConstants::InsertItemBefore	DontDelete|Function 2
 replaceItem			SVGLengthListConstants::ReplaceItem			DontDelete|Function 2
 removeItem				SVGLengthListConstants::RemoveItem			DontDelete|Function 1
 appendItem				SVGLengthListConstants::AppendItem			DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGLengthList", SVGLengthListProto, SVGLengthListProtoFunc)

ValueImp *SVGLengthList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGLengthListConstants::NumberOfItems:
			return Number(numberOfItems());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGLengthListProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGLengthList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGLengthListConstants::Clear:
		{
			obj.clear();
			return Undefined();
		}
		case SVGLengthListConstants::Initialize:
		{
			SVGLength newItem = KDOM::ecma_cast<SVGLength>(exec, args[0], &toSVGLength);
			return KDOM::safe_cache<SVGLength>(exec, obj.initialize(newItem));
		}
		case SVGLengthListConstants::GetItem:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::safe_cache<SVGLength>(exec, obj.getItem(index));
		}
		case SVGLengthListConstants::InsertItemBefore:
		{
			SVGLength newItem = KDOM::ecma_cast<SVGLength>(exec, args[0], &toSVGLength);
			unsigned long index = args[1]->toUInt32(exec);
			return KDOM::safe_cache<SVGLength>(exec, obj.insertItemBefore(newItem, index));
		}
		case SVGLengthListConstants::ReplaceItem:
		{
			SVGLength newItem = KDOM::ecma_cast<SVGLength>(exec, args[0], &toSVGLength);
			unsigned long index = args[1]->toUInt32(exec);
			return KDOM::safe_cache<SVGLength>(exec, obj.replaceItem(newItem, index));
		}
		case SVGLengthListConstants::RemoveItem:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::safe_cache<SVGLength>(exec, obj.removeItem(index));
		}
		case SVGLengthListConstants::AppendItem:
		{
			SVGLength newItem = KDOM::ecma_cast<SVGLength>(exec, args[0], &toSVGLength);
			return KDOM::safe_cache<SVGLength>(exec, obj.appendItem(newItem));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGLengthList SVGLengthList::null;

SVGLengthList::SVGLengthList() : impl(0)
{
}

SVGLengthList::SVGLengthList(SVGLengthListImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGLengthList::SVGLengthList(const SVGLengthList &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGLengthList)

unsigned long SVGLengthList::numberOfItems() const
{
	if(!impl)
		return 0;

	return impl->numberOfItems();
}

void SVGLengthList::clear()
{
	if(impl)
		impl->clear();
}

SVGLength SVGLengthList::initialize(const SVGLength &newItem)
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->initialize(newItem.handle()));
}

SVGLength SVGLengthList::getItem(unsigned long index)
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->getItem(index));
}

SVGLength SVGLengthList::insertItemBefore(const SVGLength &newItem, unsigned long index)
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->insertItemBefore(newItem.handle(), index));
}

SVGLength SVGLengthList::replaceItem(const SVGLength &newItem, unsigned long index)
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->replaceItem(newItem.handle(), index));
}

SVGLength SVGLengthList::removeItem(unsigned long index)
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->removeItem(index));
}

SVGLength SVGLengthList::appendItem(const SVGLength &newItem)
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->appendItem(newItem.handle()));
}

// vim:ts=4:noet
