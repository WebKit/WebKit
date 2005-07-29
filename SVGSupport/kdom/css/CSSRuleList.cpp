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
#include "CSSRule.h"
#include "CSSRuleImpl.h"
#include "CSSRuleList.h"
#include "CSSRuleListImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSRuleList.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSRuleList::s_hashTable 2
 length		CSSRuleListConstants::Length            DontDelete|ReadOnly
@end
@begin CSSRuleListProto::s_hashTable 2
 item		CSSRuleListConstants::Item              DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("CSSRuleList", CSSRuleListProto, CSSRuleListProtoFunc)

Value CSSRuleList::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case CSSRuleListConstants::Length:
			return Number(length());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

Value CSSRuleListProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(CSSRuleList)

	switch(id)
	{
		case CSSRuleListConstants::Item:
		{
			unsigned long index = args[0].toUInt32(exec);
			return getDOMCSSRule(exec, obj.item(index));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	return Undefined();
}

CSSRuleList CSSRuleList::null;

CSSRuleList::CSSRuleList() : d(0)
{
}

CSSRuleList::CSSRuleList(CSSRuleListImpl *i) : d(i)
{
	if(d)
		d->ref();
}

CSSRuleList::CSSRuleList(StyleListImpl *lst)
{
	d = new CSSRuleListImpl();
	d->ref();
	
	if(lst)
	{
		for(unsigned long i = 0; i < lst->length() ; ++i)
		{
			StyleBaseImpl *style = lst->item(i);
			if(style->isRule())
				d->insertRule(static_cast<CSSRuleImpl *>(style), d->length());
		}
	}
}

CSSRuleList::CSSRuleList(const CSSRuleList &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(CSSRuleList)

unsigned long CSSRuleList::length() const
{
	if(!d)
		return 0;

	return d->length();
}

CSSRule CSSRuleList::item(unsigned long index) const
{
	if(!d)
		return CSSRule::null;

	return CSSRule(d->item(index));
}

// vim:ts=4:noet
