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
#include "CSSValueList.h"
#include "CSSValueListImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSValueList.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSValueList::s_hashTable 2
 length		CSSValueListConstants::Length		DontDelete
@end
@begin CSSValueListProto::s_hashTable 2
 item		CSSValueListConstants::Item			DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("CSSValueList", CSSValueListProto, CSSValueListProtoFunc)

Value CSSValueList::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case CSSValueListConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

Value CSSValueListProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(CSSValueList)

	switch(id)
	{
		case CSSValueListConstants::Item:
		{
			unsigned long index = args[0].toUInt32(exec);
			return getDOMCSSValue(exec, obj.item(index));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
			break;
	}

	return Undefined();
}

// The qdom way...
#define impl (static_cast<CSSValueListImpl *>(d))

CSSValueList CSSValueList::null;

CSSValueList::CSSValueList() : CSSValue()
{
}

CSSValueList::CSSValueList(const CSSValueList &other) : CSSValue()
{
	(*this) = other;
}

CSSValueList::CSSValueList(const CSSValue &other) : CSSValue()
{
	(*this) = other;
}

CSSValueList::CSSValueList(CSSValueListImpl *i) : CSSValue(i)
{
}

CSSValueList::~CSSValueList()
{
}

CSSValueList &CSSValueList::operator=(const CSSValueList &other)
{
	CSSValue::operator=(other);
	return *this;
}

KDOM_CSSVALUE_DERIVED_ASSIGN_OP(CSSValueList, CSS_VALUE_LIST)

unsigned long CSSValueList::length() const
{
	if(!d)
		return 0;

	return impl->length();
}

CSSValue CSSValueList::item(unsigned long index) const
{
	if(!d)
		return CSSValue::null;

	return CSSValue(impl->item(index));
}

// vim:ts=4:noet
