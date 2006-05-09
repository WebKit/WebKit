// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KXMLCORE_FAST_MALLOC_H
#define KXMLCORE_FAST_MALLOC_H

#include <stdlib.h>
#include <new>

namespace WTF {

    void *fastMalloc(size_t n);
    void *fastCalloc(size_t n_elements, size_t element_size);
    void fastFree(void* p);
    void *fastRealloc(void* p, size_t n);

} // namespace WTF

using WTF::fastMalloc;
using WTF::fastCalloc;
using WTF::fastRealloc;
using WTF::fastFree;

#if PLATFORM(GCC) && PLATFORM(DARWIN)
#define KXMLCORE_PRIVATE_INLINE __private_extern__ inline __attribute__((always_inline))
#elif PLATFORM(GCC)
#define KXMLCORE_PRIVATE_INLINE inline __attribute__((always_inline))
#else
#define KXMLCORE_PRIVATE_INLINE inline
#endif

KXMLCORE_PRIVATE_INLINE void* operator new(size_t s) { return fastMalloc(s); }
KXMLCORE_PRIVATE_INLINE void operator delete(void* p) { fastFree(p); }
KXMLCORE_PRIVATE_INLINE void* operator new[](size_t s) { return fastMalloc(s); }
KXMLCORE_PRIVATE_INLINE void operator delete[](void* p) { fastFree(p); }

#endif /* KXMLCORE_FAST_MALLOC_H */
