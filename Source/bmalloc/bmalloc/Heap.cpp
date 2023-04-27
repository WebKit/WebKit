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

#include "Heap.h"

#include "AvailableMemory.h"
#include "BulkDecommit.h"
#include "BumpAllocator.h"
#include "Chunk.h"
#include "CryptoRandom.h"
#include "DebugHeap.h"
#include "Environment.h"
#include "Gigacage.h"
#include "HeapConstants.h"
#include "PerProcess.h"
#include "Scavenger.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include "bmalloc.h"
#include <thread>
#include <vector>

#if BOS(DARWIN)
#include "Zone.h"
#endif

#if !BUSE(LIBPAS)

namespace bmalloc {

Heap::Heap(HeapKind kind, LockHolder&)
    : m_kind { kind }, m_constants { *HeapConstants::get() }
{
    BASSERT(!Environment::get()->isDebugHeapEnabled());

    Gigacage::ensureGigacage();
#if GIGACAGE_ENABLED
    if (usingGigacage()) {
        void* base = Gigacage::allocBase(gigacageKind(m_kind));
        m_largeFree.add(LargeRange(base, Gigacage::size(gigacageKind(m_kind)), 0, 0, base));
    }
#endif
    
    m_scavenger = Scavenger::get();
}

bool Heap::usingGigacage()
{
    return isGigacage(m_kind) && Gigacage::isEnabled(gigacageKind(m_kind));
}

void* Heap::gigacageBasePtr()
{
    return Gigacage::basePtr(gigacageKind(m_kind));
}

size_t Heap::gigacageSize()
{
    return Gigacage::size(gigacageKind(m_kind));
}

size_t Heap::freeableMemory(UniqueLockHolder&)
{
    return m_freeableMemory;
}

size_t Heap::footprint()
{
    return m_footprint;
}

BINLINE void Heap::adjustStat(size_t& value, std::ptrdiff_t amount)
{
    constexpr bool check = false;

    auto result = value + amount;
    if constexpr (check) {
        // This assertion should be enabled by default after fixing all footprint/freeableMemory issues.
        BASSERT((amount >= 0 && result >= value) || (amount < 0 && result < value));
    }
    value = result;
}

BINLINE void Heap::logStat(size_t value, std::ptrdiff_t amount, const char* label, const char* note)
{
    fprintf(stderr, "%s: %zu (%zd) %s\n", label, value, amount, note);
}

BINLINE void Heap::adjustFreeableMemory(UniqueLockHolder&, std::ptrdiff_t amount, const char* note)
{
    constexpr bool verbose = false;

    adjustStat(m_freeableMemory, amount);

    if constexpr (verbose)
        logStat(m_freeableMemory, amount, "freeableMemory", note);
}

BINLINE void Heap::adjustFootprint(UniqueLockHolder&, std::ptrdiff_t amount, const char* note)
{
    constexpr bool verbose = false;

    adjustStat(m_footprint, amount);

    if constexpr (verbose)
        logStat(m_footprint, amount, "footprint", note);
}

void Heap::markAllLargeAsEligibile(const LockHolder&)
{
    m_largeFree.markAllAsEligibile();
    m_hasPendingDecommits = false;
    m_condition.notify_all();
}

void Heap::decommitLargeRange(UniqueLockHolder& lock, LargeRange& range, BulkDecommit& decommitter)
{
    BASSERT(range.hasPhysicalPages());

    adjustFootprint(lock, -range.totalPhysicalSize(), "decommitLargeRange");
    adjustFreeableMemory(lock, -range.totalPhysicalSize(), "decommitLargeRange");
    decommitter.addLazy(range.begin(), range.physicalEnd() - range.begin());
    m_hasPendingDecommits = true;
    range.setStartPhysicalSize(0);
    range.setTotalPhysicalSize(0);
    range.clearPhysicalEnd();
    BASSERT(range.isEligibile());
    range.setEligible(false);
#if ENABLE_PHYSICAL_PAGE_MAP
    m_physicalPageMap.decommit(range.begin(), range.size());
#endif
}

void Heap::scavenge(UniqueLockHolder& lock, BulkDecommit& decommitter, size_t& deferredDecommits)
{
    for (auto& list : m_freePages) {
        for (auto* chunk : list) {
            for (auto* page : chunk->freePages()) {
                if (!page->hasPhysicalPages())
                    continue;
                if (page->usedSinceLastScavenge()) {
                    page->clearUsedSinceLastScavenge();
                    deferredDecommits++;
                    continue;
                }

                size_t pageSize = bmalloc::pageSize(&list - &m_freePages[0]);
                size_t decommitSize = physicalPageSizeSloppy(page->begin()->begin(), pageSize);
                adjustFootprint(lock, -decommitSize, "scavenge");
                adjustFreeableMemory(lock, -decommitSize, "scavenge");
                decommitter.addEager(page->begin()->begin(), pageSize);
                page->setHasPhysicalPages(false);
#if ENABLE_PHYSICAL_PAGE_MAP
                m_physicalPageMap.decommit(page->begin()->begin(), pageSize);
#endif
            }
        }
    }

    for (auto& list : m_chunkCache) {
        while (!list.isEmpty())
            deallocateSmallChunk(lock, list.pop(), &list - &m_chunkCache[0]);
    }

    for (LargeRange& range : m_largeFree) {
        if (!range.hasPhysicalPages())
            continue;
        if (range.usedSinceLastScavenge()) {
            range.clearUsedSinceLastScavenge();
            deferredDecommits++;
            continue;
        }

        decommitLargeRange(lock, range, decommitter);
    }
}

void Heap::deallocateLineCache(UniqueLockHolder&, LineCache& lineCache)
{
    for (auto& list : lineCache) {
        while (!list.isEmpty()) {
            size_t sizeClass = &list - &lineCache[0];
            m_lineCache[sizeClass].push(list.popFront());
        }
    }
}

void Heap::allocateSmallChunk(UniqueLockHolder& lock, size_t pageClass, FailureAction action)
{
    RELEASE_BASSERT(isActiveHeapKind(m_kind));
    
    size_t pageSize = bmalloc::pageSize(pageClass);

    Chunk* chunk = [&]() -> Chunk* {
        if (!m_chunkCache[pageClass].isEmpty())
            return m_chunkCache[pageClass].pop();

        void* memory = allocateLarge(lock, chunkSize, chunkSize, action);
        if (!memory) {
            BASSERT(action == FailureAction::ReturnNull);
            return nullptr;
        }

        Chunk* chunk = new (memory) Chunk(pageSize);

        m_objectTypes.set(lock, chunk, ObjectType::Small);

        forEachPage(chunk, pageSize, [&](SmallPage* page) {
            page->setHasPhysicalPages(true);
            page->setUsedSinceLastScavenge();
            page->setHasFreeLines(lock, true);
            chunk->freePages().push(page);
        });

        adjustFreeableMemory(lock, chunkSize, "allocateSmallChunk");

        m_scavenger->schedule(0);

        return chunk;
    }();
    
    if (chunk)
        m_freePages[pageClass].push(chunk);
}

void Heap::deallocateSmallChunk(UniqueLockHolder& lock, Chunk* chunk, size_t pageClass)
{
    m_objectTypes.set(lock, chunk, ObjectType::Large);
    
    size_t size = m_largeAllocated.remove(chunk);
    size_t totalPhysicalSize = size;
    size_t chunkPageSize = pageSize(pageClass);
    SmallPage* firstPageWithoutPhysicalPages = nullptr;

    void* physicalEnd = chunk->address(chunk->metadataSize(chunkPageSize));
    bool lastPageHasPhysicalPages = false;
    forEachPage(chunk, chunkPageSize, [&](SmallPage* page) {
        size_t physicalSize = physicalPageSizeSloppy(page->begin()->begin(), pageSize(pageClass));
        if (!page->hasPhysicalPages()) {
            totalPhysicalSize -= physicalSize;
            lastPageHasPhysicalPages = false;
            if (!firstPageWithoutPhysicalPages)
                firstPageWithoutPhysicalPages = page;
        } else {
            physicalEnd = page->begin()->begin() + physicalSize;
            lastPageHasPhysicalPages = true;
        }
    });

    size_t startPhysicalSize = firstPageWithoutPhysicalPages ? firstPageWithoutPhysicalPages->begin()->begin() - chunk->bytes() : size;
    physicalEnd = lastPageHasPhysicalPages ? chunk->address(size) : physicalEnd;
    
    m_largeFree.add(LargeRange(chunk, size, startPhysicalSize, totalPhysicalSize, physicalEnd));
}

SmallPage* Heap::allocateSmallPage(UniqueLockHolder& lock, size_t sizeClass, LineCache& lineCache, FailureAction action)
{
    RELEASE_BASSERT(isActiveHeapKind(m_kind));

    if (!lineCache[sizeClass].isEmpty())
        return lineCache[sizeClass].popFront();

    if (!m_lineCache[sizeClass].isEmpty())
        return m_lineCache[sizeClass].popFront();

    SmallPage* page = [&]() -> SmallPage* {
        size_t pageClass = m_constants.pageClass(sizeClass);
        
        if (m_freePages[pageClass].isEmpty())
            allocateSmallChunk(lock, pageClass, action);
        if (action == FailureAction::ReturnNull && m_freePages[pageClass].isEmpty())
            return nullptr;

        Chunk* chunk = m_freePages[pageClass].tail();

        chunk->ref();

        SmallPage* page = chunk->freePages().pop();
        if (chunk->freePages().isEmpty())
            m_freePages[pageClass].remove(chunk);

        size_t pageSize = bmalloc::pageSize(pageClass);
        size_t physicalSize = physicalPageSizeSloppy(page->begin()->begin(), pageSize);
        if (page->hasPhysicalPages())
            adjustFreeableMemory(lock, -physicalSize, "allocateSmallPage");
        else {
            m_scavenger->scheduleIfUnderMemoryPressure(pageSize);
            adjustFootprint(lock, physicalSize, "allocateSmallPage");
            vmAllocatePhysicalPagesSloppy(page->begin()->begin(), pageSize);
            page->setHasPhysicalPages(true);
#if ENABLE_PHYSICAL_PAGE_MAP
            m_physicalPageMap.commit(page->begin()->begin(), pageSize);
#endif
        }
        page->setUsedSinceLastScavenge();

        return page;
    }();
    if (!page) {
        BASSERT(action == FailureAction::ReturnNull);
        return nullptr;
    }

    page->setSizeClass(sizeClass);
    return page;
}

void Heap::deallocateSmallLine(UniqueLockHolder& lock, Object object, LineCache& lineCache)
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

