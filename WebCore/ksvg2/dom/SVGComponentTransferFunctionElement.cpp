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

#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGComponentTransferFunctionElement.h"
#include "SVGComponentTransferFunctionElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedNumberList.h"
#include "SVGAnimatedNumberListImpl.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberImpl.h"

#include "SVGConstants.h"
#include "SVGComponentTransferFunctionElement.lut.h"
using namespace KSVG;

/*
@begin SVGComponentTransferFunctionElement::s_hashTable 9
 type		SVGComponentTransferFunctionElementConstants::Type	DontDelete|ReadOnly
 tableValues	SVGComponentTransferFunctionElementConstants::TableValues	DontDelete|ReadOnly
 slope		SVGComponentTransferFunctionElementConstants::Slope	DontDelete|ReadOnly
 intercept	SVGComponentTransferFunctionElementConstants::Intercept	DontDelete|ReadOnly
 amplitude	SVGComponentTransferFunctionElementConstants::Amplitude	DontDelete|ReadOnly
 exponent	SVGComponentTransferFunctionElementConstants::Exponent	DontDelete|ReadOnly
 offset		SVGComponentTransferFunctionElementConstants::Offset	DontDelete|ReadOnly
@end
*/

ValueImp *SVGComponentTransferFunctionElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGComponentTransferFunctionElementConstants::Type:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, type());
		case SVGComponentTransferFunctionElementConstants::TableValues:
			return KDOM::safe_cache<SVGAnimatedNumberList>(exec, tableValues());
		case SVGComponentTransferFunctionElementConstants::Slope:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, slope());
		case SVGComponentTransferFunctionElementConstants::Intercept:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, intercept());
		case SVGComponentTransferFunctionElementConstants::Amplitude:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, amplitude());
		case SVGComponentTransferFunctionElementConstants::Exponent:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, exponent());
		case SVGComponentTransferFunctionElementConstants::Offset:
			return KDOM::safe_cache<SVGAnimatedNumber>(exec, offset());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGComponentTransferFunctionElementImpl *>(d))

SVGComponentTransferFunctionElement SVGComponentTransferFunctionElement::null;

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement() : SVGElement()
{
}

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(SVGComponentTransferFunctionElementImpl *i) : SVGElement(i)
{
}

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const SVGComponentTransferFunctionElement &other) : SVGElement()
{
	(*this) = other;
}

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const KDOM::Node &other) : SVGElement()
{
	(*this) = other;
}

SVGComponentTransferFunctionElement::~SVGComponentTransferFunctionElement()
{
}

SVGComponentTransferFunctionElement &SVGComponentTransferFunctionElement::operator=(const SVGComponentTransferFunctionElement &other)
{
	SVGElement::operator=(other);
	return *this;
}

SVGComponentTransferFunctionElement &SVGComponentTransferFunctionElement::operator=(const KDOM::Node &other)
{
	SVGComponentTransferFunctionElementImpl *ohandle = static_cast<SVGComponentTransferFunctionElementImpl *>(other.handle());
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
		}
	}

	return *this;
}

SVGAnimatedEnumeration SVGComponentTransferFunctionElement::type() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->type());
}

SVGAnimatedNumberList SVGComponentTransferFunctionElement::tableValues() const
{
	if(!d)
		return SVGAnimatedNumberList::null;

	return SVGAnimatedNumberList(impl->tableValues());
}

SVGAnimatedNumber SVGComponentTransferFunctionElement::slope() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->slope());
}

SVGAnimatedNumber SVGComponentTransferFunctionElement::intercept() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->intercept());
}

SVGAnimatedNumber SVGComponentTransferFunctionElement::amplitude() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->amplitude());
}

SVGAnimatedNumber SVGComponentTransferFunctionElement::exponent() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->exponent());
}

SVGAnimatedNumber SVGComponentTransferFunctionElement::offset() const
{
	if(!d)
		return SVGAnimatedNumber::null;

	return SVGAnimatedNumber(impl->offset());
}

// vim:ts=4:noet
