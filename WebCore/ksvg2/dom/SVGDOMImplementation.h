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

#ifndef KSVG_DOMImplementation_H
#define KSVG_DOMImplementation_H

#include <kdom/DOMImplementation.h>

namespace KDOM
{
	class DOMString;
	class Document;
	class DocumentType;
	class DocumentImpl;
	class DocumentTypeImpl;
};

namespace KSVG
{
	class SVGDOMImplementationImpl;
	class SVGDOMImplementation : public KDOM::DOMImplementation
	{
	public:
		SVGDOMImplementation();
		explicit SVGDOMImplementation(SVGDOMImplementationImpl *i);
		SVGDOMImplementation(const SVGDOMImplementation &other);
		virtual ~SVGDOMImplementation();

		// Operators
		SVGDOMImplementation &operator=(const SVGDOMImplementation &other);
		bool operator==(const SVGDOMImplementation &other) const;
		bool operator!=(const SVGDOMImplementation &other) const;

		// 'SVGDOMImplementation' functions
		bool hasFeature(const KDOM::DOMString &feature, const KDOM::DOMString &version);
		KDOM::DocumentType createDocumentType(const KDOM::DOMString &qualifiedName, const KDOM::DOMString &publicId, const KDOM::DOMString &systemId);
		KDOM::Document createDocument(const KDOM::DOMString &namespaceURI, const KDOM::DOMString &qualifiedName, const KDOM::DocumentType &doctype);

		// DOMImplementationCSS inherited function
		KDOM::CSSStyleSheet createCSSStyleSheet(const KDOM::DOMString &title, const KDOM::DOMString &media);

		// Internal
		KSVG_INTERNAL_BASE(SVGDOMImplementation)

	protected:
		SVGDOMImplementationImpl *impl;

	private:
		static SVGDOMImplementation *s_instance;
	};
};

#endif

// vim:ts=4:noet
