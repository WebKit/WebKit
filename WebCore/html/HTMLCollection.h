/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLCollection_h
#define HTMLCollection_h

#include <wtf/RefCounted.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class AtomicString;
class AtomicStringImpl;
class Element;
class Node;
class NodeList;
class String;

class HTMLCollection : public RefCounted<HTMLCollection> {
public:
    enum Type {
        // unnamed collection types cached in the document

        DocImages,    // all <img> elements in the document
        DocApplets,   // all <object> and <applet> elements
        DocEmbeds,    // all <embed> elements
        DocObjects,   // all <object> elements
        DocForms,     // all <form> elements
        DocLinks,     // all <a> _and_ <area> elements with a value for href
        DocAnchors,   // all <a> elements with a value for name
        DocScripts,   // all <script> elements

        DocAll,       // "all" elements (IE)
        NodeChildren, // first-level children (IE)

        // named collection types cached in the document

        WindowNamedItems,
        DocumentNamedItems,

        // types not cached in the document; these are types that can't be used on a document

        TableTBodies, // all <tbody> elements in this table
        TSectionRows, // all row elements in this table section
        TRCells,      // all cells in this row
        SelectOptions,
        MapAreas,

        Other
    };

    static const Type FirstUnnamedDocumentCachedType = DocImages;
    static const unsigned NumUnnamedDocumentCachedTypes = NodeChildren - DocImages + 1;

    static const Type FirstNamedDocumentCachedType = WindowNamedItems;
    static const unsigned NumNamedDocumentCachedTypes = DocumentNamedItems - WindowNamedItems + 1;

    HTMLCollection(PassRefPtr<Node> base, Type);
    virtual ~HTMLCollection();
    
    unsigned length() const;
    
    virtual Node* item(unsigned index) const;
    virtual Node* nextItem() const;

    virtual Node* namedItem(const String& name, bool caseSensitive = true) const;
    virtual Node* nextNamedItem(const String& name) const; // In case of multiple items named the same way

    Node* firstItem() const;

    void namedItems(const AtomicString& name, Vector<RefPtr<Node> >&) const;

    PassRefPtr<NodeList> tags(const String&);

    Node* base() const { return m_base.get(); }
    Type type() const { return m_type; }

    // FIXME: This class name is a bad in two ways. First, "info" is much too vague,
    // and doesn't convey the job of this class (caching collection state).
    // Second, since this is a member of HTMLCollection, it doesn't need "collection"
    // in its name.
    struct CollectionInfo {
        CollectionInfo();
        CollectionInfo(const CollectionInfo&);
        CollectionInfo& operator=(const CollectionInfo& other)
        {
            CollectionInfo tmp(other);    
            swap(tmp);
            return *this;
        }
        ~CollectionInfo();

        void reset();
        void swap(CollectionInfo&);

        typedef HashMap<AtomicStringImpl*, Vector<Element*>*> NodeCacheMap;

        unsigned version;
        Element* current;
        unsigned position;
        unsigned length;
        int elementsArrayPosition;
        NodeCacheMap idCache;
        NodeCacheMap nameCache;
        bool hasLength;
        bool hasNameCache;

    private:
        static void copyCacheMap(NodeCacheMap&, const NodeCacheMap&);
    };

protected:
    HTMLCollection(PassRefPtr<Node> base, Type, CollectionInfo*);

    CollectionInfo* info() const { return m_info; }
    virtual void resetCollectionInfo() const;

    mutable bool m_idsDone; // for nextNamedItem()

private:
    virtual Element* itemAfter(Element*) const;
    virtual unsigned calcLength() const;
    virtual void updateNameCache() const;

    bool checkForNameMatch(Element*, bool checkName, const String &name, bool caseSensitive) const;

    RefPtr<Node> m_base;
    Type m_type;

    mutable CollectionInfo* m_info;
    mutable bool m_ownsInfo;
};

} // namespace

#endif