    size_t pageClass = m_constants.pageClass(page->sizeClass());

    adjustFreeableMemory(lock, physicalPageSizeSloppy(page->begin()->begin(), pageSize(pageClass)), "deallocateSmallLine");

    List<SmallPage>::remove(page); // 'page' may be in any thread's line cache.
    
    Chunk* chunk = Chunk::get(page);
    if (chunk->freePages().isEmpty())
        m_freePages[pageClass].push(chunk);
    chunk->freePages().push(page);

    chunk->deref();

    if (!chunk->refCount()) {
        m_freePages[pageClass].remove(chunk);

        if (!m_chunkCache[pageClass].isEmpty())
            deallocateSmallChunk(lock, m_chunkCache[pageClass].pop(), pageClass);

        m_chunkCache[pageClass].push(chunk);
    }
    
    m_scavenger->schedule(pageSize(pageClass));
}

void Heap::allocateSmallBumpRangesByMetadata(
    UniqueLockHolder& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache,
    LineCache& lineCache, FailureAction action)
{
    BUNUSED(action);
    RELEASE_BASSERT(isActiveHeapKind(m_kind));

    SmallPage* page = allocateSmallPage(lock, sizeClass, lineCache, action);
    if (!page) {
        BASSERT(action == FailureAction::ReturnNull);
        return;
    }
    SmallLine* lines = page->begin();
    BASSERT(page->hasFreeLines(lock));

    auto findSmallBumpRange = [&](size_t& lineNumber) {
        for ( ; lineNumber < m_constants.smallLineCount(); ++lineNumber) {
            if (!lines[lineNumber].refCount(lock)) {
                if (m_constants.objectCount(sizeClass, lineNumber))
                    return true;
            }
        }
        return false;
    };

    auto allocateSmallBumpRange = [&](size_t& lineNumber) -> BumpRange {
        char* begin = lines[lineNumber].begin() + m_constants.startOffset(sizeClass, lineNumber);
        unsigned short objectCount = 0;
        
        for ( ; lineNumber < m_constants.smallLineCount(); ++lineNumber) {
            if (lines[lineNumber].refCount(lock))
                break;

            auto lineObjectCount = m_constants.objectCount(sizeClass, lineNumber);
            if (!lineObjectCount)
                continue;

            objectCount += lineObjectCount;
            lines[lineNumber].ref(lock, lineObjectCount);
            page->ref(lock);
        }
        return { begin, objectCount };
    };

    size_t lineNumber = 0;
    for (;;) {
        if (!findSmallBumpRange(lineNumber)) {
            page->setHasFreeLines(lock, false);
            BASSERT(action == FailureAction::ReturnNull || allocator.canAllocate());
            return;
        }

        // In a fragmented page, some free ranges might not fit in the cache.
        if (rangeCache.size() == rangeCache.capacity()) {
            lineCache[sizeClass].push(page);
            BASSERT(action == FailureAction::ReturnNull || allocator.canAllocate());
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
    UniqueLockHolder& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache,
    LineCache& lineCache, FailureAction action)
{
    BUNUSED(action);
    RELEASE_BASSERT(isActiveHeapKind(m_kind));

    size_t size = allocator.size();
    SmallPage* page = allocateSmallPage(lock, sizeClass, lineCache, action);
    if (!page) {
        BASSERT(action == FailureAction::ReturnNull);
        return;
    }
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
    Object end(it + pageSize(m_constants.pageClass(page->sizeClass())));
    for (;;) {
        if (!findSmallBumpRange(it, end)) {
            page->setHasFreeLines(lock, false);
            BASSERT(action == FailureAction::ReturnNull || allocator.canAllocate());
            return;
        }

        // In a fragmented page, some free ranges might not fit in the cache.
        if (rangeCache.size() == rangeCache.capacity()) {
            lineCache[sizeClass].push(page);
            BASSERT(action == FailureAction::ReturnNull || allocator.canAllocate());
            return;
        }

        BumpRange bumpRange = allocateSmallBumpRange(it, end);
        if (allocator.canAllocate())
            rangeCache.push(bumpRange);
        else
            allocator.refill(bumpRange);
    }
}

LargeRange Heap::splitAndAllocate(UniqueLockHolder& lock, LargeRange& range, size_t alignment, size_t size)
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
        adjustFootprint(lock, range.size() - range.totalPhysicalSize(), "splitAndAllocate");
        vmAllocatePhysicalPagesSloppy(range.begin() + range.startPhysicalSize(), range.size() - range.startPhysicalSize());
        range.setStartPhysicalSize(range.size());
        range.setTotalPhysicalSize(range.size());
        range.setPhysicalEnd(range.begin() + range.size());
#if ENABLE_PHYSICAL_PAGE_MAP 
        m_physicalPageMap.commit(range.begin(), range.size());
#endif
    }
    
    if (prev) {
        adjustFreeableMemory(lock, prev.totalPhysicalSize(), "splitAndAllocate.prev");
        m_largeFree.add(prev);
    }

    if (next) {
        adjustFreeableMemory(lock, next.totalPhysicalSize(), "splitAndAllocate.next");
        m_largeFree.add(next);
    }

    m_objectTypes.set(lock, Chunk::get(range.begin()), ObjectType::Large);

    m_largeAllocated.set(range.begin(), range.size());
    return range;
}

