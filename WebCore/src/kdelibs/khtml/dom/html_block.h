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
// --------------------------------------------------------------------------

#ifndef HTML_BLOCK_H
#define HTML_BLOCK_H

#include <dom/html_element.h>

namespace DOM {

class HTMLBlockquoteElementImpl;
class DOMString;

/**
 * ??? See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/text.html#edef-BLOCKQUOTE">
 * BLOCKQUOTE element definition </a> in HTML 4.0.
 *
 */
class HTMLBlockquoteElement : public HTMLElement
{
public:
    HTMLBlockquoteElement();
    HTMLBlockquoteElement(const HTMLBlockquoteElement &other);
    HTMLBlockquoteElement(const Node &other) : HTMLElement()
        {(*this)=other;}
protected:
    HTMLBlockquoteElement(HTMLBlockquoteElementImpl *impl);
public:

    HTMLBlockquoteElement & operator = (const HTMLBlockquoteElement &other);
    HTMLBlockquoteElement & operator = (const Node &other);

    ~HTMLBlockquoteElement();

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
};

// --------------------------------------------------------------------------

class HTMLDivElementImpl;
class DOMString;

/**
 * Generic block container. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/global.html#edef-DIV">
 * DIV element definition </a> in HTML 4.0.
 *
 */
class HTMLDivElement : public HTMLElement
{
public:
    HTMLDivElement();
    HTMLDivElement(const HTMLDivElement &other);
    HTMLDivElement(const Node &other) : HTMLElement()
        {(*this)=other;}
protected:
    HTMLDivElement(HTMLDivElementImpl *impl);
public:

    HTMLDivElement & operator = (const HTMLDivElement &other);
    HTMLDivElement & operator = (const Node &other);

    ~HTMLDivElement();

    /**
     * Horizontal text alignment. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-align">
     * align attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString align() const;

    /**
     * see @ref align
     */
    void setAlign( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLHRElementImpl;
class DOMString;

/**
 * Create a horizontal rule. See the <a
 * href="http://www.w3.org/TR/REC-html40/present/graphics.html#edef-HR">
 * HR element definition </a> in HTML 4.0.
 *
 */
class HTMLHRElement : public HTMLElement
{
public:
    HTMLHRElement();
    HTMLHRElement(const HTMLHRElement &other);
    HTMLHRElement(const Node &other) : HTMLElement()
        {(*this)=other;}
protected:
    HTMLHRElement(HTMLHRElementImpl *impl);
public:

    HTMLHRElement & operator = (const HTMLHRElement &other);
    HTMLHRElement & operator = (const Node &other);

    ~HTMLHRElement();

    /**
     * Align the rule on the page. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-align-HR">
     * align attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString align() const;

    /**
     * see @ref align
     */
    void setAlign( const DOMString & );

    /**
     * Indicates to the user agent that there should be no shading in
     * the rendering of this element. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-noshade">
     * noshade attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    bool noShade() const;

    /**
     * see @ref noShade
     */
    void setNoShade( bool );

    /**
     * The height of the rule. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-size-HR">
     * size attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString size() const;

    /**
     * see @ref size
     */
    void setSize( const DOMString & );

    /**
     * The width of the rule. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-width-HR">
     * width attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString width() const;

    /**
     * see @ref width
     */
    void setWidth( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLHeadingElementImpl;
class DOMString;

/**
 * For the <code> H1 </code> to <code> H6 </code> elements. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/global.html#edef-H1">
 * H1 element definition </a> in HTML 4.0.
 *
 */
class HTMLHeadingElement : public HTMLElement
{
public:
    HTMLHeadingElement();
    HTMLHeadingElement(const HTMLHeadingElement &other);
    HTMLHeadingElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLHeadingElement(HTMLHeadingElementImpl *impl);
public:

    HTMLHeadingElement & operator = (const HTMLHeadingElement &other);
    HTMLHeadingElement & operator = (const Node &other);

    ~HTMLHeadingElement();

    /**
     * Horizontal text alignment. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-align">
     * align attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString align() const;

    /**
     * see @ref align
     */
    void setAlign( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLParagraphElementImpl;
class DOMString;

/**
 * Paragraphs. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/text.html#edef-P"> P
 * element definition </a> in HTML 4.0.
 *
 */
class HTMLParagraphElement : public HTMLElement
{
public:
    HTMLParagraphElement();
    HTMLParagraphElement(const HTMLParagraphElement &other);
    HTMLParagraphElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLParagraphElement(HTMLParagraphElementImpl *impl);
public:

    HTMLParagraphElement & operator = (const HTMLParagraphElement &other);
    HTMLParagraphElement & operator = (const Node &other);

    ~HTMLParagraphElement();

    /**
     * Horizontal text alignment. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-align">
     * align attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString align() const;

    /**
     * see @ref align
     */
    void setAlign( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLPreElementImpl;

/**
 * Preformatted text. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/text.html#edef-PRE">
 * PRE element definition </a> in HTML 4.0.
 *
 */
class HTMLPreElement : public HTMLElement
{
public:
    HTMLPreElement();
    HTMLPreElement(const HTMLPreElement &other);
    HTMLPreElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLPreElement(HTMLPreElementImpl *impl);
public:

    HTMLPreElement & operator = (const HTMLPreElement &other);
    HTMLPreElement & operator = (const Node &other);

    ~HTMLPreElement();

    /**
     * Fixed width for content. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/text.html#adef-width-PRE">
     * width attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    long width() const;

    /**
     * see @ref width
     */
    void setWidth( long );
};

}; //namespace

#endif
