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

#ifndef HTML_BASE_H
#define HTML_BASE_H

#include <dom/html_element.h>

namespace DOM {

class HTMLBodyElementImpl;
class DOMString;

/**
 * The HTML document body. This element is always present in the DOM
 * API, even if the tags are not present in the source document. See
 * the <a
 * href="http://www.w3.org/TR/REC-html40/struct/global.html#edef-BODY">
 * BODY element definition </a> in HTML 4.0.
 *
 */
class HTMLBodyElement : public HTMLElement
{
public:
    HTMLBodyElement();
    HTMLBodyElement(const HTMLBodyElement &other);
    HTMLBodyElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLBodyElement(HTMLBodyElementImpl *impl);
public:

    HTMLBodyElement & operator = (const HTMLBodyElement &other);
    HTMLBodyElement & operator = (const Node &other);

    ~HTMLBodyElement();

    /**
     * Color of active links (after mouse-button down, but before
     * mouse-button up). See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-alink">
     * alink attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString aLink() const;

    /**
     * see @ref aLink
     */
    void setALink( const DOMString & );

    /**
     * URI of the background texture tile image. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-background">
     * background attribute definition </a> in HTML 4.0. This
     * attribute is deprecated in HTML 4.0.
     *
     */
    DOMString background() const;

    /**
     * see @ref background
     */
    void setBackground( const DOMString & );

    /**
     * Document background color. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-bgcolor">
     * bgcolor attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    DOMString bgColor() const;

    /**
     * see @ref bgColor
     */
    void setBgColor( const DOMString & );

    /**
     * Color of links that are not active and unvisited. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-link">
     * link attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString link() const;

    /**
     * see @ref link
     */
    void setLink( const DOMString & );

    /**
     * Document text color. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-text">
     * text attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString text() const;

    /**
     * see @ref text
     */
    void setText( const DOMString & );

