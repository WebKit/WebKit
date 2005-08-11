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

#include <kdom/DOMString.h>
#include <kdom/ecma/Ecma.h>
#include <kdom/impl/DOMImplementationImpl.h>

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGGradientElement.h"
#include "SVGGradientElementImpl.h"

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedTransformList.h"
#include "SVGAnimatedTransformListImpl.h"

#include "SVGConstants.h"
#include "SVGGradientElement.lut.h"
using namespace KSVG;

/*
@begin SVGGradientElement::s_hashTable 5
 gradientUnits		SVGGradientElementConstants::GradientUnits		DontDelete|ReadOnly
 gradientTransform	SVGGradientElementConstants::GradientTransform	DontDelete|ReadOnly
 spreadMethod		SVGGradientElementConstants::SpreadMethod		DontDelete|ReadOnly
@end
*/

ValueImp *SVGGradientElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case SVGGradientElementConstants::GradientUnits:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, gradientUnits());
		case SVGGradientElementConstants::GradientTransform:
			return KDOM::safe_cache<SVGAnimatedTransformList>(exec, gradientTransform());
		case SVGGradientElementConstants::SpreadMethod:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, spreadMethod());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGGradientElementImpl *>(EventTarget::d))

SVGGradientElement SVGGradientElement::null;

SVGGradientElement::SVGGradientElement() : SVGElement(), SVGURIReference(), SVGExternalResourcesRequired(), SVGStylable()
{
}

SVGGradientElement::SVGGradientElement(SVGGradientElementImpl *i) : SVGElement(i), SVGURIReference(i), SVGExternalResourcesRequired(i), SVGStylable(i)
{
}

SVGGradientElement::SVGGradientElement(const SVGGradientElement &other) : SVGElement(), SVGURIReference(), SVGExternalResourcesRequired(), SVGStylable()
{
	(*this) = other;
}

SVGGradientElement::SVGGradientElement(const KDOM::Node &other) : SVGElement(), SVGURIReference(), SVGExternalResourcesRequired(), SVGStylable()
{
	(*this) = other;
}

SVGGradientElement::~SVGGradientElement()
{
}

SVGGradientElement &SVGGradientElement::operator=(const SVGGradientElement &other)
{
	SVGElement::operator=(other);
	SVGURIReference::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	return *this;
}

SVGGradientElement &SVGGradientElement::operator=(const KDOM::Node &other)
{
	SVGGradientElementImpl *ohandle = static_cast<SVGGradientElementImpl *>(other.handle());
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
			SVGURIReference::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedEnumeration SVGGradientElement::gradientUnits() const
{
	if(!impl)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->gradientUnits());
}

SVGAnimatedTransformList SVGGradientElement::gradientTransform() const
{
	if(!impl)
		return SVGAnimatedTransformList::null;

	return SVGAnimatedTransformList(impl->gradientTransform());
}

SVGAnimatedEnumeration SVGGradientElement::spreadMethod() const
{
	if(!impl)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->spreadMethod());
}

// vim:ts=4:noet
