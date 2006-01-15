/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Apple Computer, Inc.
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

#ifndef DOM_DOMStringImpl_h
#define DOM_DOMStringImpl_h

#include "shared.h"
#include <kxmlcore/RefPtr.h>
#include <qstring.h>

namespace khtml {
    struct Length;
}

namespace DOM {

class DOMStringImpl : public khtml::Shared<DOMStringImpl>
{
private:
    struct WithOneRef { };
    DOMStringImpl(WithOneRef) : l(0), s(0), _hash(0), _inTable(false) { ref(); }

protected:
    DOMStringImpl() : l(0), s(0), _hash(0), _inTable(false) { }
public:
    DOMStringImpl(const QChar*, unsigned len);
    DOMStringImpl(const char*);
    DOMStringImpl(const char*, unsigned len);
    DOMStringImpl(const QString&);
    ~DOMStringImpl();

    unsigned length() const { return l; }
    
    unsigned hash() const { if (_hash == 0) _hash = computeHash(s, l); return _hash; }
    static unsigned computeHash(const QChar*, unsigned len);
    static unsigned computeHash(const char*);
    
    void append(const DOMStringImpl*);
    void insert(const DOMStringImpl*, unsigned pos);
    void truncate(int len);
    void remove(unsigned pos, int len = 1);
    DOMStringImpl* split(unsigned pos);
    DOMStringImpl* copy() const { return new DOMStringImpl(s, l); }

    DOMStringImpl *substring(unsigned pos, unsigned len);

    const QChar& operator[] (int pos) const { return s[pos]; }

    khtml::Length toLength() const;
    
    bool containsOnlyWhitespace() const;
    bool containsOnlyWhitespace(unsigned from, unsigned len) const;
    
    // ignores trailing garbage, unlike QString
    int toInt(bool* ok = 0) const;

    khtml::Length* toCoordsArray(int& len) const;
    khtml::Length* toLengthArray(int& len) const;
    bool isLower() const;
    DOMStringImpl* lower() const;
    DOMStringImpl* upper() const;
    DOMStringImpl* capitalize() const;

    int find(const char *, int index = 0, bool caseSensitive = true) const;
    int find(QChar, int index = 0) const;
    int find(const DOMStringImpl*, int index, bool caseSensitive = true) const;

    bool startsWith(const DOMStringImpl* s, bool caseSensitive = true) const { return find(s, 0, caseSensitive) == 0; }
    bool endsWith(const DOMStringImpl*, bool caseSensitive = true) const;

    // This modifies the string in place if there is only one ref, makes a new string otherwise.
    DOMStringImpl *replace(QChar, QChar);

    static DOMStringImpl* empty();

    // For debugging only, leaks memory.
    const char* ascii() const;

    unsigned l;
    QChar* s;
    mutable unsigned _hash;
    bool _inTable;
};

bool equal(const DOMStringImpl*, const DOMStringImpl*);
bool equal(const DOMStringImpl*, const char*);
bool equal(const char*, const DOMStringImpl*);

bool equalIgnoringCase(const DOMStringImpl*, const DOMStringImpl*);
bool equalIgnoringCase(const DOMStringImpl*, const char*);
bool equalIgnoringCase(const char*, const DOMStringImpl*);

}

namespace KXMLCore {

    template<typename T> class DefaultHash;

    template<> struct DefaultHash<DOM::DOMStringImpl *> {
        static unsigned hash(const DOM::DOMStringImpl *key) { return key->hash(); }
        static bool equal(const DOM::DOMStringImpl *a, const DOM::DOMStringImpl *b)
        {
            if (a == b) return true;
            if (!a || !b) return false;
            
            unsigned aLength = a->l;
            unsigned bLength = b->l;
            if (aLength != bLength)
                return false;
            
            const uint32_t *aChars = reinterpret_cast<const uint32_t *>(a->s);
            const uint32_t *bChars = reinterpret_cast<const uint32_t *>(b->s);
            
            unsigned halfLength = aLength >> 1;
            for (unsigned i = 0; i != halfLength; ++i) {
                if (*aChars++ != *bChars++)
                    return false;
            }
            
            if (aLength & 1 && *reinterpret_cast<const uint16_t *>(aChars) != *reinterpret_cast<const uint16_t *>(bChars))
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
        static unsigned hash(const DOM::DOMStringImpl *str)
        {
            unsigned l = str->l;
            QChar *s = str->s;
            uint32_t hash = PHI;
            uint32_t tmp;
            
            int rem = l & 1;
            l >>= 1;
            
            // Main loop
            for (; l > 0; l--) {
                hash += s[0].lower().unicode();
                tmp = (s[1].lower().unicode() << 11) ^ hash;
                hash = (hash << 16) ^ tmp;
                s += 2;
                hash += hash >> 11;
            }
            
            // Handle end case
            if (rem) {
                hash += s[0].lower().unicode();
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

            unsigned l = length;
            const char* s = str;
            uint32_t hash = PHI;
            uint32_t tmp;
            
            int rem = l & 1;
            l >>= 1;
            
            // Main loop
            for (; l > 0; l--) {
                hash += QChar(s[0]).lower().unicode();
                tmp = (QChar(s[1]).lower().unicode() << 11) ^ hash;
                hash = (hash << 16) ^ tmp;
                s += 2;
                hash += hash >> 11;
            }
            
            // Handle end case
            if (rem) {
                hash += QChar(s[0]).lower().unicode();
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
        
        static bool equal(const DOM::DOMStringImpl *a, const DOM::DOMStringImpl *b)
        {
            if (a == b) return true;
            if (!a || !b) return false;
            unsigned length = a->l;
            if (length != b->l)
                return false;
            const QChar *as = a->s;
            const QChar *bs = b->s;
            for (unsigned i = 0; i != length; ++i)
                if (as[i].lower() != bs[i].lower())
                    return false;
            return true;
        }
    };

    template<> struct DefaultHash<RefPtr<DOM::DOMStringImpl> > {
        static unsigned hash(const RefPtr<DOM::DOMStringImpl>& key) 
        { 
            return DefaultHash<DOM::DOMStringImpl *>::hash(key.get());
        }

        static bool equal(const RefPtr<DOM::DOMStringImpl>& a, const RefPtr<DOM::DOMStringImpl>& b)
        {
            return DefaultHash<DOM::DOMStringImpl *>::equal(a.get(), b.get());
        }
    };
    
    template <typename T> class HashTraits;

    template<> struct HashTraits<RefPtr<DOM::DOMStringImpl> > {
        typedef RefPtr<DOM::DOMStringImpl> TraitType;

        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = true;
        static const TraitType emptyValue() { return TraitType(); }

        static const TraitType _deleted;
        static const TraitType& deletedValue() { return _deleted; }
    };

}

using KXMLCore::CaseInsensitiveHash;

#endif
