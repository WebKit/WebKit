/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef Cache_h
#define Cache_h

#include "Allocator.h"
#include "Deallocator.h"
#include "PerThread.h"

namespace bmalloc {

// Per-thread allocation / deallocation cache, backed by a per-process Heap.

class Cache {
public:
    void* operator new(size_t);
    void operator delete(void*, size_t);

    static void* allocate(size_t);
    static void deallocate(void*);

    Cache();

    Allocator& allocator() { return m_allocator; }
    Deallocator& deallocator() { return m_deallocator; }
    
    void scavenge();

private:
    static bool allocateFastCase(size_t, void*&);
    static void* allocateSlowCase(size_t);
    static void* allocateSlowCaseNullCache(size_t);

    static bool deallocateFastCase(void*);
    static void deallocateSlowCase(void*);
    static void deallocateSlowCaseNullCache(void*);

    Deallocator m_deallocator;
    Allocator m_allocator;
};

inline bool Cache::allocateFastCase(size_t size, void*& object)
{
    Cache* cache = PerThread<Cache>::getFastCase();
    if (!cache)
        return false;
    return cache->allocator().allocateFastCase(size, object);
}

inline bool Cache::deallocateFastCase(void* object)
{
    Cache* cache = PerThread<Cache>::getFastCase();
    if (!cache)
        return false;
    return cache->deallocator().deallocateFastCase(object);
}

inline void* Cache::allocate(size_t size)
{
    void* object;
    if (!allocateFastCase(size, object))
        return allocateSlowCase(size);
    return object;
}

inline void Cache::deallocate(void* object)
{
    if (!deallocateFastCase(object))
        deallocateSlowCase(object);
}

} // namespace bmalloc

#endif // Cache_h
