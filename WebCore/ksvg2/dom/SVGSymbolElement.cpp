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

#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGSymbolElement.h"
#include "SVGSymbolElementImpl.h"

#include "SVGConstants.h"
#include "SVGSymbolElement.lut.h"
using namespace KSVG;

/*
@begin SVGSymbolElement::s_hashTable 3
 dummy	SVGSymbolElementConstants::Dummy	DontDelete|ReadOnly
@end
*/

Value SVGSymbolElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

SVGSymbolElement SVGSymbolElement::null;

SVGSymbolElement::SVGSymbolElement()
: SVGElement(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGFitToViewBox()
{
}

SVGSymbolElement::SVGSymbolElement(SVGSymbolElementImpl *i)
: SVGElement(i), SVGLangSpace(i), SVGExternalResourcesRequired(i),
  SVGStylable(i), SVGFitToViewBox(i)
{
}

SVGSymbolElement::SVGSymbolElement(const SVGSymbolElement &other)
: SVGElement(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGFitToViewBox()
{
	(*this) = other;
}

SVGSymbolElement::SVGSymbolElement(const KDOM::Node &other)
: SVGElement(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGFitToViewBox()
{
	(*this) = other;
}

SVGSymbolElement::~SVGSymbolElement()
{
}

SVGSymbolElement &SVGSymbolElement::operator=(const SVGSymbolElement &other)
{
	SVGElement::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGFitToViewBox::operator=(other);
	return *this;
}

SVGSymbolElement &SVGSymbolElement::operator=(const KDOM::Node &other)
{
	SVGSymbolElementImpl *ohandle = static_cast<SVGSymbolElementImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(d)
				d->deref();

			Node::d = 0;
		}
		else
		{
			SVGElement::operator=(other);
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
			SVGFitToViewBox::operator=(ohandle);
		}
	}

	return *this;
}

// vim:ts=4:noet
