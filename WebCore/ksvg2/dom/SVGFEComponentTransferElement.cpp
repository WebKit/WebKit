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

#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGFEComponentTransferElement.h"
#include "SVGFEComponentTransferElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"

#include "SVGConstants.h"
#include "SVGFEComponentTransferElement.lut.h"
using namespace KSVG;

/*
@begin SVGFEComponentTransferElement::s_hashTable 3
 in1			SVGFEComponentTransferElementConstants::In1		DontDelete|ReadOnly
@end
*/

ValueImp *SVGFEComponentTransferElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFEComponentTransferElementConstants::In1:
			return KDOM::safe_cache<SVGAnimatedString>(exec, in1());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFEComponentTransferElementImpl *>(d))

SVGFEComponentTransferElement SVGFEComponentTransferElement::null;

SVGFEComponentTransferElement::SVGFEComponentTransferElement() : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFEComponentTransferElement::SVGFEComponentTransferElement(SVGFEComponentTransferElementImpl *i) : SVGElement(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFEComponentTransferElement::SVGFEComponentTransferElement(const SVGFEComponentTransferElement &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEComponentTransferElement::SVGFEComponentTransferElement(const KDOM::Node &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
	(*this) = other;
}

SVGFEComponentTransferElement::~SVGFEComponentTransferElement()
{
}

SVGFEComponentTransferElement &SVGFEComponentTransferElement::operator=(const SVGFEComponentTransferElement &other)
{
	SVGElement::operator=(other);
	SVGFilterPrimitiveStandardAttributes::operator=(other);
	return *this;
}

SVGFEComponentTransferElement &SVGFEComponentTransferElement::operator=(const KDOM::Node &other)
{
	SVGFEComponentTransferElementImpl *ohandle = static_cast<SVGFEComponentTransferElementImpl *>(other.handle());
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

SVGAnimatedString SVGFEComponentTransferElement::in1() const
{
	if(!d)
		return SVGAnimatedString::null;

	return SVGAnimatedString(impl->in1());
}

// vim:ts=4:noet
