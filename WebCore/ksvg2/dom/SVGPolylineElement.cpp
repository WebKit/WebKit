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

#include "SVGPolylineElement.h"
#include "SVGPolylineElementImpl.h"

#include "SVGConstants.h"
#include "SVGPolylineElement.lut.h"
using namespace KSVG;

/*
@begin SVGPolylineElement::s_hashTable 3
 dummy	SVGPolylineElementConstants::Dummy	DontDelete|ReadOnly
@end
*/

ValueImp *SVGPolylineElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGPolylineElementConstants::Dummy: // shouldnt happen!
			return Undefined();
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

SVGPolylineElement SVGPolylineElement::null;

SVGPolylineElement::SVGPolylineElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPoints()
{
}

SVGPolylineElement::SVGPolylineElement(SVGPolylineElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i), SVGAnimatedPoints(i)
{
}

SVGPolylineElement::SVGPolylineElement(const SVGPolylineElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPoints()
{
	(*this) = other;
}

SVGPolylineElement::SVGPolylineElement(const Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGAnimatedPoints()
{
	(*this) = other;
}

SVGPolylineElement::~SVGPolylineElement()
{
}

SVGPolylineElement &SVGPolylineElement::operator=(const SVGPolylineElement &other)
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

SVGPolylineElement &SVGPolylineElement::operator=(const KDOM::Node &other)
{
	SVGPolylineElementImpl *ohandle = static_cast<SVGPolylineElementImpl *>(other.handle());
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
