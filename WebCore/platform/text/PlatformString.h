/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef PlatformString_h
#define PlatformString_h

// This file would be called String.h, but that conflicts with <string.h>
// on systems without case-sensitive file systems.

#include "StringImpl.h"

#ifdef __OBJC__
#include <objc/objc.h>
#endif

#if USE(JSC)
#include <runtime/Identifier.h>
#else
// runtime/Identifier.h brings in a variety of wtf headers.  We explicitly
// include them in the case of non-JSC builds to keep things consistent.
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#endif

#if PLATFORM(CF) || (PLATFORM(QT) && PLATFORM(DARWIN))
typedef const struct __CFString * CFStringRef;
#endif

#if PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE
#include <QDataStream>
#endif

#if PLATFORM(WX)
class wxString;
#endif

#if PLATFORM(HAIKU)
class BString;
#endif

namespace WebCore {

class CString;
class SharedBuffer;
struct StringHash;

class String {
public:
    String() { } // gives null string, distinguishable from an empty string
    String(const UChar*, unsigned length);
    String(const UChar*); // Specifically for null terminated UTF-16
#if USE(JSC)
    String(const JSC::Identifier&);
    String(const JSC::UString&);
#endif
    String(const char*);
    String(const char*, unsigned length);
    String(StringImpl* i) : m_impl(i) { }
    String(PassRefPtr<StringImpl> i) : m_impl(i) { }
    String(RefPtr<StringImpl> i) : m_impl(i) { }

    void swap(String& o) { m_impl.swap(o.m_impl); }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    String(WTF::HashTableDeletedValueType) : m_impl(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_impl.isHashTableDeletedValue(); }

    static String adopt(StringBuffer& buffer) { return StringImpl::adopt(buffer); }
    static String adopt(Vector<UChar>& vector) { return StringImpl::adopt(vector); }

#if USE(JSC)
    operator JSC::UString() const;
#endif

    unsigned length() const;
    const UChar* characters() const;
    const UChar* charactersWithNullTermination();
    
    UChar operator[](unsigned i) const; // if i >= length(), returns 0    
    UChar32 characterStartingAt(unsigned) const; // Ditto.
    
    bool contains(UChar c) const { return find(c) != -1; }
    bool contains(const char* str, bool caseSensitive = true) const { return find(str, 0, caseSensitive) != -1; }
    bool contains(const String& str, bool caseSensitive = true) const { return find(str, 0, caseSensitive) != -1; }

    int find(UChar c, int start = 0) const
        { return m_impl ? m_impl->find(c, start) : -1; }
    int find(CharacterMatchFunctionPtr matchFunction, int start = 0) const
        { return m_impl ? m_impl->find(matchFunction, start) : -1; }
    int find(const char* str, int start = 0, bool caseSensitive = true) const
        { return m_impl ? m_impl->find(str, start, caseSensitive) : -1; }
    int find(const String& str, int start = 0, bool caseSensitive = true) const
        { return m_impl ? m_impl->find(str.impl(), start, caseSensitive) : -1; }

    int reverseFind(UChar c, int start = -1) const
        { return m_impl ? m_impl->reverseFind(c, start) : -1; }
    int reverseFind(const String& str, int start = -1, bool caseSensitive = true) const
        { return m_impl ? m_impl->reverseFind(str.impl(), start, caseSensitive) : -1; }
    
    bool startsWith(const String& s, bool caseSensitive = true) const
        { return m_impl ? m_impl->startsWith(s.impl(), caseSensitive) : s.isEmpty(); }
    bool endsWith(const String& s, bool caseSensitive = true) const
        { return m_impl ? m_impl->endsWith(s.impl(), caseSensitive) : s.isEmpty(); }

    void append(const String&);
    void append(char);
    void append(UChar);
    void append(const UChar*, unsigned length);
    void insert(const String&, unsigned pos);
    void insert(const UChar*, unsigned length, unsigned pos);

    String& replace(UChar a, UChar b) { if (m_impl) m_impl = m_impl->replace(a, b); return *this; }
    String& replace(UChar a, const String& b) { if (m_impl) m_impl = m_impl->replace(a, b.impl()); return *this; }
    String& replace(const String& a, const String& b) { if (m_impl) m_impl = m_impl->replace(a.impl(), b.impl()); return *this; }
    String& replace(unsigned index, unsigned len, const String& b) { if (m_impl) m_impl = m_impl->replace(index, len, b.impl()); return *this; }

