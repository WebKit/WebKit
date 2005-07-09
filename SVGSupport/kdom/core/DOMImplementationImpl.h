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

#ifndef KDOM_DOMImplementationImpl_H
#define KDOM_DOMImplementationImpl_H

namespace KDOM
{
	class Ecma;
	class KDOMView;
	class DOMString;
	class LSInputImpl;
	class LSOutputImpl;
	class LSParserImpl;
	class DocumentType;
	class DocumentImpl;
	class CDFInterface;
	class LSSerializerImpl;
	class DocumentTypeImpl;
	class CSSStyleSheetImpl;

	class DOMImplementationImpl 
	{
	public:
		DOMImplementationImpl();
		virtual ~DOMImplementationImpl();

		static DOMImplementationImpl *self();

		// Gives access to shared css/ecma/... handler
		CDFInterface *cdfInterface() const;

		// 'DOMImplementationImpl' functions
		virtual bool hasFeature(const DOMString &feature, const DOMString &version) const;
		virtual DocumentTypeImpl *createDocumentType(const DOMString &qualifiedName, const DOMString &publicId, const DOMString &systemId) const;

		/**
		 * @param createDocElement this is outside the specification, and is used internally for avoiding
		 * creating document elements even if a @p qualifiedName and @p namespaceURI is supplied. If true,
		 * it follows the specification, e.g, creates an element if the other arguments allows so.
		 */
		virtual DocumentImpl *createDocument(const DOMString &namespaceURI, const DOMString &qualifiedName, const DocumentType &doctype, bool createDocElement, KDOMView *view) const;

		virtual CSSStyleSheetImpl *createCSSStyleSheet(const DOMString &title, const DOMString &media) const;

		virtual LSParserImpl *createLSParser(unsigned short mode, const DOMString &schemaType) const;
		virtual LSInputImpl *createLSInput() const;
		virtual LSOutputImpl *createLSOutput() const;
		virtual LSSerializerImpl *createLSSerializer() const;

		// Map events to types...
		virtual int typeToId(const DOMString &type);
		virtual DOMString idToType(int id);

		virtual KDOM::DocumentType defaultDocumentType() const;

	protected:
		virtual CDFInterface *createCDFInterface() const;

	private:
		mutable CDFInterface *m_cdfInterface;

		static DOMImplementationImpl *s_instance;
	};
};

#endif

// vim:ts=4:noet
