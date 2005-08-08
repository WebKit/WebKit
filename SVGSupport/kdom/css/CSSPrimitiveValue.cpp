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

#include "Rect.h"
#include "Counter.h"
#include "RGBColor.h"
#include "RGBColorImpl.h"
#include "DOMException.h"
#include "DOMExceptionImpl.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueImpl.h"

#include "kdom/data/CSSConstants.h"
#include "CSSPrimitiveValue.lut.h"

namespace KDOM
{
using namespace KJS;

/*
@begin CSSPrimitiveValue::s_hashTable 2
 primitiveType		CSSPrimitiveValueConstants::PrimitiveType		DontDelete|ReadOnly
@end
@begin CSSPrimitiveValueProto::s_hashTable 9
 setFloatValue		CSSPrimitiveValueConstants::SetFloatValue		DontDelete|Function 2
 getFloatValue		CSSPrimitiveValueConstants::GetFloatValue		DontDelete|Function 1
 setStringValue		CSSPrimitiveValueConstants::SetStringValue		DontDelete|Function 2
 getStringValue		CSSPrimitiveValueConstants::GetStringValue		DontDelete|Function 0
 getCounterValue	CSSPrimitiveValueConstants::GetCounterValue		DontDelete|Function 0
 getRectValue		CSSPrimitiveValueConstants::GetRectValue		DontDelete|Function 0
 getRGBColorValue	CSSPrimitiveValueConstants::GetRGBColorValue	DontDelete|Function 0
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("CSSPrimitiveValue", CSSPrimitiveValueProto, CSSPrimitiveValueProtoFunc)

ValueImp *CSSPrimitiveValue::getValueProperty(ExecState *, int token) const
{
	switch(token)
	{
		case CSSPrimitiveValueConstants::PrimitiveType:
			return Number(primitiveType());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

ValueImp *CSSPrimitiveValueProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(CSSPrimitiveValue)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case CSSPrimitiveValueConstants::SetFloatValue:
		{
			unsigned short unitType = args[0]->toUInt16(exec);
			float floatValue = args[1]->toNumber(exec);
			obj.setFloatValue(unitType, floatValue);
			return Undefined();
		}
		case CSSPrimitiveValueConstants::GetFloatValue:
		{
			unsigned short unitType = args[0]->toUInt16(exec);
			return Number(obj.getFloatValue(unitType));
		}
		case CSSPrimitiveValueConstants::SetStringValue:
		{
			unsigned short stringType = args[0]->toUInt16(exec);
			DOMString stringValue = toDOMString(exec, args[1]);
			obj.setStringValue(stringType, stringValue);
			return Undefined();
		}
		case CSSPrimitiveValueConstants::GetStringValue:
			return getDOMString(obj.getStringValue());
		case CSSPrimitiveValueConstants::GetCounterValue:
			return safe_cache<Counter>(exec, obj.getCounterValue());
		case CSSPrimitiveValueConstants::GetRectValue:
			return safe_cache<Rect>(exec, obj.getRectValue());
		case CSSPrimitiveValueConstants::GetRGBColorValue:
			return safe_cache<RGBColor>(exec, obj.getRGBColorValue());
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
			break;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<CSSPrimitiveValueImpl *>(d))

CSSPrimitiveValue CSSPrimitiveValue::null;

CSSPrimitiveValue::CSSPrimitiveValue() : CSSValue()
{
}

CSSPrimitiveValue::CSSPrimitiveValue(const CSSPrimitiveValue &other) : CSSValue()
{
	(*this) = other;
}

CSSPrimitiveValue::CSSPrimitiveValue(const CSSValue &other) : CSSValue()
{
	(*this) = other;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPrimitiveValueImpl *i) : CSSValue(i)
{
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
}

CSSPrimitiveValue &CSSPrimitiveValue::operator=(const CSSPrimitiveValue &other)
{
	CSSValue::operator=(other);
	return *this;
}

KDOM_CSSVALUE_DERIVED_ASSIGN_OP(CSSPrimitiveValue, CSS_PRIMITIVE_VALUE)

unsigned short CSSPrimitiveValue::primitiveType() const
{
	if(!d)
		return CSS_UNKNOWN;

	return impl->primitiveType();
}

void CSSPrimitiveValue::setFloatValue(unsigned short unitType, float floatValue)
{
	if(d)
		return impl->setFloatValue(unitType, floatValue);
}

float CSSPrimitiveValue::getFloatValue(unsigned short unitType) const
{
	if(!d)
		return 0;

	return impl->getFloatValue(unitType);
}

void CSSPrimitiveValue::setStringValue(unsigned short stringType, const DOMString &stringValue)
{
	if(d)
		return impl->setStringValue(stringType, stringValue);
}

DOMString CSSPrimitiveValue::getStringValue() const
{
	if(!d)
		return DOMString();

	return DOMString(impl->getStringValue());
}

Counter CSSPrimitiveValue::getCounterValue() const
{
	if(!d)
		return Counter::null;

	return Counter(impl->getCounterValue());
}

KDOM::Rect CSSPrimitiveValue::getRectValue() const
{
	if(!d)
		return Rect::null;

	return Rect(impl->getRectValue());
}

RGBColor CSSPrimitiveValue::getRGBColorValue() const
{
	if(!d)
		return RGBColor();

	// TODO: Do we need to ref() here?
	RGBColorImpl *obj = new RGBColorImpl(impl->m_interface, impl->getRGBColorValue());
	obj->ref();
	return RGBColor(obj);
}

}

// vim:ts=4:noet
