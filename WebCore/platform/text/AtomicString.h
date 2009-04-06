/*
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef AtomicString_h
#define AtomicString_h

#include "AtomicStringImpl.h"
#include "PlatformString.h"

namespace WebCore {

struct AtomicStringHash;

class AtomicString {
public:
    static void init();

    AtomicString() { }
    AtomicString(const char* s) : m_string(add(s)) { }
    AtomicString(const UChar* s, int length) : m_string(add(s, length)) { }
    AtomicString(const UChar* s) : m_string(add(s)) { }
#if USE(JSC)
    AtomicString(const JSC::UString& s) : m_string(add(s)) { }
    AtomicString(const JSC::Identifier& s) : m_string(add(s)) { }
#endif
    AtomicString(StringImpl* imp) : m_string(add(imp)) { }
    AtomicString(AtomicStringImpl* imp) : m_string(imp) { }
    AtomicString(const String& s) : m_string(add(s.impl())) { }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    AtomicString(WTF::HashTableDeletedValueType) : m_string(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_string.isHashTableDeletedValue(); }

#if USE(JSC)
    static AtomicStringImpl* find(const JSC::Identifier&);
#endif

    operator const String&() const { return m_string; }
    const String& string() const { return m_string; };

#if USE(JSC)
    operator JSC::UString() const;
#endif

    AtomicStringImpl* impl() const { return static_cast<AtomicStringImpl *>(m_string.impl()); }
    
    const UChar* characters() const { return m_string.characters(); }
    unsigned length() const { return m_string.length(); }
    
    UChar operator[](unsigned int i) const { return m_string[i]; }
    
    bool contains(UChar c) const { return m_string.contains(c); }
    bool contains(const char* s, bool caseSensitive = true) const
        { return m_string.contains(s, caseSensitive); }
    bool contains(const String& s, bool caseSensitive = true) const
        { return m_string.contains(s, caseSensitive); }

    int find(UChar c, int start = 0) const { return m_string.find(c, start); }
    int find(const char* s, int start = 0, bool caseSentitive = true) const
        { return m_string.find(s, start, caseSentitive); }
    int find(const String& s, int start = 0, bool caseSentitive = true) const
        { return m_string.find(s, start, caseSentitive); }
    
    bool startsWith(const String& s, bool caseSensitive = true) const
        { return m_string.startsWith(s, caseSensitive); }
    bool endsWith(const String& s, bool caseSensitive = true) const
        { return m_string.endsWith(s, caseSensitive); }
    
    int toInt(bool* ok = 0) const { return m_string.toInt(ok); }
    double toDouble(bool* ok = 0) const { return m_string.toDouble(ok); }
    float toFloat(bool* ok = 0) const { return m_string.toFloat(ok); }
    bool percentage(int& p) const { return m_string.percentage(p); }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

    static void remove(StringImpl*);
    
#if PLATFORM(CF) || (PLATFORM(QT) && PLATFORM(DARWIN))
    AtomicString(CFStringRef s) :  m_string(add(String(s).impl())) { }
    CFStringRef createCFString() const { return m_string.createCFString(); }
#endif    
#ifdef __OBJC__
    AtomicString(NSString* s) : m_string(add(String(s).impl())) { }
    operator NSString*() const { return m_string; }
#endif
#if PLATFORM(QT)
    AtomicString(const QString& s) : m_string(add(String(s).impl())) { }
    operator QString() const { return m_string; }
#endif

private:
    String m_string;
    
    static PassRefPtr<StringImpl> add(const char*);
    static PassRefPtr<StringImpl> add(const UChar*, int length);
    static PassRefPtr<StringImpl> add(const UChar*);
    static PassRefPtr<StringImpl> add(StringImpl*);
#if USE(JSC)
    static PassRefPtr<StringImpl> add(const JSC::UString&);
    static PassRefPtr<StringImpl> add(const JSC::Identifier&);
#endif
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
// These are only usable from the main thread.
#ifndef ATOMICSTRING_HIDE_GLOBALS
    extern const AtomicString nullAtom;
    extern const AtomicString emptyAtom;
    extern const AtomicString textAtom;
    extern const AtomicString commentAtom;
    extern const AtomicString starAtom;
#endif

} // namespace WebCore


namespace WTF {

    // AtomicStringHash is the default hash for AtomicString
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::AtomicString> {
        typedef WebCore::AtomicStringHash Hash;
    };

} // namespace WTF

#endif // AtomicString_h
