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

#include <kdom/kdom.h>
#include <kdom/Shared.h>
#include <kdom/DOMString.h>

#include <kdom/ecma/Ecma.h>

#include "SVGHelper.h"
#include "SVGDocument.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGFEImageElement.h"
#include "SVGFEImageElementImpl.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"

#include "SVGConstants.h"
#include "SVGFEImageElement.lut.h"
using namespace KSVG;

/*
@begin SVGFEImageElement::s_hashTable 3
 preserveAspectRatio	SVGFEImageElementConstants::PreserveAspectRatio	DontDelete|ReadOnly
@end
*/

ValueImp *SVGFEImageElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFEImageElementConstants::PreserveAspectRatio:
			return KDOM::safe_cache<SVGAnimatedPreserveAspectRatio>(exec, preserveAspectRatio());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFEImageElementImpl *>(d))

SVGFEImageElement SVGFEImageElement::null;

SVGFEImageElement::SVGFEImageElement() : SVGElement(), SVGURIReference(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFEImageElement::SVGFEImageElement(SVGFEImageElementImpl *i) : SVGElement(i), SVGURIReference(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFEImageElement::SVGFEImageElement(const SVGFEImageElement &other) : SVGElement(), SVGURIReference(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEImageElement::SVGFEImageElement(const KDOM::Node &other) : SVGElement(), SVGURIReference(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEImageElement::~SVGFEImageElement()
{
}

SVGFEImageElement &SVGFEImageElement::operator=(const SVGFEImageElement &other)
{
	SVGElement::operator=(other);
	SVGURIReference::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGFilterPrimitiveStandardAttributes::operator=(other);
	return *this;
}

SVGFEImageElement &SVGFEImageElement::operator=(const KDOM::Node &other)
{
	SVGFEImageElementImpl *ohandle = static_cast<SVGFEImageElementImpl *>(other.handle());
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
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGFilterPrimitiveStandardAttributes::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedPreserveAspectRatio SVGFEImageElement::preserveAspectRatio() const
{
	if(!d)
		return SVGAnimatedPreserveAspectRatio::null;

	return SVGAnimatedPreserveAspectRatio(impl->preserveAspectRatio());
}

// vim:ts=4:noet
