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

#ifndef Attribute_h
#define Attribute_h

#include "QualifiedName.h"

namespace WebCore {

class Attr;
class CSSStyleDeclaration;
class Element;
class NamedNodeMap;

// This has no counterpart in DOM.
// It is an internal representation of the node value of an Attr.
// The actual Attr with its value as a Text child is allocated only if needed.
class Attribute : public RefCounted<Attribute> {
    friend class Attr;
    friend class NamedNodeMap;
public:
    static PassRefPtr<Attribute> create(const QualifiedName& name, const AtomicString& value)
    {
        return adoptRef(new Attribute(name, value));
    }
    virtual ~Attribute() { }
    
    const AtomicString& value() const { return m_value; }
    const AtomicString& prefix() const { return m_name.prefix(); }
    const AtomicString& localName() const { return m_name.localName(); }
    const AtomicString& namespaceURI() const { return m_name.namespaceURI(); }
    
    const QualifiedName& name() const { return m_name; }
    
    Attr* attr() const { return m_impl; }
    PassRefPtr<Attr> createAttrIfNeeded(Element*);

    bool isNull() const { return m_value.isNull(); }
    bool isEmpty() const { return m_value.isEmpty(); }
    
    virtual PassRefPtr<Attribute> clone() const;

    // An extension to get the style information for presentational attributes.
    virtual CSSStyleDeclaration* style() const { return 0; }
    
    void setValue(const AtomicString& value) { m_value = value; }
    void setPrefix(const AtomicString& prefix) { m_name.setPrefix(prefix); }

    virtual bool isMappedAttribute() { return false; }

protected:
    Attribute(const QualifiedName& name, const AtomicString& value)
        : m_name(name), m_value(value), m_impl(0)
    {
    }
    Attribute(const AtomicString& name, const AtomicString& value)
        : m_name(nullAtom, name, nullAtom), m_value(value), m_impl(0)
    {
    }

private:
    QualifiedName m_name;
    AtomicString m_value;
    Attr* m_impl;
};

} //namespace

#endif
