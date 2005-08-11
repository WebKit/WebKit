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

#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGPolygonElement.h"
#include "SVGPolygonElementImpl.h"

#include "SVGConstants.h"
#include "SVGPolygonElement.lut.h"
using namespace KSVG;
using namespace KJS;

/*
@begin SVGPolygonElement::s_hashTable 3
 dummy	SVGPolygonElementConstants::Dummy	DontDelete|ReadOnly
@end
*/

KJS::ValueImp *SVGPolygonElement::getValueProperty(KJS::ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGPolygonElementConstants::Dummy: // shouldnt happen!
			return Undefined();
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

SVGPolygonElement SVGPolygonElement::null;

SVGPolygonElement::SVGPolygonElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPoints()
{
}

SVGPolygonElement::SVGPolygonElement(SVGPolygonElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i), SVGAnimatedPoints(i)
{
}

SVGPolygonElement::SVGPolygonElement(const SVGPolygonElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPoints()
{
	(*this) = other;
}

SVGPolygonElement::SVGPolygonElement(const Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPoints()
{
	(*this) = other;
}

SVGPolygonElement::~SVGPolygonElement()
{
}

SVGPolygonElement &SVGPolygonElement::operator=(const SVGPolygonElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	SVGAnimatedPoints::operator=(other);
	return *this;
}

SVGPolygonElement &SVGPolygonElement::operator=(const KDOM::Node &other)
{
	SVGPolygonElementImpl *ohandle = static_cast<SVGPolygonElementImpl *>(other.handle());
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
			SVGAnimatedPoints::operator=(ohandle);
		}
	}

	return *this;
}

// vim:ts=4:noet
