/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 1 Specification (Recommendation)
 * http://www.w3.org/TR/REC-DOM-Level-1/
 * Copyright © World Wide Web Consortium , (Massachusetts Institute of
 * Technology , Institut National de Recherche en Informatique et en
 * Automatique , Keio University ). All Rights Reserved.
 *
 * $Id$
 */
#ifndef _DOM_Document_h_
#define _DOM_Document_h_

#include <dom/dom_node.h>
#include <dom/css_stylesheet.h>

class KHTMLView;
class KHTMLPart;

namespace DOM {

class DOMString;
class DocumentType;
class NodeList;
class CDATASection;
class Comment;
class DocumentFragment;
class Text;
class DOMImplementation;
class Element;
class Attr;
class EntityReference;
class ProcessingInstruction;
class DocumentImpl;
class Range; 
class NodeIterator;
class TreeWalker;
class NodeFilter;
class DOMImplementationImpl;
class DocumentTypeImpl;
class Event;
class AbstractView;
class CSSStyleDeclaration;
 
/**
 * The <code> DOMImplementation </code> interface provides a number of
 * methods for performing operations that are independent of any
 * particular instance of the document object model.
 *
 *  The DOM Level 1 does not specify a way of creating a document
 * instance, and hence document creation is an operation specific to
 * an implementation. Future Levels of the DOM specification are
 * expected to provide methods for creating documents directly.
 *
 */
class DOMImplementation
{
   friend class Document;
public:
    DOMImplementation();
    DOMImplementation(const DOMImplementation &other);

    DOMImplementation & operator = (const DOMImplementation &other);
    ~DOMImplementation();

    /**
     * Test if the DOM implementation implements a specific feature.
     *
     * @param feature The package name of the feature to test. In
     * Level 1, the legal values are "HTML" and "XML"
     * (case-insensitive).
     *
     * @param version This is the version number of the package name
     * to test. In Level 1, this is the string "1.0". If the version
     * is not specified, supporting any version of the feature will
     * cause the method to return <code> true </code> .
     *
     * @return <code> true </code> if the feature is implemented in
     * the specified version, <code> false </code> otherwise.
     *
     */
    bool hasFeature ( const DOMString &feature, const DOMString &version );

    /**
     * Introduced in DOM Level 2
     * This method is from the DOMImplementationCSS interface
     *
     * Creates a new CSSStyleSheet.
     *
     * @param title The advisory title. See also the Style Sheet Interfaces
     * section.
     *
     * media The comma-separated list of media associated with the new style
     * sheet. See also the Style Sheet Interfaces section.
     *
     * @return A new CSS style sheet.
     *
     * @exception SYNTAX_ERR: Raised if the specified media string value has a syntax error and is unparsable.
     */
    CSSStyleSheet createCSSStyleSheet(const DOMString &title, const DOMString &media);

    /**
     * @internal
     * not part of the DOM
     */
    DOMImplementationImpl *handle() const;
    bool isNull() const;

protected:
    DOMImplementation(DOMImplementationImpl *i);
    DOMImplementationImpl *impl;
};


/**
 * The <code> Document </code> interface represents the entire HTML or
 * XML document. Conceptually, it is the root of the document tree,
 * and provides the primary access to the document's data.
 *
 *  Since elements, text nodes, comments, processing instructions,
 * etc. cannot exist outside the context of a <code> Document </code>
 * , the <code> Document </code> interface also contains the factory
 * methods needed to create these objects. The <code> Node </code>
 * objects created have a <code> ownerDocument </code> attribute which
 * associates them with the <code> Document </code> within whose
 * context they were created.
 *
 */
class Document : public Node
{
    friend class ::KHTMLView;
    friend class ::KHTMLPart;
    friend class AbstractView;
public:
    Document();
    /**
     * don't create an implementation if false
     * use at own risk
     */
    Document(bool);
    Document(const Document &other);
    Document(const Node &other) : Node()
	     {(*this)=other;}

    Document & operator = (const Node &other);
    Document & operator = (const Document &other);

    ~Document();

