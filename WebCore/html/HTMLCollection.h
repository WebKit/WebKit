/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#ifndef HTMLCollectionImpl_H
#define HTMLCollectionImpl_H

#include "Shared.h"
#include <kxmlcore/HashMap.h>
#include <kxmlcore/Vector.h>

template <typename T> class DeprecatedValueList;

namespace WebCore {

class Node;
class String;
class AtomicString;
class AtomicStringImpl;

class HTMLCollection : public Shared<HTMLCollection>
{
public:
    enum Type {
        // from JSHTMLDocument
        DOC_IMAGES = 0, // all IMG elements in the document
        DOC_APPLETS,   // all OBJECT and APPLET elements
        DOC_EMBEDS,    // all EMBED elements
        DOC_OBJECTS,   // all OBJECT elements
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
        NODE_CHILDREN,   // first-level children (IE)
        WINDOW_NAMED_ITEMS,
        DOCUMENT_NAMED_ITEMS
    };

    enum {
        NUM_CACHEABLE_TYPES = NODE_CHILDREN + 1
    };

    HTMLCollection(Node *_base, int _type);
    virtual ~HTMLCollection();
    
    unsigned length() const;
    
    virtual Node *item(unsigned index) const;
    virtual Node *firstItem() const;
    virtual Node *nextItem() const;

    virtual Node *namedItem(const String &name, bool caseSensitive = true) const;
    // In case of multiple items named the same way
    virtual Node *nextNamedItem(const String &name) const;

    DeprecatedValueList< RefPtr<Node> > namedItems(const AtomicString &name) const;

    Node *base() { return m_base.get(); }

    struct CollectionInfo {
        CollectionInfo();
        ~CollectionInfo();
        void reset();
        unsigned int version;
        Node *current;
        unsigned int position;
        unsigned int length;
        int elementsArrayPosition;
        HashMap<AtomicStringImpl*, Vector<Node*>*> idCache;
        HashMap<AtomicStringImpl*, Vector<Node*>*> nameCache;
        bool haslength;
        bool hasNameCache;
     };

protected:
    virtual void updateNameCache() const;

    virtual Node *traverseNextItem(Node *start) const;
    bool checkForNameMatch(Node *node, bool checkName, const String &name, bool caseSensitive) const;
    virtual unsigned calcLength() const;
    virtual void resetCollectionInfo() const;
    // the base node, the collection refers to
    RefPtr<Node> m_base;
    // The collection list the following elements
    int type;
    mutable CollectionInfo *info;

    // For nextNamedItem()
    mutable bool idsDone;

    mutable bool m_ownsInfo;
};

} //namespace

#endif