void* Heap::allocateLarge(UniqueLockHolder& lock, size_t alignment, size_t size, FailureAction action)
{
#define ASSERT_OR_RETURN_ON_FAILURE(cond) do { \
        if (action == FailureAction::Crash) \
            RELEASE_BASSERT(cond); \
        else if (!(cond)) \
            return nullptr; \
    } while (false)


    RELEASE_BASSERT(isActiveHeapKind(m_kind));

    BASSERT(isPowerOfTwo(alignment));
    
    size_t roundedSize = size ? roundUpToMultipleOf(largeAlignment, size) : largeAlignment;
    ASSERT_OR_RETURN_ON_FAILURE(roundedSize >= size); // Check for overflow
    size = roundedSize;

    size_t roundedAlignment = roundUpToMultipleOf<largeAlignment>(alignment);
    ASSERT_OR_RETURN_ON_FAILURE(roundedAlignment >= alignment); // Check for overflow
    alignment = roundedAlignment;

    LargeRange range = m_largeFree.remove(alignment, size);
    if (!range) {
        if (m_hasPendingDecommits) {
            m_condition.wait(lock, [&]() { return !m_hasPendingDecommits; });
            // Now we're guaranteed we're looking at all available memory.
            return allocateLarge(lock, alignment, size, action);
        }

        ASSERT_OR_RETURN_ON_FAILURE(!usingGigacage());

        range = tryAllocateLargeChunk(alignment, size);
        ASSERT_OR_RETURN_ON_FAILURE(range);
        
        m_largeFree.add(range);
        adjustFreeableMemory(lock, range.totalPhysicalSize(), "allocateLarge");
        adjustFootprint(lock, range.totalPhysicalSize(), "allocateLarge");

        range = m_largeFree.remove(alignment, size);
    }
    adjustFreeableMemory(lock, -range.totalPhysicalSize(), "allocateLarge.reuse");

    void* result = splitAndAllocate(lock, range, alignment, size).begin();
    ASSERT_OR_RETURN_ON_FAILURE(result);
    return result;

