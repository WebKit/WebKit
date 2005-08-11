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
#include "SVGImageElement.h"
#include "SVGImageElementImpl.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedPreserveAspectRatio.h"

#include "SVGConstants.h"
#include "SVGImageElement.lut.h"
using namespace KSVG;

/*
@begin SVGImageElement::s_hashTable 7
 x					SVGImageElementConstants::X						DontDelete|ReadOnly
 y					SVGImageElementConstants::Y						DontDelete|ReadOnly
 width				SVGImageElementConstants::Width					DontDelete|ReadOnly
 height				SVGImageElementConstants::Height				DontDelete|ReadOnly
 preserveAspectRatio	SVGImageElementConstants::PreserveAspectRatio	DontDelete|ReadOnly
@end
*/

ValueImp *SVGImageElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGImageElementConstants::X:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, x());
		case SVGImageElementConstants::Y:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, y());
		case SVGImageElementConstants::Width:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, width());
		case SVGImageElementConstants::Height:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, height());
		case SVGImageElementConstants::PreserveAspectRatio:
			return KDOM::safe_cache<SVGAnimatedPreserveAspectRatio>(exec, preserveAspectRatio());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGImageElementImpl *>(d))

SVGImageElement SVGImageElement::null;

SVGImageElement::SVGImageElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGURIReference()
{
}

SVGImageElement::SVGImageElement(SVGImageElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i), SVGURIReference(i)
{
}

SVGImageElement::SVGImageElement(const SVGImageElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGURIReference()
{
	(*this) = other;
}

SVGImageElement::SVGImageElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable(), SVGURIReference()
{
	(*this) = other;
}

SVGImageElement::~SVGImageElement()
{
}

SVGImageElement &SVGImageElement::operator=(const SVGImageElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	SVGURIReference::operator=(other);
	return *this;
}

SVGImageElement &SVGImageElement::operator=(const KDOM::Node &other)
{
	SVGImageElementImpl *ohandle = static_cast<SVGImageElementImpl *>(other.handle());
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
			SVGURIReference::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedLength SVGImageElement::x() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->x());
}

SVGAnimatedLength SVGImageElement::y() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->y());
}

SVGAnimatedLength SVGImageElement::width() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->width());
}

SVGAnimatedLength SVGImageElement::height() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->height());
}

SVGAnimatedPreserveAspectRatio SVGImageElement::preserveAspectRatio() const
{
	if(!d)
		return SVGAnimatedPreserveAspectRatio::null;

	return SVGAnimatedPreserveAspectRatio(impl->preserveAspectRatio());
}

// vim:ts=4:noet
