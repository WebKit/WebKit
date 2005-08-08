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

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGEllipseElement.h"
#include "SVGAnimatedLength.h"
#include "SVGEllipseElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGEllipseElement.lut.h"
using namespace KSVG;

/*
@begin SVGEllipseElement::s_hashTable 5
 cx		SVGEllipseElementConstants::Cx		DontDelete|ReadOnly
 cy		SVGEllipseElementConstants::Cy		DontDelete|ReadOnly
 rx		SVGEllipseElementConstants::Rx		DontDelete|ReadOnly
 ry		SVGEllipseElementConstants::Ry		DontDelete|ReadOnly
@end
*/

ValueImp *SVGEllipseElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGEllipseElementConstants::Cx:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, cx());
		case SVGEllipseElementConstants::Cy:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, cy());
		case SVGEllipseElementConstants::Rx:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, rx());
		case SVGEllipseElementConstants::Ry:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, ry());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGEllipseElementImpl *>(d))

SVGEllipseElement SVGEllipseElement::null;

SVGEllipseElement::SVGEllipseElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
}

SVGEllipseElement::SVGEllipseElement(SVGEllipseElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i)
{
}

SVGEllipseElement::SVGEllipseElement(const SVGEllipseElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGEllipseElement::SVGEllipseElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGEllipseElement::~SVGEllipseElement()
{
}

SVGEllipseElement &SVGEllipseElement::operator=(const SVGEllipseElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	return *this;
}

SVGEllipseElement &SVGEllipseElement::operator=(const KDOM::Node &other)
{
	SVGEllipseElementImpl *ohandle = static_cast<SVGEllipseElementImpl *>(other.handle());
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

SVGAnimatedLength SVGEllipseElement::cx() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->cx());
}

SVGAnimatedLength SVGEllipseElement::cy() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->cy());
}

SVGAnimatedLength SVGEllipseElement::rx() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->rx());
}

SVGAnimatedLength SVGEllipseElement::ry() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->ry());
}

// vim:ts=4:noet