    /**
     * The Document Type Declaration (see <code> DocumentType </code>
     * ) associated with this document. For HTML documents as well as
     * XML documents without a document type declaration this returns
     * <code> null </code> . The DOM Level 1 does not support editing
     * the Document Type Declaration, therefore <code> docType </code>
     * cannot be altered in any way.
     *
     */
    DocumentType doctype() const;

    /**
     * The <code> DOMImplementation </code> object that handles this
     * document. A DOM application may use objects from multiple
     * implementations.
     *
     */
    DOMImplementation implementation() const;

    /**
     * This is a convenience attribute that allows direct access to
     * the child node that is the root element of the document. For
     * HTML documents, this is the element with the tagName "HTML".
     *
     */
    Element documentElement() const;

    /**
     * Creates an element of the type specified. Note that the
     * instance returned implements the Element interface, so
     * attributes can be specified directly on the returned object.
     *
     * @param tagName The name of the element type to instantiate. For
     * XML, this is case-sensitive. For HTML, the <code> tagName
     * </code> parameter may be provided in any case, but it must be
     * mapped to the canonical uppercase form by the DOM
     * implementation.
     *
     * @return A new <code> Element </code> object.
     *
     * @exception DOMException
     * INVALID_CHARACTER_ERR: Raised if the specified name contains an
     * invalid character.
     *
     */
    Element createElement ( const DOMString &tagName );

    /**
     * Introduced in DOM Level 2
     * Creates an element of the given qualified name and namespace URI.
     *
     * @param namespaceURI The namespace URI of the element to create.
     *
     * @param qualifiedName The qualified name of the element type to instantiate.
     *
     * @return A new Element object with the following attributes:
     *
     * @exception INVALID_CHARACTER_ERR Raised if the specified qualified name
     * contains an illegal character.
     *
     * @exception NAMESPACE_ERR Raised if the qualifiedName is malformed, if
     * the qualifiedName has a prefix and the namespaceURI is null, or if the
     * qualifiedName has a prefix that is "xml" and the namespaceURI is
     * different from "http://www.w3.org/XML/1998/namespace"
     */
    Element createElementNS( const DOMString &namespaceURI,
                             const DOMString &qualifiedName );

    /**
     * Creates an empty <code> DocumentFragment </code> object.
     *
     * @return A new <code> DocumentFragment </code> .
     *
     */
    DocumentFragment createDocumentFragment (  );

    /**
     * Creates a <code> Text </code> node given the specified string.
     *
     * @param data The data for the node.
     *
     * @return The new <code> Text </code> object.
     *
     */
    Text createTextNode ( const DOMString &data );

    /**
     * Creates a <code> Comment </code> node given the specified
     * string.
     *
     * @param data The data for the node.
     *
     * @return The new <code> Comment </code> object.
     *
     */
    Comment createComment ( const DOMString &data );

    /**
     * Creates a <code> CDATASection </code> node whose value is the
     * specified string.
     *
     * @param data The data for the <code> CDATASection </code>
     * contents.
     *
     * @return The new <code> CDATASection </code> object.
     *
     * @exception DOMException
     * NOT_SUPPORTED_ERR: Raised if this document is an HTML document.
     *
     */
    CDATASection createCDATASection ( const DOMString &data );

    /**
     * Creates a <code> ProcessingInstruction </code> node given the
     * specified name and data strings.
     *
     * @param target The target part of the processing instruction.
     *
     * @param data The data for the node.
     *
     * @return The new <code> ProcessingInstruction </code> object.
     *
     * @exception DOMException
     * INVALID_CHARACTER_ERR: Raised if an invalid character is
     * specified.
     *
     *  NOT_SUPPORTED_ERR: Raised if this document is an HTML
     * document.
     *
     */
    ProcessingInstruction createProcessingInstruction ( const DOMString &target, const DOMString &data );

    /**
     * Creates an <code> Attr </code> of the given name. Note that the
     * <code> Attr </code> instance can then be set on an <code>
     * Element </code> using the <code> setAttribute </code> method.
     *
     * @param name The name of the attribute.
     *
     * @return A new <code> Attr </code> object.
     *
     * @exception DOMException
     * INVALID_CHARACTER_ERR: Raised if the specified name contains an
     * invalid character.
     *
     */
    Attr createAttribute ( const DOMString &name );

