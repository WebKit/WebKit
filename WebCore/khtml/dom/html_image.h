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
#ifndef HTML_IMAGE_H
#define HTML_IMAGE_H

#include <dom/html_element.h>

namespace DOM {

class HTMLAreaElementImpl;
class DOMString;

/**
 * Client-side image map area definition. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/objects.html#edef-AREA">
 * AREA element definition </a> in HTML 4.0.
 *
 */
class HTMLAreaElement : public HTMLElement
{
public:
    HTMLAreaElement();
    HTMLAreaElement(const HTMLAreaElement &other);
    HTMLAreaElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLAreaElement(HTMLAreaElementImpl *impl);
public:

    HTMLAreaElement & operator = (const HTMLAreaElement &other);
    HTMLAreaElement & operator = (const Node &other);

    ~HTMLAreaElement();

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
     * Alternate text for user agents not rendering the normal content
     * of this element. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-alt">
     * alt attribute definition </a> in HTML 4.0.
     *
     */
    DOMString alt() const;

    /**
     * see @ref alt
     */
    void setAlt( const DOMString & );

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
     * Specifies that this area is inactive, i.e., has no associated
     * action. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-nohref">
     * nohref attribute definition </a> in HTML 4.0.
     *
     */
    bool noHref() const;

    /**
     * see @ref noHref
     */
    void setNoHref( bool );

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
};

// --------------------------------------------------------------------------

class HTMLImageElementImpl;

/**
 * Embedded image. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/objects.html#edef-IMG">
 * IMG element definition </a> in HTML 4.0.
 *
 */
class HTMLImageElement : public HTMLElement
{
public:
    HTMLImageElement();
    HTMLImageElement(const HTMLImageElement &other);
    HTMLImageElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLImageElement(HTMLImageElementImpl *impl);
public:

    HTMLImageElement & operator = (const HTMLImageElement &other);
    HTMLImageElement & operator = (const Node &other);

    ~HTMLImageElement();

    /**
     * The name of the element (for backwards compatibility).
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

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
     * Alternate text for user agents not rendering the normal content
     * of this element. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-alt">
     * alt attribute definition </a> in HTML 4.0.
     *
     */
    DOMString alt() const;

    /**
     * see @ref alt
     */
    void setAlt( const DOMString & );

    /**
     * Width of border around image. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-border-IMG">
     * border attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    long border() const;

    /**
     * see @ref border
     */
    void setBorder( long );

    /**
     * Override height. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-height-IMG">
     * height attribute definition </a> in HTML 4.0.
     *
     */
    long height() const;

    /**
     * see @ref height
     */
    void setHeight( long );

    /**
     * Horizontal space to the left and right of this image. See the
     * <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-hspace">
     * hspace attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    long hspace() const;

    /**
     * see @ref hspace
     */
    void setHspace( long );

    /**
     * Use server-side image map. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-ismap">
     * ismap attribute definition </a> in HTML 4.0.
     *
     */
    bool isMap() const;

    /**
     * see @ref isMap
     */
    void setIsMap( bool );

    /**
     * URI designating a long description of this image or frame. See
     * the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-longdesc-IMG">
     * longdesc attribute definition </a> in HTML 4.0.
     *
     */
    DOMString longDesc() const;

    /**
     * see @ref longDesc
     */
    void setLongDesc( const DOMString & );

    /**
     * URI designating the source of this image. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-src-IMG">
     * src attribute definition </a> in HTML 4.0.
     *
     */
    DOMString src() const;

    /**
     * see @ref src
     */
    void setSrc( const DOMString & );

    /**
     * Use client-side image map. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-usemap">
     * usemap attribute definition </a> in HTML 4.0.
     *
     */
    DOMString useMap() const;

    /**
     * see @ref useMap
     */
    void setUseMap( const DOMString & );

    /**
     * Vertical space above and below this image. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-vspace">
     * vspace attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    long vspace() const;

    /**
     * see @ref vspace
     */
    void setVspace( long );

    /**
     * Override width. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-width-IMG">
     * width attribute definition </a> in HTML 4.0.
     *
     */
    long width() const;

    /**
     * see @ref width
     */
    void setWidth( long );

    /**
     * Non-standard extensions to match Mozilla/Gecko.  Not part of the real ImgElement DOM.
     */
    long x() const;
    long y() const;
};

// --------------------------------------------------------------------------

class HTMLMapElementImpl;
class HTMLCollection;
class DOMString;

/**
 * Client-side image map. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/objects.html#edef-MAP">
 * MAP element definition </a> in HTML 4.0.
 *
 */
class HTMLMapElement : public HTMLElement
{
public:
    HTMLMapElement();
    HTMLMapElement(const HTMLMapElement &other);
    HTMLMapElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLMapElement(HTMLMapElementImpl *impl);
public:

    HTMLMapElement & operator = (const HTMLMapElement &other);
    HTMLMapElement & operator = (const Node &other);

    ~HTMLMapElement();

    /**
     * The list of areas defined for the image map.
     *
     */
    HTMLCollection areas() const;

    /**
     * Names the map (for use with <code> usemap </code> ). See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-name-MAP">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );
};

}; //namespace

#endif
