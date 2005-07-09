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

#include "SVGFEColorMatrixElement.h"
#include "SVGFEColorMatrixElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedNumberList.h"
#include "SVGAnimatedNumberListImpl.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"

#include "SVGConstants.h"
#include "SVGFEColorMatrixElement.lut.h"
using namespace KSVG;

/*
@begin SVGFEColorMatrixElement::s_hashTable 5
 in1		SVGFEColorMatrixElementConstants::In1		DontDelete|ReadOnly
 type		SVGFEColorMatrixElementConstants::Type		DontDelete|ReadOnly
 values		SVGFEColorMatrixElementConstants::Values	DontDelete|ReadOnly
@end
*/

Value SVGFEColorMatrixElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFEColorMatrixElementConstants::In1:
			return KDOM::safe_cache<SVGAnimatedString>(exec, in1());
		case SVGFEColorMatrixElementConstants::Type:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, type());
		case SVGFEColorMatrixElementConstants::Values:
			return KDOM::safe_cache<SVGAnimatedNumberList>(exec, values());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFEColorMatrixElementImpl *>(d))

SVGFEColorMatrixElement SVGFEColorMatrixElement::null;

SVGFEColorMatrixElement::SVGFEColorMatrixElement() : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFEColorMatrixElement::SVGFEColorMatrixElement(SVGFEColorMatrixElementImpl *i) : SVGElement(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFEColorMatrixElement::SVGFEColorMatrixElement(const SVGFEColorMatrixElement &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEColorMatrixElement::SVGFEColorMatrixElement(const KDOM::Node &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEColorMatrixElement::~SVGFEColorMatrixElement()
{
}

SVGFEColorMatrixElement &SVGFEColorMatrixElement::operator=(const SVGFEColorMatrixElement &other)
{
	SVGElement::operator=(other);
	SVGFilterPrimitiveStandardAttributes::operator=(other);
	return *this;
}

SVGFEColorMatrixElement &SVGFEColorMatrixElement::operator=(const KDOM::Node &other)
{
	SVGFEColorMatrixElementImpl *ohandle = static_cast<SVGFEColorMatrixElementImpl *>(other.handle());
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

SVGAnimatedString SVGFEColorMatrixElement::in1() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->in1());
}

SVGAnimatedEnumeration SVGFEColorMatrixElement::type() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->type());
}

SVGAnimatedNumberList SVGFEColorMatrixElement::values() const
{
	if(!d)
		return SVGAnimatedNumberList::null;

	return SVGAnimatedNumberList(impl->values());
}

// vim:ts=4:noet
