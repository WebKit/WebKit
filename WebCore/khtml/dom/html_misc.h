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
#ifndef HTML_MISC_H
#define HTML_MISC_H

#include <dom/html_element.h>
#include <qvaluelist.h>

namespace DOM {

class HTMLBaseFontElementImpl;
class DOMString;
class HTMLCollectionImpl;

/**
 * Base font. See the <a
 * href="http://www.w3.org/TR/REC-html40/present/graphics.html#edef-BASEFONT">
 * BASEFONT element definition </a> in HTML 4.0. This element is
 * deprecated in HTML 4.0.
 *
 */
class HTMLBaseFontElement : public HTMLElement
{
public:
    HTMLBaseFontElement();
    HTMLBaseFontElement(const HTMLBaseFontElement &other);
    HTMLBaseFontElement(const Node &other) : HTMLElement()
         {(*this)=other;}
protected:
    HTMLBaseFontElement(HTMLBaseFontElementImpl *impl);
public:

    HTMLBaseFontElement & operator = (const HTMLBaseFontElement &other);
    HTMLBaseFontElement & operator = (const Node &other);

    ~HTMLBaseFontElement();

    /**
     * Font color. See the <a href="http://www.w3.org/TR/REC-html40/">
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
     * href="http://www.w3.org/TR/REC-html40/"> face attribute
     * definition </a> in HTML 4.0. This attribute is deprecated in
     * HTML 4.0.
     *
     */
    DOMString face() const;

    /**
     * see @ref face
     */
    void setFace( const DOMString & );

    /**
     * Font size. See the <a
     * href="http://www.w3.org/TR/REC-html40/present/graphics.html#adef-size-BASEFONT">
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

// --------------------------------------------------------------------------

/**
 * An <code> HTMLCollection </code> is a list of nodes. An individual
 * node may be accessed by either ordinal index or the node's <code>
 * name </code> or <code> id </code> attributes. Note: Collections in
 * the HTML DOM are assumed to be live meaning that they are
 * automatically updated when the underlying document is changed.
 *
 */
class HTMLCollection
{
    friend class HTMLDocument;
    friend class HTMLSelectElement;
    friend class HTMLImageElement;
    friend class HTMLMapElement;
    friend class HTMLTableElement;
    friend class HTMLTableRowElement;
    friend class HTMLTableSectionElement;
    friend class HTMLElement;

public:
    HTMLCollection();
    HTMLCollection(const HTMLCollection &other);
    HTMLCollection(NodeImpl *base, int type);
    HTMLCollection(HTMLCollectionImpl *imp);

public:

    HTMLCollection & operator = (const HTMLCollection &other);

    ~HTMLCollection();

    /**
     * This attribute specifies the length or size of the list.
     *
     */
    unsigned long length() const;

    /**
     * This method retrieves a node specified by ordinal index. Nodes
     * are numbered in tree order (depth-first traversal order).
     *
     * @param index The index of the node to be fetched. The index
     * origin is 0.
     *
     * @return The <code> Node </code> at the corresponding position
     * upon success. A value of <code> null </code> is returned if the
     * index is out of range.
     *
     */
    Node item ( unsigned long index ) const;

    /**
     * This method retrieves a <code> Node </code> using a name. It
     * first searches for a <code> Node </code> with a matching <code>
     * id </code> attribute. If it doesn't find one, it then searches
     * for a <code> Node </code> with a matching <code> name </code>
     * attribute, but only on those elements that are allowed a name
     * attribute.
     *
     * @param name The name of the <code> Node </code> to be fetched.
     *
     * @return The <code> Node </code> with a <code> name </code> or
     * <code> id </code> attribute whose value corresponds to the
     * specified string. Upon failure (e.g., no node with this name
     * exists), returns <code> null </code> .
     *
     */
    Node namedItem ( const DOMString &name ) const;

    /**
     * @internal
     * not part of the DOM
     */
    Node base() const;
    HTMLCollectionImpl *handle() const;
    bool isNull() const;
    // Fast iteration
    Node firstItem() const;
    Node nextItem() const;
    // In case of multiple items named the same way
    Node nextNamedItem( const DOMString &name ) const;

    QValueList<Node> namedItems( const DOMString &name ) const;

protected:
    HTMLCollectionImpl *impl;
};

class HTMLFormCollection : public HTMLCollection
{
    friend class HTMLFormElement;
protected:
    HTMLFormCollection(NodeImpl *base);
};

}; //namespace

#endif
