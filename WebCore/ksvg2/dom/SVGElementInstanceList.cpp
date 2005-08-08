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
#include "SVGElementInstanceList.h"
#include "SVGHelper.h"
#include "SVGElementInstanceListImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGElementInstance.h"
#include "SVGElementImpl.h"

#include "SVGConstants.h"
#include "SVGElementInstanceList.lut.h"
using namespace KSVG;

/*
@begin SVGElementInstanceList::s_hashTable 3
 length	SVGElementInstanceListConstants::Length	DontDelete|ReadOnly
@end
@begin SVGElementInstanceListProto::s_hashTable 3
 item	SVGElementInstanceListConstants::Item	DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGElementInstanceList", SVGElementInstanceListProto, SVGElementInstanceListProtoFunc)

ValueImp *SVGElementInstanceList::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGElementInstanceListConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGElementInstanceListProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGElementInstanceList)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGElementInstanceListConstants::Item:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return KDOM::safe_cache<SVGElementInstance>(exec, obj.item(index));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

SVGElementInstanceList SVGElementInstanceList::null;

SVGElementInstanceList::SVGElementInstanceList() : impl(0)
{
}

SVGElementInstanceList::SVGElementInstanceList(SVGElementInstanceListImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGElementInstanceList::SVGElementInstanceList(const SVGElementInstanceList &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGElementInstanceList)

unsigned long SVGElementInstanceList::length() const
{
	if(!impl)
		return 0;

	return impl->numberOfItems();
}

SVGElementInstance SVGElementInstanceList::item(unsigned long index)
{
	if(!impl)
		return SVGElementInstance::null;

	return SVGElementInstance(impl->getItem(index));
}

// vim:ts=4:noet
