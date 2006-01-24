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

#ifndef DOM_AtomicString_h
#define DOM_AtomicString_h

#include "dom/dom_string.h"

namespace DOM {

class AtomicStringImpl : public DOMStringImpl
{
};

class AtomicString {
public:
    static void init();
    
    AtomicString() { }
    AtomicString(const char *s) : m_string(add(s)) { }
    AtomicString(const QChar *s, int length) : m_string(add(s, length)) { }
    AtomicString(const unsigned short* s, int length) : m_string(add((QChar*)s, length)) { }
    AtomicString(const QString& s) : m_string(add(s.unicode(), s.length())) { }
    AtomicString(DOMStringImpl* imp) : m_string(add(imp)) { }
    AtomicString(AtomicStringImpl* imp) : m_string(imp) { }
    explicit AtomicString(const DOMString &s) : m_string(add(s.impl())) { }
    
    operator const DOMString&() const { return m_string; }
    const DOMString& domString() const { return m_string; };
    QString qstring() const { return m_string.qstring(); };
    
    AtomicStringImpl* impl() const { return static_cast<AtomicStringImpl *>(m_string.impl()); }
    
    const QChar *unicode() const { return m_string.unicode(); }
    uint length() const { return m_string.length(); }
    
    const QChar &operator [](unsigned int i) const { return m_string[i]; }
    
    bool contains(QChar c) const { return m_string.contains(c); }
    bool contains(const AtomicString &s, bool caseSentitive = true) const { return (find(s, 0, caseSentitive) != -1); }

    int find(QChar c, int start = 0) const { return m_string.find(c, start); }
    int find(const AtomicString &s, int start = 0, bool caseSentitive = true) const
        { return m_string.find(s.domString(), start, caseSentitive); }
    
    bool startsWith(const AtomicString &s, bool caseSensitive = true) const { return (find(s, 0, caseSensitive) == 0); }
    bool endsWith(const AtomicString &s, bool caseSensitive = true) const
        { return m_string.endsWith(s.domString(), caseSensitive); }
    
    int toInt() const { return m_string.toInt(); }
    bool percentage(int &_percentage) const { return m_string.percentage(_percentage); }
    khtml::Length* toLengthArray(int& len) const { return m_string.toLengthArray(len); }
    khtml::Length* toCoordsArray(int& len) const { return m_string.toCoordsArray(len); }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

    static void remove(DOMStringImpl *);
    
#ifdef __OBJC__
    AtomicString(NSString *s) : m_string(add(DOMString(s).impl())) { }
    operator NSString *() const { return m_string; }
#endif

private:
    DOMString m_string;
    
    static DOMStringImpl *add(const char *);
    static DOMStringImpl *add(const QChar *, int length);
    static DOMStringImpl *add(DOMStringImpl *);
};

inline bool operator==(const AtomicString& a, const AtomicString& b) { return a.impl() == b.impl(); }
bool operator==(const AtomicString& a, const char* b);
inline bool operator==(const AtomicString& a, const DOMString& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const char* a, const AtomicString& b) { return b == a; }
inline bool operator==(const DOMString& a, const AtomicString& b) { return equal(a.impl(), b.impl()); }

inline bool operator!=(const AtomicString& a, const AtomicString& b) { return a.impl() != b.impl(); }
inline bool operator!=(const AtomicString& a, const char *b) { return !(a == b); }
inline bool operator!=(const AtomicString& a, const DOMString& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const char* a, const AtomicString& b) { return !(b == a); }
inline bool operator!=(const DOMString& a, const AtomicString& b) { return !equal(a.impl(), b.impl()); }

inline bool equalIgnoringCase(const AtomicString& a, const AtomicString& b) { return equalIgnoringCase(a.impl(), b.impl()); }
inline bool equalIgnoringCase(const AtomicString& a, const char* b) { return equalIgnoringCase(a.impl(), b); }
inline bool equalIgnoringCase(const AtomicString& a, const DOMString& b) { return equalIgnoringCase(a.impl(), b.impl()); }
inline bool equalIgnoringCase(const char* a, const AtomicString& b) { return equalIgnoringCase(a, b.impl()); }
inline bool equalIgnoringCase(const DOMString& a, const AtomicString& b) { return equalIgnoringCase(a.impl(), b.impl()); }

// Define external global variables for the commonly used atomic strings.
#if !KHTML_ATOMICSTRING_HIDE_GLOBALS
    extern const AtomicString nullAtom;
    extern const AtomicString emptyAtom;
    extern const AtomicString textAtom;
    extern const AtomicString commentAtom;
    extern const AtomicString starAtom;
#endif

}
namespace KXMLCore {

    template<typename T> class StrHash;

    template<> struct StrHash<DOM::AtomicStringImpl*> : public StrHash<DOM::DOMStringImpl*>
    {
    };
}

#endif
