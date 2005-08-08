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
#include "CSSValue.h"
#include "CSSValueImpl.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSValue.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin CSSValue::s_hashTable 3
 cssText	CSSValueConstants::CssText		DontDelete|ReadOnly
 cssValueType	CSSValueConstants::CssValueType		DontDelete|ReadOnly
@end
*/

ValueImp *CSSValue::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case CSSValueConstants::CssText:
			return getDOMString(cssText());
		case CSSValueConstants::CssValueType:
			return Number(cssValueType());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

void CSSValue::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case CSSValueConstants::CssText:
			setCssText(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

CSSValue CSSValue::null;

CSSValue::CSSValue() : d(0)
{
}

CSSValue::CSSValue(CSSValueImpl *i) : d(i)
{
	if(d)
		d->ref();
}

CSSValue::CSSValue(const CSSValue &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(CSSValue)

void CSSValue::setCssText(const DOMString &cssText)
{
	if(d)
		d->setCssText(cssText);
}

DOMString CSSValue::cssText() const
{
	if(!d)
		return DOMString();

	return d->cssText();
}

unsigned short CSSValue::cssValueType() const
{
	if(!d)
		return CSS_CUSTOM;

	return d->cssValueType();
}

bool CSSValue::isCSSValueList() const
{
	if(!d)
		return false;
		
	return d->isValueList();
}

bool CSSValue::isCSSPrimitiveValue() const
{
	if(!d)
		return false;
		
	return d->isPrimitiveValue();
}

// vim:ts=4:noet