    void truncate(unsigned len);
    void remove(unsigned pos, int len = 1);

    String substring(unsigned pos, unsigned len = UINT_MAX) const;
    String left(unsigned len) const { return substring(0, len); }
    String right(unsigned len) const { return substring(length() - len, len); }

    // Returns a lowercase/uppercase version of the string
    String lower() const;
    String upper() const;

    String stripWhiteSpace() const;
    String simplifyWhiteSpace() const;

    String removeCharacters(CharacterMatchFunctionPtr) const;

    // Return the string with case folded for case insensitive comparison.
    String foldCase() const;

    static String number(short);
    static String number(unsigned short);
    static String number(int);
    static String number(unsigned);
    static String number(long);
    static String number(unsigned long);
    static String number(long long);
    static String number(unsigned long long);
    static String number(double);
    
    static String format(const char *, ...) WTF_ATTRIBUTE_PRINTF(1, 2);

    // Returns an uninitialized string. The characters needs to be written
    // into the buffer returned in data before the returned string is used.
    // Failure to do this will have unpredictable results.
    static String createUninitialized(unsigned length, UChar*& data) { return StringImpl::createUninitialized(length, data); }

    void split(const String& separator, Vector<String>& result) const;
    void split(const String& separator, bool allowEmptyEntries, Vector<String>& result) const;
    void split(UChar separator, Vector<String>& result) const;
    void split(UChar separator, bool allowEmptyEntries, Vector<String>& result) const;

    int toIntStrict(bool* ok = 0, int base = 10) const;
    unsigned toUIntStrict(bool* ok = 0, int base = 10) const;
    int64_t toInt64Strict(bool* ok = 0, int base = 10) const;
    uint64_t toUInt64Strict(bool* ok = 0, int base = 10) const;
    intptr_t toIntPtrStrict(bool* ok = 0, int base = 10) const;

    int toInt(bool* ok = 0) const;
    unsigned toUInt(bool* ok = 0) const;
    int64_t toInt64(bool* ok = 0) const;
    uint64_t toUInt64(bool* ok = 0) const;
    intptr_t toIntPtr(bool* ok = 0) const;
    double toDouble(bool* ok = 0) const;
    float toFloat(bool* ok = 0) const;

    bool percentage(int& percentage) const;

    // Returns a StringImpl suitable for use on another thread.
    String crossThreadString() const;
    // Makes a deep copy. Helpful only if you need to use a String on another thread
    // (use crossThreadString if the method call doesn't need to be threadsafe).
    // Since the underlying StringImpl objects are immutable, there's no other reason
    // to ever prefer copy() over plain old assignment.
    String threadsafeCopy() const;

    bool isNull() const { return !m_impl; }
    bool isEmpty() const;

    StringImpl* impl() const { return m_impl.get(); }

#if PLATFORM(CF) || (PLATFORM(QT) && PLATFORM(DARWIN))
    String(CFStringRef);
    CFStringRef createCFString() const;
#endif

#ifdef __OBJC__
    String(NSString*);
    
    // This conversion maps NULL to "", which loses the meaning of NULL, but we 
    // need this mapping because AppKit crashes when passed nil NSStrings.
    operator NSString*() const { if (!m_impl) return @""; return *m_impl; }
#endif

#if PLATFORM(QT)
    String(const QString&);
    String(const QStringRef&);
    operator QString() const;
#endif

#if PLATFORM(WX)
    String(const wxString&);
    operator wxString() const;
#endif

#if PLATFORM(HAIKU)
    String(const BString&);
    operator BString() const;
#endif

#ifndef NDEBUG
    Vector<char> ascii() const;
#endif

    CString latin1() const;
    CString utf8() const;

    static String fromUTF8(const char*, size_t);
    static String fromUTF8(const char*);

    // Tries to convert the passed in string to UTF-8, but will fall back to Latin-1 if the string is not valid UTF-8.
    static String fromUTF8WithLatin1Fallback(const char*, size_t);
    
    // Determines the writing direction using the Unicode Bidi Algorithm rules P2 and P3.
    WTF::Unicode::Direction defaultWritingDirection() const { return m_impl ? m_impl->defaultWritingDirection() : WTF::Unicode::LeftToRight; }

