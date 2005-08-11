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

#include <kdom/DOMString.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DocumentImpl.h>
#include <kdom/events/EventListener.h>

#include "SVGElement.h"
#include "SVGElementImpl.h"
#include "SVGDocument.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGSVGElement.h"
#include "SVGDOMImplementation.h"

#include "SVGConstants.h"
#include "SVGElement.lut.h"
using namespace KSVG;

// The qdom way...
#define impl (static_cast<SVGElementImpl *>(d))

/*
@begin SVGElement::s_hashTable 5
 id						SVGElementConstants::Id					DontDelete
 xmlbase				SVGElementConstants::Xmlbase			DontDelete
 ownerSVGElement		SVGElementConstants::OwnerSVGElement	DontDelete|ReadOnly
 viewportElement		SVGElementConstants::ViewportElement	DontDelete|ReadOnly
@end
*/

ValueImp *SVGElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGElementConstants::Id:
			return KDOM::getDOMString(id());
		case SVGElementConstants::Xmlbase:
			return KDOM::getDOMString(xmlbase());
		case SVGElementConstants::OwnerSVGElement:
			return KDOM::getDOMNode(exec, ownerSVGElement());
		case SVGElementConstants::ViewportElement:
			return KDOM::getDOMNode(exec, viewportElement());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

void SVGElement::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGElementConstants::Id:
		{
			setAttribute("id", KDOM::toDOMString(exec, value));
			break;
		}
		case SVGElementConstants::Xmlbase:
		{
			// FIXME: Shouldn't that use setAttributeNS?!
			setAttribute("xml:base", KDOM::toDOMString(exec, value));
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
}

SVGElement SVGElement::null;

SVGElement::SVGElement() : KDOM::Element()
{
}

SVGElement::SVGElement(SVGElementImpl *i) : KDOM::Element(i)
{
}

SVGElement::SVGElement(const SVGElement &other) : KDOM::Element()
{
	(*this) = other;
}

SVGElement::SVGElement(const KDOM::Node &other) : KDOM::Element()
{
	(*this) = other;
}

SVGElement::~SVGElement()
{
}

SVGElement &SVGElement::operator=(const SVGElement &other)
{
	KDOM::Element::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(SVGElement, KDOM::ELEMENT_NODE)

KDOM::DOMString SVGElement::id() const
{
	if(!d)
		return KDOM::DOMString();

	return impl->getId();
}

KDOM::DOMString SVGElement::xmlbase() const
{
	if(!d)
		return KDOM::DOMString();

	return impl->xmlbase();
}

SVGSVGElement SVGElement::ownerSVGElement() const
{
	if(!d)
		return SVGSVGElement::null;

	return SVGSVGElement(impl->ownerSVGElement());
}

SVGElement SVGElement::viewportElement() const
{
	if(!d)
		return SVGElement::null;

	return SVGElement(impl->viewportElement());
}

// vim:ts=4:noet
