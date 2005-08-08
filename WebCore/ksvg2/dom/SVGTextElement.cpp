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
#include "SVGTextElement.h"
#include "SVGTextElementImpl.h"

#include "SVGConstants.h"
#include "SVGTextElement.lut.h"
using namespace KSVG;

/*
@begin SVGTextElement::s_hashTable 2
 dummy	SVGTextElementConstants::Dummy	DontDelete|ReadOnly
@end
*/

ValueImp *SVGTextElement::getValueProperty(ExecState *exec, int token) const
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

SVGTextElement SVGTextElement::null;

SVGTextElement::SVGTextElement()
: SVGTextPositioningElement(), SVGTransformable()
{
}

SVGTextElement::SVGTextElement(SVGTextElementImpl *i)
: SVGTextPositioningElement(i), SVGTransformable(i)
{
}

SVGTextElement::SVGTextElement(const SVGTextElement &other)
: SVGTextPositioningElement(), SVGTransformable()
{
	(*this) = other;
}

SVGTextElement::SVGTextElement(const KDOM::Node &other)
: SVGTextPositioningElement(), SVGTransformable()
{
	(*this) = other;
}

SVGTextElement::~SVGTextElement()
{
}

SVGTextElement &SVGTextElement::operator=(const SVGTextElement &other)
{
	SVGTextPositioningElement::operator=(other);
	SVGTransformable::operator=(other);
	return *this;
}

SVGTextElement &SVGTextElement::operator=(const KDOM::Node &other)
{
	SVGTextElementImpl *ohandle = static_cast<SVGTextElementImpl *>(other.handle());
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
			SVGTextPositioningElement::operator=(other);
			SVGTransformable::operator=(ohandle);
		}
	}

	return *this;
}

// vim:ts=4:noet
