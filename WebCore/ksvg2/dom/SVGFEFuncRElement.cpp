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

#include "SVGFEFuncRElement.h"
#include "SVGFEFuncRElementImpl.h"

using namespace KSVG;

// The qdom way...
#define impl (static_cast<SVGFEFuncRElementImpl *>(d))

SVGFEFuncRElement SVGFEFuncRElement::null;

SVGFEFuncRElement::SVGFEFuncRElement() : SVGComponentTransferFunctionElement()
{
}

SVGFEFuncRElement::SVGFEFuncRElement(SVGFEFuncRElementImpl *i) : SVGComponentTransferFunctionElement(i)
{
}

SVGFEFuncRElement::SVGFEFuncRElement(const SVGFEFuncRElement &other) : SVGComponentTransferFunctionElement()
{
	(*this) = other;
}

SVGFEFuncRElement::SVGFEFuncRElement(const KDOM::Node &other) : SVGComponentTransferFunctionElement()
{
	(*this) = other;
}

SVGFEFuncRElement::~SVGFEFuncRElement()
{
}

SVGFEFuncRElement &SVGFEFuncRElement::operator=(const SVGFEFuncRElement &other)
{
	SVGComponentTransferFunctionElement::operator=(other);
	return *this;
}

SVGFEFuncRElement &SVGFEFuncRElement::operator=(const KDOM::Node &other)
{
	SVGFEFuncRElementImpl *ohandle = static_cast<SVGFEFuncRElementImpl *>(other.handle());
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
			SVGComponentTransferFunctionElement::operator=(other);
		}
	}

	return *this;
}

// vim:ts=4:noet