    /**
     * Introduced in DOM Level 2
     * Creates an attribute of the given qualified name and namespace URI.
     * HTML-only DOM implementations do not need to implement this method.
     *
     * @param namespaceURI The namespace URI of the attribute to create.
     *
     * @param qualifiedName The qualified name of the attribute to instantiate.
     *
     * @return A new Attr object with the following attributes:
     * Node.nodeName - qualifiedName
     * Node.namespaceURI - namespaceURI
     * Node.prefix - prefix, extracted from qualifiedName, or null if there is no prefix
     * Node.localName - local name, extracted from qualifiedName
     * Attr.name - qualifiedName
     * Node.nodeValue - the empty string
     *
     * @exception INVALID_CHARACTER_ERR Raised if the specified qualified name
     * contains an illegal character.
     *
     * @exception NAMESPACE_ERR Raised if the qualifiedName is malformed, if
     * the qualifiedName has a prefix and the namespaceURI is null, if the
     * qualifiedName has a prefix that is "xml" and the namespaceURI is
     * different from "http://www.w3.org/XML/1998/namespace", or if the
     * qualifiedName is "xmlns" and the namespaceURI is different from
     * "http://www.w3.org/2000/xmlns/".
     */
    Attr createAttributeNS( const DOMString &namespaceURI,
                            const DOMString &qualifiedName );

    /**
     * Creates an EntityReference object.
     *
     * @param name The name of the entity to reference.
     *
     * @return The new <code> EntityReference </code> object.
     *
     * @exception DOMException
     * INVALID_CHARACTER_ERR: Raised if the specified name contains an
     * invalid character.
     *
     *  NOT_SUPPORTED_ERR: Raised if this document is an HTML
     * document.
     *
     */
    EntityReference createEntityReference ( const DOMString &name );

    /**
     * Returns a <code> NodeList </code> of all the <code> Element
     * </code> s with a given tag name in the order in which they
     * would be encountered in a preorder traversal of the <code>
     * Document </code> tree.
     *
     * @param tagname The name of the tag to match on. The special
     * value "*" matches all tags.
     *
     * @return A new <code> NodeList </code> object containing all the
     * matched <code> Element </code> s.
     *
     */
    NodeList getElementsByTagName ( const DOMString &tagname );

    /**
     * @internal
     * not part of the DOM
     */
    bool isHTMLDocument() const;

    /**
     * Introduced in DOM Level 2
     * This method is from the DocumentRange interface
     *
     * @return Range
     * The initial state of the Range returned from this method is such that
     * both of its boundary-points are positioned at the beginning of the
     * corresponding Document, before any content. The Range returned can only
     * be used to select content associated with this Document, or with
     * DocumentFragments and Attrs for which this Document is the ownerDocument.
     */
    Range createRange();

    /**
     * Introduced in DOM Level 2
     * This method is from the DocumentTraversal interface
     *
     * Create a new NodeIterator over the subtree rooted at the specified node.
     *
     * @param root The node which will be iterated together with its children.
     * The iterator is initially positioned just before this node. The
     * whatToShow flags and the filter, if any, are not considered when setting
     * this position. The root must not be null.
     *
     * @param whatToShow This flag specifies which node types may appear in the
     * logical view of the tree presented by the iterator. See the description
     * of NodeFilter for the set of possible SHOW_ values. These flags can be
     * combined using OR.
     *
     * @param filter The NodeFilter to be used with this TreeWalker, or null to
     * indicate no filter.
     *
     * @param entityReferenceExpansion The value of this flag determines
     * whether entity reference nodes are expanded.
     *
     * @return NodeIterator The newly created NodeIterator.
     *
     * @exception DOMException
     * NOT_SUPPORTED_ERR: Raised if the specified root is null.
     */
    NodeIterator createNodeIterator(Node root, unsigned long whatToShow,
                                    NodeFilter filter, bool entityReferenceExpansion);

