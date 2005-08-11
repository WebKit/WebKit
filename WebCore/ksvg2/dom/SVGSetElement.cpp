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

#include <kdom/ecma/Ecma.h>

#include "SVGException.h"
#include "SVGSetElement.h"
#include "SVGExceptionImpl.h"
#include "SVGSetElementImpl.h"

#include "SVGConstants.h"
#include "SVGSetElement.lut.h"
using namespace KSVG;

/*
@begin SVGSetElement::s_hashTable 2
 dummy	SVGSetElementConstants::Dummy	DontDelete|ReadOnly
@end
*/

ValueImp *SVGSetElement::getValueProperty(ExecState *exec, int token) const
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
#define impl (static_cast<SVGSetElementImpl *>(d))

SVGSetElement SVGSetElement::null;

SVGSetElement::SVGSetElement() : SVGAnimationElement()
{
}

SVGSetElement::SVGSetElement(SVGSetElementImpl *i) : SVGAnimationElement(i)
{
}

SVGSetElement::SVGSetElement(const SVGSetElement &other) : SVGAnimationElement()
{
	(*this) = other;
}

SVGSetElement::SVGSetElement(const KDOM::Node &other) : SVGAnimationElement()
{
	(*this) = other;
}

SVGSetElement::~SVGSetElement()
{
}

SVGSetElement &SVGSetElement::operator=(const SVGSetElement &other)
{
	SVGAnimationElement::operator=(other);
	return *this;
}

SVGSetElement &SVGSetElement::operator=(const KDOM::Node &other)
{
	SVGSetElementImpl *ohandle = static_cast<SVGSetElementImpl *>(other.handle());
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
