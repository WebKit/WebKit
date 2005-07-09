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
#ifndef HTML_ELEMENT_H
#define HTML_ELEMENT_H

#include <dom/dom_element.h>
#include "dom_qname.h"

class KHTMLView;

namespace DOM {

class HTMLElementImpl;
class DOMString;
class HTMLCollection;

/**
 * All HTML element interfaces derive from this class. Elements that
 * only expose the HTML core attributes are represented by the base
 * <code> HTMLElement </code> interface. These elements are as
 * follows:
 *
 *  <ulist> <item> HEAD
 *
 *  </item> <item> special: SUB, SUP, SPAN, BDO
 *
 *  </item> <item> font: TT, I, B, U, S, STRIKE, BIG, SMALL
 *
 *  </item> <item> phrase: EM, STRONG, DFN, CODE, SAMP, KBD, VAR,
 * CITE, ACRONYM, ABBR
 *
 *  </item> <item> list: DD, DT
 *
 *  </item> <item> NOFRAMES, NOSCRIPT
 *
 *  </item> <item> ADDRESS, CENTER
 *
 *  </item> </ulist> Note. The <code> style </code> attribute for this
 * interface is reserved for future usage.
 *
 */
class HTMLElement : public Element
{
    friend class HTMLDocument;
    friend class ::KHTMLView;
    friend class HTMLTableElement;
    friend class HTMLTableRowElement;
    friend class HTMLTableSectionElement;

public:
    HTMLElement();
    HTMLElement(const HTMLElement &other);
    HTMLElement(const Node &other) : Element()
         {(*this)=other;}

protected:
    HTMLElement(HTMLElementImpl *impl);
public:

    HTMLElement & operator = (const HTMLElement &other);
    HTMLElement & operator = (const Node &other);

    ~HTMLElement();

    /**
     * The element's identifier. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-id">
     * id attribute definition </a> in HTML 4.0.
     *
     */
    DOMString id() const;

    /**
     * see @ref id
     */
    void setId( const DOMString & );

    /**
     * The element's advisory title. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-title">
     * title attribute definition </a> in HTML 4.0.
     *
     */
    DOMString title() const;

    /**
     * see @ref title
     */
    void setTitle( const DOMString & );

    /**
     * Language code defined in RFC 1766. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/dirlang.html#adef-lang">
     * lang attribute definition </a> in HTML 4.0.
     *
     */
    DOMString lang() const;

    /**
     * see @ref lang
     */
    void setLang( const DOMString & );

    /**
     * Specifies the base direction of directionally neutral text and
     * the directionality of tables. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/dirlang.html#adef-dir">
     * dir attribute definition </a> in HTML 4.0.
     *
     */
    DOMString dir() const;

    /**
     * see @ref dir
     */
    void setDir( const DOMString & );

    /**
     * The class attribute of the element. This attribute has been
     * renamed due to conflicts with the "class" keyword exposed by
     * many languages. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-class">
     * class attribute definition </a> in HTML 4.0.
     *
     */
    DOMString className() const;

    /**
     * see @ref className
     */
    void setClassName( const DOMString & );

    /**
     * The HTML code contained in this element.
     * This function is not part of the DOM specifications as defined by the w3c.
     */
    DOMString innerHTML() const;

    /**
     * Set the HTML content of this node.
     *
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if there is the element does not allow
     * children.
     */
    void setInnerHTML( const DOMString &html );

    /**
     * The text contained in this element.
     * This function is not part of the DOM specifications as defined by the w3c.
     */
    DOMString innerText() const;

    /**
     * Set the text content of this node.
     *
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if there is the element does not allow
     * children.
     */
    void setInnerText( const DOMString &text );

    /**
     * The HTML code of this element, including the element itself.
     * This function is not part of the DOM specifications as defined by the w3c.
     */
    DOMString outerHTML() const;

    /**
     * Replace the HTML code of this element, including the element itself.
     * This function is not part of the DOM specifications as defined by the w3c.
     */
    void setOuterHTML( const DOMString &html );

    /**
     * The text contained in this element.
     * This function is not part of the DOM specifications as defined by the w3c.
     */
    DOMString outerText() const;

    /**
     * Replace this element with the given text
     *
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if there is the element does not allow
     * children.
     */
    void setOuterText( const DOMString &text );

    /**
     * Retrieves a collection of nodes that are direct descendants of this node.
     * IE-specific extension.
     */
    HTMLCollection children() const;

    bool isContentEditable() const;
    DOMString contentEditable() const;
    void setContentEditable(const DOMString &enabled);

protected:
    /*
     * @internal
     */
    void assignOther( const Node &other, const QualifiedName& tagName );
};

}; //namespace

#endif
