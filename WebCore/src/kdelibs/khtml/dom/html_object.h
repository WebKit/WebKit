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

#ifndef HTML_OBJECT_H
#define HTML_OBJECT_H

#include <dom/html_element.h>
#include <dom/html_form.h>

namespace DOM {

class HTMLAppletElementImpl;

/**
 * An embedded Java applet. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/objects.html#edef-APPLET">
 * APPLET element definition </a> in HTML 4.0. This element is
 * deprecated in HTML 4.0.
 *
 */
class HTMLAppletElement : public HTMLElement
{
public:
    HTMLAppletElement();
    HTMLAppletElement(const HTMLAppletElement &other);
    HTMLAppletElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLAppletElement(HTMLAppletElementImpl *impl);
public:

    HTMLAppletElement & operator = (const HTMLAppletElement &other);
    HTMLAppletElement & operator = (const Node &other);

    ~HTMLAppletElement();

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
     * alt attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString alt() const;

    /**
     * see @ref alt
     */
    void setAlt( const DOMString & );

    /**
     * Comma-separated archive list. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-archive-APPLET">
     * archive attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    DOMString archive() const;

    /**
     * see @ref archive
     */
    void setArchive( const DOMString & );

    /**
     * Applet class file. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-code">
     * code attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString code() const;

    /**
     * see @ref code
     */
    void setCode( const DOMString & );

    /**
     * Optional base URI for applet. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-codebase-APPLET">
     * codebase attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    DOMString codeBase() const;

    /**
     * see @ref codeBase
     */
    void setCodeBase( const DOMString & );

    /**
     * Override height. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-height-APPLET">
     * height attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString height() const;

    /**
     * see @ref height
     */
    void setHeight( const DOMString & );

    /**
     * Horizontal space to the left and right of this image, applet,
     * or object. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-hspace">
     * hspace attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString hspace() const;

    /**
     * see @ref hspace
     */
    void setHspace( const DOMString & );

    /**
     * The name of the applet. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-name-APPLET">
     * name attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * Serialized applet file. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-object">
     * object attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString object() const;

    /**
     * see @ref object
     */
    void setObject( const DOMString & );

    /**
     * Vertical space above and below this image, applet, or object.
     * See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-vspace">
     * vspace attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString vspace() const;

    /**
     * see @ref vspace
     */
    void setVspace( const DOMString & );

    /**
     * Override width. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-width-APPLET">
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

class HTMLObjectElementImpl;

/**
 * Generic embedded object. Note. In principle, all properties on the
 * object element are read-write but in some environments some
 * properties may be read-only once the underlying object is
 * instantiated. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/objects.html#edef-OBJECT">
 * OBJECT element definition </a> in HTML 4.0.
 *
 */
class HTMLObjectElement : public HTMLElement
{
public:
    HTMLObjectElement();
    HTMLObjectElement(const HTMLObjectElement &other);
    HTMLObjectElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLObjectElement(HTMLObjectElementImpl *impl);
public:

    HTMLObjectElement & operator = (const HTMLObjectElement &other);
    HTMLObjectElement & operator = (const Node &other);

    ~HTMLObjectElement();

    /**
     * Returns the <code> FORM </code> element containing this
     * control. Returns null if this control is not within the context
     * of a form.
     *
     */
    HTMLFormElement form() const;

    /**
     * Applet class file. See the <code> code </code> attribute for
     * HTMLAppletElement.
     *
     */
    DOMString code() const;

    /**
     * see @ref code
     */
    void setCode( const DOMString & );

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
     * Space-separated list of archives. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-archive-OBJECT">
     * archive attribute definition </a> in HTML 4.0.
     *
     */
    DOMString archive() const;

    /**
     * see @ref archive
     */
    void setArchive( const DOMString & );

    /**
     * Width of border around the object. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-border">
     * border attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString border() const;

    /**
     * see @ref border
     */
    void setBorder( const DOMString & );

    /**
     * Base URI for <code> classid </code> , <code> data </code> , and
     * <code> archive </code> attributes. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-codebase-OBJECT">
     * codebase attribute definition </a> in HTML 4.0.
     *
     */
    DOMString codeBase() const;

    /**
     * see @ref codeBase
     */
    void setCodeBase( const DOMString & );

    /**
     * Content type for data downloaded via <code> classid </code>
     * attribute. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-codetype">
     * codetype attribute definition </a> in HTML 4.0.
     *
     */
    DOMString codeType() const;

    /**
     * see @ref codeType
     */
    void setCodeType( const DOMString & );

    /**
     * A URI specifying the location of the object's data. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-data">
     * data attribute definition </a> in HTML 4.0.
     *
     */
    DOMString data() const;

    /**
     * see @ref data
     */
    void setData( const DOMString & );

    /**
     * Declare (for future reference), but do not instantiate, this
     * object. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-declare">
     * declare attribute definition </a> in HTML 4.0.
     *
     */
    bool declare() const;

    /**
     * see @ref declare
     */
    void setDeclare( bool );

    /**
     * Override height. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-height-IMG">
     * height attribute definition </a> in HTML 4.0.
     *
     */
    DOMString height() const;

    /**
     * see @ref height
     */
    void setHeight( const DOMString & );

    /**
     * Horizontal space to the left and right of this image, applet,
     * or object. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-hspace">
     * hspace attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString hspace() const;

    /**
     * see @ref hspace
     */
    void setHspace( const DOMString & );

    /**
     * Form control or object name when submitted with a form. See the
     * <a
     * href="http://www.w3.org/TR/REC-html40/interact/forms.html#adef-name-INPUT">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * Message to render while loading the object. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-standby">
     * standby attribute definition </a> in HTML 4.0.
     *
     */
    DOMString standby() const;

    /**
     * see @ref standby
     */
    void setStandby( const DOMString & );

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
     * Content type for data downloaded via <code> data </code>
     * attribute. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-type-OBJECT">
     * type attribute definition </a> in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType( const DOMString & );

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
     * Vertical space above and below this image, applet, or object.
     * See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-vspace">
     * vspace attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString vspace() const;

    /**
     * see @ref vspace
     */
    void setVspace( const DOMString & );

    /**
     * Override width. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-width-IMG">
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

class HTMLParamElementImpl;

/**
 * Parameters fed to the <code> OBJECT </code> element. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/objects.html#edef-PARAM">
 * PARAM element definition </a> in HTML 4.0.
 *
 */
class HTMLParamElement : public HTMLElement
{
public:
    HTMLParamElement();
    HTMLParamElement(const HTMLParamElement &other);
    HTMLParamElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLParamElement(HTMLParamElementImpl *impl);
public:

    HTMLParamElement & operator = (const HTMLParamElement &other);
    HTMLParamElement & operator = (const Node &other);

    ~HTMLParamElement();

    /**
     * The name of a run-time parameter. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-name-PARAM">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * Content type for the <code> value </code> attribute when <code>
     * valuetype </code> has the value "ref". See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-type-PARAM">
     * type attribute definition </a> in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType( const DOMString & );

    /**
     * The value of a run-time parameter. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-value-PARAM">
     * value attribute definition </a> in HTML 4.0.
     *
     */
    DOMString value() const;

    /**
     * see @ref value
     */
    void setValue( const DOMString & );

    /**
     * Information about the meaning of the <code> value </code>
     * attribute value. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/objects.html#adef-valuetype">
     * valuetype attribute definition </a> in HTML 4.0.
     *
     */
    DOMString valueType() const;

    /**
     * see @ref valueType
     */
    void setValueType( const DOMString & );
};

};

#endif
