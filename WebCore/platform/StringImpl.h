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

#include "DeprecatedString.h"
#include <kxmlcore/Forward.h>
#include <kxmlcore/Noncopyable.h>
#include <limits.h>

#if __OBJC__
@class NSString;
#endif

namespace WebCore {

class AtomicString;
struct QCharBufferTranslator;
struct CStringTranslator;
struct Length;

class StringImpl : public Shared<StringImpl>, Noncopyable
{
private:
    struct WithOneRef { };
    StringImpl(WithOneRef) : m_length(0), m_data(0), m_hash(0), m_inTable(false) { ref(); }
    void initWithChar(const char*, unsigned len);
    void initWithQChar(const QChar*, unsigned len);

protected:
    StringImpl() : m_length(0), m_data(0), m_hash(0), m_inTable(false) { }
public:
    StringImpl(const DeprecatedString&);
    StringImpl(const KJS::Identifier&);
    StringImpl(const KJS::UString&);
    StringImpl(const QChar*, unsigned len);
    StringImpl(const char*);
    StringImpl(const char*, unsigned len);
    ~StringImpl();

    const QChar* unicode() const { return m_data; }
    unsigned length() const { return m_length; }
    
    unsigned hash() const { if (m_hash == 0) m_hash = computeHash(m_data, m_length); return m_hash; }
    static unsigned computeHash(const QChar*, unsigned len);
    static unsigned computeHash(const char*);
    
    void append(const StringImpl*);
    void insert(const StringImpl*, unsigned pos);
    void truncate(int len);
    void remove(unsigned pos, int len = 1);
    
    StringImpl* split(unsigned pos);
    StringImpl* copy() const { return new StringImpl(m_data, m_length); }

    StringImpl* substring(unsigned pos, unsigned len = UINT_MAX);

    const QChar& operator[] (int pos) const { return m_data[pos]; }

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
    StringImpl* capitalize(QChar previous) const;

    int find(const char*, int index = 0, bool caseSensitive = true) const;
    int find(QChar, int index = 0) const;
    int find(const StringImpl*, int index, bool caseSensitive = true) const;

    bool startsWith(const StringImpl* m_data, bool caseSensitive = true) const { return find(m_data, 0, caseSensitive) == 0; }
    bool endsWith(const StringImpl*, bool caseSensitive = true) const;

    // Does not modify the string.
    StringImpl* replace(QChar, QChar);
    StringImpl* replace(QChar a, const StringImpl* b);
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

private:
    unsigned m_length;
    QChar* m_data;

    friend class AtomicString;
    friend struct QCharBufferTranslator;
    friend struct CStringTranslator;
    
    mutable unsigned m_hash;
    bool m_inTable;
};

bool equal(const StringImpl*, const StringImpl*);
bool equal(const StringImpl*, const char*);
bool equal(const char*, const StringImpl*);

bool equalIgnoringCase(const StringImpl*, const StringImpl*);
bool equalIgnoringCase(const StringImpl*, const char*);
bool equalIgnoringCase(const char*, const StringImpl*);

}

namespace KXMLCore {

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
