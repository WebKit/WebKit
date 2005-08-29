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
#ifndef _DOM_AtomicString_h_
#define _DOM_AtomicString_h_

#include "dom/dom_string.h"

namespace DOM {

class AtomicString {
public:
    static void init();
    
    AtomicString() { }
    AtomicString(const char *s) : m_string(add(s)) { }
    AtomicString(const QChar *s, int length) : m_string(add(s, length)) { }
    AtomicString(const unsigned short* s, int length) : m_string(add((QChar*)s, length)) { }
    AtomicString(const QString& s) : m_string(add(s.unicode(), s.length())) { }
    AtomicString(DOMStringImpl* imp) : m_string(add(imp)) { }
    explicit AtomicString(const DOMString &s) : m_string(add(s.impl())) { }
    
    operator const DOMString&() const { return m_string; }
    const DOMString& domString() const { return m_string; };
    QString qstring() const { return m_string.qstring(); };
    
    DOMStringImpl* impl() const { return m_string.impl(); }
    
    const QChar *unicode() const { return m_string.unicode(); }
    int length() const { return m_string.length(); }
    
    int find(const QChar c, int start = 0) const { return m_string.find(c, start); }
    
    int toInt() const { return m_string.toInt(); }
    bool percentage(int &_percentage) const { return m_string.percentage(_percentage); }
    khtml::Length* toLengthArray(int& len) const { return m_string.toLengthArray(len); }
    khtml::Length* toCoordsArray(int& len) const { return m_string.toCoordsArray(len); }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

    friend bool operator==(const AtomicString &, const AtomicString &);
    friend bool operator!=(const AtomicString &, const AtomicString &);
    
    friend bool operator==(const AtomicString &, const char *);
    friend bool operator!=(const AtomicString &, const char *);
    
    static void remove(DOMStringImpl *);
    
#ifdef __OBJC__
    AtomicString(NSString *);
    operator NSString *() const { return m_string; }
#endif

private:
    DOMString m_string;
    
    static bool equal(DOMStringImpl *, const char *);
    static bool equal(DOMStringImpl *, const QChar *, uint length);
    static bool equal(DOMStringImpl *, DOMStringImpl *);
    
    static bool equal(const AtomicString &a, const AtomicString &b)
    { return a.m_string.impl() == b.m_string.impl(); }
    static bool equal(const AtomicString &a, const char *b);
    
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

inline bool operator!=(const AtomicString &a, const char *b)
{ return !AtomicString::equal(a, b); }

// List of property names, passed to a macro so we can do set them up various
// ways without repeating the list.

// Define external global variables for the commonly used atomic strings.
#if !KHTML_ATOMICSTRING_HIDE_GLOBALS
    extern const AtomicString nullAtom;
    extern const AtomicString emptyAtom;
    extern const AtomicString textAtom;
    extern const AtomicString commentAtom;
    extern const AtomicString starAtom;
#endif
        
bool operator==(const AtomicString &a, const DOMString &b);
inline bool operator!=(const AtomicString &a, const DOMString &b) { return !(a == b); }

bool equalsIgnoreCase(const AtomicString &a, const DOMString &b);
bool equalsIgnoreCase(const AtomicString &a, const AtomicString &b);
}
#endif
