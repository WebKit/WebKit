/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1998 Lars Knoll <knoll@kde.org>
    Copyright (C) 2001-2003 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2003 Apple Computer, Inc
    
    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_CacheHelper_H
#define KDOM_CacheHelper_H

#include <qptrdict.h>
#include <q3ptrlist.h>

#include <kdom/cache/KDOMCachedObject.h>

#define KDOM_CACHE_MAX_SEED 47963
#define KDOM_CACHE_MAX_LRU_LISTS 20

// Toggle loader/cache debugging
#define CACHE_DEBUG 1
#define LOADER_DEBUG 1

namespace KDOM
{
    static const int kdom_primes_t[] =
    {
        31,    61,   107,   233,   353,   541,
        821,  1237,  1861,  2797,  4201,  6311,
        9467, 14207, 21313, 31973, 47963, 0
    };

    static inline int cacheNextSeed(int curSize)
    {
        for(int i = 0 ; kdom_primes_t[i] ; i++)
        {
            if(kdom_primes_t[i] > curSize)
                return kdom_primes_t[i];
        }

        return curSize;
    }

    struct LRUList
    {
        LRUList() : head(0), tail(0) { }

        CachedObject *head;
        CachedObject *tail;
    };

    static LRUList s_lruLists[KDOM_CACHE_MAX_LRU_LISTS];

    static inline LRUList *lruListFor(CachedObject *object)
    {
        int accessCount = object->accessCount();
        int queueIndex = 0;

        if(accessCount != 0)
        {
            unsigned int size = object->size();

            // Fast log2 implementation
            unsigned int log2 = 0;
            if(size & (size - 1)) log2 += 1;
            if(size >> 16) log2 += 16, size >>= 16;
            if(size >> 8) log2 += 8, size >>= 8;
            if(size >> 4) log2 += 4, size >>= 4;
            if(size >> 2) log2 += 2, size >>= 2;
            if(size >> 1) log2 += 1;

            queueIndex = (log2 / accessCount) - 1;

            if(queueIndex < 0)
                queueIndex = 0;
            if(queueIndex >= KDOM_CACHE_MAX_LRU_LISTS)
                queueIndex = KDOM_CACHE_MAX_LRU_LISTS - 1;
        }

        return &s_lruLists[queueIndex];
    }
};

#endif

// vim:ts=4:noet
