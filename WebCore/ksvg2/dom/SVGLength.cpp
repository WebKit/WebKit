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

#include <qrect.h>

#include <kdom/ecma/Ecma.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include <kcanvas/KCanvasItem.h>
#include <kcanvas/device/KRenderingStyle.h>

#include <ksvg2/ksvg.h>

#include "SVGLength.h"
#include "SVGLengthImpl.h"
#include "SVGHelper.h"

#include "SVGConstants.h"
#include "SVGLength.lut.h"
using namespace KSVG;

/*
@begin SVGLength::s_hashTable 5
 unitType					SVGLengthConstants::UnitType				DontDelete|ReadOnly
 value						SVGLengthConstants::Value					DontDelete
 valueAsString				SVGLengthConstants::ValueAsString			DontDelete
 valueInSpecifiedUnits		SVGLengthConstants::ValueInSpecifiedUnits	DontDelete
@end
@begin SVGLengthProto::s_hashTable 3
 newValueSpecifiedUnits		SVGLengthConstants::NewValueSpecifiedUnits	DontDelete|Function 2
 convertToSpecifiedUnits	SVGLengthConstants::ConvertToSpecifiedUnits	DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGLength", SVGLengthProto, SVGLengthProtoFunc)

ValueImp *SVGLength::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGLengthConstants::UnitType:
			return Number(unitType());
		case SVGLengthConstants::Value:
			return Number(value());
		case SVGLengthConstants::ValueAsString:
			return KDOM::getDOMString(valueAsString());
		case SVGLengthConstants::ValueInSpecifiedUnits:
			return Number(valueInSpecifiedUnits());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

void SVGLength::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case SVGLengthConstants::Value:
			setValue(value->toNumber(exec));
			break;
		case SVGLengthConstants::ValueAsString:
			setValueAsString(KDOM::toDOMString(exec, value));
			break;
		case SVGLengthConstants::ValueInSpecifiedUnits:
			setValueInSpecifiedUnits(value->toNumber(exec));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
}

ValueImp *SVGLengthProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGLength)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGLengthConstants::NewValueSpecifiedUnits:
		{
			unsigned short unitType = args[0]->toUInt16(exec);
			float valueInSpecifiedUnits = args[1]->toNumber(exec);
			obj.newValueSpecifiedUnits(unitType, valueInSpecifiedUnits);
			return Undefined();
		}
		case SVGLengthConstants::ConvertToSpecifiedUnits:
		{
			unsigned short unitType = args[0]->toUInt16(exec);
			obj.convertToSpecifiedUnits(unitType);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(KDOM::DOMException)
	return Undefined();
}

SVGLength SVGLength::null;

SVGLength::SVGLength() : impl(0)
{
}

SVGLength::SVGLength(SVGLengthImpl *i) : impl(i)
{
	if(impl)
		impl->ref();
}

SVGLength::SVGLength(const SVGLength &other) : impl(0)
{
	(*this) = other;
}

SVGLength::~SVGLength()
{
	if(impl)
	{
		if(impl->refCount() == 1)
			KDOM::ScriptInterpreter::forgetDOMObject(impl);

		impl->deref();
	}
}

SVGLength &SVGLength::operator=(const SVGLength &other)
{
	if(impl != other.impl)
	{
		if(impl)
		{
			if(impl->refCount() == 1)
				KDOM::ScriptInterpreter::forgetDOMObject(impl);

			impl->deref();
		}

		impl = other.impl;

		if(impl)
			impl->ref();
	}

	return *this;
}

bool SVGLength::operator==(const SVGLength &other) const
{
	return impl == other.impl;
}

bool SVGLength::operator!=(const SVGLength &other) const
{
	return !operator==(other);
}

unsigned short SVGLength::unitType() const
{
	if(!impl)
		return SVG_LENGTHTYPE_UNKNOWN;

	return impl->unitType();
}

void SVGLength::setValue(float value)
{
	if(impl)
		impl->setValue(value);
}

float SVGLength::value() const
{
	if(!impl)
		return 0;

	return impl->value();
}

void SVGLength::setValueInSpecifiedUnits(float valueInSpecifiedUnits)
{
	if(impl)
		impl->setValueInSpecifiedUnits(valueInSpecifiedUnits);
}

float SVGLength::valueInSpecifiedUnits() const
{
	if(!impl)
		return 0;

	return impl->valueInSpecifiedUnits();
}

void SVGLength::setValueAsString(const KDOM::DOMString &valueAsString)
{
	if(impl)
		impl->setValueAsString(valueAsString);
}

KDOM::DOMString SVGLength::valueAsString() const
{
	if(!impl)
		return KDOM::DOMString("");

	return impl->valueAsString();		
}

void SVGLength::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
	if(impl)
		impl->newValueSpecifiedUnits(unitType, valueInSpecifiedUnits);
}

void SVGLength::convertToSpecifiedUnits(unsigned short unitType)
{
	if(impl)
		impl->convertToSpecifiedUnits(unitType);
}

// vim:ts=4:noet
