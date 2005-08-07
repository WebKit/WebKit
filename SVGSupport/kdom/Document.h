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

#ifndef KDOM_Document_H
#define KDOM_Document_H

#include <kurl.h>
#include <kdom/Node.h>
#include <kdom/views/DocumentView.h>
#include <kdom/events/DocumentEvent.h>
#include <kdom/css/DocumentStyle.h>
#include <kdom/traversal/DocumentTraversal.h>
#include <kdom/range/DocumentRange.h>
#include <kdom/xpointer/XPointerEvaluator.h>
#include <kdom/xpath/XPathEvaluator.h>

class KURL;

namespace KDOM
{
	class Attr;
	class Text;
	class Ecma;
	class Comment;
	class Element;
	class NodeList;
	class CDATASection;
	class DocumentType;
	class EntityReference;
	class DocumentFragment;
	class DOMImplementation;
	class DOMConfiguration;
	class ProcessingInstruction;
	class DocumentImpl;

	/**
	 * @short The Document interface represents the entire HTML or XML
	 * document. Conceptually, it is the root of the document tree, and
	 * provides the primary access to the document's data.
	 *
	 * Since elements, text nodes, comments, processing instructions,
	 * etc. cannot exist outside the context of a Document, the Document
	 * interface also contains the factory methods needed to create these
	 * objects. The Node objects created have a ownerDocument attribute
	 * which associates them with the Document within whose context they
	 * were created.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class Document : public Node,
					 public DocumentView,
					 public DocumentEvent,
					 public DocumentStyle,
					 public DocumentTraversal,
					 public DocumentRange,
					 public XPointer::XPointerEvaluator,
					 public XPathEvaluator
	{
	public:
		Document();
		explicit Document(DocumentImpl *p);
		Document(const Document &other);
		Document(const Node &other);
		virtual ~Document();

		// Operators
		Document &operator=(const Document &other);
		Document &operator=(const Node &other);

		// 'Document' functions
		/**
		 * The Document Type Declaration (see DocumentType) associated with
		 * this document. For XML documents without a document type declaration
		 * this returns null. For HTML documents, a DocumentType object may be
		 * returned, independently of the presence or absence of document type
		 * declaration in the HTML document.
		 * This provides direct access to the DocumentType node, child node of
		 * this Document. This node can be set at document creation time and
		 * later changed through the use of child nodes manipulation methods,
		 * such as Node.insertBefore, or Node.replaceChild. Note, however, that
		 * while some implementations may instantiate different types of Document
		 * objects supporting additional features than the "Core", such as "HTML"
		 * [DOM Level 2 HTML], based on the DocumentType specified at creation
		 * time, changing it afterwards is very unlikely to result in a change
		 * of the features supported.
		 */
		DocumentType doctype() const;

		/**
		 * The DOMImplementation object that handles this document. A DOM application
		 * may use objects from multiple implementations.
		 */
		DOMImplementation implementation() const;

		/**
		 * This is a convenience attribute that allows direct access to the child
		 * node that is the document element of the document.
		 */
		Element documentElement() const;

		/**
		 * Creates an element of the type specified. Note that the instance
		 * returned implements the Element interface, so attributes can be specified
		 * directly on the returned object.
		 * In addition, if there are known attributes with default values, Attr nodes
		 * representing them are automatically created and attached to the element.
		 * To create an element with a qualified name and namespace URI, use the
		 * createElementNS method.
		 * 
		 * @param tagName The name of the element type to instantiate. For XML,
		 * this is case-sensitive, otherwise it depends on the case-sensitivity
		 * of the markup language in use. In that case, the name is mapped to the
		 * canonical form of that markup by the DOM implementation.
		 *
		 * @returns A new Element object with the nodeName attribute set to tagName,
		 * and localName, prefix, and namespaceURI set to null.
		 */
		Element createElement(const DOMString &tagName);

		/**
		 * Creates an element of the given qualified name and namespace URI.
		 * Per [XML Namespaces], applications must use the value null as the
		 * namespaceURI parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the element to create.
		 * @param qualifiedName The qualified name of the element type to instantiate.
		 *
		 * @returns A new Element object.
		 */
		 Element createElementNS(const DOMString &namespaceURI, const DOMString &qualifiedName);

		/**
		 * Creates an empty DocumentFragment object.
		 *
		 * @returns A new DocumentFragment.
		 */
		DocumentFragment createDocumentFragment();

		/**
		 * Creates a Text node given the specified string.
		 *
		 * @param data The data for the node.
		 *
		 * @returns The new Text object.
		 */
		Text createTextNode(const DOMString &data);

		/**
		 * Creates a Comment node given the specified string.
		 *
		 * @param data The data for the node.
		 *
		 * @returns The new Comment object.
		 */
		Comment createComment(const DOMString &data);

