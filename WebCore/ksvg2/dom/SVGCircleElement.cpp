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

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGCircleElement.h"
#include "SVGAnimatedLength.h"
#include "SVGCircleElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGCircleElement.lut.h"
using namespace KSVG;

/*
@begin SVGCircleElement::s_hashTable 5
 cx		SVGCircleElementConstants::Cx		DontDelete|ReadOnly
 cy		SVGCircleElementConstants::Cy		DontDelete|ReadOnly
 r		SVGCircleElementConstants::R		DontDelete|ReadOnly
@end
*/

ValueImp *SVGCircleElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGCircleElementConstants::Cx:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, cx());
		case SVGCircleElementConstants::Cy:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, cy());
		case SVGCircleElementConstants::R:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, r());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGCircleElementImpl *>(d))

SVGCircleElement SVGCircleElement::null;

SVGCircleElement::SVGCircleElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
}

SVGCircleElement::SVGCircleElement(SVGCircleElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i)
{
}

SVGCircleElement::SVGCircleElement(const SVGCircleElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGCircleElement::SVGCircleElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGCircleElement::~SVGCircleElement()
{
}

SVGCircleElement &SVGCircleElement::operator=(const SVGCircleElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	return *this;
}

SVGCircleElement &SVGCircleElement::operator=(const KDOM::Node &other)
{
	SVGCircleElementImpl *ohandle = static_cast<SVGCircleElementImpl *>(other.handle());
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
			SVGTests::operator=(ohandle);
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
			SVGTransformable::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedLength SVGCircleElement::cx() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->cx());
}

SVGAnimatedLength SVGCircleElement::cy() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->cy());
}

SVGAnimatedLength SVGCircleElement::r() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->r());
}

// vim:ts=4:noet
