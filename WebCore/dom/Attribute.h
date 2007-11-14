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

#ifndef Attribute_h
#define Attribute_h

#include "QualifiedName.h"

namespace WebCore {

class Attr;
class CSSStyleDeclaration;
class Element;
class NamedAttrMap;

// this has no counterpart in DOM, purely internal
// representation of the nodevalue of an Attr.
// the actual Attr (Attr) with its value as textchild
// is only allocated on demand by the DOM bindings.
// Any use of Attr inside khtml should be avoided.
class Attribute : public RefCounted<Attribute> {
    friend class Attr;
    friend class Element;
    friend class NamedAttrMap;
public:
    // null value is forbidden !
    Attribute(const QualifiedName& name, const AtomicString& value)
        : m_name(name), m_value(value), m_impl(0)
    {}
    
    Attribute(const AtomicString& name, const AtomicString& value)
        : m_name(nullAtom, name, nullAtom), m_value(value), m_impl(0)
    {}

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
    
    virtual Attribute* clone(bool preserveDecl=true) const;

    // An extension to get the style information for presentational attributes.
    virtual CSSStyleDeclaration* style() const { return 0; }
    
    void setValue(const AtomicString& value) { m_value = value; }
    void setPrefix(const AtomicString& prefix) { m_name.setPrefix(prefix); }

private:
    QualifiedName m_name;
    AtomicString m_value;
    Attr* m_impl;
};

} //namespace

#endif