#undef ASSERT_OR_RETURN_ON_FAILURE
}

LargeRange Heap::tryAllocateLargeChunk(size_t alignment, size_t size)
{
    // We allocate VM in aligned multiples to increase the chances that
    // the OS will provide contiguous ranges that we can merge.
    size_t roundedAlignment = roundUpToMultipleOf<chunkSize>(alignment);
    if (roundedAlignment < alignment) // Check for overflow
        return LargeRange();
    alignment = roundedAlignment;

    size_t roundedSize = roundUpToMultipleOf<chunkSize>(size);
    if (roundedSize < size) // Check for overflow
        return LargeRange();
    size = roundedSize;

    void* memory = tryVMAllocate(alignment, size);
    if (!memory)
        return LargeRange();
    
#if BOS(DARWIN)
    PerProcess<Zone>::get()->addRange(Range(memory, size));
#endif

    return LargeRange(memory, size, size, size, static_cast<char*>(memory) + size);
}

size_t Heap::largeSize(UniqueLockHolder&, void* object)
{
    return m_largeAllocated.get(object);
}

void Heap::shrinkLarge(UniqueLockHolder& lock, const Range& object, size_t newSize)
{
    BASSERT(object.size() > newSize);

    size_t size = m_largeAllocated.remove(object.begin());
    LargeRange range = LargeRange(object, size, size, object.begin() + size);
    splitAndAllocate(lock, range, alignment, newSize);

    m_scavenger->schedule(size);
}