    /**
     * Color of links that have been visited by the user. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-vlink">
     * vlink attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString vLink() const;

    /**
     * see @ref vLink
     */
    void setVLink( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLFrameElementImpl;
class DOMString;

/**
 * Create a frame. See the <a
 * href="http://www.w3.org/TR/REC-html40/present/frames.html#edef-FRAME">
 * FRAME element definition </a> in HTML 4.0.
 *
 */
class HTMLFrameElement : public HTMLElement
{
public:
    HTMLFrameElement();
    HTMLFrameElement(const HTMLFrameElement &other);
    HTMLFrameElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLFrameElement(HTMLFrameElementImpl *impl);
public:

    HTMLFrameElement & operator = (const HTMLFrameElement &other);
    HTMLFrameElement & operator = (const Node &other);

    ~HTMLFrameElement();

    /**
     * Request frame borders. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-frameborder">
     * frameborder attribute definition </a> in HTML 4.0.
     *
     */
    DOMString frameBorder() const;

    /**
     * see @ref frameBorder
     */
    void setFrameBorder( const DOMString & );

    /**
     * URI designating a long description of this image or frame. See
     * the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-longdesc-FRAME">
     * longdesc attribute definition </a> in HTML 4.0.
     *
     */
    DOMString longDesc() const;

    /**
     * see @ref longDesc
     */
    void setLongDesc( const DOMString & );

    /**
     * Frame margin height, in pixels. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-marginheight">
     * marginheight attribute definition </a> in HTML 4.0.
     *
     */
    DOMString marginHeight() const;

    /**
     * see @ref marginHeight
     */
    void setMarginHeight( const DOMString & );

    /**
     * Frame margin width, in pixels. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-marginwidth">
     * marginwidth attribute definition </a> in HTML 4.0.
     *
     */
    DOMString marginWidth() const;

    /**
     * see @ref marginWidth
     */
    void setMarginWidth( const DOMString & );

    /**
     * The frame name (object of the <code> target </code> attribute).
     * See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-name-FRAME">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * When true, forbid user from resizing frame. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-noresize">
     * noresize attribute definition </a> in HTML 4.0.
     *
     */
    bool noResize() const;

    /**
     * see @ref noResize
     */
    void setNoResize( bool );

    /**
     * Specify whether or not the frame should have scrollbars. See
     * the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-scrolling">
     * scrolling attribute definition </a> in HTML 4.0.
     *
     */
    DOMString scrolling() const;

    /**
     * see @ref scrolling
     */
    void setScrolling( const DOMString & );

    /**
     * A URI designating the initial frame contents. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-src-FRAME">
     * src attribute definition </a> in HTML 4.0.
     *
     */
    DOMString src() const;

    /**
     * see @ref src
     */
    void setSrc( const DOMString & );

    /**
     * Introduced in DOM Level 2
     *
     * Returns the document this frame contains, if there is any and
     * it is available, a Null document otherwise. The attribute is
     * read-only.
     *
     * @return The content Document if available.
     */
    Document contentDocument() const;
};

// --------------------------------------------------------------------------

class HTMLFrameSetElementImpl;
class DOMString;

/**
 * Create a grid of frames. See the <a
 * href="http://www.w3.org/TR/REC-html40/present/frames.html#edef-FRAMESET">
 * FRAMESET element definition </a> in HTML 4.0.
 *
 */
class HTMLFrameSetElement : public HTMLElement
{
public:
    HTMLFrameSetElement();
    HTMLFrameSetElement(const HTMLFrameSetElement &other);
    HTMLFrameSetElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLFrameSetElement(HTMLFrameSetElementImpl *impl);
public:

    HTMLFrameSetElement & operator = (const HTMLFrameSetElement &other);
    HTMLFrameSetElement & operator = (const Node &other);

    ~HTMLFrameSetElement();

    /**
     * The number of columns of frames in the frameset. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-cols-FRAMESET">
     * cols attribute definition </a> in HTML 4.0.
     *
     */
    DOMString cols() const;

    /**
     * see @ref cols
     */
    void setCols( const DOMString & );

    /**
     * The number of rows of frames in the frameset. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-rows-FRAMESET">
     * rows attribute definition </a> in HTML 4.0.
     *
     */
    DOMString rows() const;

    /**
     * see @ref rows
     */
    void setRows( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLIFrameElementImpl;

/**
 * Inline subwindows. See the <a
 * href="http://www.w3.org/TR/REC-html40/present/frames.html#edef-IFRAME">
 * IFRAME element definition </a> in HTML 4.0.
 *
 */
class HTMLIFrameElement : public HTMLElement
{
public:
    HTMLIFrameElement();
    HTMLIFrameElement(const HTMLIFrameElement &other);
    HTMLIFrameElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLIFrameElement(HTMLIFrameElementImpl *impl);
public:

    HTMLIFrameElement & operator = (const HTMLIFrameElement &other);
    HTMLIFrameElement & operator = (const Node &other);

    ~HTMLIFrameElement();

    /**
     * Aligns this object (vertically or horizontally) with respect to
     * its surrounding text. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-align-IMG">
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
     * Request frame borders. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-frameborder">
     * frameborder attribute definition </a> in HTML 4.0.
     *
     */
    DOMString frameBorder() const;

    /**
     * see @ref frameBorder
     */
    void setFrameBorder( const DOMString & );

    /**
     * Frame height. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-height-IFRAME">
     * height attribute definition </a> in HTML 4.0.
     *
     */
    DOMString height() const;

    /**
     * see @ref height
     */
    void setHeight( const DOMString & );

    /**
     * URI designating a long description of this image or frame. See
     * the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-longdesc-IFRAME">
     * longdesc attribute definition </a> in HTML 4.0.
     *
     */
    DOMString longDesc() const;

    /**
     * see @ref longDesc
     */
    void setLongDesc( const DOMString & );

    /**
     * Frame margin height, in pixels. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-marginheight">
     * marginheight attribute definition </a> in HTML 4.0.
     *
     */
    DOMString marginHeight() const;

    /**
     * see @ref marginHeight
     */
    void setMarginHeight( const DOMString & );

    /**
     * Frame margin width, in pixels. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-marginwidth">
     * marginwidth attribute definition </a> in HTML 4.0.
     *
     */
    DOMString marginWidth() const;

    /**
     * see @ref marginWidth
     */
    void setMarginWidth( const DOMString & );

    /**
     * The frame name (object of the <code> target </code> attribute).
     * See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-name-IFRAME">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * Specify whether or not the frame should have scrollbars. See
     * the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-scrolling">
     * scrolling attribute definition </a> in HTML 4.0.
     *
     */
    DOMString scrolling() const;

    /**
     * see @ref scrolling
     */
    void setScrolling( const DOMString & );

    /**
     * A URI designating the initial frame contents. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-src-FRAME">
     * src attribute definition </a> in HTML 4.0.
     *
     */
    DOMString src() const;

    /**
     * see @ref src
     */
    void setSrc( const DOMString & );

    /**
     * Frame width. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/frames.html#adef-width-IFRAME">
     * width attribute definition </a> in HTML 4.0.
     *
     */
    DOMString width() const;

    /**
     * see @ref width
     */
    void setWidth( const DOMString & );

    /**
     * Introduced in DOM Level 2
     *
     * Returns the document this iframe contains, if there is any and
     * it is available, a Null document otherwise. The attribute is
     * read-only.
     *
     * @return The content Document if available.
     */
    Document contentDocument() const;
};

// --------------------------------------------------------------------------

class HTMLHeadElementImpl;
class DOMString;

/**
 * Document head information. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/global.html#edef-HEAD">
 * HEAD element definition </a> in HTML 4.0.
 *
 */
class HTMLHeadElement : public HTMLElement
{
public:
    HTMLHeadElement();
    HTMLHeadElement(const HTMLHeadElement &other);
    HTMLHeadElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLHeadElement(HTMLHeadElementImpl *impl);
public:

    HTMLHeadElement & operator = (const HTMLHeadElement &other);
    HTMLHeadElement & operator = (const Node &other);

    ~HTMLHeadElement();

    /**
     * URI designating a metadata profile. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-profile">
     * profile attribute definition </a> in HTML 4.0.
     *
     */
    DOMString profile() const;

    /**
     * see @ref profile
     */
    void setProfile( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLHtmlElementImpl;
class DOMString;

/**
 * Root of an HTML document. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/global.html#edef-HTML">
 * HTML element definition </a> in HTML 4.0.
 *
 */
class HTMLHtmlElement : public HTMLElement
{
public:
    HTMLHtmlElement();
    HTMLHtmlElement(const HTMLHtmlElement &other);
    HTMLHtmlElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLHtmlElement(HTMLHtmlElementImpl *impl);
public:

    HTMLHtmlElement & operator = (const HTMLHtmlElement &other);
    HTMLHtmlElement & operator = (const Node &other);

    ~HTMLHtmlElement();

    /**
     * Version information about the document's DTD. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-version">
     * version attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    DOMString version() const;

    /**
     * see @ref version
     */
    void setVersion( const DOMString & );
};

}; //namespace

#endif
