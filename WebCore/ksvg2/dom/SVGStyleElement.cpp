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

#include <kdom/DOMString.h>
#include <kdom/ecma/Ecma.h>
#include <kdom/css/StyleSheet.h>
#include <kdom/css/impl/CSSStyleSheetImpl.h>

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGStyleElement.h"
#include "SVGStyleElementImpl.h"

#include "SVGConstants.h"
#include "SVGStyleElement.lut.h"
using namespace KSVG;

/*
@begin SVGStyleElement::s_hashTable 5
 xmlspace	SVGStyleElementConstants::Xmlspace		DontDelete
 type		SVGStyleElementConstants::Type			DontDelete
 media		SVGStyleElementConstants::Media			DontDelete
 title		SVGStyleElementConstants::Title			DontDelete
@end
*/

Value SVGStyleElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case SVGStyleElementConstants::Xmlspace:
			return KDOM::getDOMString(xmlspace());
		case SVGStyleElementConstants::Type:
			return KDOM::getDOMString(type());
		case SVGStyleElementConstants::Media:
			return KDOM::getDOMString(media());
		case SVGStyleElementConstants::Title:
			return KDOM::getDOMString(title());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

void SVGStyleElement::putValueProperty(ExecState *exec, int token, const Value &value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGStyleElementConstants::Xmlspace:
		{
			setXmlspace(KDOM::toDOMString(exec, value));
			break;
		}
		case SVGStyleElementConstants::Type:
		{
			setType(KDOM::toDOMString(exec, value));
			break;
		}
		case SVGStyleElementConstants::Media:
		{
			setMedia(KDOM::toDOMString(exec, value));
			break;
		}
		case SVGStyleElementConstants::Title:
		{
			setTitle(KDOM::toDOMString(exec, value));
			break;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
}

// The qdom way...
#define impl (static_cast<SVGStyleElementImpl *>(d))

SVGStyleElement SVGStyleElement::null;

SVGStyleElement::SVGStyleElement() : SVGElement(), KDOM::LinkStyle()
{
}

SVGStyleElement::SVGStyleElement(SVGStyleElementImpl *i) : SVGElement(i), KDOM::LinkStyle()
{
}

SVGStyleElement::SVGStyleElement(const SVGStyleElement &other) : SVGElement(), KDOM::LinkStyle()
{
	(*this) = other;
}

SVGStyleElement::SVGStyleElement(const KDOM::Node &other) : SVGElement(), KDOM::LinkStyle()
{
	(*this) = other;
}

SVGStyleElement::~SVGStyleElement()
{
}

SVGStyleElement &SVGStyleElement::operator=(const SVGStyleElement &other)
{
	SVGElement::operator=(other);
	return *this;
}

SVGStyleElement &SVGStyleElement::operator=(const KDOM::Node &other)
{
	SVGStyleElementImpl *ohandle = static_cast<SVGStyleElementImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(d)
				d->deref();
	
			d = 0;
		}
		else
			SVGElement::operator=(other);
	}

	return *this;
}

KDOM::DOMString SVGStyleElement::xmlspace() const
{
	if(!d)
		return KDOM::DOMString();

	return impl->xmlspace();
}

void SVGStyleElement::setXmlspace(const KDOM::DOMString &xmlspace)
{
	if(d)
		impl->setXmlspace(xmlspace.implementation());
}

KDOM::DOMString SVGStyleElement::type() const
{
	if(!d)
		return KDOM::DOMString();

	return impl->type();
}

void SVGStyleElement::setType(const KDOM::DOMString &type)
{
	if(d)
		impl->setType(type.implementation());
}

KDOM::DOMString SVGStyleElement::media() const
{
	if(!d)
		return KDOM::DOMString();

	return impl->media();
}

void SVGStyleElement::setMedia(const KDOM::DOMString &media)
{
	if(d)
		impl->setMedia(media.implementation());
}

KDOM::DOMString SVGStyleElement::title() const
{
	if(!d)
		return KDOM::DOMString();

	return impl->title();
}

void SVGStyleElement::setTitle(const KDOM::DOMString &title)
{
	if(d)
		impl->setTitle(title.implementation());
}

KDOM::StyleSheet SVGStyleElement::sheet() const
{
	if(!d)
		return KDOM::StyleSheet();

	return KDOM::StyleSheet(impl->sheet());
}

// vim:ts=4:noet
