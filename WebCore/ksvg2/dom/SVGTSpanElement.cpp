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
#include "SVGTSpanElement.h"
#include "SVGTSpanElementImpl.h"

#include "SVGConstants.h"
#include "SVGTSpanElement.lut.h"
using namespace KSVG;

/*
@begin SVGTSpanElement::s_hashTable 2
 dummy	SVGTSpanElementConstants::Dummy	DontDelete|ReadOnly
@end
*/

ValueImp *SVGTSpanElement::getValueProperty(ExecState *exec, int token) const
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

SVGTSpanElement SVGTSpanElement::null;

SVGTSpanElement::SVGTSpanElement()
: SVGTextPositioningElement()
{
}

SVGTSpanElement::SVGTSpanElement(SVGTSpanElementImpl *i)
: SVGTextPositioningElement(i)
{
}

SVGTSpanElement::SVGTSpanElement(const SVGTSpanElement &other)
: SVGTextPositioningElement()
{
	(*this) = other;
}

SVGTSpanElement::SVGTSpanElement(const KDOM::Node &other)
: SVGTextPositioningElement()
{
	(*this) = other;
}

SVGTSpanElement::~SVGTSpanElement()
{
}

SVGTSpanElement &SVGTSpanElement::operator=(const SVGTSpanElement &other)
{
	SVGTextPositioningElement::operator=(other);
	return *this;
}

SVGTSpanElement &SVGTSpanElement::operator=(const KDOM::Node &other)
{
	SVGTSpanElementImpl *ohandle = static_cast<SVGTSpanElementImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(d)
				d->deref();
				
			d = 0;
		}
		else
			SVGTextPositioningElement::operator=(other);
	}

	return *this;
}

// vim:ts=4:noet
