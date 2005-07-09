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

#ifndef KDOM_DOMImplementation_H
#define KDOM_DOMImplementation_H

#include <kdom/ecma/DOMLookup.h>
#include <kdom/css/DOMImplementationCSS.h>
#include <kdom/ls/DOMImplementationLS.h>

namespace KDOM
{
	class Document;
	class DOMString;
	class DOMObject;
	class DocumentType;
	class DOMImplementationImpl;

	/**
	 * @short The DOMImplementation interface provides a number of methods
	 * for performing operations that are independent of any particular
	 * instance of the document object model.
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class DOMImplementation : public DOMImplementationCSS,
							  public DOMImplementationLS
	{
	public:
		DOMImplementation();
		explicit DOMImplementation(DOMImplementationImpl *i);
		DOMImplementation(const DOMImplementation &other);
		virtual ~DOMImplementation();

		// Operators
		DOMImplementation &operator=(const DOMImplementation &other);
		bool operator==(const DOMImplementation &other) const;
		bool operator!=(const DOMImplementation &other) const;

		// 'DOMImplementation' functions
		/**
		 * Test if the DOM implementation implements a specific feature
		 * and version, as specified in DOM Features.
		 *
		 * @param feature The name of the feature to test.
		 * @param version This is the version number of the feature to test.
		 *
		 * @returns true if the feature is implemented in the specified
		 * version, false otherwise.
		 */
		bool hasFeature(const DOMString &feature, const DOMString &version);

		/**
		 * Creates an empty DocumentType node. Entity declarations and notations
		 * are not made available. Entity reference expansions and default
		 * attribute additions do not occur.
		 *
		 * @param qualifiedName The qualified name of the document type to be created.
		 * @param publicId The external subset public identifier.
		 * @param systemId The external subset system identifier.
		 *
		 * @returns A new DocumentType node with Node.ownerDocument set to null.
		 */
		DocumentType createDocumentType(const DOMString &qualifiedName, const DOMString &publicId,
						                const DOMString &systemId) const;

		/**
		 * Creates a DOM Document object of the specified type with its document
		 * element. Note that based on the DocumentType given to create the
		 * document, the implementation may instantiate specialized Document
		 * objects that support additional features than the "Core", such as
		 * "HTML" [DOM Level 2 HTML]. On the other hand, setting the
		 * DocumentType after the document was created makes this very
		 * unlikely to happen. Alternatively, specialized Document creation
		 * methods, such as createHTMLDocument [DOM Level 2 HTML], can be
		 * used to obtain specific types of Document objects.
		 *
		 * @param namespaceURI The namespace URI of the document element to
		 * create or null.
		 * @param qualifiedName The qualified name of the document element to
		 * be created or null.
		 * @param doctype The type of document to be created or null. When
		 * doctype is not null, its Node.ownerDocument attribute is set to
		 * the document being created.
		 *
		 * @returns A new Document object with its document element. If the
		 * NamespaceURI, qualifiedName, and doctype are null, the returned
		 * Document is empty with no document element.
		 */
		Document createDocument(const DOMString &namespaceURI, const DOMString &qualifiedName,
						        const DocumentType &doctype) const;

		/**
		 * This method returns a specialized object which implements the
		 * specialized APIs of the specified feature and version, as specified
		 * in DOM Features. The specialized object may also be obtained by
		 * using binding-specific casting methods but is not necessarily expected
		 * to, as discussed in Mixed DOM Implementations. This method also allow
		 * the implementation to provide specialized objects which do not support
		 * the DOMImplementation interface.
		 *
		 * @param feature The name of the feature requested. Note that any plus
		 * sign "+" prepended to the name of the feature will be ignored since
		 * it is not significant in the context of this method. 
		 * @param version This is the version number of the feature to test.
		 *
		 * @returns Returns an object which implements the specialized APIs
		 * of the specified feature and version, if any, or null if there is
		 * no object which implements interfaces associated with that feature.
		 * If the DOMObject returned by this method implements the
		 * DOMImplementation interface, it must delegate to the primary
		 * core DOMImplementation and not return results inconsistent with the
		 * primary core DOMImplementation such as hasFeature, getFeature, etc.
		 */
		DOMObject getFeature(const DOMString &feature, const DOMString &version) const; // DOM3

		// DOMImplementationCSS inherited function
		CSSStyleSheet createCSSStyleSheet(const DOMString &title, const DOMString &media) const;

		// DOMImplementationLS inherited function
		virtual LSParser createLSParser(unsigned short mode, const DOMString &schemaType) const;
		virtual LSSerializer createLSSerializer() const;
		virtual LSInput createLSInput() const;
		virtual LSOutput createLSOutput() const;

		static DOMImplementation self();

		// Internal
		KDOM_INTERNAL_BASE(DOMImplementation)

	protected:
		DOMImplementationImpl *d;

	public: // EcmaScript section
		KDOM_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(DOMImplementationProto)
KDOM_IMPLEMENT_PROTOFUNC(DOMImplementationProtoFunc, DOMImplementation)

#endif

// vim:ts=4:noet
