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

#include <kdom/kdom.h>
#include <kdom/Shared.h>
#include <kdom/DOMString.h>

#include <kdom/ecma/Ecma.h>

#include "SVGHelper.h"
#include "SVGDocument.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGRectElement.h"
#include "SVGRectElementImpl.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGRectElement.lut.h"
using namespace KSVG;

/*
@begin SVGRectElement::s_hashTable 7
 x		SVGRectElementConstants::X		DontDelete|ReadOnly
 y		SVGRectElementConstants::Y		DontDelete|ReadOnly
 width	SVGRectElementConstants::Width	DontDelete|ReadOnly
 height	SVGRectElementConstants::Height	DontDelete|ReadOnly
 rx		SVGRectElementConstants::Rx		DontDelete|ReadOnly
 ry		SVGRectElementConstants::Ry		DontDelete|ReadOnly
@end
*/

ValueImp *SVGRectElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGRectElementConstants::X:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, x());
		case SVGRectElementConstants::Y:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, y());
		case SVGRectElementConstants::Width:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, width());
		case SVGRectElementConstants::Height:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, height());
		case SVGRectElementConstants::Rx:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, rx());
		case SVGRectElementConstants::Ry:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, ry());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGRectElementImpl *>(d))

SVGRectElement SVGRectElement::null;

SVGRectElement::SVGRectElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
}

SVGRectElement::SVGRectElement(SVGRectElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i)
{
}

SVGRectElement::SVGRectElement(const SVGRectElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGRectElement::SVGRectElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGRectElement::~SVGRectElement()
{
}

SVGRectElement &SVGRectElement::operator=(const SVGRectElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	return *this;
}

SVGRectElement &SVGRectElement::operator=(const KDOM::Node &other)
{
	SVGRectElementImpl *ohandle = static_cast<SVGRectElementImpl *>(other.handle());
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

SVGAnimatedLength SVGRectElement::x() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->x());
}

SVGAnimatedLength SVGRectElement::y() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->y());
}

SVGAnimatedLength SVGRectElement::width() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->width());
}

SVGAnimatedLength SVGRectElement::height() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->height());
}

SVGAnimatedLength SVGRectElement::rx() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->rx());
}

SVGAnimatedLength SVGRectElement::ry() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->ry());
}

// vim:ts=4:noet
