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

#include "dom/dom_string.h"

namespace DOM {

class AtomicString {
public:
    static void init();
    
    AtomicString() { }
    AtomicString(const char *s) : m_string(add(s)) { }
    AtomicString(const QChar *s, int length) : m_string(add(s, length)) { }
    AtomicString(const unsigned short* s, int length) : m_string(add((QChar*)s, length)) { }
    AtomicString(DOMStringImpl* imp) :m_string(add(imp)) { }
    explicit AtomicString(const DOMString &s) : m_string(add(s.implementation())) { }
    
    const DOMString& string() const { return m_string; };
    QString qstring() const { return m_string.string(); };
    
    const DOMStringImpl* implementation() { return m_string.implementation(); }
    
    const QChar *unicode() const { return m_string.unicode(); }
    int length() const { return m_string.length(); }
    
    const char *ascii() const { return m_string.string().ascii(); }

    int find(const QChar c, int start = 0) const { return m_string.find(c, start); }
    
    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

    static const AtomicString &null();
    
    friend bool operator==(const AtomicString &, const AtomicString &);
    friend bool operator!=(const AtomicString &, const AtomicString &);
    
    friend bool operator==(const AtomicString &, const char *);
    
    static void remove(DOMStringImpl *);
    
private:
    DOMString m_string;
    
    static bool equal(DOMStringImpl *, const char *);
    static bool equal(DOMStringImpl *, const QChar *, uint length);
    static bool equal(DOMStringImpl *, DOMStringImpl *);
    
    static bool equal(const AtomicString &a, const AtomicString &b)
    { return a.m_string.implementation() == b.m_string.implementation(); }
    static bool equal(const AtomicString &a, const char *b)
    { return equal(a.m_string.implementation(), b); }
    
    static DOMStringImpl *add(const char *);
    static DOMStringImpl *add(const QChar *, int length);
    static DOMStringImpl *add(DOMStringImpl *);
    
    static void insert(DOMStringImpl *);
    
    static void rehash(int newTableSize);
    static void expand();
    static void shrink();
    
    static DOMStringImpl **_table;
    static int _tableSize;
    static int _tableSizeMask;
    static int _keyCount;
};

inline bool operator==(const AtomicString &a, const AtomicString &b)
{ return AtomicString::equal(a, b); }

inline bool operator!=(const AtomicString &a, const AtomicString &b)
{ return !AtomicString::equal(a, b); }

inline bool operator==(const AtomicString &a, const char *b)
{ return AtomicString::equal(a, b); }

// List of property names, passed to a macro so we can do set them up various
// ways without repeating the list.
#define KHTML_ATOMICSTRING_EACH_GLOBAL(macro)

    // Define external global variables for all property names above (and one more).
#if !KHTML_ATOMICSTRING_HIDE_GLOBALS
#define KHTML_ATOMICSTRING_DECLARE_GLOBAL(name) extern const AtomicString name ## PropertyName;
    KHTML_ATOMICSTRING_EACH_GLOBAL(KHTML_ATOMICSTRING_DECLARE_GLOBAL)
    KHTML_ATOMICSTRING_DECLARE_GLOBAL(specialPrototype)
#undef KHTML_ATOMICSTRING_DECLARE_GLOBAL
#endif
        
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
