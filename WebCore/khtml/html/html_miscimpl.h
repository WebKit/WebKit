/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
 */
#ifndef HTML_MISCIMPL_H
#define HTML_MISCIMPL_H

#include "html_elementimpl.h"
#include "misc/shared.h"

namespace DOM {

class Node;
class DOMString;
class HTMLCollection;

class HTMLBaseFontElementImpl : public HTMLElementImpl
{
public:
    HTMLBaseFontElementImpl(DocumentPtr *doc);

    ~HTMLBaseFontElementImpl();

    virtual Id id() const;
};

// -------------------------------------------------------------------------

class HTMLCollectionImpl : public khtml::Shared<HTMLCollectionImpl>
{
    friend class DOM::HTMLCollection;
public:
    enum Type {
        // from HTMLDocument
        DOC_IMAGES,    // all IMG elements in the document
        DOC_APPLETS,   // all OBJECT and APPLET elements
        DOC_FORMS,     // all FORMS
        DOC_LINKS,     // all A _and_ AREA elements with a value for href
        DOC_ANCHORS,      // all A elements with a value for name
        // from HTMLTable, HTMLTableSection, HTMLTableRow
        TABLE_ROWS,    // all rows in this table or tablesection
        TABLE_TBODIES, // all TBODY elements in this table
        TSECTION_ROWS, // all rows elements in this table section
        TR_CELLS,      // all CELLS in this row
        // from SELECT
        SELECT_OPTIONS,
        // from HTMLMap
        MAP_AREAS,
        DOC_ALL,        // "all" elements (IE)
        NODE_CHILDREN   // first-level children (IE)
    };

    HTMLCollectionImpl(NodeImpl *_base, int _tagId);

    virtual ~HTMLCollectionImpl();
    unsigned long length() const;
    // This method is o(n), so you should't use it to iterate over all items. Use firstItem/nextItem instead.
    NodeImpl *item ( unsigned long index ) const;
    virtual NodeImpl *firstItem() const;
    virtual NodeImpl *nextItem() const;

    NodeImpl *namedItem ( const DOMString &name, bool caseSensitive = true ) const;
    // In case of multiple items named the same way
    NodeImpl *nextNamedItem( const DOMString &name ) const;

protected:
    virtual unsigned long calcLength(NodeImpl *current) const;
    virtual NodeImpl *getItem(NodeImpl *current, int index, int &pos) const;
    virtual NodeImpl *getNamedItem(NodeImpl *current, int attr_id, const DOMString &name, bool caseSensitive = true) const;
    virtual NodeImpl *nextNamedItemInternal( const DOMString &name ) const;
    // the base node, the collection refers to
    NodeImpl *base;
    // The collection list the following elements
    int type;

    // ### add optimization, so that a linear loop through the
    // Collection [using item(i)] is O(n) and not O(n^2)!
    // But for that we need to get notified in case of changes in the dom structure...
    //NodeImpl *current;
    //int currentPos;

    // For firstItem()/nextItem()
    mutable NodeImpl *currentItem;
    // For nextNamedItem()
    mutable bool idsDone;
};

// this whole class is just a big hack to find form elements even in
// malformed HTML elements
// the famous <table><tr><form><td> problem
class HTMLFormCollectionImpl : public HTMLCollectionImpl
{
public:
    // base must inherit HTMLGenericFormElementImpl or this won't work
    HTMLFormCollectionImpl(NodeImpl* _base)
        : HTMLCollectionImpl(_base, 0)
    {};
    ~HTMLFormCollectionImpl() { };

    virtual NodeImpl *firstItem() const;
    virtual NodeImpl *nextItem() const;
protected:
    virtual unsigned long calcLength(NodeImpl* current) const;
    virtual NodeImpl *getItem(NodeImpl *current, int index, int& pos) const;
    virtual NodeImpl *getNamedItem(NodeImpl* current, int attr_id, const DOMString& name, bool caseSensitive) const;
    virtual NodeImpl *nextNamedItemInternal( const DOMString &name ) const;
private:
    NodeImpl* getNamedFormItem(int attr_id, const DOMString& name, int duplicateNumber, bool caseSensitive) const;
    NodeImpl* getNamedImgItem(NodeImpl* current, int attr_id, const DOMString& name, int& duplicateNumber, bool caseSensitive) const;
    mutable int currentPos;
};


}; //namespace

#endif