    /**
     * Introduced in DOM Level 2
     * This method is from the DocumentTraversal interface
     *
     * Create a new TreeWalker over the subtree rooted at the specified node.
     *
     * @param root The node which will serve as the root for the TreeWalker.
     * The whatToShow flags and the NodeFilter are not considered when setting
     * this value; any node type will be accepted as the root. The currentNode
     * of the TreeWalker is initialized to this node, whether or not it is
     * visible. The root functions as a stopping point for traversal methods
     * that look upward in the document structure, such as parentNode and
     * nextNode. The root must not be null.
     *
     * @param whatToShow This flag specifies which node types may appear in the
     * logical view of the tree presented by the tree-walker. See the
     * description of NodeFilter for the set of possible SHOW_ values. These
     * flags can be combined using OR.
     *
     * @param filter The NodeFilter to be used with this TreeWalker, or null to
     * indicate no filter.
     *
     * @param entityReferenceExpansion If this flag is false, the contents of
     * EntityReference nodes are not presented in the logical view.
     *
     * @return The newly created TreeWalker.
     *
     * @exception DOMException
     * NOT_SUPPORTED_ERR: Raised if the specified root is null.
     */
    TreeWalker createTreeWalker(Node root, unsigned long whatToShow, NodeFilter filter,
                                bool entityReferenceExpansion);

    /**
     * Introduced in DOM Level 2
     * This method is from the DocumentEvent interface
     *
     * The createEvent method is used in creating Events when it is either
     * inconvenient or unnecessary for the user to create an Event themselves.
     * In cases where the implementation provided Event is insufficient, users
     * may supply their own Event implementations for use with the dispatchEvent method.
     *
     * @param eventType The eventType parameter specifies the type of Event
     * interface to be created. If the Event interface specified is supported
     * by the implementation this method will return a new Event of the
     * interface type requested. If the Event is to be dispatched via the
     * dispatchEvent method the appropriate event init method must be called
     * after creation in order to initialize the Event's values. As an example,
     * a user wishing to synthesize some kind of UIEvent would call createEvent
     * with the parameter "UIEvents". The initUIEvent method could then be
     * called on the newly created UIEvent to set the specific type of UIEvent
     * to be dispatched and set its context information.
     *
     * @return The newly created EventExceptions
     *
     * @exception DOMException
     * NOT_SUPPORTED_ERR: Raised if the implementation does not support the type of Event interface requested
     */
    Event createEvent(const DOMString &eventType);

    /**
     * Introduced in DOM Level 2
     * This method is from the DocumentView interface
     *
     * The default AbstractView for this Document, or null if none available.
     */
    AbstractView defaultView() const;

    /**
     * Introduced in DOM Level 2
     * This method is from the DocumentStyle interface
     *
     * A list containing all the style sheets explicitly linked into or
     * embedded in a document. For HTML documents, this includes external style
     * sheets, included via the HTML LINK element, and inline STYLE elements.
     * In XML, this includes external style sheets, included via style sheet
     * processing instructions (see [XML-StyleSheet]).
     */
    StyleSheetList styleSheets() const;

    /**
     * @return The KHTML view widget of this document.
     */
    KHTMLView *view() const;

    /**
     * Introduced in DOM Level 2
     * This method is from the DocumentCSS interface
     *
     * This method is used to retrieve the override style declaration for a
     * specified element and a specified pseudo-element.
     *
     * @param elt The element whose style is to be modified. This parameter cannot be null.
     *
     * @param pseudoElt The pseudo-element or null if none.
     *
     * @return The override style declaration.
     */
    CSSStyleDeclaration getOverrideStyle(const Element &elt, const DOMString &pseudoElt);

protected:
    Document( DocumentImpl *i);

