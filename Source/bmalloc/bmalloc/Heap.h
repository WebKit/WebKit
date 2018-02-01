/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#include "HeapKind.h"
#include "LargeMap.h"
#include "LineMetadata.h"
#include "List.h"
#include "Map.h"
#include "Mutex.h"
#include "Object.h"
#include "PerHeapKind.h"
#include "PerProcess.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include "Vector.h"
#include <array>
#include <mutex>

namespace bmalloc {

class BeginTag;
class BumpAllocator;
class DebugHeap;
class EndTag;
class Scavenger;

class Heap {
public:
    Heap(HeapKind, std::lock_guard<StaticMutex>&);
    
    static StaticMutex& mutex() { return PerProcess<PerHeapKind<Heap>>::mutex(); }
    
    HeapKind kind() const { return m_kind; }
    
    DebugHeap* debugHeap() { return m_debugHeap; }

    void allocateSmallBumpRanges(std::lock_guard<StaticMutex>&, size_t sizeClass,
        BumpAllocator&, BumpRangeCache&, LineCache&);
    void derefSmallLine(std::lock_guard<StaticMutex>&, Object, LineCache&);
    void deallocateLineCache(std::lock_guard<StaticMutex>&, LineCache&);

    void* allocateLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t);
    void* tryAllocateLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t);
    void deallocateLarge(std::lock_guard<StaticMutex>&, void*);

    bool isLarge(std::lock_guard<StaticMutex>&, void*);
    size_t largeSize(std::lock_guard<StaticMutex>&, void*);
    void shrinkLarge(std::lock_guard<StaticMutex>&, const Range&, size_t);

    void scavenge(std::lock_guard<StaticMutex>&);

private:
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
    
    void initializeLineMetadata();
    void initializePageMetadata();

    void allocateSmallBumpRangesByMetadata(std::lock_guard<StaticMutex>&,
        size_t sizeClass, BumpAllocator&, BumpRangeCache&, LineCache&);
    void allocateSmallBumpRangesByObject(std::lock_guard<StaticMutex>&,
        size_t sizeClass, BumpAllocator&, BumpRangeCache&, LineCache&);

    SmallPage* allocateSmallPage(std::lock_guard<StaticMutex>&, size_t sizeClass, LineCache&);
    void deallocateSmallLine(std::lock_guard<StaticMutex>&, Object, LineCache&);

    void allocateSmallChunk(std::lock_guard<StaticMutex>&, size_t pageClass);
    void deallocateSmallChunk(Chunk*, size_t pageClass);

    void mergeLarge(BeginTag*&, EndTag*&, Range&);
    void mergeLargeLeft(EndTag*&, BeginTag*&, Range&, bool& inVMHeap);
    void mergeLargeRight(EndTag*&, BeginTag*&, Range&, bool& inVMHeap);

    LargeRange splitAndAllocate(LargeRange&, size_t alignment, size_t);

    HeapKind m_kind;
    
    size_t m_vmPageSizePhysical;
    Vector<LineMetadata> m_smallLineMetadata;
    std::array<size_t, sizeClassCount> m_pageClasses;

    LineCache m_lineCache;
    std::array<List<Chunk>, pageClassCount> m_freePages;
    std::array<List<Chunk>, pageClassCount> m_chunkCache;

    Map<void*, size_t, LargeObjectHash> m_largeAllocated;
    LargeMap m_largeFree;

    Map<Chunk*, ObjectType, ChunkHash> m_objectTypes;

    Scavenger* m_scavenger { nullptr };
    DebugHeap* m_debugHeap { nullptr };
};

inline void Heap::allocateSmallBumpRanges(
    std::lock_guard<StaticMutex>& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache,
    LineCache& lineCache)
{
    if (sizeClass < bmalloc::sizeClass(smallLineSize))
        return allocateSmallBumpRangesByMetadata(lock, sizeClass, allocator, rangeCache, lineCache);
    return allocateSmallBumpRangesByObject(lock, sizeClass, allocator, rangeCache, lineCache);
}

inline void Heap::derefSmallLine(std::lock_guard<StaticMutex>& lock, Object object, LineCache& lineCache)
{
    if (!object.line()->deref(lock))
        return;
    deallocateSmallLine(lock, object, lineCache);
}

} // namespace bmalloc

#endif // Heap_h
