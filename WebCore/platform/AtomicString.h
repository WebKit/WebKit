/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef AtomicString_h
#define AtomicString_h

#include "AtomicStringImpl.h"
#include "PlatformString.h"

namespace WebCore {

class AtomicString {
public:
    static void init();
    
    AtomicString() { }
    AtomicString(const char* s) : m_string(add(s)) { }
    AtomicString(const QChar* s, int length) : m_string(add(s, length)) { }
    AtomicString(const unsigned short* s, int length) : m_string(add((QChar*)s, length)) { }
    AtomicString(const DeprecatedString& s) : m_string(add(s.unicode(), s.length())) { }
    AtomicString(const KJS::UString& s) : m_string(add(s)) { }
    AtomicString(const KJS::Identifier& s) : m_string(add(s)) { }
    AtomicString(StringImpl* imp) : m_string(add(imp)) { }
    AtomicString(AtomicStringImpl* imp) : m_string(imp) { }
    explicit AtomicString(const String& s) : m_string(add(s.impl())) { }
    
    operator const String&() const { return m_string; }
    const String& domString() const { return m_string; };
    DeprecatedString deprecatedString() const { return m_string.deprecatedString(); };
    
    operator KJS::Identifier() const;
    operator KJS::UString() const;

    AtomicStringImpl* impl() const { return static_cast<AtomicStringImpl *>(m_string.impl()); }
    
    const QChar* unicode() const { return m_string.unicode(); }
    unsigned length() const { return m_string.length(); }
    
    const QChar& operator [](unsigned int i) const { return m_string[i]; }
    
    bool contains(QChar c) const { return m_string.contains(c); }
    bool contains(const AtomicString& s, bool caseSensitive = true) const
        { return m_string.contains(s.domString(), caseSensitive); }

    int find(QChar c, int start = 0) const { return m_string.find(c, start); }
    int find(const AtomicString& s, int start = 0, bool caseSentitive = true) const
        { return m_string.find(s.domString(), start, caseSentitive); }
    
    bool startsWith(const AtomicString& s, bool caseSensitive = true) const
        { return m_string.startsWith(s.domString(), caseSensitive); }
    bool endsWith(const AtomicString& s, bool caseSensitive = true) const
        { return m_string.endsWith(s.domString(), caseSensitive); }
    
    int toInt() const { return m_string.toInt(); }
    bool percentage(int& p) const { return m_string.percentage(p); }
    Length* toLengthArray(int& len) const { return m_string.toLengthArray(len); }
    Length* toCoordsArray(int& len) const { return m_string.toCoordsArray(len); }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

    static void remove(StringImpl*);
    
#ifdef __OBJC__
    AtomicString(NSString* s) : m_string(add(String(s).impl())) { }
    operator NSString*() const { return m_string; }
#endif

private:
    String m_string;
    
    static StringImpl* add(const char*);
    static StringImpl* add(const QChar*, int length);
    static StringImpl* add(StringImpl*);
    static StringImpl* add(const KJS::UString&);
    static StringImpl* add(const KJS::Identifier&);
};

inline bool operator==(const AtomicString& a, const AtomicString& b) { return a.impl() == b.impl(); }
bool operator==(const AtomicString& a, const char* b);
inline bool operator==(const AtomicString& a, const String& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const char* a, const AtomicString& b) { return b == a; }
inline bool operator==(const String& a, const AtomicString& b) { return equal(a.impl(), b.impl()); }

inline bool operator!=(const AtomicString& a, const AtomicString& b) { return a.impl() != b.impl(); }
inline bool operator!=(const AtomicString& a, const char *b) { return !(a == b); }
inline bool operator!=(const AtomicString& a, const String& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const char* a, const AtomicString& b) { return !(b == a); }
inline bool operator!=(const String& a, const AtomicString& b) { return !equal(a.impl(), b.impl()); }

inline bool equalIgnoringCase(const AtomicString& a, const AtomicString& b) { return equalIgnoringCase(a.impl(), b.impl()); }
inline bool equalIgnoringCase(const AtomicString& a, const char* b) { return equalIgnoringCase(a.impl(), b); }
inline bool equalIgnoringCase(const AtomicString& a, const String& b) { return equalIgnoringCase(a.impl(), b.impl()); }
inline bool equalIgnoringCase(const char* a, const AtomicString& b) { return equalIgnoringCase(a, b.impl()); }
inline bool equalIgnoringCase(const String& a, const AtomicString& b) { return equalIgnoringCase(a.impl(), b.impl()); }

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

    template<> struct StrHash<WebCore::AtomicStringImpl*> : public StrHash<WebCore::StringImpl*>
    {
    };

}

#endif
