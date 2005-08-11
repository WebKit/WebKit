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

#include "Ecma.h"
#include "NodeImpl.h"
#include "StyleSheet.h"
#include "StyleSheetList.h"
#include "StyleSheetListImpl.h"

#include "kdom/data/CSSConstants.h"
#include "StyleSheetList.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin StyleSheetList::s_hashTable 2
 length		StyleSheetListConstants::Length		DontDelete
@end
@begin StyleSheetListProto::s_hashTable 2
 item		StyleSheetListConstants::Item		DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("StyleSheetList", StyleSheetListProto, StyleSheetListProtoFunc)

ValueImp *StyleSheetList::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case StyleSheetListConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

ValueImp *StyleSheetListProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(StyleSheetList)

	switch(id)
	{
		case StyleSheetListConstants::Item:
		{
			unsigned long index = args[0]->toUInt32(exec);
			return safe_cache<StyleSheet>(exec, obj.item(index));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
			break;
	}

	return Undefined();
}

StyleSheetList StyleSheetList::null;

StyleSheetList::StyleSheetList() : d(0)
{
}

StyleSheetList::StyleSheetList(StyleSheetListImpl *i) : d(i)
{
	if(d)
		d->ref();
}

StyleSheetList::StyleSheetList(const StyleSheetList &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(StyleSheetList)

unsigned long StyleSheetList::length() const
{
	if(!d)
		return 0;

	return d->length();
}

StyleSheet StyleSheetList::item(unsigned long index) const
{
	if(!d)
		return StyleSheet::null;

	return StyleSheet(d->item(index));
}

// vim:ts=4:noet
