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

#include "SVGFEFloodElement.h"
#include "SVGFEFloodElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"

#include "SVGConstants.h"
#include "SVGFEFloodElement.lut.h"
using namespace KSVG;

/*
@begin SVGFEFloodElement::s_hashTable 3
 in1			SVGFEFloodElementConstants::In1		DontDelete|ReadOnly
@end
*/

ValueImp *SVGFEFloodElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFEFloodElementConstants::In1:
			return KDOM::safe_cache<SVGAnimatedString>(exec, in1());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFEFloodElementImpl *>(d))

SVGFEFloodElement SVGFEFloodElement::null;

SVGFEFloodElement::SVGFEFloodElement() : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFEFloodElement::SVGFEFloodElement(SVGFEFloodElementImpl *i) : SVGElement(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFEFloodElement::SVGFEFloodElement(const SVGFEFloodElement &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEFloodElement::SVGFEFloodElement(const KDOM::Node &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEFloodElement::~SVGFEFloodElement()
{
}

SVGFEFloodElement &SVGFEFloodElement::operator=(const SVGFEFloodElement &other)
{
	SVGElement::operator=(other);
	SVGFilterPrimitiveStandardAttributes::operator=(other);
	return *this;
}

SVGFEFloodElement &SVGFEFloodElement::operator=(const KDOM::Node &other)
{
	SVGFEFloodElementImpl *ohandle = static_cast<SVGFEFloodElementImpl *>(other.handle());
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

SVGAnimatedString SVGFEFloodElement::in1() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->in1());
}

// vim:ts=4:noet
