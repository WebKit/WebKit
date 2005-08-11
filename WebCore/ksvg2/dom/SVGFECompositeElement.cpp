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

#include <kdom/ecma/Ecma.h>

#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGFECompositeElement.h"
#include "SVGFECompositeElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"

#include "SVGConstants.h"
#include "SVGFECompositeElement.lut.h"
using namespace KSVG;

/*
@begin SVGFECompositeElement::s_hashTable 5
 in1		SVGFECompositeElementConstants::In1			DontDelete|ReadOnly
 in2		SVGFECompositeElementConstants::In2			DontDelete|ReadOnly
 operator	SVGFECompositeElementConstants::Operator	DontDelete|ReadOnly
 k1			SVGFECompositeElementConstants::K1			DontDelete|ReadOnly
 k2			SVGFECompositeElementConstants::K2			DontDelete|ReadOnly
 k3			SVGFECompositeElementConstants::K3			DontDelete|ReadOnly
 k4			SVGFECompositeElementConstants::K4			DontDelete|ReadOnly
@end
*/

ValueImp *SVGFECompositeElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFECompositeElementConstants::In1:
			return KDOM::safe_cache<SVGAnimatedString>(exec, in1());
		case SVGFECompositeElementConstants::Operator:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, _operator());
		case SVGFECompositeElementConstants::K1:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, k1());
		case SVGFECompositeElementConstants::K2:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, k2());
		case SVGFECompositeElementConstants::K3:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, k3());
		case SVGFECompositeElementConstants::K4:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, k4());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFECompositeElementImpl *>(d))

SVGFECompositeElement SVGFECompositeElement::null;

SVGFECompositeElement::SVGFECompositeElement() : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFECompositeElement::SVGFECompositeElement(SVGFECompositeElementImpl *i) : SVGElement(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFECompositeElement::SVGFECompositeElement(const SVGFECompositeElement &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFECompositeElement::SVGFECompositeElement(const KDOM::Node &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFECompositeElement::~SVGFECompositeElement()
{
}

SVGFECompositeElement &SVGFECompositeElement::operator=(const SVGFECompositeElement &other)
{
	SVGElement::operator=(other);
	SVGFilterPrimitiveStandardAttributes::operator=(other);
	return *this;
}

SVGFECompositeElement &SVGFECompositeElement::operator=(const KDOM::Node &other)
{
	SVGFECompositeElementImpl *ohandle = static_cast<SVGFECompositeElementImpl *>(other.handle());
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

SVGAnimatedString SVGFECompositeElement::in1() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->in1());
}

SVGAnimatedString SVGFECompositeElement::in2() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->in2());
}

SVGAnimatedEnumeration SVGFECompositeElement::_operator() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->_operator());
}

SVGAnimatedNumber SVGFECompositeElement::k1() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->k1());
}

SVGAnimatedNumber SVGFECompositeElement::k2() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->k2());
}

SVGAnimatedNumber SVGFECompositeElement::k3() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->k3());
}

SVGAnimatedNumber SVGFECompositeElement::k4() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->k4());
}

// vim:ts=4:noet
