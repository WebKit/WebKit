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

#include <kdom/DOMString.h>
#include <kdom/ecma/Ecma.h>
#include <kdom/impl/DOMImplementationImpl.h>

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGPatternElement.h"
#include "SVGPatternElementImpl.h"

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedTransformList.h"
#include "SVGAnimatedTransformListImpl.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGPatternElement.lut.h"
using namespace KSVG;

/*
@begin SVGPatternElement::s_hashTable 9
 patternUnits			SVGPatternElementConstants::PatternUnits		DontDelete|ReadOnly
 patternContentUnits	SVGPatternElementConstants::PatternContentUnits		DontDelete|ReadOnly
 patternTransform		SVGPatternElementConstants::PatternTransform	DontDelete|ReadOnly
 x						SVGPatternElementConstants::X		DontDelete|ReadOnly
 y						SVGPatternElementConstants::Y		DontDelete|ReadOnly
 width					SVGPatternElementConstants::Width	DontDelete|ReadOnly
 height					SVGPatternElementConstants::Height	DontDelete|ReadOnly
@end
*/

ValueImp *SVGPatternElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case SVGPatternElementConstants::PatternUnits:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, patternUnits());
		case SVGPatternElementConstants::PatternContentUnits:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, patternContentUnits());
		case SVGPatternElementConstants::PatternTransform:
			return KDOM::safe_cache<SVGAnimatedTransformList>(exec, patternTransform());
		case SVGPatternElementConstants::X:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, x());
		case SVGPatternElementConstants::Y:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, y());
		case SVGPatternElementConstants::Width:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, width());
		case SVGPatternElementConstants::Height:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, height());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGPatternElementImpl *>(EventTarget::d))

SVGPatternElement SVGPatternElement::null;

SVGPatternElement::SVGPatternElement() : SVGElement(), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGFitToViewBox()
{
}

SVGPatternElement::SVGPatternElement(SVGPatternElementImpl *i) : SVGElement(i), SVGURIReference(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGFitToViewBox(i)
{
}

SVGPatternElement::SVGPatternElement(const SVGPatternElement &other) : SVGElement(), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGFitToViewBox()
{
	(*this) = other;
}

SVGPatternElement::SVGPatternElement(const KDOM::Node &other) : SVGElement(), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGFitToViewBox()
{
	(*this) = other;
}

SVGPatternElement::~SVGPatternElement()
{
}

SVGPatternElement &SVGPatternElement::operator=(const SVGPatternElement &other)
{
	SVGElement::operator=(other);
	SVGURIReference::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGFitToViewBox::operator=(other);
	return *this;
}

SVGPatternElement &SVGPatternElement::operator=(const KDOM::Node &other)
{
	SVGPatternElementImpl *ohandle = static_cast<SVGPatternElementImpl *>(other.handle());
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
			SVGTests::operator=(ohandle);
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
			SVGFitToViewBox::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedEnumeration SVGPatternElement::patternUnits() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->patternUnits());
}

SVGAnimatedEnumeration SVGPatternElement::patternContentUnits() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->patternContentUnits());
}

SVGAnimatedTransformList SVGPatternElement::patternTransform() const
{
	if(!d)
		return SVGAnimatedTransformList::null;

	return SVGAnimatedTransformList(impl->patternTransform());
}

SVGAnimatedLength SVGPatternElement::x() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->x());
}

SVGAnimatedLength SVGPatternElement::y() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->y());
}

SVGAnimatedLength SVGPatternElement::width() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->width());
}

SVGAnimatedLength SVGPatternElement::height() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->height());
}

// vim:ts=4:noet
