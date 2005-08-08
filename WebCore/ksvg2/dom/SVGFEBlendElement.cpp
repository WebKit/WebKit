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

#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGFEBlendElement.h"
#include "SVGFEBlendElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"

#include "SVGConstants.h"
#include "SVGFEBlendElement.lut.h"
using namespace KSVG;

/*
@begin SVGFEBlendElement::s_hashTable 5
 in1			SVGFEBlendElementConstants::In1		DontDelete|ReadOnly
 in2			SVGFEBlendElementConstants::In2		DontDelete|ReadOnly
 mode			SVGFEBlendElementConstants::Mode	DontDelete|ReadOnly
@end
*/

ValueImp *SVGFEBlendElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFEBlendElementConstants::In1:
			return KDOM::safe_cache<SVGAnimatedString>(exec, in1());
		case SVGFEBlendElementConstants::In2:
			return KDOM::safe_cache<SVGAnimatedString>(exec, in2());
		case SVGFEBlendElementConstants::Mode:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, mode());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFEBlendElementImpl *>(d))

SVGFEBlendElement SVGFEBlendElement::null;

SVGFEBlendElement::SVGFEBlendElement() : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFEBlendElement::SVGFEBlendElement(SVGFEBlendElementImpl *i) : SVGElement(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFEBlendElement::SVGFEBlendElement(const SVGFEBlendElement &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEBlendElement::SVGFEBlendElement(const KDOM::Node &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEBlendElement::~SVGFEBlendElement()
{
}

SVGFEBlendElement &SVGFEBlendElement::operator=(const SVGFEBlendElement &other)
{
	SVGElement::operator=(other);
	SVGFilterPrimitiveStandardAttributes::operator=(other);
	return *this;
}

SVGFEBlendElement &SVGFEBlendElement::operator=(const KDOM::Node &other)
{
	SVGFEBlendElementImpl *ohandle = static_cast<SVGFEBlendElementImpl *>(other.handle());
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
			SVGFilterPrimitiveStandardAttributes::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedString SVGFEBlendElement::in1() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->in1());
}

SVGAnimatedString SVGFEBlendElement::in2() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->in2());
}

SVGAnimatedEnumeration SVGFEBlendElement::mode() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->mode());
}

// vim:ts=4:noet
