/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef StringImpl_h
#define StringImpl_h

#include <wtf/RefCounted.h>
#include <wtf/unicode/Unicode.h>
#include <kjs/identifier.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <limits.h>

#if PLATFORM(CF)
typedef const struct __CFString * CFStringRef;
#endif

#ifdef __OBJC__
@class NSString;
#endif

namespace WebCore {

class AtomicString;
class DeprecatedString;
struct CStringTranslator;
struct Length;
struct StringHash;
struct UCharBufferTranslator;

class StringImpl : public RefCounted<StringImpl> {
private:
    struct WithOneRef { };
    StringImpl(WithOneRef) : m_length(0), m_data(0), m_hash(0), m_inTable(false), m_hasTerminatingNullCharacter(false) { ref(); }
    void init(const char*, unsigned length);
    void init(const UChar*, unsigned length);

protected:
    StringImpl() : m_length(0), m_data(0), m_hash(0), m_inTable(false), m_hasTerminatingNullCharacter(false) { }
public:
    StringImpl(const UChar*, unsigned length);
    StringImpl(const char*, unsigned length);
    StringImpl(const char*);
    StringImpl(const KJS::Identifier&);
    StringImpl(const KJS::UString&);
    ~StringImpl();

    struct WithTerminatingNullCharacter { };
    StringImpl(const StringImpl&, WithTerminatingNullCharacter);

    static PassRefPtr<StringImpl> createStrippingNull(const UChar*, unsigned len);
    static StringImpl* adopt(Vector<UChar>&);

    const UChar* characters() const { return m_data; }
    unsigned length() const { return m_length; }

    bool hasTerminatingNullCharacter() const { return m_hasTerminatingNullCharacter; }

    unsigned hash() const { if (m_hash == 0) m_hash = computeHash(m_data, m_length); return m_hash; }
    static unsigned computeHash(const UChar*, unsigned len);
    static unsigned computeHash(const char*);
    
    // Makes a deep copy. Helpful only if you need to use a String on another thread.
    // Since StringImpl objects are immutable, there's no other reason to make a copy.
    StringImpl* copy() const { return new StringImpl(m_data, m_length); }

    StringImpl* substring(unsigned pos, unsigned len = UINT_MAX) const;

    UChar operator[](int pos) const { return m_data[pos]; }
    UChar32 characterStartingAt(unsigned) const;

    Length toLength() const;

    bool containsOnlyWhitespace() const;
    bool containsOnlyWhitespace(unsigned from, unsigned len) const;

    int toInt(bool* ok = 0) const; // ignores trailing garbage, unlike DeprecatedString
    int64_t toInt64(bool* ok = 0) const; // ignores trailing garbage, unlike DeprecatedString
    uint64_t toUInt64(bool* ok = 0) const; // ignores trailing garbage, unlike DeprecatedString
    double toDouble(bool* ok = 0) const;
    float toFloat(bool* ok = 0) const;

    Length* toCoordsArray(int& len) const;
    Length* toLengthArray(int& len) const;
    bool isLower() const;
    StringImpl* lower() const;
    StringImpl* upper() const;
    StringImpl* secure(UChar aChar) const;
    StringImpl* capitalize(UChar previousCharacter) const;
    StringImpl* foldCase() const;

    StringImpl* stripWhiteSpace() const;
    StringImpl* simplifyWhiteSpace() const;

    int find(const char*, int index = 0, bool caseSensitive = true) const;
    int find(UChar, int index = 0) const;
    int find(const StringImpl*, int index, bool caseSensitive = true) const;

    int reverseFind(UChar, int index) const;
    int reverseFind(const StringImpl*, int index, bool caseSensitive = true) const;
    
    bool startsWith(const StringImpl* m_data, bool caseSensitive = true) const { return find(m_data, 0, caseSensitive) == 0; }
    bool endsWith(const StringImpl*, bool caseSensitive = true) const;

    // Does not modify the string.
    StringImpl* replace(UChar, UChar) const;
    StringImpl* replace(UChar, const StringImpl*) const;
    StringImpl* replace(const StringImpl*, const StringImpl*) const;
    StringImpl* replace(unsigned index, unsigned len, const StringImpl*) const;

    static StringImpl* empty();

    Vector<char> ascii() const;

    WTF::Unicode::Direction defaultWritingDirection() const;

#if PLATFORM(CF)
    StringImpl(CFStringRef);
    CFStringRef createCFString() const;
#endif
#ifdef __OBJC__
    StringImpl(NSString*);
    operator NSString*() const;
#endif
#if PLATFORM(SYMBIAN)
    StringImpl(const TDesC&);
    TPtrC des() const;
#endif

    StringImpl(const DeprecatedString&);

private:
    unsigned m_length;
    UChar* m_data;

    friend class AtomicString;
    friend struct UCharBufferTranslator;
    friend struct CStringTranslator;
    
    mutable unsigned m_hash;
    bool m_inTable;
    bool m_hasTerminatingNullCharacter;
};

bool equal(const StringImpl*, const StringImpl*);
bool equal(const StringImpl*, const char*);
inline bool equal(const char* a, const StringImpl* b) { return equal(b, a); }

bool equalIgnoringCase(const StringImpl*, const StringImpl*);
bool equalIgnoringCase(const StringImpl*, const char*);
inline bool equalIgnoringCase(const char* a, const StringImpl* b) { return equalIgnoringCase(b, a); }

}

namespace WTF {

    // WebCore::StringHash is the default hash for StringImpl* and RefPtr<StringImpl>
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::StringImpl*> {
        typedef WebCore::StringHash Hash;
    };
    template<> struct DefaultHash<RefPtr<WebCore::StringImpl> > {
        typedef WebCore::StringHash Hash;
    };

}

#endif
