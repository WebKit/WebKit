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

#include "SVGFEFuncGElement.h"
#include "SVGFEFuncGElementImpl.h"

using namespace KSVG;

// The qdom way...
#define impl (static_cast<SVGFEFuncGElementImpl *>(d))

SVGFEFuncGElement SVGFEFuncGElement::null;

SVGFEFuncGElement::SVGFEFuncGElement() : SVGComponentTransferFunctionElement()
{
}

SVGFEFuncGElement::SVGFEFuncGElement(SVGFEFuncGElementImpl *i) : SVGComponentTransferFunctionElement(i)
{
}

SVGFEFuncGElement::SVGFEFuncGElement(const SVGFEFuncGElement &other) : SVGComponentTransferFunctionElement()
{
	(*this) = other;
}

SVGFEFuncGElement::SVGFEFuncGElement(const KDOM::Node &other) : SVGComponentTransferFunctionElement()
{
	(*this) = other;
}

SVGFEFuncGElement::~SVGFEFuncGElement()
{
}

SVGFEFuncGElement &SVGFEFuncGElement::operator=(const SVGFEFuncGElement &other)
{
	SVGComponentTransferFunctionElement::operator=(other);
	return *this;
}

SVGFEFuncGElement &SVGFEFuncGElement::operator=(const KDOM::Node &other)
{
	SVGFEFuncGElementImpl *ohandle = static_cast<SVGFEFuncGElementImpl *>(other.handle());
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
