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
#ifndef HTML_HEAD_H
#define HTML_HEAD_H

#include <dom/html_element.h>
#include <dom/css_stylesheet.h>

namespace DOM {

class HTMLBaseElementImpl;
class DOMString;

/**
 * Document base URI. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/links.html#edef-BASE">
 * BASE element definition </a> in HTML 4.0.
 *
 */
class HTMLBaseElement : public HTMLElement
{
public:
    HTMLBaseElement();
    HTMLBaseElement(const HTMLBaseElement &other);
    HTMLBaseElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLBaseElement(HTMLBaseElementImpl *impl);
public:

    HTMLBaseElement & operator = (const HTMLBaseElement &other);
    HTMLBaseElement & operator = (const Node &other);

    ~HTMLBaseElement();

    /**
     * The base URI See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/links.html#adef-href-BASE">
     * href attribute definition </a> in HTML 4.0.
     *
     */
    DOMString href() const;

    /**
     * see @ref href
     */
    void setHref( const DOMString & );

    /**
     * The default target frame. See the <a
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

class HTMLLinkElementImpl;

/**
 * The <code> LINK </code> element specifies a link to an external
 * resource, and defines this document's relationship to that resource
 * (or vice versa). See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/links.html#edef-LINK">
 * LINK element definition </a> in HTML 4.0.
 *
 */
class HTMLLinkElement : public HTMLElement
{
public:
    HTMLLinkElement();
    HTMLLinkElement(const HTMLLinkElement &other);
    HTMLLinkElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLLinkElement(HTMLLinkElementImpl *impl);
public:

    HTMLLinkElement & operator = (const HTMLLinkElement &other);
    HTMLLinkElement & operator = (const Node &other);

    ~HTMLLinkElement();

    /**
     * Enables/disables the link. This is currently only used for
     * style sheet links, and may be used to activate or deactivate
     * style sheets.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * The character encoding of the resource being linked to. See the
     * <a
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
     * Designed for use with one or more target media. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/styles.html#adef-media">
     * media attribute definition </a> in HTML 4.0.
     *
     */
    DOMString media() const;

    /**
     * see @ref media
     */
    void setMedia( const DOMString & );

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
     * Introduced in DOM Level 2
     * This method is from the LinkStyle interface
     *
     * The style sheet.
     */
    StyleSheet sheet() const;

};

// --------------------------------------------------------------------------

class HTMLMetaElementImpl;

/**
 * This contains generic meta-information about the document. See the
 * <a
 * href="http://www.w3.org/TR/REC-html40/struct/global.html#edef-META">
 * META element definition </a> in HTML 4.0.
 *
 */
class HTMLMetaElement : public HTMLElement
{
public:
    HTMLMetaElement();
    HTMLMetaElement(const HTMLMetaElement &other);
    HTMLMetaElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLMetaElement(HTMLMetaElementImpl *impl);
public:

    HTMLMetaElement & operator = (const HTMLMetaElement &other);
    HTMLMetaElement & operator = (const Node &other);

    ~HTMLMetaElement();

    /**
     * Associated information. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-content">
     * content attribute definition </a> in HTML 4.0.
     *
     */
    DOMString content() const;

    /**
     * see @ref content
     */
    void setContent( const DOMString & );

    /**
     * HTTP response header name. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-http-equiv">
     * http-equiv attribute definition </a> in HTML 4.0.
     *
     */
    DOMString httpEquiv() const;

    /**
     * see @ref httpEquiv
     */
    void setHttpEquiv( const DOMString & );

    /**
     * Meta information name. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-name-META">
     * name attribute definition </a> in HTML 4.0.
     *
     */
    DOMString name() const;

    /**
     * see @ref name
     */
    void setName( const DOMString & );

    /**
     * Select form of content. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/global.html#adef-scheme">
     * scheme attribute definition </a> in HTML 4.0.
     *
     */
    DOMString scheme() const;