		/**
		 * Creates a CDATASection node whose value is the specified string.
		 *
		 * @param data The data for the CDATASection contents.
		 *
		 * @returns The new CDATASection object.
		 */
		CDATASection createCDATASection(const DOMString &data);

		/**
		 * Creates a ProcessingInstruction node given the specified name and data strings.
		 *
		 * @param target The target part of the processing instruction.
		 * Unlike Document.createElementNS or Document.createAttributeNS, no
		 * namespace well-formed checking is done on the target name. Applications
		 * should invoke Document.normalizeDocument() with the parameter "namespaces"
		 * set to true in order to ensure that the target name is namespace well-formed. 
		 * @param data The data for the node.
		 *
		 * @returns The new ProcessingInstruction object.
		 */
		ProcessingInstruction createProcessingInstruction(const DOMString &target, const DOMString &data);

		/**
		 * Creates an Attr of the given name. Note that the Attr instance can then
		 * be set on an Element using the setAttributeNode method. 
		 * To create an attribute with a qualified name and namespace URI, use the
		 * createAttributeNS method.
		 *
		 * @param name The name of the attribute.
		 *
		 * @returns A new Attr object with the nodeName attribute set to name, and
		 * localName, prefix, and namespaceURI set to null. The value of the attribute
		 * is the empty string.
		 */
		Attr createAttribute(const DOMString &name);

