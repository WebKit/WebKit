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
#ifndef HTML_LIST_H
#define HTML_LIST_H

#include <dom/html_element.h>

namespace DOM {

class HTMLDListElementImpl;
class HTMLUListElementImpl;
class HTMLOListElementImpl;
class HTMLDirectoryElementImpl;
class HTMLMenuElementImpl;
class HTMLLIElementImpl;

class DOMString;

/**
 * Definition list. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/lists.html#edef-DL">
 * DL element definition </a> in HTML 4.0.
 *
 */
class HTMLDListElement : public HTMLElement
{
public:
    HTMLDListElement();
    HTMLDListElement(const HTMLDListElement &other);
    HTMLDListElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLDListElement(HTMLDListElementImpl *impl);
public:

    HTMLDListElement & operator = (const HTMLDListElement &other);
    HTMLDListElement & operator = (const Node &other);

    ~HTMLDListElement();

    /**
     * Reduce spacing between list items. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-compact">
     * compact attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    bool compact() const;

    /**
     * see @ref compact
     */
    void setCompact( bool );
};

// --------------------------------------------------------------------------

/**
 * Directory list. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/lists.html#edef-DIR">
 * DIR element definition </a> in HTML 4.0. This element is deprecated
 * in HTML 4.0.
 *
 */
class HTMLDirectoryElement : public HTMLElement
{
public:
    HTMLDirectoryElement();
    HTMLDirectoryElement(const HTMLDirectoryElement &other);
    HTMLDirectoryElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLDirectoryElement(HTMLDirectoryElementImpl *impl);
public:

    HTMLDirectoryElement & operator = (const HTMLDirectoryElement &other);
    HTMLDirectoryElement & operator = (const Node &other);

    ~HTMLDirectoryElement();

    /**
     * Reduce spacing between list items. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-compact">
     * compact attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    bool compact() const;

    /**
     * see @ref compact
     */
    void setCompact( bool );
};

// --------------------------------------------------------------------------

/**
 * List item. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/lists.html#edef-LI">
 * LI element definition </a> in HTML 4.0.
 *
 */
class HTMLLIElement : public HTMLElement
{
public:
    HTMLLIElement();
    HTMLLIElement(const HTMLLIElement &other);
    HTMLLIElement(const Node &other) : HTMLElement()
         {(*this)=other;}

protected:
    HTMLLIElement(HTMLLIElementImpl *impl);
public:

    HTMLLIElement & operator = (const HTMLLIElement &other);
    HTMLLIElement & operator = (const Node &other);

    ~HTMLLIElement();

    /**
     * List item bullet style. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-type-LI">
     * type attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType( const DOMString & );

    /**
     * Reset sequence number when used in <code> OL </code> See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-value-LI">
     * value attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    long value() const;

    /**
     * see @ref value
     */
    void setValue( long );
};

// --------------------------------------------------------------------------

/**
 * Menu list. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/lists.html#edef-MENU">
 * MENU element definition </a> in HTML 4.0. This element is
 * deprecated in HTML 4.0.
 *
 */
class HTMLMenuElement : public HTMLElement
{
public:
    HTMLMenuElement();
    HTMLMenuElement(const HTMLMenuElement &other);
    HTMLMenuElement(const Node &other) : HTMLElement()
         {(*this)=other;}

protected:
    HTMLMenuElement(HTMLMenuElementImpl *impl);
public:

    HTMLMenuElement & operator = (const HTMLMenuElement &other);
    HTMLMenuElement & operator = (const Node &other);

    ~HTMLMenuElement();

    /**
     * Reduce spacing between list items. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-compact">
     * compact attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    bool compact() const;

    /**
     * see @ref compact
     */
    void setCompact( bool );
};

// --------------------------------------------------------------------------

/**
 * Ordered list. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/lists.html#edef-OL">
 * OL element definition </a> in HTML 4.0.
 *
 */
class HTMLOListElement : public HTMLElement
{
public:
    HTMLOListElement();
    HTMLOListElement(const HTMLOListElement &other);
    HTMLOListElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLOListElement(HTMLOListElementImpl *impl);
public:

    HTMLOListElement & operator = (const HTMLOListElement &other);
    HTMLOListElement & operator = (const Node &other);

    ~HTMLOListElement();

    /**
     * Reduce spacing between list items. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-compact">
     * compact attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    bool compact() const;

    /**
     * see @ref compact
     */
    void setCompact( bool );

    /**
     * Starting sequence number. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-start">
     * start attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    long start() const;

    /**
     * see @ref start
     */
    void setStart( long );

    /**
     * Numbering style. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-type-OL">
     * type attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType( const DOMString & );
};

// --------------------------------------------------------------------------


/**
 * Unordered list. See the <a
 * href="http://www.w3.org/TR/REC-html40/struct/lists.html#edef-UL">
 * UL element definition </a> in HTML 4.0.
 *
 */
class HTMLUListElement : public HTMLElement
{
public:
    HTMLUListElement();
    HTMLUListElement(const HTMLUListElement &other);
    HTMLUListElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLUListElement(HTMLUListElementImpl *impl);
public:

    HTMLUListElement & operator = (const HTMLUListElement &other);
    HTMLUListElement & operator = (const Node &other);

    ~HTMLUListElement();

    /**
     * Reduce spacing between list items. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-compact">
     * compact attribute definition </a> in HTML 4.0. This attribute
     * is deprecated in HTML 4.0.
     *
     */
    bool compact() const;

    /**
     * see @ref compact
     */
    void setCompact( bool );

    /**
     * Bullet style. See the <a
     * href="http://www.w3.org/TR/REC-html40/struct/lists.html#adef-type-UL">
     * type attribute definition </a> in HTML 4.0. This attribute is
     * deprecated in HTML 4.0.
     *
     */
    DOMString type() const;

    /**
     * see @ref type
     */
    void setType( const DOMString & );
};

}; //namespace

#endif
