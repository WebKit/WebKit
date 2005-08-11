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

#ifndef KDOM_Helper_H
#define KDOM_Helper_H

#include <qstring.h>

class QTextStream;

namespace KDOM
{
	class Node;
	class Length;
	class AttrImpl;
	class NodeImpl;
	class DOMString;
	class ElementImpl;
	class DOMException;
	class DocumentImpl;
	class DOMStringImpl;

	/**
	 * Provides convenience and helper functions related to KDOM.
	 *
	 * @note all functions prefixed with 'Validate' never throw exceptions.
	 * However, those starting with 'Check' may.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class Helper
	{
	public:
		Helper() { }
		~Helper() { }

		/**
		 * Splits @p qualifiedName into @prefix and @p name. @p qualifiedName
		 * remains unchanged.
		 *
		 * TODO wrong
		 *
		 * @param prefix where to save the prefix of @p qualifiedName
		 * @param name where to save the  local name of @p qualifiedName
		 * @param qualifiedName the string to split
		 */
		static void SplitNamespace(DOMString &prefix, DOMString &name,
								   DOMStringImpl *qualifiedName);

		/**
		 *
		 * @param
		 * @param
		 * @param
		 * @param
		 */
		static void SplitPrefixLocalName(DOMStringImpl *qualifiedName, DOMString &prefix,
										 DOMString &localName, int colonPos = -2);

		/**
		 *
		 * @param
		 * @param
		 * @param
		 */
		static void CheckPrefix(const DOMString &prefix, const DOMString &name,
								const DOMString &namespaceURI);

		/**
		 * @p nameCanBeNull and @p nameCanBeEmpty provides fine control of the validation.
		 *
		 * @param qualifiedName
		 * @param namespaceURI
		 * @param colonPos
		 * @param nameCanBeNull
		 * @param nameCanBeEmpty
		 */
		static void CheckQualifiedName(const DOMString &qualifiedName,
									   const DOMString &namespaceURI,
									   int &colonPos,
									   bool nameCanBeNull,
									   bool nameCanBeEmpty);

		/**
		 * Returns the position of the colon in @p qualifiedName. If no colon
		 * is found, -1 is returned.
		 *
		 * @param qualifiedName the string to search for the colon
		 * @returns the position of the colon
		 */
		static int CheckMalformedQualifiedName(const DOMString &qualifiedName);

		/**
		 * TODO
		 */
		static bool IsMalformedPrefix(const DOMString &prefix);

		/**
		 * Returns true if @p prefix is a valid XML namespace prefix. A value
		 * of null is considered valid.
		 *
		 * @param prefix the prefix to validate
		 * @returns true if prefix is valid
		 */
		static bool ValidatePrefix(DOMStringImpl *name);	

		/**
		 * Checks whether @p name is a valid attribute name.
		 *
		 * @param name the attribute name to validate
		 * @returns true if @p name is a valid attribute name
		 */
		static bool ValidateAttributeName(DOMStringImpl *name);

		/**
		 * Checks whether @p name is a valid qualified XML name.
		 *
		 * FIXME: AFAICT, this means a local name validates as a QName - should
		 * it? In that case I think the func name is misleading /englich
		 *
		 * @param name the name to validate
		 * @returns true if @p name is a valid XML name
		 */
		static bool ValidateQualifiedName(DOMStringImpl *name);

		/**
		 * Raises the exception INUSE_ATTRIBUTE_ERR if @p ownerElement is not the
		 * document of @p attr. If it isn't, it's likely because of an abscence of
		 * cloning.
		 *
		 * @note if a node, such as an attribute, should be part of another
		 * document, it must be cloned.
		 *
		 * @param ownerElement the owner element to check against
		 * @param attr the attribute to check
		 *
		 */
		static void CheckInUse(ElementImpl *ownerElement, AttrImpl *attr);	

		/**
		 * Raises the exception WRONG_DOCUMENT_ERR if @p node was created from a document
		 * other than @p ownerDocument.
		 *
		 * @param ownerDocument the document to check against
		 * @param node the node whose owner document should be checked
		 *
		 */
		static void CheckWrongDocument(DocumentImpl *ownerDocument, NodeImpl *node);

		/**
		 * Prints a debug statement showing the DOM exception constant
		 * name for the exception @p e. Useful for debugging exceptions.
		 *
		 * @param e the exception to generate the message for
		 */
		static void ShowException(const DOMException &e);	

		/**
		 * Prints a text representation of @p node to text stream @p ret with
		 * indentation level @p level. The node is serialized into its proper
		 * representation depending on node type.
		 *
		 * @param ret the text stream to output to
		 * @param node the node to serialize
		 * @param prettyIndent if true, the text is indented according to @p level 
		 * @param level the indentation level counted in spaces
		 *
		 */
		static void PrintNode(QTextStream &ret, const Node &node,
							  bool prettyIndent = true, const QString &indent = QString::fromLatin1("  "),
							  unsigned short level = 0);

		/**
		 * Builds an absolute URI from a base URI plus a relative URI. 
		 * This is useful for resolving relative URIs in XML documents.
		 *
		 * A typical use is to build URIs in an xml:base-aware way. For example:
		 * \code
		 * Element element = document.getElementByID( "foobar" );
		 * 
		 * DOMString href = element.getAttribute( "href" );
		 * DOMString documentBase = element.baseURI();
		 *
		 * DOMString theCompleteURL = Helper::BuildURI( documentBase, href );
		 * \endcode
		 *
		 * @param base the base URI to resolve against
		 * @param relative the URI that should be applied to @p base
		 * @returns an URI built from @p base and @relative
		 */
		static DOMString ResolveURI(const DOMString &relative, const DOMString &base);

		/**
		 * Determines whether the string is valid according to the NCName production.
		 * A NCName -- a non-colon name -- is for example the prefix found in
		 * attribute/element names, or the name after the prefix (without the colon).
		 *
		 * @param string to perform validation
		 * @returns true if @p str is a valid NCName
		 */
		static bool IsValidNCName(const DOMString &data);

		/**
		 * Determines whether the string is valid according to the QName production.
		 * A QName -- a qualified name -- is a namespace-aware XML name, for example
		 * "xsl:template" or an NCName, for example "body".
		 *
		 * @param string to perform validation
		 * @returns true if @p str is a valid QName
		 */
		static bool IsValidQName(const DOMString &data);

		/**
		 * mostly just removes the url("...") brace
		 */
		static DOMString parseURL(const DOMString &url);

		/**
		 * Take a string like "1,2,3,4" and returns an array of those lengths.
		 * Contains work arounds to fix strings like "1,2px 3 ,4"  because
		 * web authors can do all kinds of crazy things.
		 *
		 * @param domstr that contains "1,2,3,4" or something similiar
		 * @param len is set to the size of the array.  At minimum this will be 1
		 * @returns An array of Length's of size @p len
		 */
		static Length *stringToLengthArray(const DOMString &data, int &len);
	};
};

#endif

// vim:ts=4:noet
