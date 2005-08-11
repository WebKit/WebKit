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
#include "SVGClipPathElement.h"
#include "SVGClipPathElementImpl.h"
#include "SVGAnimatedEnumeration.h"

#include "SVGConstants.h"
#include "SVGClipPathElement.lut.h"
using namespace KSVG;

/*
@begin SVGClipPathElement::s_hashTable 3
 clipPathUnits	SVGClipPathElementConstants::ClipPathUnits	DontDelete|ReadOnly
@end
*/

ValueImp *SVGClipPathElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGClipPathElementConstants::ClipPathUnits:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, clipPathUnits());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGClipPathElementImpl *>(d))

SVGClipPathElement SVGClipPathElement::null;

SVGClipPathElement::SVGClipPathElement() : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
}

SVGClipPathElement::SVGClipPathElement(SVGClipPathElementImpl *i) : SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i)
{
}

SVGClipPathElement::SVGClipPathElement(const SVGClipPathElement &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGClipPathElement::SVGClipPathElement(const KDOM::Node &other) : SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGClipPathElement::~SVGClipPathElement()
{
}

SVGClipPathElement &SVGClipPathElement::operator=(const SVGClipPathElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	return *this;
}

SVGClipPathElement &SVGClipPathElement::operator=(const KDOM::Node &other)
{
	SVGClipPathElementImpl *ohandle = static_cast<SVGClipPathElementImpl *>(other.handle());
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

SVGAnimatedEnumeration SVGClipPathElement::clipPathUnits() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->clipPathUnits());
}

// vim:ts=4:noet