		/**
		 * Creates an attribute of the given qualified name and namespace URI.
		 * Per [XML Namespaces], applications must use the value null as the
		 * namespaceURI parameter for methods if they wish to have no namespace.
		 *
		 * @param namespaceURI The namespace URI of the attribute to create.
		 * @param qualifiedName The qualified name of the attribute to instantiate.
		 *
		 * @returns A new Attr object.
		 */
		Attr createAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName);

		/**
		 * Creates an EntityReference object. In addition, if the referenced
		 * entity is known, the child list of the EntityReference node is made the
		 * same as that of the corresponding Entity node.
		 * Note: If any descendant of the Entity node has an unbound namespace
		 * prefix, the corresponding descendant of the created EntityReference
		 * node is also unbound; (its namespaceURI is null). The DOM Level 2 and
		 * 3 do not support any mechanism to resolve namespace prefixes in this case.
		 *
		 * @param name The name of the entity to reference.
		 * Unlike Document.createElementNS or Document.createAttributeNS, no
		 * namespace well-formed checking is done on the entity name. Applications
		 * should invoke Document.normalizeDocument() with the parameter
		 * "namespaces" set to true in order to ensure that the entity name is
		 * namespace well-formed.
		 *
		 * @returns The new EntityReference object.
		 */
		EntityReference createEntityReference(const DOMString &name);

		/**
		 * Returns a NodeList of all the Elements in document order with a given
		 * tag name and are contained in the document.
		 *
		 * @param tagName The name of the tag to match on. The special value
		 * "*" matches all tags. For XML, the tagname parameter is
		 * case-sensitive, otherwise it depends on the case-sensitivity of
		 * the markup language in use.
		 *
		 * @returns A new NodeList object containing all the matched Elements.
		 */
		NodeList getElementsByTagName(const DOMString &tagName);

		/**
		 * Returns a NodeList of all the Elements with a given local name and
		 * namespace URI in document order.
		 *
		 * @param namespaceURI The namespace URI of the elements to match on. The
		 * special value "*" matches all namespaces.
		 * @param localName The local name of the elements to match on. The special
		 * value "*" matches all local names.
		 *
		 * @returns A new NodeList object containing all the matched Elements.
		 */
		NodeList getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName);

		/**
		 * Returns the Element that has an ID attribute with the given value.
		 * If no such element exists, this returns null. If more than one
		 * element has an ID attribute with that value, what is returned is undefined. 
		 * The DOM implementation is expected to use the attribute Attr.isId to
		 * determine if an attribute is of type ID. 
		 *
		 * @param elementId The unique id value for an element.
		 *
		 * @returns The matching element or null if there is none.
		 */
		Element getElementById(const DOMString &elementId);

		/**
		 * Imports a node from another document to this document, without
		 * altering or removing the source node from the original document;
		 * this method creates a new copy of the source node. The returned
		 * node has no parent; (parentNode is null).
		 *
		 * @param importedNode The node to import.
		 * @param deep If true, recursively import the subtree under the
		 * specified node; if false, import only the node itself, as explained
		 * above. This has no effect on nodes that cannot have any children, and
		 * on Attr, and EntityReference nodes.
		 *
		 * @returns The imported node that belongs to this Document.
		 */
		Node importNode(const Node &importedNode, bool deep);

		/**
		 *  
		 This method acts as if the document was going through a save and load
		 * cycle, putting the document in a "normal" form. As a consequence,
		 * this method updates the replacement tree of EntityReference nodes
		 * and normalizes Text nodes, as defined in the method Node.normalize(). 
		 * Otherwise, the actual result depends on the features being set on
		 * the Document.domConfig object and governing what operations actually
		 * take place. Noticeably this method could also make the document
		 * namespace well-formed according to the algorithm described in
		 * Namespace Normalization, check the character normalization, remove
		 * the CDATASection nodes, etc. See DOMConfiguration for details.
		 */
		void normalizeDocument(); // DOM3

		/**
		 * Rename an existing node of type ELEMENT_NODE or ATTRIBUTE_NODE.
		 *
		 * @param n The node to rename.
		 * @param namespaceURI The new namespace URI.
		 * @param qualifiedName The new qualified name.
		 *
		 * @returns The renamed node. This is either the specified node or the
		 * new node that was created to replace the specified node.
		 */
		Node renameNode(const Node &n, const DOMString &namespaceURI, const DOMString &qualifiedName); // DOM3

		/**
		 * The location of the document or null if undefined or if the Document
		 * was created using DOMImplementation.createDocument. No lexical checking
		 * is performed when setting this attribute; this could result in a null
		 * value returned when using Node.baseURI. 
		 * Beware that when the Document supports the feature "HTML" [DOM Level 2 HTML],
		 * the href attribute of the HTML BASE element takes precedence over this
		 * attribute when computing Node.baseURI. 
		 */
		DOMString documentURI() const; // DOM3
		void setDocumentURI(const DOMString &uri); // DOM3

		/**
		 * The configuration used when Document.normalizeDocument() is invoked.
		 */
		DOMConfiguration domConfig() const; // DOM3

		/**
		 * An attribute specifying the encoding used for this document at
		 * the time of the parsing. This is null when it is not known, such
		 * as when the Document was created in memory.
		 */
		DOMString inputEncoding() const; // DOM3

		/**
		 * An attribute specifying, as part of the XML declaration, the encoding of
		 * this document. This is null when unspecified or when it is not known, such
		 * as when the Document was created in memory.
		 */
		DOMString xmlEncoding() const; // DOM3

		/**
		 * An attribute specifying, as part of the XML declaration, whether this
		 * document is standalone. This is false when unspecified.
		 */
		bool xmlStandalone() const; // DOM3
		void setXmlStandalone(bool) const; // DOM3

		/**
		 * An attribute specifying, as part of the XML declaration, the version
		 * number of this document. If there is no declaration and if this
		 * document supports the "XML" feature, the value is "1.0". If this
		 * document does not support the "XML" feature, the value is always null.
		 * Changing this attribute will affect methods that check for invalid
		 * characters in XML names. Application should invoke
		 * Document.normalizeDocument() in order to check for invalid characters
		 * in the Nodes that are already part of this Document. 
		 * DOM applications may use the DOMImplementation.hasFeature(feature, version)
		 * method with parameter values "XMLVersion" and "1.0" (respectively) to
		 * determine if an implementation supports [XML 1.0]. DOM applications
		 * may use the same method with parameter values "XMLVersion" and "1.1"
		 * (respectively) to determine if an implementation supports [XML 1.1].
		 * In both cases, in order to support XML, an implementation must also
		 * support the "XML" feature defined in this specification. Document
		 * objects supporting a version of the "XMLVersion" feature must not
		 * raise a NOT_SUPPORTED_ERR exception for the same version number when
		 * using Document.xmlVersion. 
		 */
		DOMString xmlVersion() const; // DOM3
		void setXmlVersion(const DOMString &xmlVersion);

		/**
		 * An attribute specifying whether error checking is enforced or not.
		 * When set to false, the implementation is free to not test every
		 * possible error case normally defined on DOM operations, and not raise
		 * any DOMException on DOM operations or report errors while using
		 * Document.normalizeDocument(). In case of error, the behavior is
		 * undefined. This attribute is true by default.
		 */
		bool strictErrorChecking() const; // DOM3
		void setStrictErrorChecking(bool strictErrorChecking);

		/**
		 * Attempts to adopt a node from another document to this document.
		 *
		 * @param source The node to move into this document.
		 *
		 * @returns The adopted node, or null if this operation fails, such as when
		 * the source node comes from a different implementation.
		 */
		Node adoptNode(const Node &source) const; // DOM3

		// Internal
		KDOM_INTERNAL(Document)

		KURL documentKURI() const;
		void setDocumentKURI(const KURL &url);

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, const KJS::Value &value, int attr);		
	};
}

KDOM_DEFINE_PROTOTYPE(DocumentProto)
KDOM_IMPLEMENT_PROTOFUNC(DocumentProtoFunc, Document)

#endif

// vim:ts=4:noet
