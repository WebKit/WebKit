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

#include "Heap.h"

#include "AvailableMemory.h"
#include "BumpAllocator.h"
#include "Chunk.h"
#include "Environment.h"
#include "Gigacage.h"
#include "DebugHeap.h"
#include "PerProcess.h"
#include "Scavenger.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include "VMHeap.h"
#include "bmalloc.h"
#include <thread>

namespace bmalloc {

Heap::Heap(HeapKind kind, std::lock_guard<StaticMutex>&)
    : m_kind(kind)
    , m_vmPageSizePhysical(vmPageSizePhysical())
    , m_debugHeap(nullptr)
{
    RELEASE_BASSERT(vmPageSizePhysical() >= smallPageSize);
    RELEASE_BASSERT(vmPageSize() >= vmPageSizePhysical());

    initializeLineMetadata();
    initializePageMetadata();
    
    if (PerProcess<Environment>::get()->isDebugHeapEnabled())
        m_debugHeap = PerProcess<DebugHeap>::get();
    else {
        Gigacage::ensureGigacage();
#if GIGACAGE_ENABLED
        if (usingGigacage()) {
            RELEASE_BASSERT(gigacageBasePtr());
            m_largeFree.add(LargeRange(gigacageBasePtr(), GIGACAGE_SIZE, 0));
        }
#endif
    }
    
    m_scavenger = PerProcess<Scavenger>::get();
}

bool Heap::usingGigacage()
{
    return isGigacage(m_kind) && gigacageBasePtr();
}

void* Heap::gigacageBasePtr()
{
    return Gigacage::basePtr(gigacageKind(m_kind));
}

void Heap::initializeLineMetadata()
{
    size_t sizeClassCount = bmalloc::sizeClass(smallLineSize);
    size_t smallLineCount = m_vmPageSizePhysical / smallLineSize;
    m_smallLineMetadata.grow(sizeClassCount * smallLineCount);

    for (size_t sizeClass = 0; sizeClass < sizeClassCount; ++sizeClass) {
        size_t size = objectSize(sizeClass);
        LineMetadata* pageMetadata = &m_smallLineMetadata[sizeClass * smallLineCount];

        size_t object = 0;
        size_t line = 0;
        while (object < m_vmPageSizePhysical) {
            line = object / smallLineSize;
            size_t leftover = object % smallLineSize;

            size_t objectCount;
            size_t remainder;
            divideRoundingUp(smallLineSize - leftover, size, objectCount, remainder);

            pageMetadata[line] = { static_cast<unsigned char>(leftover), static_cast<unsigned char>(objectCount) };

            object += objectCount * size;
        }

        // Don't allow the last object in a page to escape the page.
        if (object > m_vmPageSizePhysical) {
            BASSERT(pageMetadata[line].objectCount);
            --pageMetadata[line].objectCount;
        }
    }
}

void Heap::initializePageMetadata()
{
    auto computePageSize = [&](size_t sizeClass) {
        size_t size = objectSize(sizeClass);
        if (sizeClass < bmalloc::sizeClass(smallLineSize))
            return m_vmPageSizePhysical;

        for (size_t pageSize = m_vmPageSizePhysical;
            pageSize < pageSizeMax;
            pageSize += m_vmPageSizePhysical) {
            RELEASE_BASSERT(pageSize <= chunkSize / 2);
            size_t waste = pageSize % size;
            if (waste <= pageSize / pageSizeWasteFactor)
                return pageSize;
        }
        
        return pageSizeMax;
    };

    for (size_t i = 0; i < sizeClassCount; ++i)
        m_pageClasses[i] = (computePageSize(i) - 1) / smallPageSize;
}

void Heap::scavenge(std::lock_guard<StaticMutex>&)
{
    for (auto& list : m_freePages) {
        for (auto* chunk : list) {
            for (auto* page : chunk->freePages()) {
                if (!page->hasPhysicalPages())
                    continue;

                vmDeallocatePhysicalPagesSloppy(page->begin()->begin(), pageSize(&list - &m_freePages[0]));

                page->setHasPhysicalPages(false);
            }
        }
    }
    
    for (auto& list : m_chunkCache) {
        while (!list.isEmpty())
            deallocateSmallChunk(list.pop(), &list - &m_chunkCache[0]);
    }

    for (auto& range : m_largeFree) {
        vmDeallocatePhysicalPagesSloppy(range.begin(), range.size());

        range.setPhysicalSize(0);
    }
}

void Heap::deallocateLineCache(std::lock_guard<StaticMutex>&, LineCache& lineCache)
{
    for (auto& list : lineCache) {
        while (!list.isEmpty()) {
            size_t sizeClass = &list - &lineCache[0];
            m_lineCache[sizeClass].push(list.popFront());
        }
    }
}

void Heap::allocateSmallChunk(std::lock_guard<StaticMutex>& lock, size_t pageClass)
{
    size_t pageSize = bmalloc::pageSize(pageClass);

    Chunk* chunk = [&]() {
        if (!m_chunkCache[pageClass].isEmpty())
            return m_chunkCache[pageClass].pop();

        void* memory = allocateLarge(lock, chunkSize, chunkSize);

        Chunk* chunk = new (memory) Chunk(pageSize);

        m_objectTypes.set(chunk, ObjectType::Small);

        forEachPage(chunk, pageSize, [&](SmallPage* page) {
            page->setHasPhysicalPages(true);
            page->setHasFreeLines(lock, true);
            chunk->freePages().push(page);
        });
        
        m_scavenger->schedule(0);

        return chunk;
    }();
    
    m_freePages[pageClass].push(chunk);
}

void Heap::deallocateSmallChunk(Chunk* chunk, size_t pageClass)
{
    m_objectTypes.set(chunk, ObjectType::Large);
    
    size_t size = m_largeAllocated.remove(chunk);

    bool hasPhysicalPages = true;
    forEachPage(chunk, pageSize(pageClass), [&](SmallPage* page) {
        if (!page->hasPhysicalPages())
            hasPhysicalPages = false;
    });
    size_t physicalSize = hasPhysicalPages ? size : 0;

    m_largeFree.add(LargeRange(chunk, size, physicalSize));
}

SmallPage* Heap::allocateSmallPage(std::lock_guard<StaticMutex>& lock, size_t sizeClass, LineCache& lineCache)
{
    if (!lineCache[sizeClass].isEmpty())
        return lineCache[sizeClass].popFront();

    if (!m_lineCache[sizeClass].isEmpty())
        return m_lineCache[sizeClass].popFront();

    m_scavenger->didStartGrowing();
    
    SmallPage* page = [&]() {
        size_t pageClass = m_pageClasses[sizeClass];
        
        if (m_freePages[pageClass].isEmpty())
            allocateSmallChunk(lock, pageClass);

        Chunk* chunk = m_freePages[pageClass].tail();

        chunk->ref();

        SmallPage* page = chunk->freePages().pop();
        if (chunk->freePages().isEmpty())
            m_freePages[pageClass].remove(chunk);

        if (!page->hasPhysicalPages()) {
            m_scavenger->scheduleIfUnderMemoryPressure(pageSize(pageClass));

            vmAllocatePhysicalPagesSloppy(page->begin()->begin(), pageSize(pageClass));
            page->setHasPhysicalPages(true);
        }

        return page;
    }();

    page->setSizeClass(sizeClass);
    return page;
}

void Heap::deallocateSmallLine(std::lock_guard<StaticMutex>& lock, Object object, LineCache& lineCache)
{
    BASSERT(!object.line()->refCount(lock));
    SmallPage* page = object.page();
    page->deref(lock);

    if (!page->hasFreeLines(lock)) {
        page->setHasFreeLines(lock, true);
        lineCache[page->sizeClass()].push(page);
    }

    if (page->refCount(lock))
        return;

    size_t sizeClass = page->sizeClass();
    size_t pageClass = m_pageClasses[sizeClass];

    List<SmallPage>::remove(page); // 'page' may be in any thread's line cache.
    
    Chunk* chunk = Chunk::get(page);
    if (chunk->freePages().isEmpty())
        m_freePages[pageClass].push(chunk);
    chunk->freePages().push(page);

    chunk->deref();

    if (!chunk->refCount()) {
        m_freePages[pageClass].remove(chunk);

        if (!m_chunkCache[pageClass].isEmpty())
            deallocateSmallChunk(m_chunkCache[pageClass].pop(), pageClass);

        m_chunkCache[pageClass].push(chunk);
    }
    
    m_scavenger->schedule(pageSize(pageClass));
}

void Heap::allocateSmallBumpRangesByMetadata(
    std::lock_guard<StaticMutex>& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache,
    LineCache& lineCache)
{
    SmallPage* page = allocateSmallPage(lock, sizeClass, lineCache);
    SmallLine* lines = page->begin();
    BASSERT(page->hasFreeLines(lock));
    size_t smallLineCount = m_vmPageSizePhysical / smallLineSize;
    LineMetadata* pageMetadata = &m_smallLineMetadata[sizeClass * smallLineCount];
    
    auto findSmallBumpRange = [&](size_t& lineNumber) {
        for ( ; lineNumber < smallLineCount; ++lineNumber) {
            if (!lines[lineNumber].refCount(lock)) {
                if (pageMetadata[lineNumber].objectCount)
                    return true;
            }
        }
        return false;
    };

    auto allocateSmallBumpRange = [&](size_t& lineNumber) -> BumpRange {
        char* begin = lines[lineNumber].begin() + pageMetadata[lineNumber].startOffset;
        unsigned short objectCount = 0;
        
        for ( ; lineNumber < smallLineCount; ++lineNumber) {
            if (lines[lineNumber].refCount(lock))
                break;

            if (!pageMetadata[lineNumber].objectCount)
                continue;

            objectCount += pageMetadata[lineNumber].objectCount;
            lines[lineNumber].ref(lock, pageMetadata[lineNumber].objectCount);
            page->ref(lock);
        }
        return { begin, objectCount };
    };

    size_t lineNumber = 0;
    for (;;) {
        if (!findSmallBumpRange(lineNumber)) {
            page->setHasFreeLines(lock, false);
            BASSERT(allocator.canAllocate());
            return;
        }

        // In a fragmented page, some free ranges might not fit in the cache.
        if (rangeCache.size() == rangeCache.capacity()) {
            lineCache[sizeClass].push(page);
            BASSERT(allocator.canAllocate());
            return;
        }

        BumpRange bumpRange = allocateSmallBumpRange(lineNumber);
        if (allocator.canAllocate())
            rangeCache.push(bumpRange);
        else
            allocator.refill(bumpRange);
    }
}

void Heap::allocateSmallBumpRangesByObject(
    std::lock_guard<StaticMutex>& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache,
    LineCache& lineCache)
{
    size_t size = allocator.size();
    SmallPage* page = allocateSmallPage(lock, sizeClass, lineCache);
    BASSERT(page->hasFreeLines(lock));

    auto findSmallBumpRange = [&](Object& it, Object& end) {
        for ( ; it + size <= end; it = it + size) {
            if (!it.line()->refCount(lock))
                return true;
        }
        return false;
    };

    auto allocateSmallBumpRange = [&](Object& it, Object& end) -> BumpRange {
        char* begin = it.address();
        unsigned short objectCount = 0;
        for ( ; it + size <= end; it = it + size) {
            if (it.line()->refCount(lock))
                break;

            ++objectCount;
            it.line()->ref(lock);
            it.page()->ref(lock);
        }
        return { begin, objectCount };
    };

    Object it(page->begin()->begin());
    Object end(it + pageSize(m_pageClasses[sizeClass]));
    for (;;) {
        if (!findSmallBumpRange(it, end)) {
            page->setHasFreeLines(lock, false);
            BASSERT(allocator.canAllocate());
            return;
        }

        // In a fragmented page, some free ranges might not fit in the cache.
        if (rangeCache.size() == rangeCache.capacity()) {
            lineCache[sizeClass].push(page);
            BASSERT(allocator.canAllocate());
            return;
        }

        BumpRange bumpRange = allocateSmallBumpRange(it, end);
        if (allocator.canAllocate())
            rangeCache.push(bumpRange);
        else
            allocator.refill(bumpRange);
    }
}

LargeRange Heap::splitAndAllocate(LargeRange& range, size_t alignment, size_t size, AllocationKind allocationKind)
{
    LargeRange prev;
    LargeRange next;

    size_t alignmentMask = alignment - 1;
    if (test(range.begin(), alignmentMask)) {
        size_t prefixSize = roundUpToMultipleOf(alignment, range.begin()) - range.begin();
        std::pair<LargeRange, LargeRange> pair = range.split(prefixSize);
        prev = pair.first;
        range = pair.second;
    }

    if (range.size() - size > size / pageSizeWasteFactor) {
        std::pair<LargeRange, LargeRange> pair = range.split(size);
        range = pair.first;
        next = pair.second;
    }
    
    switch (allocationKind) {
    case AllocationKind::Virtual:
        if (range.physicalSize())
            vmDeallocatePhysicalPagesSloppy(range.begin(), range.size());
        break;
        
    case AllocationKind::Physical:
        if (range.physicalSize() < range.size()) {
            m_scavenger->scheduleIfUnderMemoryPressure(range.size());
            
            vmAllocatePhysicalPagesSloppy(range.begin() + range.physicalSize(), range.size() - range.physicalSize());
            range.setPhysicalSize(range.size());
        }
        break;
    }
    
    if (prev)
        m_largeFree.add(prev);

    if (next)
        m_largeFree.add(next);

    m_objectTypes.set(Chunk::get(range.begin()), ObjectType::Large);

    m_largeAllocated.set(range.begin(), range.size());
    return range;
}

void* Heap::tryAllocateLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t size, AllocationKind allocationKind)
{
    BASSERT(isPowerOfTwo(alignment));
    
    if (m_debugHeap)
        return m_debugHeap->memalignLarge(alignment, size, allocationKind);
    
    m_scavenger->didStartGrowing();
    
    size_t roundedSize = size ? roundUpToMultipleOf(largeAlignment, size) : largeAlignment;
    if (roundedSize < size) // Check for overflow
        return nullptr;
    size = roundedSize;

    size_t roundedAlignment = roundUpToMultipleOf<largeAlignment>(alignment);
    if (roundedAlignment < alignment) // Check for overflow
        return nullptr;
    alignment = roundedAlignment;

    LargeRange range = m_largeFree.remove(alignment, size);
    if (!range) {
        if (usingGigacage())
            return nullptr;

        range = PerProcess<VMHeap>::get()->tryAllocateLargeChunk(alignment, size, allocationKind);
        if (!range)
            return nullptr;
        
        m_largeFree.add(range);

        range = m_largeFree.remove(alignment, size);
    }

    return splitAndAllocate(range, alignment, size, allocationKind).begin();
}

