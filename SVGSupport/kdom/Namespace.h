/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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

#ifndef KDOM_Namespace_h
#define KDOM_Namespace_h

/**
 * @file Namespace.h
 * @short A collection of common namespaces.
 */

/* Micro styleguide for the Namespace class:
 *
 * - Names are capitalized ala the DOM style: all caps.
 *
 * - Don't overflow with tons of namespaces just for the sake of it or 
 *  completeness, we don't want overengineering or bloat. Add a namespace when 
 *  it is actually used, or it with confidence will be.
 */

#include <kdom/DOMString.h>

namespace KDOM
{
	/**
	 * Namespace for XML Inclusions(XInclude).
	 *
	 * Specification: http://www.w3.org/TR/xinclude/
	 */
	const DOMString NS_XINCLUDE = "http://www.w3.org/2001/XInclude";

	/**
	 * Namespace for the special XML namespace. It is by definition
	 * bound to the "xml" prefix, and should have no usage in 
	 * ordinary code.
	 *
	 * Specification: http://www.w3.org/TR/REC-xml-names/
	 */
	const DOMString NS_XML = "http://www.w3.org/XML/1998/namespace";

	/**
	 * The namespace for the xmlns prefix. The Namespaces in XML recommendation 
	 * explicitly states that xmlns should not have a namespace, but has since 
	 * been changed. See:
	 *
	 * http://www.w3.org/2000/xmlns/
	 */
	const DOMString NS_XMLNS = "http://www.w3.org/2000/xmlns/";

	/**
	 * Namespace for XML Hypertext Markup Language(XHTML).
	 *
	 * Specification: http://www.w3.org/TR/xhtml1/
	 */
	const DOMString NS_XHTML = "http://www.w3.org/1999/xhtml";

	/**
	 * Namespace for Scalable Vector Graphics(SVG).
	 *
	 * Specification: http://www.w3.org/TR/SVG11/
	 */
	const DOMString NS_SVG = "http://www.w3.org/2000/svg";

	/**
	 * Namespace for OASIS XML Catalogs.
	 *
	 * Specification: http://www.oasis-open.org/committees/entity/spec.html
	 */
	const DOMString NS_CATALOG = "urn:oasis:names:tc:entity:xmlns:xml:catalog";

	/**
	 * Not a namespace to be precise, but is a URI 
	 * which identifies a schema of type DTD.
	 *
	 * Specification: http://www.w3.org/TR/DOM-Level-3-LS/
	 */
	const DOMString NS_SCHEMATYPE_DTD = "http://www.w3.org/TR/REC-xml";

	/**
	 * Not a namespace to be precise, but is a URI 
	 * which identifies a schema of type W3C XML Schema(WXS).
	 *
	 * Specification: http://www.w3.org/TR/DOM-Level-3-LS/
	 */
	const DOMString NS_SCHEMATYPE_WXS = "http://www.w3.org/2001/XMLSchema";

	/**
	 * The namespace for one of the two parts in XSL: XSL-T. This namespace
	 * is used for XSL-T 1.0 and 2.0.
	 *
	 * Microsoft's XSL-T engine in MSXML, used in Internet Explorer, 
	 * does in some circumstances use a proprietary namespace
	 * which sometimes causes confusion.
	 *
	 * For XSL-T 1.0, see: http://www.w3.org/TR/xslt; for XSL-T 2.0 see: 
	 * http://www.w3.org/TR/xslt20/
	 */
	const DOMString NS_XSLT = "http://www.w3.org/1999/XSL/Transform";

	/**
	 * The namespace for W3C XML Schema. This is used for the XML language it
	 * is, as well as its built-in data types.
	 *
	 * Specification: http://www.w3.org/TR/xmlschema-2/
	 * @see <a href="http://www.w3.org/TR/xmlschema-2/datatypes.html#namespaces">XML Schema 
	 * Part 2: Datatypes Second Edition, 3.1 Namespace considerations</a>
	 */
	const DOMString NS_WXS = "http://www.w3.org/2001/XMLSchema";

	/**
	 * The namespace for W3C XML Schema attributes used in schema instances.
	 *
	 * Specification: http://www.w3.org/TR/xmlschema-2/
	 *
	 * @see <a href="http://www.w3.org/TR/xmlschema-1/structures.html#Instance_Document_Constructions">XML 
	 * Schema Part 1: Structures Second Edition, 2.6 Schema-Related Markup * in Documents Being Validated</a>
	 */
	const DOMString NS_XSI = "http://www.w3.org/2001/XMLSchema-instance";

	/**
	 * The namespace for built-in XPath functions, as defined in for example
	 * XQuery 1.0 and XPath 2.0 Functions and Operators and XSLT.
	 *
	 * Specification: http://www.w3.org/TR/xquery-operators/
	 */
	const DOMString NS_XFN = "http://www.w3.org/2005/04/xpath-functions";

	/**
	 * The namespace for XPath Data Model Types. These are builtin data types 
	 * defined by the XPath Data Model which are used in addition to W3C 
	 * XML Schema's built-ins.
	 * 
	 * Specification: http://www.w3.org/TR/xpath-datamodel/
	 */
	const DOMString NS_XDT = "http://www.w3.org/2005/04/xpath-datatypes";

	/**
	 * The namespace for identifying errors in XPath.
	 *
	 * Specification: http://www.w3.org/TR/xpath20/#id-identifying-errors
	 */
	const DOMString NS_XPERR = "http://www.w3.org/2004/07/xqt-errors";
};

#endif // KDOM_Namespace_h

// vim:ts=4:noet
