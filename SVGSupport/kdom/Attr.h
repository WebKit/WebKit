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

#ifndef KDOM_Attr_H
#define KDOM_Attr_H

#include <kdom/Node.h>

namespace KDOM
{
	class Element;
	class TypeInfo;
	class DOMString;
	class AttrImpl;

	/**
	 * @short The Attr interface represents an attribute in an Element object.
	 * Typically the allowable values for the attribute are defined in a schema
	 * associated with the document.
	 *
	 * Attr objects inherit the Node interface, but since they are not actually 
	 * child nodes of the element they describe, the DOM does not consider them part 
	 * of the document tree. Thus, the Node attributes parentNode, previousSibling, 
	 * and nextSibling have a null value for Attr objects. The DOM takes the view 
	 * that attributes are properties of elements rather than having a separate 
	 * identity from the elements they are associated with; this should make it more 
	 * efficient to implement such features as default attributes associated with 
	 * all elements of a given type. Furthermore, Attr nodes may not be immediate 
	 * children of a DocumentFragment. However, they can be associated with Element 
	 * nodes contained within a DocumentFragment. In short, users and implementors 
	 * of the DOM need to be aware that Attr nodes have some things in common with 
	 * other objects inheriting the Node interface, but they also are quite 
	 * distinct.
	 *
	 * The attribute's effective value is determined as follows: if this attribute
	 * has been explicitly assigned any value, that value is the attribute's effective 
	 * value; otherwise, if there is a declaration for this attribute, and that 
	 * declaration includes a default value, then that default value is the attribute's 
	 * effective value; otherwise, the attribute does not exist on this element in the 
	 * structure model until it has been explicitly added. Note that the Node.nodeValue 
	 * attribute on the Attr instance can also be used to retrieve the string version 
	 * of the attribute's value(s).
	 *
	 * If the attribute was not explicitly given a value in the instance document but 
	 * has a default value provided by the schema associated with the document, an 
	 * attribute node will be created with specified set to false. Removing attribute 
	 * nodes for which a default value is defined in the schema generates a new 
	 * attribute node with the default value and specified set to false. If validation 
	 * occurred while invoking Document.normalizeDocument(), attribute nodes with 
	 * specified equals to false are recomputed according to the default attribute 
	 * values provided by the schema. If no default value is associate with this 
	 * attribute in the schema, the attribute node is discarded.
	 *
	 * In XML, where the value of an attribute can contain entity references, the child 
	 * nodes of the Attr node may be either Text or EntityReference nodes (when these 
	 * are in use; see the description of EntityReference for discussion). 
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class Attr : public Node 
	{
	public:
		Attr();
		explicit Attr(AttrImpl *i);
		Attr(const Attr &other);
		Attr(const Node &other);
		virtual ~Attr();

		// Operators
		Attr &operator=(const Attr &other);
		Attr &operator=(const Node &other);

		// 'Attr' functions
		/**
		 * Returns the name of this attribute. If Node.localName is different from null,
		 * this attribute is a qualified name.
		 */
		DOMString name() const;

		/**
		 * True if this attribute was explicitly given a value in the instance document,
		 * false otherwise. If the application changed the value of this attribute node
		 * (even if it ends up having the same value as the default value) then it is set
		 * to true. The implementation may handle attributes with default values from other
		 * schemas similarly but applications should use Document.normalizeDocument()
		 * to guarantee this information is up-to-date. 
		 */
		bool specified() const;

		/**
		 * On retrieval, the value of the attribute is returned as a string. Character and
		 * general entity references are replaced with their values. See also the method
		 * getAttribute on the Element interface.
		 *
		 * On setting, this creates a Text node with the unparsed contents of the string, i.e.
		 * any characters that an XML processor would recognize as markup are instead treated
		 * as literal text. See also the method Element.setAttribute().
		 */
		DOMString value() const;
		void setValue(const DOMString &value);

		/**
		 * The Element node this attribute is attached to or null if this attribute is
		 * not in use.
		 */
		Element ownerElement() const;

		/**
		 * Returns whether this attribute is known to be of type ID (i.e. to contain an
		 * identifier for its owner element) or not. When it is and its value is unique,
		 * the ownerElement of this attribute can be retrieved using the method
		 * Document.getElementById.
		 */
		bool isId() const; // DOM3

		// Internal
		KDOM_INTERNAL(Attr)

		/**
		 * The type information associated with this attribute. While 
		 * the type information contained in this attribute is guarantee 
		 * to be correct after loading the document or invoking 
		 * Document.normalizeDocument(), schemaTypeInfo may not be 
		 * reliable if the node was moved.
		 */
		TypeInfo schemaTypeInfo() const;

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT
		KDOM_CAST

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);
	};

	KDOM_DEFINE_CAST(Attr)
};

#endif

// vim:ts=4:noet
