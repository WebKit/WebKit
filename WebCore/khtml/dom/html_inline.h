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
#ifndef HTML_INLINE_H
#define HTML_INLINE_H

#include <dom/html_element.h>

namespace DOM {
class HTMLGenericElementImpl;
class HTMLAnchorElementImpl;
class DOMString;

/**
 * The anchor element. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/links.html#edef-A"> A
 * element definition </a> in HTML 4.0.
 *
 */
class HTMLAnchorElement : public HTMLElement
{
public:
    HTMLAnchorElement();
    HTMLAnchorElement(const HTMLAnchorElement &other);
    HTMLAnchorElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLAnchorElement(HTMLAnchorElementImpl *impl);
public:

    HTMLAnchorElement & operator = (const HTMLAnchorElement &other);
    HTMLAnchorElement & operator = (const Node &other);

    ~HTMLAnchorElement();

    /**
     * A single character access key to give access to the form
     * control. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-accesskey">
     * accesskey attribute definition </a> in HTML 4.0.
     *
     */
    DOMString accessKey() const;

    /**
     * see @ref accessKey
     */
    void setAccessKey( const DOMString & );

    /**
     * The character encoding of the linked resource. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-charset">
     * charset attribute definition </a> in HTML 4.0.
     *
     */
    DOMString charset() const;

    /**
     * see @ref charset
     */
    void setCharset( const DOMString & );

    /**
     * Comma-separated list of lengths, defining an active region
     * geometry. See also <code> shape </code> for the shape of the
     * region. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-coords">
     * coords attribute definition </a> in HTML 4.0.
     *
     */
    DOMString coords() const;

    /**
     * see @ref coords
     */
    void setCoords( const DOMString & );

    /**
     * The URI of the linked resource. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-href">
     * href attribute definition </a> in HTML 4.0.
     *
     */
    DOMString href() const;

    /**
     * see @ref href
     */
    void setHref( const DOMString & );

    /**
     * Language code of the linked resource. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-hreflang">
     * hreflang attribute definition </a> in HTML 4.0.
     *
     */
    DOMString hreflang() const;

    /**
     * see @ref hreflang
     */
    void setHreflang( const DOMString & );

    /**
     * Anchor name. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-name-A">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * Forward link type. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-rel">
     * rel attribute definition </a> in HTML 4.0.
     *
     */
    DOMString rel() const;

    /**
     * see @ref rel
     */
    void setRel( const DOMString & );

    /**
     * Reverse link type. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-rev">
     * rev attribute definition </a> in HTML 4.0.
     *
     */
    DOMString rev() const;

    /**
     * see @ref rev
     */
    void setRev( const DOMString & );

    /**
     * The shape of the active area. The coordinates are given by
     * <code> coords </code> . See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-shape">
     * shape attribute definition </a> in HTML 4.0.
     *
     */
    DOMString shape() const;

    /**
     * see @ref shape
     */
    void setShape( const DOMString & );

    /**
     * Index that represents the element's position in the tabbing
     * order. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-tabindex">
     * tabindex attribute definition </a> in HTML 4.0.
     *
     */
    long tabIndex() const;

    /**
     * see @ref tabIndex
     */
    void setTabIndex( long );

    /**
     * Frame to render the resource in. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-target">
     * target attribute definition </a> in HTML 4.0.
     *
     */
    DOMString target() const;

    /**
     * see @ref target
     */
    void setTarget( const DOMString & );

    /**
     * Advisory content type. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-type-A">
     * type attribute definition </a> in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType( const DOMString & );

    /**
     * Removes keyboard focus from this element.
     *
     * @return
     *
     */
    void blur (  );

    /**
     * Gives keyboard focus to this element.
     *
     * @return
     *
     */
    void focus (  );
};

// --------------------------------------------------------------------------

class HTMLBRElementImpl;

/**
 * Force a line break. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/text.html#edef-BR"> BR
 * element definition </a> in HTML 4.0.
 *
 */
class HTMLBRElement : public HTMLElement
{
public:
    HTMLBRElement();
    HTMLBRElement(const HTMLBRElement &other);
    HTMLBRElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLBRElement(HTMLBRElementImpl *impl);
public:

