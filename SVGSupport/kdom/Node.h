/*
    Copyright (C) 2004 Nikolas Zimmermann   <wildfox@kde.org>
                  2004 Rob Buis             <buis@kde.org>

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

#ifndef KDOM_Node_H
#define KDOM_Node_H

#include <kdom/Shared.h>
#include <kdom/ecma/DOMLookup.h>
#include <kdom/events/EventTarget.h>

class KURL;

namespace KDOM
{
	class Document;
	class NodeList;
	class DOMString;
	class NamedNodeMap;
	class DOMObject;
	class NodeImpl;

	/**
	 * @short The Node interface is the primary datatype for the
	 * entire Document Object Model. 
	 *
	 * It represents a single node in the document tree. While all
	 * objects implementing the Node interface expose methods for
	 * dealing with children, not all objects implementing the Node
	 * interface may have children. For example, Text nodes may not
	 * have children, and adding children to such nodes results in a
	 * DOMException being raised.`
	 *
	 * The attributes nodeName, nodeValue and attributes are included
	 * as a mechanism to get at node information without casting down
	 * to the specific derived interface. In cases where there is no
	 * obvious mapping of these attributes for a specific nodeType
	 * (e.g., nodeValue for an Element or attributes for a Comment),
	 * this returns null. Note that the specialized interfaces may
	 * contain additional and more convenient mechanisms to get and
	 * set the relevant information.
	 * 
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class Node : public EventTarget
	{
	public:
		Node();
		Node(NodeImpl *p);
		Node(const Node &other);
		virtual ~Node();

		// Operators
		Node &operator=(const Node &other);

		// 'Node' functions
		/**
		 * The name of this node, depending on its type; see the nodeType enum
		 */
		DOMString nodeName() const;

		/**
		 * The value of this node, depending on its type; see nodeType enum
		 * above. When it is defined to be null, setting it has no effect,
		 * including if the node is read-only.
		 */
		DOMString nodeValue() const;
		void setNodeValue(const DOMString &nodeValue);

		/**
		 * A code representing the type of the underlying object, as
		 * defined in the nodeType enum.
		 */
		unsigned short nodeType() const;

		/**
		 * The parent of this node. All nodes, except Attr, Document,
		 * DocumentFragment, Entity, and Notation may have a parent.
		 * However, if a node has just been created and not yet added
		 * to the tree, or if it has been removed from the tree, this
		 * is null. 
		 */
		Node parentNode() const;

		/**
		 * A NodeList that contains all children of this node. If there
		 * are no children, this is a NodeList containing no nodes.
		 */
		NodeList childNodes() const;

		/**
		 * The first child of this node. If there is no such node, this returns null.
		 */
		Node firstChild() const;

		/**
		 * The last child of this node. If there is no such node, this returns null.
		 */
		Node lastChild() const;

		/**
		 * The node immediately preceding this node. If there is no such
		 * node, this returns null.
		 */
		Node previousSibling() const;

		/**
		 * The node immediately following this node. If there is no such
		 * node, this returns null.
		 */
		Node nextSibling() const;

		/**
		 * A NamedNodeMap containing the attributes of this node (if it
		 * is an Element) or null otherwise.
		 */
		NamedNodeMap attributes() const;

		/**
		 * The Document object associated with this node. This is also
		 * the Document object used to create new nodes. When this node
		 * is a Document or a DocumentType which is not used with any
		 * Document yet, this is null.
		 */
		Document ownerDocument() const;

		/**
		 * Inserts the node newChild before the existing child node refChild.
		 * If refChild is null, insert newChild at the end of the list of children.
		 * If newChild is a DocumentFragment object, all of its children are
		 * inserted, in the same order, before refChild. If the newChild is
		 * already in the tree, it is first removed.
		 *
		 * Note: Inserting a node before itself is implementation dependent.
		 *
		 * @param newChild The node to insert.
		 * @param refChild The reference node, i.e., the node before which
		 * the new node must be inserted.
		 *
		 * @returns The node being inserted.
		 */
		Node insertBefore(const Node &newChild, const Node &refChild);

		/**
		 * Replaces the child node oldChild with newChild in the list of
		 * children, and returns the oldChild node.  If newChild is a
		 * DocumentFragment object, oldChild is replaced by all of the
		 * DocumentFragment children, which are inserted in the same order.
		 * If the newChild is already in the tree, it is first removed.
		 *
		 * Note: Replacing a node with itself is implementation dependent.
		 *
		 * @param newChild The new node to put in the child list.
		 * @param oldChild The node being replaced in the list.
		 *
		 * @returns The node replaced.
		 */
		Node replaceChild(const Node &newChild, const Node &oldChild);

		/**
		 * Removes the child node indicated by oldChild from the list of
		 * children, and returns it.
		 *
		 * @param oldChild The node being removed.
		 *
		 * @returns The node removed.
		 */
		Node removeChild(const Node &oldChild);

		/**
		 * Adds the node newChild to the end of the list of children of
		 * this node. If the newChild is already in the tree, it is first
		 * removed.
		 *
		 * @param newChild The node to add.
		 * If it is a DocumentFragment object, the entire contents of
		 * the document fragment are moved into the child list of this
		 * node.
		 *
		 * @returns The node added.
		 */
		Node appendChild(const Node &newChild);

		/**
		 * Returns whether this node has any children.
		 *
		 * @returns Returns true if this node has any children, false
		 * otherwise.
		 */
		bool hasChildNodes() const;

		/**
		 * Returns a duplicate of this node, i.e., serves as a
		 * generic copy constructor for nodes. The duplicate node
		 * has no parent (parentNode is null) and no user data. User
		 * data associated to the imported node is not carried over.
		 * However, if any UserDataHandlers has been specified along
		 * with the associated data these handlers will be called with
		 * the appropriate parameters before this method returns.
		 *
		 * Cloning an Element copies all attributes and their values,
		 * including those generated by the XML processor to represent
		 * defaulted attributes, but this method does not copy any
		 * children it contains unless it is a deep clone. This includes
		 * text contained in an the Element since the text is contained
		 * in a child Text node. Cloning an Attr directly, as opposed
		 * to be cloned as part of an Element cloning operation,
		 * returns a specified attribute (specified is true). Cloning an
		 * Attr always clones its children, since they represent its
		 * value, no matter whether this is a deep clone or not. Cloning
		 * an EntityReference automatically constructs its subtree if
		 * a corresponding Entity is available, no matter whether this
		 * is a deep clone or not. Cloning any other type of node
		 * simply returns a copy of this node.
		 * Note that cloning an immutable subtree results in a
		 * mutable copy, but the children of an EntityReference clone
		 * are readonly. In addition, clones of unspecified Attr nodes
		 * are specified. And, cloning Document, DocumentType, Entity,
		 * and Notation nodes is implementation dependent.
		 *
		 * @param deep If true, recursively clone the subtree under
		 * the specified node; if false, clone only the node itself
		 * (and its attributes, if it is an Element).
		 *
		 * #returns The duplicate node.
		 */
		Node cloneNode(bool deep) const;

		/**
		 * Puts all Text nodes in the full depth of the sub-tree underneath
		 * this Node, including attribute nodes, into a "normal" form where
		 * only structure (e.g., elements, comments, processing instructions,
		 * CDATA sections, and entity references) separates Text nodes,
		 * i.e., there are neither adjacent Text nodes nor empty Text nodes.
		 * This can be used to ensure that the DOM view of a document is the
		 * same as if it were saved and re-loaded, and is useful when
		 * operations (such as XPointer [XPointer] lookups) that depend
		 * on a particular document tree structure are to be used. If the
		 * parameter "normalize-characters" of the DOMConfiguration object
		 * attached to the Node.ownerDocument is true, this method will
		 * also fully normalize the characters of the Text nodes. 
		 *
		 * Note: In cases where the document contains CDATASections, the
		 * normalize operation alone may not be sufficient, since XPointers
		 * do not differentiate between Text nodes and CDATASection nodes.
		 */
		void normalize();

		/**
		 * Tests whether the DOM implementation implements a specific
		 * feature and that feature is supported by this node, as
		 * specified in DOM Features.
		 *
		 * @param feature The name of the feature to test.
		 * @param version This is the version number of the feature to test.
		 *
		 * @returns Returns true if the specified feature is supported on
		 * this node, false otherwise.
		 */
		bool isSupported(const DOMString &feature, const DOMString &version) const;

		/**
		 * The namespace URI of this node, or null if it is unspecified
		 *(see XML Namespaces). This is not a computed value that is the
		 * result of a namespace lookup based on an examination of the
		 * namespace declarations in scope. It is merely the namespace
		 * URI given at creation time.
		 * For nodes of any type other than ELEMENT_NODE and ATTRIBUTE_NODE
		 * and nodes created with a DOM Level 1 method, such as
		 * Document.createElement(), this is always null.
		 *
		 * Note: Per the Namespaces in XML Specification [XML Namespaces] an
		 * attribute does not inherit its namespace from the element it
		 * is attached to. If an attribute is not explicitly given a
		 * namespace, it simply has no namespace.
		 */
		DOMString namespaceURI() const;

		/**
		 * The namespace prefix of this node, or null if it is
		 * unspecified. When it is defined to be null, setting it has
		 * no effect, including if the node is read-only.
		 * Note that setting this attribute, when permitted, changes
		 * the nodeName attribute, which holds the qualified name, as
		 * well as the tagName and name attributes of the Element and
		 * Attr interfaces, when applicable.
		 * Setting the prefix to null makes it unspecified, setting it
		 * to an empty string is implementation dependent.
		 * Note also that changing the prefix of an attribute that is
		 * known to have a default value, does not make a new attribute
		 * with the default value and the original prefix appear, since
		 * the namespaceURI and localName do not change.
		 * For nodes of any type other than ELEMENT_NODE and ATTRIBUTE_NODE
		 * and nodes created with a DOM Level 1 method, such as createElement
		 * from the Document interface, this is always null.
		 */
		DOMString prefix() const;
		void setPrefix(const DOMString &prefix);

		/**
		 * Returns the local part of the qualified name of this node.
		 * For nodes of any type other than ELEMENT_NODE and ATTRIBUTE_NODE
		 * and nodes created with a DOM Level 1 method, such as
		 * Document.createElement(), this is always null.
		 */
		DOMString localName() const;

		/**
		 * Returns whether this node has any children.
		 *
		 * @returns Returns true if this node has any children, false otherwise.
		 */
		bool hasAttributes() const;

		/**
		 * The absolute base URI of this node or null if the implementation
		 * wasn't able to obtain an absolute URI. This value is computed
		 * as described in Base URIs. However, when the Document supports
		 * the feature "HTML" [DOM Level 2 HTML], the base URI is computed
		 * using first the value of the href attribute of the HTML BASE
		 * element if any, and the value of the documentURI attribute from
		 * the Document interface otherwise.
		 */
		DOMString baseURI() const; // DOM3

		KURL baseKURI() const;

		/**
		 * Compares the reference node, i.e. the node on which this method
		 * is being called, with a node, i.e. the one passed as a parameter,
		 * with regard to their position in the document and according to
		 * the document order.
		 *
		 * @param other The node to compare against the reference node.
		 *
		 * @returns Returns how the node is positioned relatively to
		 * the reference node.
		 */
		unsigned short compareDocumentPosition(const Node &other) const; // DOM3

		/**
		 * This attribute returns the text content of this node and
		 * its descendants. When it is defined to be null, setting
		 * it has no effect. On setting, any possible children this
		 * node may have are removed and, if it the new string is not
		 * empty or null, replaced by a single Text node containing
		 * the string this attribute is set to. 
		 * On getting, no serialization is performed, the returned
		 * string does not contain any markup. No whitespace normalization
		 * is performed and the returned string does not contain the white
		 * spaces in element content (see the
		 * attribute Text.isElementContentWhitespace).
		 * Similarly, on setting, no parsing is performed either, the
		 * input string is taken as pure textual content. 
		 */
		DOMString textContent() const; // DOM3
		void setTextContent(const DOMString &text); // DOM3

		/**
		 * Returns whether this node is the same node as the given one.
		 * This method provides a way to determine whether two Node
		 * references returned by the implementation reference the same
		 * object. When two Node references are references to the same
		 * object, even if through a proxy, the references may be used
		 * completely interchangeably, such that all attributes have
		 * the same values and calling the same DOM method on either
		 * reference always has exactly the same effect.
		 *
		 * @param other The node to test against.
		 *
		 * @returns Returns true if the nodes are the same, false
		 * otherwise.
		 */
		bool isSameNode(const Node &other) const; // DOM3

		/**
		 * Look up the namespace URI associated to the given prefix,
		 * starting from this node. See Namespace URI Lookup for
		 * details on the algorithm used by this method.
		 *
		 * @param prefix The prefix to look for. If this parameter is null,
		 * the method will return the default namespace URI if any.
		 *
		 * @returns Returns the associated namespace URI or null if none
		 * is found.
		 */
		DOMString lookupNamespaceURI(const DOMString &prefix) const; // DOM3

		/**
		 * This method checks if the specified namespaceURI is the
		 * default namespace or not.
		 *
		 * @param namespaceURI The namespace URI to look for.
		 *
		 * @returns Returns true if the specified namespaceURI is
		 * the default namespace, false otherwise.
		 */
		bool isDefaultNamespace(const DOMString &namespaceURI) const; // DOM3

		/**
		 * Look up the prefix associated to the given namespace URI,
		 * starting from this node. The default namespace
		 * declarations are ignored by this method.
		 * See Namespace Prefix Lookup for details on the algorithm
		 * used by this method.
		 *
		 * @param namespaceURI The namespace URI to look for.
		 *
		 * @returns Returns an associated namespace prefix if found or
		 * null if none is found. If more than one prefix are
		 * associated to the namespace prefix, the returned namespace
		 * prefix is implementation dependent.
		 */
		DOMString lookupPrefix(const DOMString &namespaceURI) const; // DOM3

		/**
		 * Tests whether two nodes are equal.
		 * This method tests for equality of nodes, not sameness
		 * (i.e., whether the two nodes are references to the
		 * same object) which can be tested with Node.isSameNode().
		 * All nodes that are the same will also be equal, though the
		 * reverse may not be true.
		 *
		 * @param arg The node to compare equality with.
		 *
		 * @returns Returns true if the nodes are equal, false otherwise.
		 */
		bool isEqualNode(const Node &arg) const; // DOM3

		/**
		 * This method returns a specialized object which implements
		 * the specialized APIs of the specified feature and version,
		 * as specified in DOM Features. The specialized object may
		 * also be obtained by using binding-specific casting methods
		 * but is not necessarily expected to, as discussed in Mixed DOM
		 * Implementations. This method also allow the implementation to
		 * provide specialized objects which do not support the Node
		 * interface.
		 *
		 * @param feature The name of the feature requested. Note that
		 * any plus sign "+" prepended to the name of the feature will
		 * be ignored since it is not significant in the context of this
		 * method. 
		 * @param version This is the version number of the feature to test.
		 *
		 * @returns Returns an object which implements the specialized
		 * APIs of the specified feature and version, if any, or null
		 * if there is no object which implements interfaces associated
		 * with that feature. If the DOMObject returned by this method
		 * implements the Node interface, it must delegate to the
		 * primary core Node and not return results inconsistent with
		 * the primary core Node such as attributes, childNodes, etc.
		 */
		DOMObject getFeature(const DOMString &feature, const DOMString &version) const; // DOM3

		// Internal
		KDOM_INTERNAL(Node)

	public: // EcmaScript section
		KDOM_BASECLASS_GET
		KDOM_PUT
		KDOM_CAST

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);
	};

	KDOM_DEFINE_CAST(Node)

// template for easing assigning of Nodes to derived classes.
// NODE_TYPE is used to make sure the type is correct, otherwise
// the d pointer is set to 0.
#define KDOM_NODE_DERIVED_ASSIGN_OP(T, NODE_TYPE) \
T &T::operator=(const KDOM::Node &other) \
{ \
	KDOM::NodeImpl *ohandle = static_cast<KDOM::NodeImpl *>(other.handle()); \
	if(d != ohandle) { \
		if(!ohandle || ohandle->nodeType() != NODE_TYPE) { \
			if(d) d->deref(); \
			d = 0; \
		} \
		else \
			KDOM::Node::operator=(other); \
	} \
	return *this; \
}

}

KDOM_DEFINE_PROTOTYPE(NodeProto)
KDOM_IMPLEMENT_PROTOFUNC(NodeProtoFunc, Node)

#endif

// vim:ts=4:noet
