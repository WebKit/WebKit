/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#include "BulkDecommit.h"
#include "BumpAllocator.h"
#include "Chunk.h"
#include "CryptoRandom.h"
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
#include <vector>

namespace bmalloc {

static_assert(isPowerOfTwo(smallPageSize), "");

Heap::Heap(HeapKind kind, std::lock_guard<Mutex>&)
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
            uint64_t random[2];
            cryptoRandom(reinterpret_cast<unsigned char*>(random), sizeof(random));
            size_t size = roundDownToMultipleOf(vmPageSize(), gigacageSize() - (random[0] % Gigacage::maximumCageSizeReductionForSlide));
            ptrdiff_t offset = roundDownToMultipleOf(vmPageSize(), random[1] % (gigacageSize() - size));
            void* base = reinterpret_cast<unsigned char*>(gigacageBasePtr()) + offset;
            m_largeFree.add(LargeRange(base, size, 0, 0));
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

size_t Heap::gigacageSize()
{
    return Gigacage::size(gigacageKind(m_kind));
}

void Heap::initializeLineMetadata()
{
    size_t sizeClassCount = bmalloc::sizeClass(smallLineSize);
    size_t smallLineCount = smallPageSize / smallLineSize;
    m_smallLineMetadata.grow(sizeClassCount * smallLineCount);

    for (size_t sizeClass = 0; sizeClass < sizeClassCount; ++sizeClass) {
        size_t size = objectSize(sizeClass);
        LineMetadata* pageMetadata = &m_smallLineMetadata[sizeClass * smallLineCount];

        size_t object = 0;
        size_t line = 0;
        while (object < smallPageSize) {
            line = object / smallLineSize;
            size_t leftover = object % smallLineSize;

            size_t objectCount;
            size_t remainder;
            divideRoundingUp(smallLineSize - leftover, size, objectCount, remainder);

            pageMetadata[line] = { static_cast<unsigned char>(leftover), static_cast<unsigned char>(objectCount) };

            object += objectCount * size;
        }

        // Don't allow the last object in a page to escape the page.
        if (object > smallPageSize) {
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
            return smallPageSize;

        // We want power of 2 pageSizes sizes below physical page size and multiples of physical pages size above that.
        size_t pageSize = smallPageSize;
        for (; pageSize < m_vmPageSizePhysical; pageSize *= 2) {
            RELEASE_BASSERT(pageSize <= chunkSize / 2);
            size_t waste = pageSize % size;
            if (waste <= pageSize / pageSizeWasteFactor)
                return pageSize;
        }

        for (; pageSize < pageSizeMax; pageSize += m_vmPageSizePhysical) {
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

size_t Heap::freeableMemory(std::lock_guard<Mutex>&)
{
    return m_freeableMemory;
}

size_t Heap::footprint()
{
    BASSERT(!m_debugHeap);
    return m_footprint;
}

void Heap::markAllLargeAsEligibile(std::lock_guard<Mutex>&)
{
    m_largeFree.markAllAsEligibile();
    m_hasPendingDecommits = false;
    m_condition.notify_all();
}

void Heap::decommitLargeRange(std::lock_guard<Mutex>&, LargeRange& range, BulkDecommit& decommitter)
{
    m_footprint -= range.totalPhysicalSize();
    m_freeableMemory -= range.totalPhysicalSize();
    decommitter.addLazy(range.begin(), range.size());
    m_hasPendingDecommits = true;
    range.setStartPhysicalSize(0);
    range.setTotalPhysicalSize(0);
    BASSERT(range.isEligibile());
    range.setEligible(false);
#if ENABLE_PHYSICAL_PAGE_MAP 
    m_physicalPageMap.decommit(range.begin(), range.size());
#endif
}

void Heap::scavenge(std::lock_guard<Mutex>& lock, BulkDecommit& decommitter)
{
    for (auto& list : m_freePages) {
        for (auto* chunk : list) {
            for (auto* page : chunk->freePages()) {
                if (!page->hasPhysicalPages())
                    continue;

                size_t pageSize = bmalloc::pageSize(&list - &m_freePages[0]);
                if (pageSize >= m_vmPageSizePhysical) {
                    size_t decommitSize = physicalPageSizeSloppy(page->begin()->begin(), pageSize);
                    m_freeableMemory -= decommitSize;
                    m_footprint -= decommitSize;
                    decommitter.addEager(page->begin()->begin(), pageSize);
                    page->setHasPhysicalPages(false);
#if ENABLE_PHYSICAL_PAGE_MAP
                    m_physicalPageMap.decommit(page->begin()->begin(), pageSize);
#endif
                } else
                    tryDecommitSmallPagesInPhysicalPage(lock, decommitter, page, pageSize);
            }
        }
    }
    
    for (auto& list : m_chunkCache) {
        while (!list.isEmpty())
            deallocateSmallChunk(list.pop(), &list - &m_chunkCache[0]);
    }

    for (LargeRange& range : m_largeFree) {
        m_highWatermark = std::min(m_highWatermark, static_cast<void*>(range.begin()));
        decommitLargeRange(lock, range, decommitter);
    }

    m_freeableMemory = 0;
}

void Heap::scavengeToHighWatermark(std::lock_guard<Mutex>& lock, BulkDecommit& decommitter)
{
    void* newHighWaterMark = nullptr;
    for (LargeRange& range : m_largeFree) {
        if (range.begin() <= m_highWatermark)
            newHighWaterMark = std::min(newHighWaterMark, static_cast<void*>(range.begin()));
        else
            decommitLargeRange(lock, range, decommitter);
    }
    m_highWatermark = newHighWaterMark;
}

void Heap::deallocateLineCache(std::unique_lock<Mutex>&, LineCache& lineCache)
{
    for (auto& list : lineCache) {
        while (!list.isEmpty()) {
            size_t sizeClass = &list - &lineCache[0];
            m_lineCache[sizeClass].push(list.popFront());
        }
    }
}

void Heap::allocateSmallChunk(std::unique_lock<Mutex>& lock, size_t pageClass)
{
    RELEASE_BASSERT(isActiveHeapKind(m_kind));
    
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

        m_freeableMemory += chunkSize;
        
        m_scavenger->schedule(0);

        return chunk;
    }();
    
    m_freePages[pageClass].push(chunk);
}

void Heap::tryDecommitSmallPagesInPhysicalPage(std::lock_guard<Mutex>& lock, BulkDecommit& decommitter, SmallPage* smallPage, size_t pageSize)
{
    Chunk* chunk = Chunk::get(smallPage);

    char* pageBegin = smallPage->begin()->begin();
    char* physicalPageBegin = roundDownToMultipleOf(m_vmPageSizePhysical, pageBegin);

    // The first page in a physical page takes care of decommitting its physical neighbors
    if (pageBegin != physicalPageBegin)
        return;

    size_t beginPageOffset = chunk->offset(physicalPageBegin);
    size_t endPageOffset = beginPageOffset + m_vmPageSizePhysical;

    Object begin(chunk, beginPageOffset);
    Object end(chunk, endPageOffset);

    for (auto it = begin; it + pageSize <= end; it = it + pageSize) {
        if (it.page()->refCount(lock))
            return;
    }

    size_t decommitSize = m_vmPageSizePhysical;
    m_freeableMemory -= decommitSize;
    m_footprint -= decommitSize;

    decommitter.addEager(physicalPageBegin, decommitSize);

    for (auto it = begin; it + pageSize <= end; it = it + pageSize)
        it.page()->setHasPhysicalPages(false);
#if ENABLE_PHYSICAL_PAGE_MAP
    m_physicalPageMap.decommit(smallPage, decommitSize);
#endif
}

void Heap::commitSmallPagesInPhysicalPage(std::unique_lock<Mutex>&, SmallPage* page, size_t pageSize)
{
    Chunk* chunk = Chunk::get(page);

    char* physicalPageBegin = roundDownToMultipleOf(m_vmPageSizePhysical, page->begin()->begin());

    size_t beginPageOffset = chunk->offset(physicalPageBegin);
    size_t endPageOffset = beginPageOffset + m_vmPageSizePhysical;

    Object begin(chunk, beginPageOffset);
    Object end(chunk, endPageOffset);

    m_footprint += m_vmPageSizePhysical;
    vmAllocatePhysicalPagesSloppy(physicalPageBegin, m_vmPageSizePhysical);

    for (auto it = begin; it + pageSize <= end; it = it + pageSize)
        it.page()->setHasPhysicalPages(true);
#if ENABLE_PHYSICAL_PAGE_MAP
    m_physicalPageMap.commit(begin.page(), m_vmPageSizePhysical);
#endif
}

void Heap::deallocateSmallChunk(Chunk* chunk, size_t pageClass)
{
    m_objectTypes.set(chunk, ObjectType::Large);
    
    size_t size = m_largeAllocated.remove(chunk);
    size_t totalPhysicalSize = size;

    size_t accountedInFreeable = 0;

    bool hasPhysicalPages = true;
    forEachPage(chunk, pageSize(pageClass), [&](SmallPage* page) {
        size_t physicalSize = physicalPageSizeSloppy(page->begin()->begin(), pageSize(pageClass));
        if (!page->hasPhysicalPages()) {
            totalPhysicalSize -= physicalSize;
            hasPhysicalPages = false;
        } else
            accountedInFreeable += physicalSize;
    });

    m_freeableMemory -= accountedInFreeable;
    m_freeableMemory += totalPhysicalSize;

    size_t startPhysicalSize = hasPhysicalPages ? size : 0;
    m_largeFree.add(LargeRange(chunk, size, startPhysicalSize, totalPhysicalSize));
}

SmallPage* Heap::allocateSmallPage(std::unique_lock<Mutex>& lock, size_t sizeClass, LineCache& lineCache)
{
    RELEASE_BASSERT(isActiveHeapKind(m_kind));

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

        size_t pageSize = bmalloc::pageSize(pageClass);
        size_t physicalSize = physicalPageSizeSloppy(page->begin()->begin(), pageSize);
        if (page->hasPhysicalPages())
            m_freeableMemory -= physicalSize;
        else {
            m_scavenger->scheduleIfUnderMemoryPressure(pageSize);
            if (pageSize >= m_vmPageSizePhysical) {
                m_footprint += physicalSize;
                vmAllocatePhysicalPagesSloppy(page->begin()->begin(), pageSize);
                page->setHasPhysicalPages(true);
#if ENABLE_PHYSICAL_PAGE_MAP
                m_physicalPageMap.commit(page->begin()->begin(), pageSize);
#endif
            } else
                commitSmallPagesInPhysicalPage(lock, page, pageSize);
        }

        return page;
    }();

    page->setSizeClass(sizeClass);
    return page;
}

void Heap::deallocateSmallLine(std::unique_lock<Mutex>& lock, Object object, LineCache& lineCache)
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

    m_freeableMemory += physicalPageSizeSloppy(page->begin()->begin(), pageSize(pageClass));

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
    std::unique_lock<Mutex>& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache,
    LineCache& lineCache)
{
    RELEASE_BASSERT(isActiveHeapKind(m_kind));

    SmallPage* page = allocateSmallPage(lock, sizeClass, lineCache);
    SmallLine* lines = page->begin();
    BASSERT(page->hasFreeLines(lock));
    size_t smallLineCount = smallPageSize / smallLineSize;
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
    std::unique_lock<Mutex>& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache,
    LineCache& lineCache)
{
    RELEASE_BASSERT(isActiveHeapKind(m_kind));

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

LargeRange Heap::splitAndAllocate(std::unique_lock<Mutex>&, LargeRange& range, size_t alignment, size_t size)
{
    RELEASE_BASSERT(isActiveHeapKind(m_kind));

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
    
    if (range.startPhysicalSize() < range.size()) {
        m_scavenger->scheduleIfUnderMemoryPressure(range.size());
        m_footprint += range.size() - range.totalPhysicalSize();
        vmAllocatePhysicalPagesSloppy(range.begin() + range.startPhysicalSize(), range.size() - range.startPhysicalSize());
        range.setStartPhysicalSize(range.size());
        range.setTotalPhysicalSize(range.size());
#if ENABLE_PHYSICAL_PAGE_MAP 
        m_physicalPageMap.commit(range.begin(), range.size());
#endif
    }
    
    if (prev) {
        m_freeableMemory += prev.totalPhysicalSize();
        m_largeFree.add(prev);
    }

    if (next) {
        m_freeableMemory += next.totalPhysicalSize();
        m_largeFree.add(next);
    }

    m_objectTypes.set(Chunk::get(range.begin()), ObjectType::Large);

    m_largeAllocated.set(range.begin(), range.size());
    return range;
}

void* Heap::tryAllocateLarge(std::unique_lock<Mutex>& lock, size_t alignment, size_t size)
{
    RELEASE_BASSERT(isActiveHeapKind(m_kind));

    BASSERT(isPowerOfTwo(alignment));
    
    if (m_debugHeap)
        return m_debugHeap->memalignLarge(alignment, size);
    
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
        if (m_hasPendingDecommits) {
            m_condition.wait(lock, [&]() { return !m_hasPendingDecommits; });
            // Now we're guaranteed we're looking at all available memory.
            return tryAllocateLarge(lock, alignment, size);
        }

        if (usingGigacage())
            return nullptr;

        range = PerProcess<VMHeap>::get()->tryAllocateLargeChunk(alignment, size);
        if (!range)
            return nullptr;
        
        m_largeFree.add(range);
        range = m_largeFree.remove(alignment, size);
    }

    m_freeableMemory -= range.totalPhysicalSize();

    void* result = splitAndAllocate(lock, range, alignment, size).begin();
    m_highWatermark = std::max(m_highWatermark, result);
    return result;
}

void* Heap::allocateLarge(std::unique_lock<Mutex>& lock, size_t alignment, size_t size)
{
    void* result = tryAllocateLarge(lock, alignment, size);
    RELEASE_BASSERT(result);
    return result;
}

bool Heap::isLarge(std::unique_lock<Mutex>&, void* object)
{
    return m_objectTypes.get(Object(object).chunk()) == ObjectType::Large;
}

size_t Heap::largeSize(std::unique_lock<Mutex>&, void* object)
{
    return m_largeAllocated.get(object);
}

void Heap::shrinkLarge(std::unique_lock<Mutex>& lock, const Range& object, size_t newSize)
{
    BASSERT(object.size() > newSize);

    size_t size = m_largeAllocated.remove(object.begin());
    LargeRange range = LargeRange(object, size, size);
    splitAndAllocate(lock, range, alignment, newSize);

    m_scavenger->schedule(size);
}

void Heap::deallocateLarge(std::unique_lock<Mutex>&, void* object)
{
    if (m_debugHeap)
        return m_debugHeap->freeLarge(object);

    size_t size = m_largeAllocated.remove(object);
    m_largeFree.add(LargeRange(object, size, size, size));
    m_freeableMemory += size;
    m_scavenger->schedule(size);
}

void Heap::externalCommit(void* ptr, size_t size)
{
    std::unique_lock<Mutex> lock(Heap::mutex());
    externalCommit(lock, ptr, size);
}

void Heap::externalCommit(std::unique_lock<Mutex>&, void* ptr, size_t size)
{
    BUNUSED_PARAM(ptr);

    m_footprint += size;
#if ENABLE_PHYSICAL_PAGE_MAP 
    m_physicalPageMap.commit(ptr, size);
#endif
}

void Heap::externalDecommit(void* ptr, size_t size)
{
    std::unique_lock<Mutex> lock(Heap::mutex());
    externalDecommit(lock, ptr, size);
}

void Heap::externalDecommit(std::unique_lock<Mutex>&, void* ptr, size_t size)
{
    BUNUSED_PARAM(ptr);

    m_footprint -= size;
#if ENABLE_PHYSICAL_PAGE_MAP 
    m_physicalPageMap.decommit(ptr, size);
#endif
}

} // namespace bmalloc
