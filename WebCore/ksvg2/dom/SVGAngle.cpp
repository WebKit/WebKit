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

#include <ksvg2/ksvg.h>

#include "SVGAngle.h"
#include "SVGAngleImpl.h"
#include "SVGElementImpl.h"

#include "SVGConstants.h"
#include "SVGAngle.lut.h"
using namespace KSVG;

/*
@begin SVGAngle::s_hashTable 5
 value						SVGAngleConstants::Value					DontDelete
 valueInSpecifiedUnits		SVGAngleConstants::ValueInSpecifiedUnits	DontDelete
 valueAsString				SVGAngleConstants::ValueAsString			DontDelete
 unitType					SVGAngleConstants::UnitType					DontDelete|ReadOnly
@end
@begin SVGAngleProto::s_hashTable 3
 convertToSpecifiedUnits	SVGAngleConstants::ConvertToSpecifiedUnits	DontDelete|Function 1
 newValueSpecifiedUnits		SVGAngleConstants::NewValueSpecifiedUnits	DontDelete|Function 2
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGAngle", SVGAngleProto, SVGAngleProtoFunc)

ValueImp *SVGAngle::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAngleConstants::Value:
			return Number(value());
		case SVGAngleConstants::ValueInSpecifiedUnits:
			return Number(valueInSpecifiedUnits());
		case SVGAngleConstants::ValueAsString:
			return getDOMString(valueAsString());
		case SVGAngleConstants::UnitType:
			return Number(unitType());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

void SVGAngle::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAngleConstants::Value:
		{
			setValue(value->toNumber(exec));
			break;
		}
		case SVGAngleConstants::ValueInSpecifiedUnits:
		{
			setValueInSpecifiedUnits(value->toNumber(exec));
			break;
		}
		case SVGAngleConstants::ValueAsString:
		{
			setValueAsString(KDOM::toDOMString(exec, value));
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

ValueImp *SVGAngleProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGAngle)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGAngleConstants::ConvertToSpecifiedUnits:
		{
			unsigned short unitType = args[0]->toUInt16(exec);
			obj.convertToSpecifiedUnits(unitType);
		}
		case SVGAngleConstants::NewValueSpecifiedUnits:
		{
			unsigned short unitType = args[0]->toUInt16(exec);
			float valueInSpecifiedUnits = args[1]->toNumber(exec);
			obj.newValueSpecifiedUnits(unitType, valueInSpecifiedUnits);
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(KDOM::DOMException)
	return Undefined();
}

SVGAngle SVGAngle::null;

SVGAngle::SVGAngle() : impl(0)
{
}

SVGAngle::SVGAngle(SVGAngleImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGAngle::SVGAngle(const SVGAngle &other) : impl(0)
{
	(*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAngle)

unsigned short SVGAngle::unitType() const
{
	if(!impl)
		return SVG_ANGLETYPE_UNKNOWN;

	return impl->unitType();
}

void SVGAngle::setValue(float value)
{
	if(impl)
		impl->setValue(value);
}

float SVGAngle::value() const
{
	if(!impl)
		return -1;

	return impl->value();
}

void SVGAngle::setValueInSpecifiedUnits(float valueInSpecifiedUnits)
{
	if(impl)
		impl->setValueInSpecifiedUnits(valueInSpecifiedUnits);
}

float SVGAngle::valueInSpecifiedUnits() const
{
	if(!impl)
		return -1;

	return impl->valueInSpecifiedUnits();
}

void SVGAngle::setValueAsString(const KDOM::DOMString &valueAsString)
{
	if(impl)
		impl->setValueAsString(valueAsString);
}

KDOM::DOMString SVGAngle::valueAsString() const
{
	if(!impl)
		return KDOM::DOMString();

	return impl->valueAsString();
}

void SVGAngle::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
	if(impl)
		impl->newValueSpecifiedUnits(unitType, valueInSpecifiedUnits);
}

void SVGAngle::convertToSpecifiedUnits(unsigned short unitType)
{
	if(impl)
		impl->convertToSpecifiedUnits(unitType);
}

// vim:ts=4:noet
