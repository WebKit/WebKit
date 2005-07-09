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
#include "SVGTextPositioningElement.h"
#include "SVGTextPositioningElementImpl.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedLengthListImpl.h"
#include "SVGAnimatedNumberList.h"
#include "SVGAnimatedNumberListImpl.h"

#include "SVGConstants.h"
#include "SVGTextPositioningElement.lut.h"
using namespace KSVG;

/*
@begin SVGTextPositioningElement::s_hashTable 7
 x			SVGTextPositioningElementConstants::X		DontDelete|ReadOnly
 y			SVGTextPositioningElementConstants::Y		DontDelete|ReadOnly
 dx			SVGTextPositioningElementConstants::Dx		DontDelete|ReadOnly
 dy			SVGTextPositioningElementConstants::Dy		DontDelete|ReadOnly
 rotate		SVGTextPositioningElementConstants::Rotate	DontDelete|ReadOnly
@end
*/

Value SVGTextPositioningElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGTextPositioningElementConstants::X:
			return KDOM::safe_cache<SVGAnimatedLengthList>(exec, x());
		case SVGTextPositioningElementConstants::Y:
			return KDOM::safe_cache<SVGAnimatedLengthList>(exec, y());
		case SVGTextPositioningElementConstants::Dx:
			return KDOM::safe_cache<SVGAnimatedLengthList>(exec, dx());
		case SVGTextPositioningElementConstants::Dy:
			return KDOM::safe_cache<SVGAnimatedLengthList>(exec, dy());
		case SVGTextPositioningElementConstants::Rotate:
			return KDOM::safe_cache<SVGAnimatedNumberList>(exec, rotate());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGTextPositioningElementImpl *>(d))

SVGTextPositioningElement SVGTextPositioningElement::null;

SVGTextPositioningElement::SVGTextPositioningElement() : SVGTextContentElement()
{
}

SVGTextPositioningElement::SVGTextPositioningElement(SVGTextPositioningElementImpl *i) : SVGTextContentElement(i)
{
}

SVGTextPositioningElement::SVGTextPositioningElement(const SVGTextPositioningElement &other) : SVGTextContentElement()
{
	(*this) = other;
}

SVGTextPositioningElement::SVGTextPositioningElement(const KDOM::Node &other) : SVGTextContentElement()
{
	(*this) = other;
}

SVGTextPositioningElement::~SVGTextPositioningElement()
{
}

SVGTextPositioningElement &SVGTextPositioningElement::operator=(const SVGTextPositioningElement &other)
{
	SVGTextContentElement::operator=(other);
	return *this;
}

SVGTextPositioningElement &SVGTextPositioningElement::operator=(const KDOM::Node &other)
{
	SVGTextPositioningElementImpl *ohandle = static_cast<SVGTextPositioningElementImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(d)
				d->deref();
				
			d = 0;
		}
		else
			SVGTextContentElement::operator=(other);
	}

	return *this;
}

SVGAnimatedLengthList SVGTextPositioningElement::x() const
{
	if(!d)
		return SVGAnimatedLengthList::null;

	return SVGAnimatedLengthList(impl->x());
}

SVGAnimatedLengthList SVGTextPositioningElement::y() const
{
	if(!d)
		return SVGAnimatedLengthList::null;

	return SVGAnimatedLengthList(impl->y());
}

SVGAnimatedLengthList SVGTextPositioningElement::dx() const
{
	if(!d)
		return SVGAnimatedLengthList::null;

	return SVGAnimatedLengthList(impl->dx());
}

SVGAnimatedLengthList SVGTextPositioningElement::dy() const
{
	if(!d)
		return SVGAnimatedLengthList::null;

	return SVGAnimatedLengthList(impl->dy());
}

SVGAnimatedNumberList SVGTextPositioningElement::rotate() const
{
	if(!d)
		return SVGAnimatedNumberList::null;

	return SVGAnimatedNumberList(impl->rotate());
}

// vim:ts=4:noet
