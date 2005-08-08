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
#include "SVGTitleElement.h"
#include "SVGExceptionImpl.h"
#include "SVGTitleElementImpl.h"

#include "SVGConstants.h"
#include "SVGTitleElement.lut.h"
using namespace KSVG;

/*
@begin SVGTitleElement::s_hashTable 2
 dummy	SVGTitleElementConstants::Dummy	DontDelete|ReadOnly
@end
*/

ValueImp *SVGTitleElement::getValueProperty(ExecState *exec, int token) const
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
#define impl (static_cast<SVGTitleElementImpl *>(d))

SVGTitleElement SVGTitleElement::null;

SVGTitleElement::SVGTitleElement() : SVGElement(), SVGLangSpace(), SVGStylable()
{
}

SVGTitleElement::SVGTitleElement(SVGTitleElementImpl *i) : SVGElement(i), SVGLangSpace(i), SVGStylable(i)
{
}

SVGTitleElement::SVGTitleElement(const SVGTitleElement &other) : SVGElement(), SVGLangSpace(), SVGStylable()
{
	(*this) = other;
}

SVGTitleElement::SVGTitleElement(const KDOM::Node &other) : SVGElement(), SVGLangSpace(), SVGStylable()
{
	(*this) = other;
}

SVGTitleElement::~SVGTitleElement()
{
}

SVGTitleElement &SVGTitleElement::operator=(const SVGTitleElement &other)
{
	SVGElement::operator=(other);
	SVGLangSpace::operator=(other);
	SVGStylable::operator=(other);
	return *this;
}

SVGTitleElement &SVGTitleElement::operator=(const KDOM::Node &other)
{
	SVGTitleElementImpl *ohandle = static_cast<SVGTitleElementImpl *>(other.handle());
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
			SVGLangSpace::operator=(ohandle);
			SVGStylable::operator=(ohandle);
		}
	}

	return *this;
}

// vim:ts=4:noet
