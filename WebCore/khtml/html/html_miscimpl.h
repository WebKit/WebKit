/**
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
 * $Id$
 */
#ifndef HTML_MISCIMPL_H
#define HTML_MISCIMPL_H

#include "html_elementimpl.h"

namespace DOM {

class Node;
class DOMString;
class HTMLCollection;

class HTMLBaseFontElementImpl : public HTMLElementImpl
{
public:
    HTMLBaseFontElementImpl(DocumentPtr *doc);

    ~HTMLBaseFontElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
};

// -------------------------------------------------------------------------

class HTMLCollectionImpl : public DomShared
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
        DOC_ALL        // "all" elements
    };

    HTMLCollectionImpl(NodeImpl *_base, int _tagId);

    virtual ~HTMLCollectionImpl();
    unsigned long length() const;
    NodeImpl *item ( unsigned long index ) const;
    NodeImpl *namedItem ( const DOMString &name ) const;

protected:
    virtual unsigned long calcLength(NodeImpl *current) const;
    virtual NodeImpl *getItem(NodeImpl *current, int index, int &pos) const;
    virtual NodeImpl *getNamedItem( NodeImpl *current, int attr_id,
                            const DOMString &name ) const;
   // the base node, the collection refers to
    NodeImpl *base;
    // The collection list the following elements
    int type;

    // ### add optimization, so that a linear loop through the
    // Collection is O(n) and not O(n^2)!
    //NodeImpl *current;
    //int currentPos;
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

protected:
    virtual unsigned long calcLength(NodeImpl* current) const;
    virtual NodeImpl* getItem(NodeImpl *current, int index, int& pos) const;
    virtual NodeImpl* getNamedItem(NodeImpl* current, int attr_id, const DOMString& name) const;
};


}; //namespace

#endif
