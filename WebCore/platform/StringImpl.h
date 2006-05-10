/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
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

#ifndef StringImpl_h
#define StringImpl_h

#include "Shared.h"
#include <kjs/identifier.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <unicode/umachine.h>
#include <limits.h>

#if __OBJC__
@class NSString;
#endif

class DeprecatedString;

namespace WebCore {

class AtomicString;
struct UCharBufferTranslator;
struct CStringTranslator;
struct Length;

class StringImpl : public Shared<StringImpl>, Noncopyable {
private:
    struct WithOneRef { };
    StringImpl(WithOneRef) : m_length(0), m_data(0), m_hash(0), m_inTable(false) { ref(); }
    void init(const char*, unsigned len);
    void init(const UChar*, unsigned len);

protected:
    StringImpl() : m_length(0), m_data(0), m_hash(0), m_inTable(false) { }
public:
    StringImpl(const UChar*, unsigned len);
    StringImpl(const char*, unsigned len);
    StringImpl(const char*);
    StringImpl(const KJS::Identifier&);
    StringImpl(const KJS::UString&);
    ~StringImpl();

    const UChar* characters() const { return m_data; }
    unsigned length() const { return m_length; }
    
    unsigned hash() const { if (m_hash == 0) m_hash = computeHash(m_data, m_length); return m_hash; }
    static unsigned computeHash(const UChar*, unsigned len);
    static unsigned computeHash(const char*);
    
    void append(const StringImpl*);
    void insert(const StringImpl*, unsigned pos);
    void truncate(int len);
    void remove(unsigned pos, int len = 1);

    StringImpl* split(unsigned pos);
    StringImpl* copy() const { return new StringImpl(m_data, m_length); }

    StringImpl* substring(unsigned pos, unsigned len = UINT_MAX);

    UChar operator[](int pos) const { return m_data[pos]; }

    Length toLength() const;

    bool containsOnlyWhitespace() const;
    bool containsOnlyWhitespace(unsigned from, unsigned len) const;

    // ignores trailing garbage, unlike DeprecatedString
    int toInt(bool* ok = 0) const;

    Length* toCoordsArray(int& len) const;
    Length* toLengthArray(int& len) const;
    bool isLower() const;
    StringImpl* lower() const;
    StringImpl* upper() const;
    StringImpl* capitalize(UChar previousCharacter) const;
    StringImpl* foldCase() const;

    int find(const char*, int index = 0, bool caseSensitive = true) const;
    int find(UChar, int index = 0) const;
    int find(const StringImpl*, int index, bool caseSensitive = true) const;

    bool startsWith(const StringImpl* m_data, bool caseSensitive = true) const { return find(m_data, 0, caseSensitive) == 0; }
    bool endsWith(const StringImpl*, bool caseSensitive = true) const;

    // Does not modify the string.
    StringImpl* replace(UChar, UChar);
    StringImpl* replace(UChar, const StringImpl*);
    StringImpl* replace(unsigned index, unsigned len, const StringImpl*);

    static StringImpl* empty();

    // For debugging only, leaks memory.
    const char* ascii() const;

#if __APPLE__
    StringImpl(CFStringRef);
    CFStringRef createCFString() const;
#endif
#if __OBJC__
    StringImpl(NSString*);
    operator NSString*() const;
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
};

bool equal(const StringImpl*, const StringImpl*);
bool equal(const StringImpl*, const char*);
inline bool equal(const char* a, const StringImpl* b) { return equal(b, a); }

bool equalIgnoringCase(const StringImpl*, const StringImpl*);
bool equalIgnoringCase(const StringImpl*, const char*);
inline bool equalIgnoringCase(const char* a, const StringImpl* b) { return equalIgnoringCase(b, a); }

}

namespace WTF {

    // StrHash is the default hash for StringImpl* and RefPtr<StringImpl>
    template<typename T> struct DefaultHash;
    template<typename T> struct StrHash;
    template<> struct DefaultHash<WebCore::StringImpl*> {
        typedef StrHash<WebCore::StringImpl*> Hash;
    };
    template<> struct DefaultHash<RefPtr<WebCore::StringImpl> > {
        typedef StrHash<RefPtr<WebCore::StringImpl> > Hash;
    };

}

#endif
