/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include <wtf/Vector.h>

#ifdef __OBJC__
#define id id_AVOID_KEYWORD
#endif

namespace WebCore {

// the map of attributes of an element
class NamedAttrMap : public NamedNodeMap {
    friend class Element;
protected:
    NamedAttrMap(Element* element) : m_element(element) { }
public:
    static PassRefPtr<NamedAttrMap> create(Element* element) { return adoptRef(new NamedAttrMap(element)); }

    virtual ~NamedAttrMap();

    void setAttributes(const NamedAttrMap&);

    size_t length() const { return m_attributes.size(); }
    Attribute* attributeItem(unsigned index) const { return m_attributes[index].get(); }
    Attribute* getAttributeItem(const QualifiedName&) const;

    void shrinkToLength() { m_attributes.shrinkCapacity(length()); }
    void reserveInitialCapacity(unsigned capacity) { m_attributes.reserveInitialCapacity(capacity); }

    // used during parsing: only inserts if not already there
    // no error checking!
    void insertAttribute(PassRefPtr<Attribute> newAttribute, bool allowDuplicates)
    {
        ASSERT(!m_element);
        if (allowDuplicates || !getAttributeItem(newAttribute->name()))
            addAttribute(newAttribute);
    }

    virtual bool isMappedAttributeMap() const;
    
    const AtomicString& id() const { return m_id; }
    void setID(const AtomicString& newId) { m_id = newId; }

    bool mapsEquivalent(const NamedAttrMap* otherMap) const;

    // These functions do no error checking.
    void addAttribute(PassRefPtr<Attribute>);
    void removeAttribute(const QualifiedName&);

protected:
    virtual void clearAttributes();

    Element* element() const { return m_element; }

private:
    void detachAttributesFromElement();
    void detachFromElement();
    Attribute* getAttributeItem(const String& name, bool shouldIgnoreAttributeCase) const;

    virtual PassRefPtr<Node> getNamedItem(const String& name) const;
    virtual PassRefPtr<Node> removeNamedItem(const String& name, ExceptionCode&);

    virtual PassRefPtr<Node> getNamedItemNS(const String& namespaceURI, const String& localName) const;
    virtual PassRefPtr<Node> removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode&);

    virtual PassRefPtr<Node> getNamedItem(const QualifiedName& name) const;
    virtual PassRefPtr<Node> removeNamedItem(const QualifiedName& name, ExceptionCode&);
    virtual PassRefPtr<Node> setNamedItem(Node*, ExceptionCode&);

    virtual PassRefPtr<Node> item(unsigned index) const;
    virtual size_t virtualLength() const;

    Element* m_element;
    Vector<RefPtr<Attribute> > m_attributes;
    AtomicString m_id;
};

} //namespace

#undef id

#endif
