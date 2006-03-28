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
#include "Shared.h"
#include <kxmlcore/Noncopyable.h>
#include <kxmlcore/RefPtr.h>
#include <limits.h>

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#if __OBJC__
@class NSString;
#endif

namespace WebCore {

class AtomicString;
struct QCharBufferTranslator;
struct CStringTranslator;
struct Length;

class StringImpl : public Shared<StringImpl>
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

private:
    unsigned m_length;
    QChar* m_data;

private:
    friend class AtomicString;
    friend struct QCharBufferTranslator;
    friend struct CStringTranslator;
    
    mutable unsigned m_hash;
    bool m_inTable;

public:
#if __APPLE__
    StringImpl(CFStringRef);
    CFStringRef createCFString() const;
#endif
#if __OBJC__
    StringImpl(NSString*);
    operator NSString*() const;
#endif
};

bool equal(const StringImpl*, const StringImpl*);
bool equal(const StringImpl*, const char*);
bool equal(const char*, const StringImpl*);

bool equalIgnoringCase(const StringImpl*, const StringImpl*);
bool equalIgnoringCase(const StringImpl*, const char*);
bool equalIgnoringCase(const char*, const StringImpl*);

}

namespace KXMLCore {

    template<typename T> class DefaultHash;
    template<typename T> class StrHash;

    template<> struct StrHash<WebCore::StringImpl*> {
        static unsigned hash(const WebCore::StringImpl* key) { return key->hash(); }
        static bool equal(const WebCore::StringImpl* a, const WebCore::StringImpl* b)
        {
            if (a == b) return true;
            if (!a || !b) return false;
            
            unsigned aLength = a->length();
            unsigned bLength = b->length();
            if (aLength != bLength)
                return false;
            
            const uint32_t* aChars = reinterpret_cast<const uint32_t*>(a->unicode());
            const uint32_t* bChars = reinterpret_cast<const uint32_t*>(b->unicode());
            
            unsigned halfLength = aLength >> 1;
            for (unsigned i = 0; i != halfLength; ++i)
                if (*aChars++ != *bChars++)
                    return false;
            
            if (aLength & 1 && *reinterpret_cast<const uint16_t*>(aChars) != *reinterpret_cast<const uint16_t*>(bChars))
                return false;
            
            return true;
        }
    };
    
    class CaseInsensitiveHash {
    private:
        // Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
        static const unsigned PHI = 0x9e3779b9U;
    public:
        // Paul Hsieh's SuperFastHash
        // http://www.azillionmonkeys.com/qed/hash.html
        static unsigned hash(const WebCore::StringImpl* str)
        {
            unsigned m_length = str->length();
            const QChar* m_data = str->unicode();
            uint32_t hash = PHI;
            uint32_t tmp;
            
            int rem = m_length & 1;
            m_length >>= 1;
            
            // Main loop
            for (; m_length > 0; m_length--) {
                hash += m_data[0].lower().unicode();
                tmp = (m_data[1].lower().unicode() << 11) ^ hash;
                hash = (hash << 16) ^ tmp;
                m_data += 2;
                hash += hash >> 11;
            }
            
            // Handle end case
            if (rem) {
                hash += m_data[0].lower().unicode();
                hash ^= hash << 11;
                hash += hash >> 17;
            }
            
            // Force "avalanching" of final 127 bits
            hash ^= hash << 3;
            hash += hash >> 5;
            hash ^= hash << 2;
            hash += hash >> 15;
            hash ^= hash << 10;
            
            // this avoids ever returning a hash code of 0, since that is used to
            // signal "hash not computed yet", using a value that is likely to be
            // effectively the same as 0 when the low bits are masked
            if (hash == 0)
                hash = 0x80000000;
            
            return hash;
        }
        
        static unsigned hash(const char* str, unsigned length)
        {
            // This hash is designed to work on 16-bit chunks at a time. But since the normal case
            // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
            // were 16-bit chunks, which will give matching results.

            unsigned m_length = length;
            const char* m_data = str;
            uint32_t hash = PHI;
            uint32_t tmp;
            
            int rem = m_length & 1;
            m_length >>= 1;
            
            // Main loop
            for (; m_length > 0; m_length--) {
                hash += QChar(m_data[0]).lower().unicode();
                tmp = (QChar(m_data[1]).lower().unicode() << 11) ^ hash;
                hash = (hash << 16) ^ tmp;
                m_data += 2;
                hash += hash >> 11;
            }
            
            // Handle end case
            if (rem) {
                hash += QChar(m_data[0]).lower().unicode();
                hash ^= hash << 11;
                hash += hash >> 17;
            }
            
            // Force "avalanching" of final 127 bits
            hash ^= hash << 3;
            hash += hash >> 5;
            hash ^= hash << 2;
            hash += hash >> 15;
            hash ^= hash << 10;
            
            // this avoids ever returning a hash code of 0, since that is used to
            // signal "hash not computed yet", using a value that is likely to be
            // effectively the same as 0 when the low bits are masked
            if (hash == 0)
                hash = 0x80000000;
            
            return hash;
        }
        
        static bool equal(const WebCore::StringImpl* a, const WebCore::StringImpl* b)
        {
            if (a == b) return true;
            if (!a || !b) return false;
            unsigned length = a->length();
            if (length != b->length())
                return false;
            const QChar* as = a->unicode();
            const QChar* bs = b->unicode();
            for (unsigned i = 0; i != length; ++i)
                if (as[i].lower() != bs[i].lower())
                    return false;
            return true;
        }
    };

    template<> struct StrHash<RefPtr<WebCore::StringImpl> > {
        static unsigned hash(const RefPtr<WebCore::StringImpl>& key) 
        { 
            return StrHash<WebCore::StringImpl*>::hash(key.get());
        }

        static bool equal(const RefPtr<WebCore::StringImpl>& a, const RefPtr<WebCore::StringImpl>& b)
        {
            return StrHash<WebCore::StringImpl*>::equal(a.get(), b.get());
        }
    };

    template<> struct DefaultHash<WebCore::StringImpl*> {
        typedef StrHash<WebCore::StringImpl*> Hash;
    };

    template<> struct DefaultHash<RefPtr<WebCore::StringImpl> > {
        typedef StrHash<RefPtr<WebCore::StringImpl> > Hash;
    };
    
    template <typename T> class HashTraits;

    template<> struct HashTraits<RefPtr<WebCore::StringImpl> > {
        typedef RefPtr<WebCore::StringImpl> TraitType;

        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = true;
        static const TraitType emptyValue() { return TraitType(); }

        static const TraitType _deleted;
        static const TraitType& deletedValue() { return _deleted; }
    };

}

using KXMLCore::CaseInsensitiveHash;

#endif
