/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
#ifndef _DOM_DOMStringImpl_h_
#define _DOM_DOMStringImpl_h_

#include <qstring.h>

#include "misc/khtmllayout.h"
#include "misc/shared.h"

#define QT_ALLOC_QCHAR_VEC(N) static_cast<QChar*>(fastMalloc(sizeof(QChar)*(N)))
#define QT_DELETE_QCHAR_VEC(P) fastFree(P)

namespace DOM {

class DOMStringImpl : public khtml::Shared<DOMStringImpl>
{
private:
    struct WithOneRef { };
    DOMStringImpl(WithOneRef) { s = 0; l = 0; _hash = 0; _inTable = false; ref(); }

protected:
    DOMStringImpl() { s = 0, l = 0; _hash = 0; _inTable = false; }
public:
    DOMStringImpl(const QChar *str, unsigned int len);
    DOMStringImpl(const char *str);
    DOMStringImpl(const char *str, unsigned int len);
    DOMStringImpl(const QChar &ch);
    ~DOMStringImpl();
    
    unsigned hash() const { if (_hash == 0) _hash = computeHash(s, l); return _hash; }
    static unsigned computeHash(const QChar *, int length);
    static unsigned computeHash(const char *);
    
    void append(DOMStringImpl *str);
    void insert(DOMStringImpl *str, unsigned int pos);
    void truncate(int len);
    void remove(unsigned int pos, int len=1);
    DOMStringImpl *split(unsigned int pos);
    DOMStringImpl *copy() const {
        return new DOMStringImpl(s,l);
    };

    DOMStringImpl *substring(unsigned int pos, unsigned int len);

    const QChar &operator [] (int pos)
	{ return *(s+pos); }

    khtml::Length toLength() const;
    
    bool containsOnlyWhitespace() const;
    bool containsOnlyWhitespace(unsigned int from, unsigned int len) const;
    
    // ignores trailing garbage, unlike QString
    int toInt(bool* ok=0) const;

    khtml::Length* toCoordsArray(int& len) const;
    khtml::Length* toLengthArray(int& len) const;
    bool isLower() const;
    DOMStringImpl *lower() const;
    DOMStringImpl *upper() const;
    DOMStringImpl *capitalize() const;

    // This modifies the string in place if there is only one ref, makes a new string otherwise.
    DOMStringImpl *replace(QChar, QChar);

    static DOMStringImpl* empty();

    // For debugging only, leaks memory.
    const char *ascii() const;

    unsigned int l;
    QChar *s;
    mutable unsigned _hash;
    bool _inTable;
};

}

namespace KXMLCore {

    template<typename T> class DefaultHash;

    template<> struct DefaultHash<DOM::DOMStringImpl *> {
        static unsigned hash(const DOM::DOMStringImpl *key) { return key->hash(); }
        static bool equal(const DOM::DOMStringImpl *a, const DOM::DOMStringImpl *b)
        {
            if (a == b) return true;
            if (!a || !b) return false;
            
            uint aLength = a->l;
            uint bLength = b->l;
            if (aLength != bLength)
                return false;
            
            const uint32_t *aChars = reinterpret_cast<const uint32_t *>(a->s);
            const uint32_t *bChars = reinterpret_cast<const uint32_t *>(b->s);
            
            uint halfLength = aLength >> 1;
            for (uint i = 0; i != halfLength; ++i) {
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
        
        static bool equal(const DOM::DOMStringImpl *a, const DOM::DOMStringImpl *b)
        {
            if (a == b) return true;
            if (!a || !b) return false;
            uint length = a->l;
            if (length != b->l)
                return false;
            const QChar *as = a->s;
            const QChar *bs = b->s;
            for (uint i = 0; i != length; ++i)
                if (as[i].lower() != bs[i].lower())
                    return false;
            return true;
        }
    };

}

using KXMLCore::CaseInsensitiveHash;

#endif
