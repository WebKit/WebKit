/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef _DOM_NameImpl_h_
#define _DOM_NameImpl_h_

#include <kjs/identifier.h>

namespace DOM {

typedef KJS::Identifier AtomicString;
typedef KJS::UChar AtomicChar;

class AtomicStringList {
public:
    AtomicStringList() :m_next(0) {}
    AtomicStringList(const AtomicString& str, AtomicStringList* n = 0) :m_string(str), m_next(n) {}
    ~AtomicStringList() { delete m_next; }
    
    AtomicStringList* next() const { return m_next; }
    void setNext(AtomicStringList* n) { m_next = n; }
    AtomicString string() const { return m_string; }
    void setString(const AtomicString& str) { m_string = str; }

    AtomicStringList* clone();
    void clear() { m_string = AtomicString::null(); delete m_next; m_next = 0; }
    
private:
    AtomicString m_string;
    AtomicStringList* m_next;
};

class Name {
public:
    Name() {};
    Name(const AtomicString& namespaceURI, const AtomicString& localName)
        :m_namespaceURI(namespaceURI), m_localName(localName) {}

    AtomicString namespaceURI() const { return m_namespaceURI; }
    AtomicString localName() const { return m_localName; }

private:
    AtomicString m_namespaceURI;
    AtomicString m_localName;
    
    friend bool operator==(const Name& a, const Name& b);
};

inline bool operator==(const Name& a, const Name& b)
{
    return a.m_namespaceURI == b.m_namespaceURI && a.m_localName == b.m_localName;
}

inline bool operator!=(const Name& a, const Name& b)
{
    return !(a == b);
}

bool operator==(const AtomicString &a, const DOMString &b);
inline bool operator!=(const AtomicString &a, const DOMString &b) { return !(a == b); }

bool equalsIgnoreCase(const AtomicString &a, const DOMString &b);
bool equalsIgnoreCase(const AtomicString &a, const AtomicString &b);
}
#endif
