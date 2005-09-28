// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
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

#ifndef KXMLCORE_HASH_FUNCTIONS_H
#define KXMLCORE_HASH_FUNCTIONS_H

namespace KXMLCore {

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
    
} // namespace KXMLCore

using KXMLCore::DefaultHash;
using KXMLCore::PointerHash;

#endif // KXLMCORE_HASH_FUNCTIONS_H
