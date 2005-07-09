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
#include <kdom/css/RGBColor.h>

#include "ksvg.h"
#include "SVGColor.h"
#include "SVGHelper.h"
#include "SVGColorImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"

#include "SVGConstants.h"
#include "SVGColor.lut.h"
using namespace KSVG;

/*
@begin SVGColor::s_hashTable 5
 colorType				SVGColorConstants::ColorType			DontDelete|ReadOnly
 rgbColor				SVGColorConstants::RgbColor				DontDelete|ReadOnly
 iccColor				SVGColorConstants::IccColor				DontDelete|ReadOnly
@end
@begin SVGColorProto::s_hashTable 5
 setRGBColor			SVGColorConstants::SetRGBColor			DontDelete|Function 1
 setRGBColorICCColor	SVGColorConstants::SetRGBColorICCColor	DontDelete|Function 2
 setColor				SVGColorConstants::SetColor				DontDelete|Function 3
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGColor", SVGColorProto, SVGColorProtoFunc)

Value SVGColor::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGColorConstants::ColorType:
			return Number(colorType());
		case SVGColorConstants::RgbColor:
			return KDOM::safe_cache<KDOM::RGBColor>(exec, rgbColor());
		// TODO: case SVGColorConstants::IccColor:
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

Value SVGColorProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(SVGColor)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGColorConstants::SetRGBColor:
		{
			KDOM::DOMString rgbColor = KDOM::toDOMString(exec, args[0]);
			obj.setRGBColor(rgbColor);
			return Undefined();
		}
		case SVGColorConstants::SetRGBColorICCColor:
		{
			KDOM::DOMString rgbColor = KDOM::toDOMString(exec, args[0]);
			KDOM::DOMString iccColor = KDOM::toDOMString(exec, args[1]);
			obj.setRGBColorICCColor(rgbColor, iccColor);
			return Undefined();
		}
		case SVGColorConstants::SetColor:
		{
			unsigned short colorType = args[0].toUInt16(exec);
			KDOM::DOMString rgbColor = KDOM::toDOMString(exec, args[1]);
			KDOM::DOMString iccColor = KDOM::toDOMString(exec, args[2]);
			obj.setColor(colorType, rgbColor, iccColor);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGColorImpl *>(d))

SVGColor SVGColor::null;

SVGColor::SVGColor() : KDOM::CSSValue()
{
}

SVGColor::SVGColor(SVGColorImpl *i) : KDOM::CSSValue(i)
{
}

SVGColor::SVGColor(const SVGColor &other) : KDOM::CSSValue()
{
	(*this) = other;
}

SVGColor::SVGColor(const KDOM::CSSValue &other) : KDOM::CSSValue()
{
	(*this) = other;
}

SVGColor::~SVGColor()
{
}

SVGColor &SVGColor::operator=(const SVGColor &other)
{
	KDOM::CSSValue::operator=(other);
	return *this;
}

SVGColor &SVGColor::operator=(const KDOM::CSSValue &other)
{
	SVGColorImpl *ohandle = static_cast<SVGColorImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle)
		{
			if(d)
				d->deref();
			
			d = 0;
		}
		else
			KDOM::CSSValue::operator=(other);
	}

	return *this;
}


unsigned short SVGColor::colorType() const
{
	if(!d)
		return SVG_COLORTYPE_UNKNOWN;

	return impl->colorType();
}

KDOM::RGBColor SVGColor::rgbColor() const
{
	if(!d)
		return KDOM::RGBColor::null;

	return KDOM::RGBColor(impl->rgbColor());
}

void SVGColor::setRGBColor(const KDOM::DOMString &rgbColor)
{
	if(d)
		impl->setRGBColor(rgbColor.implementation());
}

void SVGColor::setRGBColorICCColor(const KDOM::DOMString &rgbColor, const KDOM::DOMString &iccColor)
{
	if(d)
		impl->setRGBColorICCColor(rgbColor.implementation(), iccColor.implementation());
}

void SVGColor::setColor(unsigned short colorType, const KDOM::DOMString &rgbColor, const KDOM::DOMString &iccColor)
{
	if(d)
		impl->setColor(colorType, rgbColor.implementation(), iccColor.implementation());
}

// vim:ts=4:noet
