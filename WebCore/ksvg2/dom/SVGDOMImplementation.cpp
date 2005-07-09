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

#include <kstaticdeleter.h>

#include <kdom/kdom.h>
#include <kdom/Helper.h>
#include <kdom/Element.h>
#include <kdom/DOMString.h>
#include <kdom/DOMException.h>
#include <kdom/DocumentType.h>
#include <kdom/impl/DocumentTypeImpl.h>
#include <kdom/css/CSSStyleSheet.h>
#include <kdom/css/impl/MediaListImpl.h>

#include "SVGDocument.h"
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGDOMImplementation.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

//static KStaticDeleter<SVGDOMImplementation> instanceDeleter;
SVGDOMImplementation *SVGDOMImplementation::s_instance = 0;

SVGDOMImplementation SVGDOMImplementation::null;

SVGDOMImplementation::SVGDOMImplementation() : KDOM::DOMImplementation(), impl(0)
{
}

SVGDOMImplementation::SVGDOMImplementation(SVGDOMImplementationImpl *i) : KDOM::DOMImplementation(i), impl(i)
{
}

SVGDOMImplementation::SVGDOMImplementation(const SVGDOMImplementation &other) : KDOM::DOMImplementation(), impl(0)
{
	(*this) = other;
}

SVGDOMImplementation::~SVGDOMImplementation()
{
}

SVGDOMImplementation &SVGDOMImplementation::operator=(const SVGDOMImplementation &other)
{
	KDOM::DOMImplementation::operator=(other);
	
	impl = other.impl;
	return *this;
}

bool SVGDOMImplementation::operator==(const SVGDOMImplementation &other) const
{
	return impl == other.impl;
}

bool SVGDOMImplementation::operator!=(const SVGDOMImplementation &other) const
{
	return !operator==(other);
}

bool SVGDOMImplementation::hasFeature(const KDOM::DOMString &feature, const KDOM::DOMString &version)
{
	if(!impl)
		return false;

	return impl->hasFeature(feature, version);
}

KDOM::DocumentType SVGDOMImplementation::createDocumentType(const KDOM::DOMString &qualifiedName, const KDOM::DOMString &publicId, const KDOM::DOMString &systemId)
{
	if(!impl)
		return KDOM::DocumentType::null;

	return KDOM::DocumentType(impl->createDocumentType(qualifiedName, publicId, systemId));
}

KDOM::Document SVGDOMImplementation::createDocument(const KDOM::DOMString &namespaceURI, const KDOM::DOMString &qualifiedName, const KDOM::DocumentType &doctype)
{
	if(!impl)
		return KDOM::Document::null;

	return KDOM::Document(impl->createDocument(namespaceURI, qualifiedName, doctype, true, 0 /* no view */));
}

KDOM::CSSStyleSheet SVGDOMImplementation::createCSSStyleSheet(const KDOM::DOMString &title, const KDOM::DOMString &media)
{
	if(!impl)
		return KDOM::CSSStyleSheet::null;

	return KDOM::CSSStyleSheet(impl->createCSSStyleSheet(title, media));
}

// vim:ts=4:noet
