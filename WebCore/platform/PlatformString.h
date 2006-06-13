/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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

#ifndef String_h
#define String_h

// This file would be called String.h, but that conflicts with <string.h>
// on systems without case-sensitive file systems.

#include "StringImpl.h"

namespace WebCore {

/**
 * Currently, strings are explicitly shared (they behave like pointers), meaning
 * that modifications to one instance will also modify all others. If you
 * wish to get a String that is independent, use @ref copy(). In the future,
 * we plan to change this to have immutable copy on write semantics.
 */

class String {
public:
    String() { } // gives null string, distinguishable from an empty string
    String(const UChar*, unsigned length);
    explicit String(const UChar*); // Specifically and explicitly for null terminated UTF-16
    String(const KJS::Identifier&);
    String(const KJS::UString&);
    String(const char*);
    String(const char*, unsigned length);
    String(StringImpl* i) : m_impl(i) { }

    operator KJS::Identifier() const;
    operator KJS::UString() const;

    unsigned length() const;
    const UChar* characters() const;

    UChar operator[](unsigned i) const; // if i >= length(), returns 0
    
    bool contains(UChar c) const { return find(c) != -1; }
    bool contains(const char* str, bool caseSensitive = true) const { return find(str, 0, caseSensitive) != -1; }
    bool contains(const String& str, bool caseSensitive = true) const { return find(str, 0, caseSensitive) != -1; }

    int find(UChar c, int start = 0) const
        { return m_impl ? m_impl->find(c, start) : -1; }
    int find(const char* str, int start = 0, bool caseSensitive = true) const
        { return m_impl ? m_impl->find(str, start, caseSensitive) : -1; }
    int find(const String& str, int start = 0, bool caseSensitive = true) const
        { return m_impl ? m_impl->find(str.impl(), start, caseSensitive) : -1; }
    
    bool startsWith(const String& s, bool caseSensitive = true) const
        { return m_impl ? m_impl->startsWith(s.impl(), caseSensitive) : s.isEmpty(); }
    bool endsWith(const String& s, bool caseSensitive = true) const
        { return m_impl ? m_impl->endsWith(s.impl(), caseSensitive) : s.isEmpty(); }

    void append(const String&);
    void append(char);
    void append(UChar);
    void insert(const String&, unsigned pos);

    String& replace(UChar a, UChar b) { if (m_impl) m_impl = m_impl->replace(a, b); return *this; }
    String& replace(UChar a, const String& b) { if (m_impl) m_impl = m_impl->replace(a, b.impl()); return *this; }
    String& replace(unsigned index, unsigned len, const String& b) { if (m_impl) m_impl = m_impl->replace(index, len, b.impl()); return *this; }

    void truncate(unsigned len);
    void remove(unsigned pos, int len = 1);

    String substring(unsigned pos, unsigned len = UINT_MAX) const;
    String left(unsigned len) const { return substring(0, len); }
    String right(unsigned len) const { return substring(length() - len, len); }

    // Splits the string into two. The original string gets truncated to pos, and the rest is returned.
    String split(unsigned pos);

    // Returns a lowercase/uppercase version of the string
    String lower() const;
    String upper() const;

    // Return the string with case folded for case insensitive comparison.
    String foldCase() const;

    static String number(int);
    static String number(unsigned);
    static String number(long);
    static String number(unsigned long);
    static String number(double);
    
    static String sprintf(const char *, ...)
#if __GNUC__
        __attribute__ ((format (printf, 1, 2)))
#endif
        ;

    int toInt(bool* ok = 0) const;
    Length* toLengthArray(int& len) const;
    Length* toCoordsArray(int& len) const;
    bool percentage(int &_percentage) const;

    String copy() const;

    bool isNull()  const { return !m_impl; }
    bool isEmpty()  const;

    StringImpl* impl() const { return m_impl.get(); }

#if __APPLE__
    String(CFStringRef);
    CFStringRef createCFString() const { return m_impl ? m_impl->createCFString() : CFSTR(""); }
#endif
#if __OBJC__
    String(NSString*);
    operator NSString*() const { if (!m_impl) return @""; return *m_impl; }
#endif

#ifndef NDEBUG
    // For debugging only, leaks memory.
    Vector<char> ascii() const;
#endif

    String(const DeprecatedString&);
    DeprecatedString deprecatedString() const;
    
private:
    RefPtr<StringImpl> m_impl;
};

String operator+(const String&, const String&);
String operator+(const String&, const char*);
String operator+(const char*, const String&);

inline String& operator+=(String& a, const String& b) { a.append(b); return a; }

inline bool operator==(const String& a, const String& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const String& a, const char* b) { return equal(a.impl(), b); }
inline bool operator==(const char* a, const String& b) { return equal(a, b.impl()); }

inline bool operator!=(const String& a, const String& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const String& a, const char* b) { return !equal(a.impl(), b); }
inline bool operator!=(const char* a, const String& b) { return !equal(a, b.impl()); }

inline bool equalIgnoringCase(const String& a, const String& b) { return equalIgnoringCase(a.impl(), b.impl()); }
inline bool equalIgnoringCase(const String& a, const char* b) { return equalIgnoringCase(a.impl(), b); }
inline bool equalIgnoringCase(const char* a, const String& b) { return equalIgnoringCase(a, b.impl()); }

bool operator==(const String& a, const DeprecatedString& b);
inline bool operator==(const DeprecatedString& b, const String& a) { return a == b; }
inline bool operator!=(const String& a, const DeprecatedString& b) { return !(a == b); }
inline bool operator!=(const DeprecatedString& b, const String& a ) { return !(a == b); }

}

namespace WTF {

    // StrHash is the default hash for String
    template<typename T> struct DefaultHash;
    template<typename T> struct StrHash;
    template<> struct DefaultHash<WebCore::String> {
        typedef StrHash<WebCore::String> Hash;
    };

}

#endif
