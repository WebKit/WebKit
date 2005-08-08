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

#ifndef KDOM_Element_H
#define KDOM_Element_H

#include <kdom/Node.h>
#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class Attr;
	class NodeList;
	class TypeInfo;
	class DOMString;
	class ElementImpl;

	/**
	 * @short The Element interface represents an element in an HTML or XML document.
	 * Elements may * have attributes associated with them; since the Element interface
	 * inherits from Node, the generic Node interface attribute attributes may be used
	 * to retrieve the set of all attributes for an element. There are methods on the
	 * Element interface to retrieve either an Attr object by name or an attribute value
	 * by name. In XML, where an attribute value may contain entity
	 * references, an Attr object should be retrieved to examine the possibly fairly complex
	 * sub-tree representing the attribute value. On the other hand, in HTML, where all
	 * attributes have simple string values, methods to directly access an attribute value
	 * can safely be used as a convenience.
	 *
	 * Note: In DOM Level 2, the method normalize is inherited from the Node interface
	 * where it was moved.
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class Element : public Node 
	{
	public:
		Element();
		Element(ElementImpl *p);
		Element(const Element &other);
		Element(const Node &other);
		virtual ~Element();

		// Operators
		Element &operator=(const Element &other);
		Element &operator=(const Node &other);

		// 'Element' functions
		/**
		 * The name of the element. If Node.localName is different from
		 * null, this attribute is a qualified name. For example, in:
		 * &lt;elementExample id=&quot;demo&quot;&gt; ... &lt;/elementExample&gt; ,
		 * \c tagName has the value \c &quot;elementExample&quot;
		 * . Note that this is case-preserving in XML, as are all
		 * of the operations of the DOM. The HTML DOM returns the
		 * \c tagName of an HTML element in the canonical uppercase
		 * form, regardless of the case in the source HTML document.
		 */
		DOMString tagName() const;

		/**
		 * Retrieves an attribute value by name.
		 *
		 * @param name The name of the attribute to retrieve.
		 *
		 * @returns The Attr value as a string, or the empty string if
		 * that attribute does not have a specified or default value.
		 */
		DOMString getAttribute(const DOMString &name) const;

		/**
		 * Adds a new attribute node. If an attribute with that name is already present
		 * in the element, its value is changed to be that of the value parameter. This
		 * value is a simple string; it is not parsed as it is being set. So any markup
		 * (such as syntax to be recognized as an entity reference) is treated as literal
		 * text, and needs to be appropriately escaped by the implementation when it is
		 * written out. In order to assign an attribute value that contains entity references,
		 * the user must create an Attr node plus any Text and EntityReference nodes, build
		 * the appropriate subtree, and use setAttributeNode to assign it as the value of
		 * an attribute.
		 * To set an attribute with a qualified name and namespace URI, use the
		 * setAttributeNS method.
		 *
		 * @param name The name of the attribute to create or alter.
		 * @param value ValueImp *to set in string form.
		 */
		void setAttribute(const DOMString &name, const DOMString &value);

		/**
		 * Removes an attribute by name. If a default value for the removed attribute is
		 * defined in the DTD, a new attribute immediately appears with the default value
		 * as well as the corresponding namespace URI, local name, and prefix when applicable.
		 * The implementation may handle default values from other schemas similarly but
		 * applications should use Document.normalizeDocument() to guarantee this information
		 * is up-to-date.
		 * If no attribute with this name is found, this method has no effect.
		 * To remove an attribute by local name and namespace URI, use the removeAttributeNS
		 * method.
		 *
		 * @param name The name of the attribute to remove.
		 */
		void removeAttribute(const DOMString &name);

		/**
		 * Retrieves an attribute node by name.
		 * To retrieve an attribute node by qualified name and namespace URI, use the
		 * getAttributeNodeNS method.
		 *
		 * @param name The name (nodeName) of the attribute to retrieve.
		 *
		 * @returns The Attr node with the specified name (nodeName) or null if there is
		 * no such attribute.
		 */
		Attr getAttributeNode(const DOMString &name) const;

		/**
		 * Adds a new attribute node. If an attribute with that name (nodeName) is already
		 * present in the element, it is replaced by the new one. Replacing an attribute
		 * node by itself has no effect.
		 * To add a new attribute node with a qualified name and namespace URI, use the
		 * setAttributeNodeNS method.
		 *
		 * @param newAttr The Attr node to add to the attribute list.
		 *
		 * @returns If the newAttr attribute replaces an existing attribute, the replaced
		 * Attr node is returned, otherwise null is returned.
		 */
		Attr setAttributeNode(const Attr &newAttr);

		/**
		 * Removes the specified attribute node. If a default value for the removed Attr
		 * node is defined in the DTD, a new node immediately appears with the default
		 * value as well as the corresponding namespace URI, local name, and prefix when
		 * applicable. The implementation may handle default values from other schemas
		 * similarly but applications should use Document.normalizeDocument() to guarantee
		 * this information is up-to-date.
		 *
		 * @param oldAttr The Attr node to remove from the attribute list.
		 *
		 * @returns The Attr node that was removed.
		 */
		Attr removeAttributeNode(const Attr &oldAttr);

		/**
		 *
		 * Returns a NodeList of all the descendant Elements  with a given local name and
		 * namespace URI in document order.
		 *
		 * @param namespaceURI The namespace URI of the elements to match on. The special
		 * value "*" matches all namespaces.
		 * @param localName The local name of the elements to match on. The special value
		 * "*" matches
		 * all local names.
		 *
		 * @returns A new NodeList object containing all the matched Elements.
		 */
		NodeList getElementsByTagName(const DOMString &name) const;

		/**
		 * Retrieves an attribute value by local name and namespace URI.
		 * Per [XML Namespaces], applications must use the value null as the
		 * namespaceURI parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the attribute to retrieve.
		 * @param localName The local name of the attribute to retrieve.
		 *
		 * @returns The Attr value as a string, or the empty string if that attribute
		 * does not have a specified or default value.
		 */
		DOMString getAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const;

		/**
		 * Adds a new attribute. If an attribute with the same local name and
		 * namespace URI is already present on the element, its prefix is changed
		 * to be the prefix part of the qualifiedName, and its value is changed to
		 * be the value parameter. This value is a simple string; it is not parsed
		 * as it is being set. So any markup (such as syntax to be recognized as
		 * an entity reference) is treated as literal text, and needs to be
		 * appropriately escaped by the implementation when it is written out. In
		 * order to assign an attribute value that contains entity references, the
		 * user must create an Attr node plus any Text and EntityReference nodes,
		 * build the appropriate subtree, and use setAttributeNodeNS or
		 * setAttributeNode to assign it as the value of an attribute.
		 *
		 * Per [XML Namespaces], applications must use the value null as the
		 * namespaceURI parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the attribute to create or alter.
		 * @param qualifiedName The qualified name of the attribute to create or alter.
		 * @param value The value to set in string form.
		 */
		void setAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, const DOMString &value);

		/**
		 * Removes an attribute by local name and namespace URI. If a default
		 * value for the removed attribute is defined in the DTD, a new attribute
		 * immediately appears with the default value as well as the corresponding
		 * namespace URI, local name, and prefix when applicable. The implementation
		 * may handle default values from other schemas similarly but applications
		 * should use Document.normalizeDocument() to guarantee this information is
		 * up-to-date. If no attribute with this local name and namespace URI is found,
		 * this method has no effect.
		 * Per [XML Namespaces], applications must use the value null as the namespaceURI
		 * parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the attribute to remove.
		 * @param localName The local name of the attribute to remove.
		 */
		void removeAttributeNS(const DOMString &namespaceURI, const DOMString &localName);

		/**
		 * Retrieves an Attr node by local name and namespace URI.
		 * Per [XML Namespaces], applications must use the value null as the namespaceURI
		 * parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the attribute to retrieve.
		 * @param localName The local name of the attribute to retrieve.
		 *
		 * @returns The Attr node with the specified attribute local name and namespace
		 * URI or null if there is no such attribute.
		 */
		Attr getAttributeNodeNS(const DOMString &namespaceURI, const DOMString &localName) const;

		/**
		 * Adds a new attribute. If an attribute with that local name and that namespace
		 * URI is already present in the element, it is replaced by the new one. Replacing
		 * an attribute node by itself has no effect.
		 * Per [XML Namespaces], applications must use the value null as the namespaceURI
		 * parameter for methods if they wish to have no namespace.
		 *
		 * @param newAttr The Attr node to add to the attribute list.
		 *
		 * @returns If the newAttr attribute replaces an existing attribute with the same
		 * local name and namespace URI, the replaced Attr node is returned, otherwise null
		 * is returned.
		 */
		Attr setAttributeNodeNS(const Attr &newAttr);

		/**
		 * Returns a NodeList of all the descendant Elements with a given local name
		 * and namespace URI in document order.
		 *
		 * @param namespaceURI The namespace URI of the elements to match on. The special
		 * value "*" matches all namespaces.
		 * @param localName The local name of the elements to match on. The special value
		 * "*" matches all local names.
		 *
		 * @param A new NodeList object containing all the matched Elements.
		 */
		NodeList getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName) const;

		/**
		 * Returns true when an attribute with a given name is specified on this element
		 * or has a default value, false  otherwise.
		 *
		 * @param name The name of the attribute to look for.
		 *
		 * @returns true if an attribute with the given name is specified on this element
		 * or has a default value, false  otherwise.
		 */
		bool hasAttribute(const DOMString &name) const;

		/**
		 * Returns true when an attribute with a given local name and namespace URI is
		 * specified on this element or has a default value, false otherwise.
		 * Per [XML Namespaces], applications must use the value null as the namespaceURI
		 * parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the attribute to look for.
		 * @param localName The local name of the attribute to look for.
		 *
		 * @returns true if an attribute with the given local name and namespace URI is
		 * specified or has a default value on this element, false otherwise.
		 */
		bool hasAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const;

		/**
		 * If the parameter isId is true, this method declares the specified attribute to
		 * be a user-determined ID attribute. This affects the value of Attr.isId and the
		 * behavior of Document.getElementById, but does not change any schema that may be
		 * in use, in particular this does not affect the Attr.schemaTypeInfo of the specified
		 * Attr node. Use the value false for the parameter isId to undeclare an attribute
		 * for being a user-determined ID attribute.
		 * To specify an attribute by local name and namespace URI, use the setIdAttributeNS
		 * method.
		 *
		 * @param name The name of the attribute.
		 * @param isId Whether the attribute is a of type ID.
		 */
		void setIdAttribute(const DOMString &name, bool isId); // DOM3

		/**
		 * If the parameter isId is true, this method declares the specified attribute to be
		 * a user-determined ID attribute. This affects the value of Attr.isId and the behavior
		 * of Document.getElementById, but does not change any schema that may be in use, in
		 * particular this does not affect the Attr.schemaTypeInfo of the specified Attr node.
		 * Use the value false for the parameter isId to undeclare an attribute for being a
		 * user-determined ID attribute.
		 *
		 * @param namespaceURI The namespace URI of the attribute.
		 * @param localName The local name of the attribute.
		 * @param isId Whether the attribute is a of type ID.
		 */
		void setIdAttributeNS(const DOMString &namespaceURI, const DOMString &localName, bool isId); // DOM3

		/**
		 * If the parameter isId is true, this method declares the specified attribute to be
		 * a user-determined ID attribute. This affects the value of Attr.isId and the behavior
		 * of Document.getElementById, but does not change any schema that may be in use, in
		 * particular this does not affect the Attr.schemaTypeInfo of the specified Attr node.
		 * Use the value false for the parameter isId to undeclare an attribute for being a
		 * user-determined ID attribute.
		 *
		 * @param isAttr The attribute node.
		 * @param isId Whether the attribute is a of type ID.
		 */
		 void setIdAttributeNode(const Attr &idAttr, bool isId); // DOM3

		/**
		 * The type information associated with this element. 
		 */
		TypeInfo schemaTypeInfo() const;

		// Internal
		KDOM_INTERNAL(Element)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(ElementProto)
KDOM_IMPLEMENT_PROTOFUNC(ElementProtoFunc, Element)

#endif

// vim:ts=4:noet