    HTMLBRElement & operator = (const HTMLBRElement &other);
    HTMLBRElement & operator = (const Node &other);

    ~HTMLBRElement();

    /**
     * Control flow of text around floats. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-clear">
     * clear attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString clear() const;

    /**
     * see @ref clear
     */
    void setClear( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLFontElementImpl;
class DOMString;

/**
 * Local change to font. See the <a
 * href="http://www.w3.org/TR/REC-html40/present/graphics.html#edef-FONT">
 * FONT element definition </a> in HTML 4.0. This element is
 * deprecated in HTML 4.0.
 *
 */
class HTMLFontElement : public HTMLElement
{
public:
    HTMLFontElement();
    HTMLFontElement(const HTMLFontElement &other);
    HTMLFontElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLFontElement(HTMLFontElementImpl *impl);
public:

    HTMLFontElement & operator = (const HTMLFontElement &other);
    HTMLFontElement & operator = (const Node &other);

    ~HTMLFontElement();

    /**
     * Font color. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-color-FONT">
     * color attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString color() const;

    /**
     * see @ref color
     */
    void setColor( const DOMString & );

    /**
     * Font face identifier. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-face-FONT">
     * face attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString face() const;

    /**
     * see @ref face
     */
    void setFace( const DOMString & );

    /**
     * Font size. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-size-FONT">
     * size attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString size() const;

    /**
     * see @ref size
     */
    void setSize( const DOMString & );
};

class HTMLModElementImpl;
class DOMString;

/**
 * Notice of modification to part of a document. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/text.html#edef-ins">
 * INS </a> and <a
 * href="http://www.w3.org/TR/REC-html40/struct/text.html#edef-del">
 * DEL </a> element definitions in HTML 4.0.
 *
 */
class HTMLModElement : public HTMLElement
{
public:
    HTMLModElement();
    HTMLModElement(const HTMLModElement &other);
    HTMLModElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLModElement(HTMLElementImpl *impl);
public:

    HTMLModElement & operator = (const HTMLModElement &other);
    HTMLModElement & operator = (const Node &other);

    ~HTMLModElement();

    /**
     * A URI designating a document that describes the reason for the
     * change. See the <a href="http://www.w3.org/TR/REC-html40/">
     * cite attribute definition </a> in HTML 4.0.
     *
     */
    DOMString cite() const;

    /**
     * see @ref cite
     */
    void setCite( const DOMString & );

    /**
     * The date and time of the change. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/text.html#adef-datetime">
     * datetime attribute definition </a> in HTML 4.0.
     *
     */
    DOMString dateTime() const;

    /**
     * see @ref dateTime
     */
    void setDateTime( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLQuoteElementImpl;

/**
 * For the <code> Q </code> and <code> BLOCKQUOTE </code> elements.
 * See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/text.html#edef-Q"> Q
 * element definition </a> in HTML 4.0.
 *
 * Note: The DOM is not quite consistent here. They also define the
 * HTMLBlockQuoteElement interface, to represent the <code>BLOCKQUOTE</code>
 * element. To resolve ambiquities, we use this one for the <code>Q</code>
 * element only.
 */
class HTMLQuoteElement : public HTMLElement
{
public:
    HTMLQuoteElement();
    HTMLQuoteElement(const HTMLQuoteElement &other);
    HTMLQuoteElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLQuoteElement(HTMLQuoteElementImpl *impl);
public:

    HTMLQuoteElement & operator = (const HTMLQuoteElement &other);
    HTMLQuoteElement & operator = (const Node &other);

    ~HTMLQuoteElement();

    /**
     * A URI designating a document that designates a source document
     * or message. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/text.html#adef-cite-Q">
     * cite attribute definition </a> in HTML 4.0.
     *
     */
    DOMString cite() const;

    /**
     * see @ref cite
     */
    void setCite( const DOMString & );
};

}; //namespace

#endif