    friend class Node;
};

class DocumentFragmentImpl;

/**
 * <code> DocumentFragment </code> is a "lightweight" or "minimal"
 * <code> Document </code> object. It is very common to want to be
 * able to extract a portion of a document's tree or to create a new
 * fragment of a document. Imagine implementing a user command like
 * cut or rearranging a document by moving fragments around. It is
 * desirable to have an object which can hold such fragments and it is
 * quite natural to use a Node for this purpose. While it is true that
 * a <code> Document </code> object could fulfil this role, a <code>
 * Document </code> object can potentially be a heavyweight object,
 * depending on the underlying implementation. What is really needed
 * for this is a very lightweight object. <code> DocumentFragment
 * </code> is such an object.
 *
 *  Furthermore, various operations -- such as inserting nodes as
 * children of another <code> Node </code> -- may take <code>
 * DocumentFragment </code> objects as arguments; this results in all
 * the child nodes of the <code> DocumentFragment </code> being moved
 * to the child list of this node.
 *
 *  The children of a <code> DocumentFragment </code> node are zero or
 * more nodes representing the tops of any sub-trees defining the
 * structure of the document. <code> DocumentFragment </code> nodes do
 * not need to be well-formed XML documents (although they do need to
 * follow the rules imposed upon well-formed XML parsed entities,
 * which can have multiple top nodes). For example, a <code>
 * DocumentFragment </code> might have only one child and that child
 * node could be a <code> Text </code> node. Such a structure model
 * represents neither an HTML document nor a well-formed XML document.
 *
 *  When a <code> DocumentFragment </code> is inserted into a <code>
 * Document </code> (or indeed any other <code> Node </code> that may
 * take children) the children of the <code> DocumentFragment </code>
 * and not the <code> DocumentFragment </code> itself are inserted
 * into the <code> Node </code> . This makes the <code>
 * DocumentFragment </code> very useful when the user wishes to create
 * nodes that are siblings; the <code> DocumentFragment </code> acts
 * as the parent of these nodes so that the user can use the standard
 * methods from the <code> Node </code> interface, such as <code>
 * insertBefore() </code> and <code> appendChild() </code> .
 *
 */
class DocumentFragment : public Node
{
    friend class Document;
    friend class Range;

public:
    DocumentFragment();
    DocumentFragment(const DocumentFragment &other);
    DocumentFragment(const Node &other) : Node()
         {(*this)=other;}

    DocumentFragment & operator = (const Node &other);
    DocumentFragment & operator = (const DocumentFragment &other);

    ~DocumentFragment();

protected:
    DocumentFragment(DocumentFragmentImpl *i);
};

class NamedNodeMap;
class DOMString;

/**
 * Each <code> Document </code> has a <code> doctype </code> attribute
 * whose value is either <code> null </code> or a <code> DocumentType
 * </code> object. The <code> DocumentType </code> interface in the
 * DOM Level 1 Core provides an interface to the list of entities that
 * are defined for the document, and little else because the effect of
 * namespaces and the various XML scheme efforts on DTD representation
 * are not clearly understood as of this writing.
 *
 *  The DOM Level 1 doesn't support editing <code> DocumentType
 * </code> nodes.
 *
 */
class DocumentType : public Node
{
    friend class Document;
public:
    DocumentType();
    DocumentType(const DocumentType &other);

    DocumentType(const Node &other) : Node()
         {(*this)=other;}
    DocumentType & operator = (const Node &other);
    DocumentType & operator = (const DocumentType &other);

    ~DocumentType();

    /**
     * The name of DTD; i.e., the name immediately following the
     * <code> DOCTYPE </code> keyword.
     *
     */
    DOMString name() const;

    /**
     * A <code> NamedNodeMap </code> containing the general entities,
     * both external and internal, declared in the DTD. Duplicates are
     * discarded. For example in: &lt;!DOCTYPE ex SYSTEM "ex.dtd" [
     * &lt;!ENTITY foo "foo"> &lt;!ENTITY bar "bar"> &lt;!ENTITY % baz
     * "baz"> ]> &lt;ex/> the interface provides access to <code> foo
     * </code> and <code> bar </code> but not <code> baz </code> .
     * Every node in this map also implements the <code> Entity
     * </code> interface.
     *
     *  The DOM Level 1 does not support editing entities, therefore
     * <code> entities </code> cannot be altered in any way.
     *
     */
    NamedNodeMap entities() const;

    /**
     * A <code> NamedNodeMap </code> containing the notations declared
     * in the DTD. Duplicates are discarded. Every node in this map
     * also implements the <code> Notation </code> interface.
     *
     *  The DOM Level 1 does not support editing notations, therefore
     * <code> notations </code> cannot be altered in any way.
     *
     */
    NamedNodeMap notations() const;
protected:
    DocumentType(DocumentTypeImpl *impl);
};

}; //namespace
#endif
