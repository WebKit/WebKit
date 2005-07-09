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
#include <kdom/Shared.h>
#include "RGBColor.h"
#include "CSSHelper.h"
#include "RGBColorImpl.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueImpl.h"

#include "CSSConstants.h"
#include "RGBColor.lut.h"
namespace KDOM
{
    using namespace KJS;

/*
@begin RGBColor::s_hashTable 5
 red	RGBColorConstants::Red		DontDelete|ReadOnly
 green	RGBColorConstants::Green	DontDelete|ReadOnly
 blue	RGBColorConstants::Blue		DontDelete|ReadOnly
@end
*/

Value RGBColor::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case RGBColorConstants::Red:
			return safe_cache<CSSPrimitiveValue>(exec, red());
		case RGBColorConstants::Blue:
			return safe_cache<CSSPrimitiveValue>(exec, blue());
		case RGBColorConstants::Green:
			return safe_cache<CSSPrimitiveValue>(exec, green());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

RGBColor RGBColor::null;

RGBColor::RGBColor() : d(0)
{
}

RGBColor::RGBColor(RGBColorImpl *i) : d(i)
{
	if(d)
		d->ref();
}

RGBColor::RGBColor(const RGBColor &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(RGBColor)

CSSPrimitiveValue RGBColor::red() const
{
	if(!d)
		return CSSPrimitiveValue::null;

	return CSSPrimitiveValue(d->red());
}

CSSPrimitiveValue RGBColor::green() const
{
	if(!d)
		return CSSPrimitiveValue::null;

	return CSSPrimitiveValue(d->green());
}

CSSPrimitiveValue RGBColor::blue() const
{
	if(!d)
		return CSSPrimitiveValue::null;

	return CSSPrimitiveValue(d->blue());
}

}

// vim:ts=4:noet
