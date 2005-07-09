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
#include "SVGLineElement.h"
#include "SVGAnimatedLength.h"
#include "SVGLineElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGLineElement.lut.h"
using namespace KSVG;

/*
@begin SVGLineElement::s_hashTable 5
 x1		SVGLineElementConstants::X1		DontDelete|ReadOnly
 y1		SVGLineElementConstants::Y1		DontDelete|ReadOnly
 x2		SVGLineElementConstants::X2		DontDelete|ReadOnly
 y2		SVGLineElementConstants::Y2		DontDelete|ReadOnly
@end
*/

Value SVGLineElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGLineElementConstants::X1:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, x1());
		case SVGLineElementConstants::Y1:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, y1());
		case SVGLineElementConstants::X2:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, x2());
		case SVGLineElementConstants::Y2:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, y2());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGLineElementImpl *>(d))

SVGLineElement SVGLineElement::null;

SVGLineElement::SVGLineElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
}

SVGLineElement::SVGLineElement(SVGLineElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i)
{
}

SVGLineElement::SVGLineElement(const SVGLineElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGLineElement::SVGLineElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGLineElement::~SVGLineElement()
{
}

SVGLineElement &SVGLineElement::operator=(const SVGLineElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	return *this;
}

SVGLineElement &SVGLineElement::operator=(const KDOM::Node &other)
{
	SVGLineElementImpl *ohandle = static_cast<SVGLineElementImpl *>(other.handle());
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

SVGAnimatedLength SVGLineElement::x1() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->x1());
}

SVGAnimatedLength SVGLineElement::y1() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->y1());
}

SVGAnimatedLength SVGLineElement::x2() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->x2());
}

SVGAnimatedLength SVGLineElement::y2() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->y2());
}

// vim:ts=4:noet