    // Counts the number of grapheme clusters. A surrogate pair or a sequence
    // of a non-combining character and following combining characters is
    // counted as 1 grapheme cluster.
    unsigned numGraphemeClusters() const;
    // Returns the number of characters which will be less than or equal to
    // the specified grapheme cluster length.
    unsigned numCharactersInGraphemeClusters(unsigned) const;

private:
    RefPtr<StringImpl> m_impl;
};

#if PLATFORM(QT)
QDataStream& operator<<(QDataStream& stream, const String& str);
QDataStream& operator>>(QDataStream& stream, String& str);
#endif

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

inline bool equalIgnoringNullity(const String& a, const String& b) { return equalIgnoringNullity(a.impl(), b.impl()); }

inline bool operator!(const String& str) { return str.isNull(); }

inline void swap(String& a, String& b) { a.swap(b); }

// String Operations

bool charactersAreAllASCII(const UChar*, size_t);

int charactersToIntStrict(const UChar*, size_t, bool* ok = 0, int base = 10);
unsigned charactersToUIntStrict(const UChar*, size_t, bool* ok = 0, int base = 10);
int64_t charactersToInt64Strict(const UChar*, size_t, bool* ok = 0, int base = 10);
uint64_t charactersToUInt64Strict(const UChar*, size_t, bool* ok = 0, int base = 10);
intptr_t charactersToIntPtrStrict(const UChar*, size_t, bool* ok = 0, int base = 10);

int charactersToInt(const UChar*, size_t, bool* ok = 0); // ignores trailing garbage
unsigned charactersToUInt(const UChar*, size_t, bool* ok = 0); // ignores trailing garbage
int64_t charactersToInt64(const UChar*, size_t, bool* ok = 0); // ignores trailing garbage
uint64_t charactersToUInt64(const UChar*, size_t, bool* ok = 0); // ignores trailing garbage
intptr_t charactersToIntPtr(const UChar*, size_t, bool* ok = 0); // ignores trailing garbage

double charactersToDouble(const UChar*, size_t, bool* ok = 0);
float charactersToFloat(const UChar*, size_t, bool* ok = 0);

int find(const UChar*, size_t, UChar, int startPosition = 0);
int reverseFind(const UChar*, size_t, UChar, int startPosition = -1);

#ifdef __OBJC__
// This is for situations in WebKit where the long standing behavior has been
// "nil if empty", so we try to maintain longstanding behavior for the sake of
// entrenched clients
inline NSString* nsStringNilIfEmpty(const String& str) {  return str.isEmpty() ? nil : (NSString*)str; }
#endif

inline bool charactersAreAllASCII(const UChar* characters, size_t length)
{
    UChar ored = 0;
    for (size_t i = 0; i < length; ++i)
        ored |= characters[i];
    return !(ored & 0xFF80);
}

inline int find(const UChar* characters, size_t length, UChar character, int startPosition)
{
    if (startPosition >= static_cast<int>(length))
        return -1;
    for (size_t i = startPosition; i < length; ++i) {
        if (characters[i] == character)
            return static_cast<int>(i);
    }
    return -1;
}

inline int find(const UChar* characters, size_t length, CharacterMatchFunctionPtr matchFunction, int startPosition)
{
    if (startPosition >= static_cast<int>(length))
        return -1;
    for (size_t i = startPosition; i < length; ++i) {
        if (matchFunction(characters[i]))
            return static_cast<int>(i);
    }
    return -1;
}

inline int reverseFind(const UChar* characters, size_t length, UChar character, int startPosition)
{
    if (startPosition >= static_cast<int>(length) || !length)
        return -1;
    if (startPosition < 0)
        startPosition += static_cast<int>(length);
    while (true) {
        if (characters[startPosition] == character)
            return startPosition;
        if (!startPosition)
            return -1;
        startPosition--;
    }
    ASSERT_NOT_REACHED();
    return -1;
}

inline void append(Vector<UChar>& vector, const String& string)
{
    vector.append(string.characters(), string.length());
}

inline void appendNumber(Vector<UChar>& vector, unsigned char number)
{
    int numberLength = number > 99 ? 3 : (number > 9 ? 2 : 1);
    size_t vectorSize = vector.size();
    vector.grow(vectorSize + numberLength);

    switch (numberLength) {
    case 3:
        vector[vectorSize + 2] = number % 10 + '0';
        number /= 10;

    case 2:
        vector[vectorSize + 1] = number % 10 + '0';
        number /= 10;

    case 1:
        vector[vectorSize] = number % 10 + '0';
    }
}



PassRefPtr<SharedBuffer> utf8Buffer(const String&);

} // namespace WebCore

namespace WTF {

    // StringHash is the default hash for String
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::String> {
        typedef WebCore::StringHash Hash;
    };

}

#endif
