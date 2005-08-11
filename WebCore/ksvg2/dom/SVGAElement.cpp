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

#include <kdom/Shared.h>
#include <kdom/DOMString.h>

#include <kdom/ecma/Ecma.h>

#include "SVGHelper.h"
#include "SVGDocument.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAElement.h"
#include "SVGAElementImpl.h"
#include "SVGAnimatedString.h"

#include "SVGConstants.h"
#include "SVGAElement.lut.h"
using namespace KSVG;

/*
@begin SVGAElement::s_hashTable 3
 target	SVGAElementConstants::Target		DontDelete|ReadOnly
@end
*/

ValueImp *SVGAElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGAElementConstants::Target:
			return KDOM::safe_cache<SVGAnimatedString>(exec, target());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGAElementImpl *>(d))

SVGAElement SVGAElement::null;

SVGAElement::SVGAElement() : SVGElement(), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
}

SVGAElement::SVGAElement(SVGAElementImpl *i) : SVGElement(i), SVGURIReference(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i), SVGTransformable(i)
{
}

SVGAElement::SVGAElement(const SVGAElement &other) : SVGElement(), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGAElement::SVGAElement(const KDOM::Node &other) : SVGElement(), SVGURIReference(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable(), SVGTransformable()
{
	(*this) = other;
}

SVGAElement::~SVGAElement()
{
}

SVGAElement &SVGAElement::operator=(const SVGAElement &other)
{
	SVGElement::operator=(other);
	SVGURIReference::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGTransformable::operator=(other);
	return *this;
}

SVGAElement &SVGAElement::operator=(const KDOM::Node &other)
{
	SVGAElementImpl *ohandle = static_cast<SVGAElementImpl *>(other.handle());
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
			SVGTransformable::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedString SVGAElement::target() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->target());
}

// vim:ts=4:noet
