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

#include "SVGFEOffsetElement.h"
#include "SVGFEOffsetElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"

#include "SVGConstants.h"
#include "SVGFEOffsetElement.lut.h"
using namespace KSVG;

/*
@begin SVGFEOffsetElement::s_hashTable 5
 in1	SVGFEOffsetElementConstants::In1	DontDelete|ReadOnly
 dx		SVGFEOffsetElementConstants::Dx		DontDelete|ReadOnly
 dy		SVGFEOffsetElementConstants::Dy		DontDelete|ReadOnly
@end
*/

Value SVGFEOffsetElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFEOffsetElementConstants::In1:
			return KDOM::safe_cache<SVGAnimatedString>(exec, in1());
		case SVGFEOffsetElementConstants::Dx:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, dx());
		case SVGFEOffsetElementConstants::Dy:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, dy());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFEOffsetElementImpl *>(d))

SVGFEOffsetElement SVGFEOffsetElement::null;

SVGFEOffsetElement::SVGFEOffsetElement() : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFEOffsetElement::SVGFEOffsetElement(SVGFEOffsetElementImpl *i) : SVGElement(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFEOffsetElement::SVGFEOffsetElement(const SVGFEOffsetElement &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEOffsetElement::SVGFEOffsetElement(const KDOM::Node &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEOffsetElement::~SVGFEOffsetElement()
{
}

SVGFEOffsetElement &SVGFEOffsetElement::operator=(const SVGFEOffsetElement &other)
{
	SVGElement::operator=(other);
	SVGFilterPrimitiveStandardAttributes::operator=(other);
	return *this;
}

SVGFEOffsetElement &SVGFEOffsetElement::operator=(const KDOM::Node &other)
{
	SVGFEOffsetElementImpl *ohandle = static_cast<SVGFEOffsetElementImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(d)
				d->deref();
			
			d = 0;
		}
		else
		{
			SVGElement::operator=(other);
			SVGFilterPrimitiveStandardAttributes::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedString SVGFEOffsetElement::in1() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->in1());
}

SVGAnimatedNumber SVGFEOffsetElement::dx() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->dx());
}

SVGAnimatedNumber SVGFEOffsetElement::dy() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->dy());
}

// vim:ts=4:noet
