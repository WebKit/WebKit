/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#ifndef Heap_h
#define Heap_h

#include "BumpRange.h"
#include "Chunk.h"
#include "FailureAction.h"
#include "HeapKind.h"
#include "LargeMap.h"
#include "List.h"
#include "Map.h"
#include "Mutex.h"
#include "Object.h"
#include "PerHeapKind.h"
#include "PerProcess.h"
#include "PhysicalPageMap.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include <array>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace bmalloc {

class BulkDecommit;
class BumpAllocator;
class DebugHeap;
class HeapConstants;
class Scavenger;

class Heap {
public:
    Heap(HeapKind, LockHolder&);
    
    static Mutex& mutex() { return PerProcess<PerHeapKind<Heap>>::mutex(); }
    
    HeapKind kind() const { return m_kind; }
    
    void allocateSmallBumpRanges(UniqueLockHolder&, size_t sizeClass,
        BumpAllocator&, BumpRangeCache&, LineCache&, FailureAction);
    void derefSmallLine(UniqueLockHolder&, Object, LineCache&);
    void deallocateLineCache(UniqueLockHolder&, LineCache&);

    void* allocateLarge(UniqueLockHolder&, size_t alignment, size_t, FailureAction);
    void deallocateLarge(UniqueLockHolder&, void*);

    bool isLarge(UniqueLockHolder&, void*);
    size_t largeSize(UniqueLockHolder&, void*);
    void shrinkLarge(UniqueLockHolder&, const Range&, size_t);

#if BUSE(PARTIAL_SCAVENGE)
    void scavengeToHighWatermark(const LockHolder&, BulkDecommit&);
    void scavenge(const LockHolder&, BulkDecommit&);
#else
    void scavenge(const LockHolder&, BulkDecommit&, size_t& deferredDecommits);
#endif
    void scavenge(const LockHolder&, BulkDecommit&, size_t& freed, size_t goal);

    size_t freeableMemory(const LockHolder&);
    size_t footprint();

    void externalDecommit(void* ptr, size_t);
    void externalDecommit(UniqueLockHolder&, void* ptr, size_t);
    void externalCommit(void* ptr, size_t);
    void externalCommit(UniqueLockHolder&, void* ptr, size_t);

    void markAllLargeAsEligibile(const LockHolder&);

private:
    void decommitLargeRange(const LockHolder&, LargeRange&, BulkDecommit&);

    struct LargeObjectHash {
        static unsigned hash(void* key)
        {
            return static_cast<unsigned>(
                reinterpret_cast<uintptr_t>(key) / smallMax);
        }
    };

    ~Heap() = delete;
    
    bool usingGigacage();
    void* gigacageBasePtr(); // May crash if !usingGigacage().
    size_t gigacageSize();

    void allocateSmallBumpRangesByMetadata(UniqueLockHolder&,
        size_t sizeClass, BumpAllocator&, BumpRangeCache&, LineCache&, FailureAction);
    void allocateSmallBumpRangesByObject(UniqueLockHolder&,
        size_t sizeClass, BumpAllocator&, BumpRangeCache&, LineCache&, FailureAction);

    SmallPage* allocateSmallPage(UniqueLockHolder&, size_t sizeClass, LineCache&, FailureAction);
    void deallocateSmallLine(UniqueLockHolder&, Object, LineCache&);

    void allocateSmallChunk(UniqueLockHolder&, size_t pageClass, FailureAction);
    void deallocateSmallChunk(Chunk*, size_t pageClass);

    LargeRange tryAllocateLargeChunk(size_t alignment, size_t);
    LargeRange splitAndAllocate(UniqueLockHolder&, LargeRange&, size_t alignment, size_t);

    HeapKind m_kind;
    HeapConstants& m_constants;

    bool m_hasPendingDecommits { false };
    std::condition_variable_any m_condition;

    LineCache m_lineCache;
    std::array<List<Chunk>, pageClassCount> m_freePages;
    std::array<List<Chunk>, pageClassCount> m_chunkCache;

    Map<void*, size_t, LargeObjectHash> m_largeAllocated;
    LargeMap m_largeFree;

    Map<Chunk*, ObjectType, ChunkHash> m_objectTypes;

    Scavenger* m_scavenger { nullptr };

    size_t m_footprint { 0 };
    size_t m_freeableMemory { 0 };

#if ENABLE_PHYSICAL_PAGE_MAP 
    PhysicalPageMap m_physicalPageMap;
#endif
    
#if BUSE(PARTIAL_SCAVENGE)
    void* m_highWatermark { nullptr };
#endif
};

inline void Heap::allocateSmallBumpRanges(
    UniqueLockHolder& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache,
    LineCache& lineCache, FailureAction action)
{
    if (sizeClass < bmalloc::sizeClass(smallLineSize))
        return allocateSmallBumpRangesByMetadata(lock, sizeClass, allocator, rangeCache, lineCache, action);
    return allocateSmallBumpRangesByObject(lock, sizeClass, allocator, rangeCache, lineCache, action);
}

inline void Heap::derefSmallLine(UniqueLockHolder& lock, Object object, LineCache& lineCache)
{
    if (!object.line()->deref(lock))
        return;
    deallocateSmallLine(lock, object, lineCache);
}

} // namespace bmalloc

#endif // Heap_h
