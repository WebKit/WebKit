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

#ifndef KSVG_SVGDOMImplementationImpl_H
#define KSVG_SVGDOMImplementationImpl_H

#include <kdom/impl/DOMImplementationImpl.h>
#include <qstringlist.h>

namespace KDOM
{
	class DOMString;
	class CDFInterface;
	class DocumentType;
	class DocumentImpl;
	class DocumentTypeImpl;
};

namespace KSVG
{
	class SVGDOMImplementationImpl : public KDOM::DOMImplementationImpl
	{
	public:
		SVGDOMImplementationImpl();
		virtual ~SVGDOMImplementationImpl();

		static SVGDOMImplementationImpl *self();

		// 'SVGDOMImplementationImpl' functions
		bool hasFeature(const KDOM::DOMString &feature, const KDOM::DOMString &version) const;
		KDOM::DocumentTypeImpl *createDocumentType(const KDOM::DOMString &qualifiedName, const KDOM::DOMString &publicId, const KDOM::DOMString &systemId) const;
		KDOM::DocumentImpl *createDocument(const KDOM::DOMString &namespaceURI, const KDOM::DOMString &qualifiedName, const KDOM::DocumentType &doctype, bool createDocElement, KDOM::KDOMView *view) const;

		virtual KDOM::CSSStyleSheetImpl *createCSSStyleSheet(const KDOM::DOMString &title, const KDOM::DOMString &media) const;

		// Internal
		virtual int typeToId(const KDOM::DOMString &type);
		virtual KDOM::DOMString idToType(int eventId);

		virtual KDOM::DocumentType defaultDocumentType() const;

		bool inAnimationContext() const;
		void setAnimationContext(bool value);

	protected:
		virtual KDOM::CDFInterface *createCDFInterface() const;

	private:
		bool m_animationContext : 1;

		static SVGDOMImplementationImpl *s_instance;
		static QStringList s_features;
	};
};

#endif

// vim:ts=4:noet
