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

#ifndef KDOM_NamedNodeMap_H
#define KDOM_NamedNodeMap_H

namespace KDOM
{
	class Node;
	class DOMString;
	class NamedNodeMapImpl;

	/**
	 * @object Objects implementing the NamedNodeMap interface are used to
	 * represent collections of nodes that can be accessed by name. Note that
	 * NamedNodeMap does not inherit from NodeList; NamedNodeMaps are not
	 * maintained in any particular order. Objects contained in an object
	 * implementing NamedNodeMap may also be accessed by an ordinal index, but
	 * this is simply to allow convenient enumeration of the contents of a
	 * NamedNodeMap, and does not imply that the DOM specifies an order to
	 * these Nodes. 
	 *
	 * NamedNodeMap objects in the DOM are live.
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class NamedNodeMap 
	{
	public:
		NamedNodeMap();
		explicit NamedNodeMap(NamedNodeMapImpl *i);
		NamedNodeMap(const NamedNodeMap &other);
		virtual ~NamedNodeMap();

		// Operators
		NamedNodeMap &operator=(const NamedNodeMap &other);
		bool operator==(const NamedNodeMap &other) const;
		bool operator!=(const NamedNodeMap &other) const;

		// 'NamedNodeMap' functions
		/**
		 * Retrieves a node specified by name.
		 *
		 * @param name The nodeName of a node to retrieve.
		 *
		 * @returns A Node (of any type) with the specified nodeName, or null if it
		 * does not identify any node in this map.
		 */
		Node getNamedItem(const DOMString &name) const;

		/**
		 * Adds a node using its nodeName attribute. If a node with that name is
		 * already present in this map, it is replaced by the new one. Replacing
		 * a node by itself has no effect.
		 * As the nodeName attribute is used to derive the name which the node
		 * must be stored under, multiple nodes of certain types (those that have
		 * a "special" string value) cannot be stored as the names would clash. This
		 * is seen as preferable to allowing nodes to be aliased.
		 *
		 * @param arg A node to store in this map. The node will later be accessible
		 * using the value of its nodeName attribute.
		 *
		 * @returns If the new Node replaces an existing node the replaced Node is
		 * returned, otherwise null is returned.
		 */
		Node setNamedItem(const Node &arg);

		/**
		 * Removes a node specified by name. When this map contains the
		 * attributes attached to an element, if the removed attribute is
		 * known to have a default value, an attribute immediately appears
		 * containing the default value as well as the corresponding namespace
		 * URI, local name, and prefix when applicable.
		 *
		 * @param name The nodeName of the node to remove.
		 *
		 * @returns The node removed from this map if a node with such a name exists.
		 */
		Node removeNamedItem(const DOMString &name);

		/**
		 * Returns the indexth item in the map. If index is greater than or
		 * equal to the number of nodes in this map, this returns null.
		 *
		 * @param index Index into this map.
		 *
		 * @returns The node at the indexth position in the map, or null if
		 * that is not a valid index.
		 */
		Node item(unsigned long index) const;

		/**
		 * The number of nodes in this map. The range of valid child node
		 * indices is 0 to length-1 inclusive.
		 */
		unsigned long length() const;

		/**
		 * Retrieves a node specified by local name and namespace URI.
		 * Per [XML Namespaces], applications must use the value null as the
		 * namespaceURI parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the node to retrieve.
		 * @param localName The local name of the node to retrieve.
		 *
		 * @returns A Node (of any type) with the specified local name and
		 * namespace URI, or null if they do not identify any node in this map.
		 */
		Node getNamedItemNS(const DOMString &namespaceURI, const DOMString &localName) const;

		/**
		 * Adds a node using its namespaceURI and localName. If a node with
		 * that namespace URI and that local name is already present in this
		 * map, it is replaced by the new one. Replacing a node by itself has no
		 * effect. Per [XML Namespaces], applications must use the value null as
		 * the namespaceURI parameter for methods if they wish to have no namespace.
		 *
		 * @param arg A node to store in this map. The node will later be accessible
		 * using the value of its namespaceURI and localName attributes.
		 *
		 * @returns If the new Node replaces an existing node the replaced Node is
		 * returned, otherwise null is returned.
		 */
		Node setNamedItemNS(const Node &arg);

		/**
		 * Removes a node specified by local name and namespace URI. A removed
		 * attribute may be known to have a default value when this map contains
		 * the attributes attached to an element, as returned by the attributes
		 * attribute of the Node interface. If so, an attribute immediately
		 * appears containing the default value as well as the corresponding
		 * namespace URI, local name, and prefix when applicable.
		 * Per [XML Namespaces], applications must use the value null as the
		 * namespaceURI parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the node to remove.
		 * @param localName The local name of the node to remove.
		 *
		 * @returns The node removed from this map if a node with such a local
		 * name and namespace URI exists.
		 */
		Node removeNamedItemNS(const DOMString &namespaceURI, const DOMString &localName);

		// Internal
		KDOM_INTERNAL(NamedNodeMap)

	protected:
		NamedNodeMapImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_FORWARDPUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);
	};
};

KDOM_DEFINE_PROTOTYPE(NamedNodeMapProto)
KDOM_IMPLEMENT_PROTOFUNC(NamedNodeMapProtoFunc, NamedNodeMap)

#endif

// vim:ts=4:noet
