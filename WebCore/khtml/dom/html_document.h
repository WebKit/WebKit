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
 */

#ifndef HTML_DOCUMENT_H
#define HTML_DOCUMENT_H

#include <dom/dom_doc.h>
#include <dom/dom_string.h>

class KHTMLView;
class KHTMLPart;

namespace DOM {

class HTMLDocumentImpl;
class DOMImplementation;
class HTMLCollection;
class NodeList;
class Element;
class HTMLElement;

/**
 * An <code> HTMLDocument </code> is the root of the HTML hierarchy
 * and holds the entire content. Beside providing access to the
 * hierarchy, it also provides some convenience methods for accessing
 * certain sets of information from the document.
 *
 *  The following properties have been deprecated in favor of the
 * corresponding ones for the BODY element:
 *
 *  <ulist> <item> alinkColor
 *
 *  </item> <item> background
 *
 *  </item> <item> bgColor
 *
 *  </item> <item> fgColor
 *
 *  </item> <item> linkColor
 *
 *  </item> <item> vlinkColor
 *
 *  </item> </ulist>
 *
 */
class HTMLDocument : public Document
{
    friend class ::KHTMLView;
    friend class ::KHTMLPart;
    friend class DOMImplementation;
public:
    HTMLDocument();
    /**
     * The parent is the widget the document should render itself in.
     * Rendering information (like sizes, etc...) is only created if
     * parent != 0
     */
    HTMLDocument(KHTMLView *parent);
    HTMLDocument(const HTMLDocument &other);
    HTMLDocument(const Node &other) : Document(false)
         {(*this)=other;}
protected:
    HTMLDocument(HTMLDocumentImpl *impl);
public:

    HTMLDocument & operator = (const HTMLDocument &other);
    HTMLDocument & operator = (const Node &other);

    ~HTMLDocument();

    /**
     * The title of a document as specified by the <code> TITLE
     * </code> element in the head of the document.
     *
     */
    DOMString title() const;

    /**
     * see @ref title
     */
    void setTitle( const DOMString & );

    /**
     * Returns the URI of the page that linked to this page. The value
     * is an empty string if the user navigated to the page directly
     * (not through a link, but, for example, via a bookmark).
     */
    DOMString referrer() const;

    /**
     * The domain name of the server that served the document, or a
     * null string if the server cannot be identified by a domain
     * name.
     *
     */
    DOMString domain() const;

    /**
     * The absolute URI of the document.
     */
    DOMString URL() const;

    /**
     * The element that contains the content for the document. In
     * documents with <code> BODY </code> contents, returns the <code>
     * BODY </code> element, and in frameset documents, this returns
     * the outermost <code> FRAMESET </code> element.
     *
     */
    HTMLElement body() const;

    /**
     * see @ref body
     */
    void setBody(const HTMLElement &);

    /**
     * A collection of all the <code> IMG </code> elements in a
     * document. The behavior is limited to <code> IMG </code>
     * elements for backwards compatibility.
     *
     */
    HTMLCollection images() const;

    /**
     * A collection of all the <code> OBJECT </code> elements that
     * include applets and <code> APPLET </code> ( deprecated )
     * elements in a document.
     *
     */
    HTMLCollection applets() const;

    /**
     * A collection of all <code> AREA </code> elements and anchor (
     * <code> A </code> ) elements in a document with a value for the
     * <code> href </code> attribute.
     *
     */
    HTMLCollection links() const;

    /**
     * A collection of all the forms of a document.
     *
     */
    HTMLCollection forms() const;

    /**
     * A collection of all the anchor ( <code> A </code> ) elements in
     * a document with a value for the <code> name </code> attribute.
     * Note. For reasons of backwards compatibility, the returned set
     * of anchors only contains those anchors created with the <code>
     * name </code> attribute, not those created with the <code> id
     * </code> attribute.
     *
     */
    HTMLCollection anchors() const;

    /**
     * The cookies associated with this document. If there are none,
     * the value is an empty string. Otherwise, the value is a string:
     * a semicolon-delimited list of "name, value" pairs for all the
     * cookies associated with the page. For example, <code>
     * name=value;expires=date </code> .
     *
     */
    DOMString cookie() const;

    /**
     * see @ref cookie
     */
    void setCookie( const DOMString & );

#if APPLE_CHANGES
    /**
     * The base URL of the top level document. This is used to determine cookie policy.
     */
    void setPolicyBaseURL( const DOMString & );
#endif

    /**
     * Note. This method and the ones following allow a user to add to
     * or replace the structure model of a document using strings of
     * unparsed HTML. At the time of writing alternate methods for
     * providing similar functionality for both HTML and XML documents
     * were being considered. The following methods may be deprecated
     * at some point in the future in favor of a more general-purpose
     * mechanism.
     *
     *  Open a document stream for writing. If a document exists in
     * the target, this method clears it.
     *
     * @return
     *
     */
    void open (  );

    /**
     * Closes a document stream opened by <code> open() </code> and
     * forces rendering.
     *
     * @return
     *
     */
    void close (  );

    /**
     * Write a string of text to a document stream opened by <code>
     * open() </code> . The text is parsed into the document's
     * structure model.
     *
     * @param text The string to be parsed into some structure in the
     * document structure model.
     *
     * @return
     *
     */
    void write ( const DOMString &text );

    /**
     * Write a string of text followed by a newline character to a
     * document stream opened by <code> open() </code> . The text is
     * parsed into the document's structure model.
     *
     * @param text The string to be parsed into some structure in the
     * document structure model.
     *
     * @return
     *
     */
    void writeln ( const DOMString &text );

    /**
     * Returns the (possibly empty) collection of elements whose
     * <code> name </code> value is given by <code> elementName
     * </code> .
     *
     * @param elementName The <code> name </code> attribute value for
     * an element.
     *
     * @return The matching elements.
     *
     */
    NodeList getElementsByName ( const DOMString &elementName );

    /**
     * not part of the DOM
     *
     * converts the given (potentially relative) URL in a
     * full-qualified one, using the baseURL / document URL for
     * the missing parts.
     */
    DOMString completeURL( const DOMString& url) const;

    /**
     * Not part of the DOM
     *
     * The date the document was last modified.
     */
    DOMString lastModified() const;

    /**
     * Not part of the DOM
     *
     * A collection of all the <code>IMG</code>, <code>OBJECT</code>,
     * <code>AREA</code>, <code>A</code>, forms and anchor elements of
     * a document.
     */
    HTMLCollection all() const;
};

}; //namespace

#endif
