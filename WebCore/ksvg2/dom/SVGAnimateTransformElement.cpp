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

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimateTransformElement.h"
#include "SVGAnimateTransformElementImpl.h"

#include "SVGConstants.h"
#include "SVGAnimateTransformElement.lut.h"
using namespace KSVG;

/*
@begin SVGAnimateTransformElement::s_hashTable 2
 dummy	SVGAnimateTransformElementConstants::Dummy	DontDelete|ReadOnly
@end
*/

Value SVGAnimateTransformElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGAnimateTransformElementImpl *>(d))

SVGAnimateTransformElement SVGAnimateTransformElement::null;

SVGAnimateTransformElement::SVGAnimateTransformElement() : SVGAnimationElement()
{
}

SVGAnimateTransformElement::SVGAnimateTransformElement(SVGAnimateTransformElementImpl *i) : SVGAnimationElement(i)
{
}

SVGAnimateTransformElement::SVGAnimateTransformElement(const SVGAnimateTransformElement &other) : SVGAnimationElement()
{
	(*this) = other;
}

SVGAnimateTransformElement::SVGAnimateTransformElement(const KDOM::Node &other) : SVGAnimationElement()
{
	(*this) = other;
}

SVGAnimateTransformElement::~SVGAnimateTransformElement()
{
}

SVGAnimateTransformElement &SVGAnimateTransformElement::operator=(const SVGAnimateTransformElement &other)
{
	SVGAnimationElement::operator=(other);
	return *this;
}

SVGAnimateTransformElement &SVGAnimateTransformElement::operator=(const KDOM::Node &other)
{
	SVGAnimateTransformElementImpl *ohandle = static_cast<SVGAnimateTransformElementImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(d)
				d->deref();
	
			d = 0;
		}
		else
			SVGAnimationElement::operator=(other);
	}

	return *this;
}

// vim:ts=4:noet
