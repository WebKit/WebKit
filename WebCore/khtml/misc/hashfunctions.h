/*
 * This file is part of the KDE libraries
 *
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

#ifndef HASHFUNCTIONS_H
#define HASHFUNCTIONS_H

#include "xml/dom_stringimpl.h"

namespace khtml {

template<typename T> class DefaultHash;

template<int size> unsigned pointerHash(void *pointer);

// Thomas Wang's 32 bit mix
// http://www.cris.com/~Ttwang/tech/inthash.htm
template<> inline unsigned pointerHash<4>(void *pointer) 
{
  uint32_t key = reinterpret_cast<uint32_t>(pointer);
  key += ~(key << 15);
  key ^= (key >> 10);
  key += (key << 3);
  key ^= (key >> 6);
  key += ~(key << 11);
  key ^= (key >> 16);
  return key;
}

// Thomas Wang's 64 bit mix
// http://www.cris.com/~Ttwang/tech/inthash.htm
template<> inline unsigned pointerHash<8>(void *pointer)
{
  uint64_t key = reinterpret_cast<uint64_t>(pointer);
  key += ~(key << 32);
  key ^= (key >> 22);
  key += ~(key << 13);
  key ^= (key >> 8);
  key += (key << 3);
  key ^= (key >> 15);
  key += ~(key << 27);
  key ^= (key >> 31);
  return key;
}

template<> struct DefaultHash<void *> {
    static unsigned hash(void *key) { return pointerHash<sizeof(void *)>(key); }
    static bool equal(void *a, void *b) { return a == b; }
};

// pointer identity hash - default for void *, must be requested explicitly for other 
// pointer types; also should work for integer types
template<typename T> struct PointerHash {
    static unsigned hash(T key) { return pointerHash<sizeof(void *)>((void *)key); }
    static bool equal(T a, T b) { return a == b; }
};

template<> struct DefaultHash<DOM::DOMStringImpl *> {
    static unsigned hash(const DOM::DOMStringImpl *key) { return key->hash(); }
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
            if (as[i] != bs[i])
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

} // namespace khtml

#endif // HASHFUNCTIONS_H