void* Heap::allocateLarge(std::lock_guard<StaticMutex>& lock, size_t alignment, size_t size, AllocationKind allocationKind)
{
    void* result = tryAllocateLarge(lock, alignment, size, allocationKind);
    RELEASE_BASSERT(result);
    return result;
}

bool Heap::isLarge(std::lock_guard<StaticMutex>&, void* object)
{
    return m_objectTypes.get(Object(object).chunk()) == ObjectType::Large;
}

size_t Heap::largeSize(std::lock_guard<StaticMutex>&, void* object)
{
    return m_largeAllocated.get(object);
}

void Heap::shrinkLarge(std::lock_guard<StaticMutex>&, const Range& object, size_t newSize)
{
    BASSERT(object.size() > newSize);

    size_t size = m_largeAllocated.remove(object.begin());
    LargeRange range = LargeRange(object, size);
    splitAndAllocate(range, alignment, newSize, AllocationKind::Physical);

    m_scavenger->schedule(size);
}

void Heap::deallocateLarge(std::lock_guard<StaticMutex>&, void* object, AllocationKind allocationKind)
{
    if (m_debugHeap)
        return m_debugHeap->freeLarge(object, allocationKind);

    size_t size = m_largeAllocated.remove(object);
    m_largeFree.add(LargeRange(object, size, allocationKind == AllocationKind::Physical ? size : 0));
    m_scavenger->schedule(size);
}

} // namespace bmalloc
