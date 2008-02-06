/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef NamedAttrMap_h
#define NamedAttrMap_h

#include "Attribute.h"
#include "NamedNodeMap.h"

#ifdef __OBJC__
#define id id_AVOID_KEYWORD
#endif

namespace WebCore {

// the map of attributes of an element
class NamedAttrMap : public NamedNodeMap {
    friend class Element;
public:
    NamedAttrMap(Element*);
    virtual ~NamedAttrMap();
    NamedAttrMap(const NamedAttrMap&);
    NamedAttrMap &operator =(const NamedAttrMap &other);

    // DOM methods & attributes for NamedNodeMap

    virtual PassRefPtr<Node> getNamedItem(const String& name) const;
    virtual PassRefPtr<Node> removeNamedItem(const String& name, ExceptionCode&);

    virtual PassRefPtr<Node> getNamedItemNS(const String& namespaceURI, const String& localName) const;
    virtual PassRefPtr<Node> removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode&);

    virtual PassRefPtr<Node> getNamedItem(const QualifiedName& name) const;
    virtual PassRefPtr<Node> removeNamedItem(const QualifiedName& name, ExceptionCode&);
    virtual PassRefPtr<Node> setNamedItem(Node* arg, ExceptionCode&);

    virtual PassRefPtr<Node> item(unsigned index) const;
    unsigned length() const { return len; }

    // Other methods (not part of DOM)
    Attribute* attributeItem(unsigned index) const { return attrs[index]; }
    Attribute* getAttributeItem(const QualifiedName& name) const;
    Attribute* getAttributeItem(const String& name) const;
    virtual bool isReadOnlyNode();

    // used during parsing: only inserts if not already there
    // no error checking!
    void insertAttribute(PassRefPtr<Attribute> newAttribute, bool allowDuplicates) {
        ASSERT(!element);
        if (allowDuplicates || !getAttributeItem(newAttribute->name()))
            addAttribute(newAttribute);
    }

    virtual bool isMappedAttributeMap() const;

    const AtomicString& id() const { return m_id; }
    void setID(const AtomicString& _id) { m_id = _id; }
    
    bool mapsEquivalent(const NamedAttrMap* otherMap) const;

protected:
    // this method is internal, does no error checking at all
    void addAttribute(PassRefPtr<Attribute> newAttribute);
    // this method is internal, does no error checking at all
    void removeAttribute(const QualifiedName& name);
    virtual void clearAttributes();
    void detachFromElement();

    Element *element;
    Attribute **attrs;
    unsigned len;
    AtomicString m_id;
};

} //namespace

#undef id

#endif