void Heap::deallocateLarge(UniqueLockHolder& lock, void* object)
{
    size_t size = m_largeAllocated.remove(object);
    m_largeFree.add(LargeRange(object, size, size, size, static_cast<char*>(object) + size));
    adjustFreeableMemory(lock, size, "deallocateLarge");
    m_scavenger->schedule(size);
}

void Heap::externalCommit(void* ptr, size_t size)
{
    UniqueLockHolder lock(Heap::mutex());
    externalCommit(lock, ptr, size);
}

void Heap::externalCommit(UniqueLockHolder& lock, void* ptr, size_t size)
{
    BUNUSED_PARAM(ptr);

    adjustFootprint(lock, size, "externalCommit");
#if ENABLE_PHYSICAL_PAGE_MAP 
    m_physicalPageMap.commit(ptr, size);
#endif
}

void Heap::externalDecommit(void* ptr, size_t size)
{
    UniqueLockHolder lock(Heap::mutex());
    externalDecommit(lock, ptr, size);
}

void Heap::externalDecommit(UniqueLockHolder& lock, void* ptr, size_t size)
{
    BUNUSED_PARAM(ptr);

    adjustFootprint(lock, -size, "externalDecommit");
#if ENABLE_PHYSICAL_PAGE_MAP 
    m_physicalPageMap.decommit(ptr, size);
#endif
}

} // namespace bmalloc

#endif