    /**
     * see @ref scheme
     */
    void setScheme( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLScriptElementImpl;

/**
 * Script statements. See the <a
 * href="http://www.w3.org/TR/REC-html40/interact/scripts.html#edef-SCRIPT">
 * SCRIPT element definition </a> in HTML 4.0.
 *
 */
class HTMLScriptElement : public HTMLElement
{
public:
    HTMLScriptElement();
    HTMLScriptElement(const HTMLScriptElement &other);
    HTMLScriptElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLScriptElement(HTMLScriptElementImpl *impl);
public:

    HTMLScriptElement & operator = (const HTMLScriptElement &other);
    HTMLScriptElement & operator = (const Node &other);

    ~HTMLScriptElement();

    /**
     * The script content of the element.
     *
     */
    DOMString text() const;

    /**
     * see @ref text
     */
    void setText( const DOMString & );

    /**
     * Reserved for future use.
     *
     */
    DOMString htmlFor() const;

    /**
     * see @ref htmlFor
     */
    void setHtmlFor( const DOMString & );

    /**
     * Reserved for future use.
     *
     */
    DOMString event() const;

    /**
     * see @ref event
     */
    void setEvent( const DOMString & );

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
     * Indicates that the user agent can defer processing of the
     * script. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/scripts.html#adef-defer">
     * defer attribute definition </a> in HTML 4.0.
     *
     */
    bool defer() const;

    /**
     * see @ref defer
     */
    void setDefer( bool );

    /**
     * URI designating an external script. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/scripts.html#adef-src-SCRIPT">
     * src attribute definition </a> in HTML 4.0.
     *
     */
    DOMString src() const;

    /**
     * see @ref src
     */
    void setSrc( const DOMString & );

    /**
     * The content type of the script language. See the <a
     * href="http://www.w3.org/TR/REC-html40/interact/scripts.html#adef-type-SCRIPT">
     * type attribute definition </a> in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType( const DOMString & );
};

// --------------------------------------------------------------------------

class HTMLStyleElementImpl;

/**
 * Style information. A more detailed style sheet object model is
 * planned to be defined in a separate document. See the <a
 * href="http://www.w3.org/TR/REC-html40/present/styles.html#edef-STYLE">
 * STYLE element definition </a> in HTML 4.0.
 *
 */
class HTMLStyleElement : public HTMLElement
{
public:
    HTMLStyleElement();
    HTMLStyleElement(const HTMLStyleElement &other);
    HTMLStyleElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLStyleElement(HTMLStyleElementImpl *impl);
public:

    HTMLStyleElement & operator = (const HTMLStyleElement &other);
    HTMLStyleElement & operator = (const Node &other);

    ~HTMLStyleElement();

    /**
     * Enables/disables the style sheet.
     *
     */
    bool disabled() const;

    /**
     * see @ref disabled
     */
    void setDisabled( bool );

    /**
     * Designed for use with one or more target media. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/styles.html#adef-media">
     * media attribute definition </a> in HTML 4.0.
     *
     */
    DOMString media() const;

    /**
     * see @ref media
     */
    void setMedia( const DOMString & );

    /**
     * The style sheet language (Internet media type). See the <a
     * href="http://www.w3.org/TR/REC-html40/present/styles.html#adef-type-STYLE">
     * type attribute definition </a> in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType( const DOMString & );

    /**
     * Introduced in DOM Level 2
     * This method is from the LinkStyle interface
     *
     * The style sheet.
     */
    StyleSheet sheet() const;

};

// --------------------------------------------------------------------------

class HTMLTitleElementImpl;

/**
 * The document title. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/global.html#edef-TITLE">
 * TITLE element definition </a> in HTML 4.0.
 *
 */
class HTMLTitleElement : public HTMLElement
{
public:
    HTMLTitleElement();
    HTMLTitleElement(const HTMLTitleElement &other);
    HTMLTitleElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLTitleElement(HTMLTitleElementImpl *impl);
public:

    HTMLTitleElement & operator = (const HTMLTitleElement &other);
    HTMLTitleElement & operator = (const Node &other);

    ~HTMLTitleElement();

    /**
     * The specified title as a string.
     *
     */
    DOMString text() const;

    /**
     * see @ref text
     */
    void setText( const DOMString & );
};

}; //namespace

#endif
