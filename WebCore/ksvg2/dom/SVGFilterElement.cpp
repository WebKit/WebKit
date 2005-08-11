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
#include "SVGFilterElement.h"
#include "SVGFilterElementImpl.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedIntegerImpl.h"

#include "SVGConstants.h"
#include "SVGFilterElement.lut.h"
using namespace KSVG;

/*
@begin SVGFilterElement::s_hashTable 9
 filterUnits	SVGFilterElementConstants::FilterUnits		DontDelete|ReadOnly
 primitiveUnits	SVGFilterElementConstants::PrimitiveUnits	DontDelete|ReadOnly
 x				SVGFilterElementConstants::X				DontDelete|ReadOnly
 y				SVGFilterElementConstants::Y				DontDelete|ReadOnly
 width			SVGFilterElementConstants::Width			DontDelete|ReadOnly
 height			SVGFilterElementConstants::Height			DontDelete|ReadOnly
 filterResX		SVGFilterElementConstants::FilterResX		DontDelete|ReadOnly
 filterResY		SVGFilterElementConstants::FilterResY		DontDelete|ReadOnly
@end
@begin SVGFilterElementProto::s_hashTable 3
 setFilterRes	SVGFilterElementConstants::SetFilterRes		DontDelete|Function 2
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGFilterElement", SVGFilterElementProto, SVGFilterElementProtoFunc)

ValueImp *SVGFilterElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGFilterElementConstants::FilterUnits:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, filterUnits());
		case SVGFilterElementConstants::PrimitiveUnits:
			return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, primitiveUnits());
		case SVGFilterElementConstants::X:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, x());
		case SVGFilterElementConstants::Y:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, y());
		case SVGFilterElementConstants::Width:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, width());
		case SVGFilterElementConstants::Height:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, height());
		case SVGFilterElementConstants::FilterResX:
			return KDOM::safe_cache<SVGAnimatedInteger>(exec, filterResX());
		case SVGFilterElementConstants::FilterResY:
			return KDOM::safe_cache<SVGAnimatedInteger>(exec, filterResY());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

ValueImp *SVGFilterElementProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	SVGFilterElement obj(cast(exec, thisObj));
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGFilterElementConstants::SetFilterRes:
		{
			unsigned long filterResX = args[0]->toUInt32(exec);;
			unsigned long filterResY = args[1]->toUInt32(exec);;
			obj.setFilterRes(filterResX, filterResY);
			return Undefined();
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFilterElementImpl *>(d))

SVGFilterElement SVGFilterElement::null;

SVGFilterElement::SVGFilterElement() : SVGElement(), SVGURIReference(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable()
{
}

SVGFilterElement::SVGFilterElement(SVGFilterElementImpl *i) : SVGElement(i), SVGURIReference(i), SVGLangSpace(i), SVGExternalResourcesRequired(i), SVGStylable(i)
{
}

SVGFilterElement::SVGFilterElement(const SVGFilterElement &other) : SVGElement(), SVGURIReference(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable()
{
	(*this) = other;
}

SVGFilterElement::SVGFilterElement(const KDOM::Node &other) : SVGElement(), SVGURIReference(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGStylable()
{
	(*this) = other;
}

SVGFilterElement::~SVGFilterElement()
{
}

SVGFilterElement &SVGFilterElement::operator=(const SVGFilterElement &other)
{
	SVGElement::operator=(other);
	SVGURIReference::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	return *this;
}

SVGFilterElement &SVGFilterElement::operator=(const KDOM::Node &other)
{
	SVGFilterElementImpl *ohandle = static_cast<SVGFilterElementImpl *>(other.handle());
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
			SVGURIReference::operator=(ohandle);
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedEnumeration SVGFilterElement::filterUnits() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->filterUnits());
}

SVGAnimatedEnumeration SVGFilterElement::primitiveUnits() const
{
	if(!d)
		return SVGAnimatedEnumeration::null;

	return SVGAnimatedEnumeration(impl->primitiveUnits());
}

SVGAnimatedLength SVGFilterElement::x() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->x());
}

SVGAnimatedLength SVGFilterElement::y() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->y());
}

SVGAnimatedLength SVGFilterElement::width() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->width());
}

SVGAnimatedLength SVGFilterElement::height() const
{
	if(!d)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->height());
}

SVGAnimatedInteger SVGFilterElement::filterResX() const
{
	if(!d)
		return SVGAnimatedInteger::null;

	return SVGAnimatedInteger(impl->filterResX());
}

SVGAnimatedInteger SVGFilterElement::filterResY() const
{
	if(!d)
		return SVGAnimatedInteger::null;

	return SVGAnimatedInteger(impl->filterResY());
}

void SVGFilterElement::setFilterRes(unsigned long filterResX, unsigned long filterResY) const
{
	if(d)
		return impl->setFilterRes(filterResX, filterResY);
}

// vim:ts=4:noet
