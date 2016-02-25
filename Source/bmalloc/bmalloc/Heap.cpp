/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
#include "BumpAllocator.h"
#include "LargeChunk.h"
#include "LargeObject.h"
#include "PerProcess.h"
#include "SmallChunk.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include <thread>

namespace bmalloc {

Heap::Heap(std::lock_guard<StaticMutex>&)
    : m_largeObjects(VMState::HasPhysical::True)
    , m_isAllocatingPages(false)
    , m_scavenger(*this, &Heap::concurrentScavenge)
{
    initializeLineMetadata();
}

void Heap::initializeLineMetadata()
{
    // We assume that m_smallLineMetadata is zero-filled.

    for (size_t size = alignment; size <= smallMax; size += alignment) {
        size_t sizeClass = bmalloc::sizeClass(size);
        auto& metadata = m_smallLineMetadata[sizeClass];

        size_t object = 0;
        size_t line = 0;
        while (object < vmPageSize) {
            line = object / smallLineSize;
            size_t leftover = object % smallLineSize;

            size_t objectCount;
            size_t remainder;
            divideRoundingUp(smallLineSize - leftover, size, objectCount, remainder);

            metadata[line] = { static_cast<unsigned short>(leftover), static_cast<unsigned short>(objectCount) };

            object += objectCount * size;
        }

        // Don't allow the last object in a page to escape the page.
        if (object > vmPageSize) {
            BASSERT(metadata[line].objectCount);
            --metadata[line].objectCount;
        }
    }
}

void Heap::concurrentScavenge()
{
    std::unique_lock<StaticMutex> lock(PerProcess<Heap>::mutex());
    scavenge(lock, scavengeSleepDuration);
}

void Heap::scavenge(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    waitUntilFalse(lock, sleepDuration, m_isAllocatingPages);

    scavengeSmallPages(lock, sleepDuration);
    scavengeLargeObjects(lock, sleepDuration);

    sleep(lock, sleepDuration);
}

void Heap::scavengeSmallPages(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    while (m_smallPages.size()) {
        m_vmHeap.deallocateSmallPage(lock, m_smallPages.pop());
        waitUntilFalse(lock, sleepDuration, m_isAllocatingPages);
    }
}

void Heap::scavengeLargeObjects(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    while (LargeObject largeObject = m_largeObjects.takeGreedy()) {
        m_vmHeap.deallocateLargeObject(lock, largeObject);
        waitUntilFalse(lock, sleepDuration, m_isAllocatingPages);
    }
}

void Heap::allocateSmallBumpRanges(std::lock_guard<StaticMutex>& lock, size_t sizeClass, BumpAllocator& allocator, BumpRangeCache& rangeCache)
{
    BASSERT(!rangeCache.size());
    SmallPage* page = allocateSmallPage(lock, sizeClass);
    SmallLine* lines = page->begin();
    BASSERT(page->hasFreeLines(lock));

    // Find a free line.
    for (size_t lineNumber = 0; lineNumber < smallLineCount; ++lineNumber) {
        if (lines[lineNumber].refCount(lock))
            continue;

        LineMetadata& lineMetadata = m_smallLineMetadata[sizeClass][lineNumber];
        if (!lineMetadata.objectCount)
            continue;

        // In a fragmented page, some free ranges might not fit in the cache.
        if (rangeCache.size() == rangeCache.capacity()) {
            m_smallPagesWithFreeLines[sizeClass].push(page);
            return;
        }

        char* begin = lines[lineNumber].begin() + lineMetadata.startOffset;
        unsigned short objectCount = lineMetadata.objectCount;
        lines[lineNumber].ref(lock, lineMetadata.objectCount);
        page->ref(lock);

        // Merge with subsequent free lines.
        while (++lineNumber < smallLineCount) {
            if (lines[lineNumber].refCount(lock))
                break;

            LineMetadata& lineMetadata = m_smallLineMetadata[sizeClass][lineNumber];
            if (!lineMetadata.objectCount)
                continue;

            objectCount += lineMetadata.objectCount;
            lines[lineNumber].ref(lock, lineMetadata.objectCount);
            page->ref(lock);
        }

        if (!allocator.canAllocate())
            allocator.refill({ begin, objectCount });
        else
            rangeCache.push({ begin, objectCount });
    }

    page->setHasFreeLines(lock, false);
}

SmallPage* Heap::allocateSmallPage(std::lock_guard<StaticMutex>& lock, size_t sizeClass)
{
    Vector<SmallPage*>& smallPagesWithFreeLines = m_smallPagesWithFreeLines[sizeClass];
    while (smallPagesWithFreeLines.size()) {
        SmallPage* page = smallPagesWithFreeLines.pop();
        if (!page->refCount(lock) || page->sizeClass() != sizeClass) // Page was promoted to the pages list.
            continue;
        return page;
    }

    SmallPage* page = [this, &lock]() {
        if (m_smallPages.size())
            return m_smallPages.pop();

        m_isAllocatingPages = true;
        SmallPage* page = m_vmHeap.allocateSmallPage(lock);
        return page;
    }();

    page->setSizeClass(sizeClass);
    return page;
}

void Heap::deallocateSmallLine(std::lock_guard<StaticMutex>& lock, SmallLine* line)
{
    BASSERT(!line->refCount(lock));
    SmallPage* page = SmallPage::get(line);
    page->deref(lock);

    if (!page->hasFreeLines(lock)) {
        page->setHasFreeLines(lock, true);
        m_smallPagesWithFreeLines[page->sizeClass()].push(page);

        BASSERT(page->refCount(lock));
        return;
    }

    if (page->refCount(lock))
        return;

    m_smallPages.push(page);
    m_scavenger.run();
}

void* Heap::allocateXLarge(std::lock_guard<StaticMutex>& lock, size_t alignment, size_t size)
{
    void* result = tryAllocateXLarge(lock, alignment, size);
    RELEASE_BASSERT(result);
    return result;
}

void* Heap::allocateXLarge(std::lock_guard<StaticMutex>& lock, size_t size)
{
    return allocateXLarge(lock, superChunkSize, size);
}

void* Heap::tryAllocateXLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t size)
{
    BASSERT(isPowerOfTwo(alignment));
    BASSERT(alignment >= superChunkSize);
    BASSERT(size == roundUpToMultipleOf<xLargeAlignment>(size));

    void* result = tryVMAllocate(alignment, size);
    if (!result)
        return nullptr;
    m_xLargeObjects.push(Range(result, size));
    return result;
}

Range& Heap::findXLarge(std::unique_lock<StaticMutex>&, void* object)
{
    for (auto& range : m_xLargeObjects) {
        if (range.begin() != object)
            continue;
        return range;
    }

    RELEASE_BASSERT(false);
    return *static_cast<Range*>(nullptr); // Silence compiler error.
}

void Heap::deallocateXLarge(std::unique_lock<StaticMutex>& lock, void* object)
{
    Range toDeallocate = m_xLargeObjects.pop(&findXLarge(lock, object));

    lock.unlock();
    vmDeallocate(toDeallocate.begin(), toDeallocate.size());
    lock.lock();
}

inline LargeObject& Heap::splitAndAllocate(LargeObject& largeObject, size_t size)
{
    BASSERT(largeObject.isFree());

    LargeObject nextLargeObject;

    if (largeObject.size() - size > largeMin) {
        std::pair<LargeObject, LargeObject> split = largeObject.split(size);
        largeObject = split.first;
        nextLargeObject = split.second;
    }

    largeObject.setFree(false);

    if (nextLargeObject) {
        BASSERT(!nextLargeObject.nextCanMerge());
        m_largeObjects.insert(nextLargeObject);
    }

    return largeObject;
}

inline LargeObject& Heap::splitAndAllocate(LargeObject& largeObject, size_t alignment, size_t size)
{
    LargeObject prevLargeObject;
    LargeObject nextLargeObject;

    size_t alignmentMask = alignment - 1;
    if (test(largeObject.begin(), alignmentMask)) {
        size_t prefixSize = roundUpToMultipleOf(alignment, largeObject.begin() + largeMin) - largeObject.begin();
        std::pair<LargeObject, LargeObject> pair = largeObject.split(prefixSize);
        prevLargeObject = pair.first;
        largeObject = pair.second;
    }

    BASSERT(largeObject.isFree());

    if (largeObject.size() - size > largeMin) {
        std::pair<LargeObject, LargeObject> split = largeObject.split(size);
        largeObject = split.first;
        nextLargeObject = split.second;
    }

    largeObject.setFree(false);

    if (prevLargeObject) {
        LargeObject merged = prevLargeObject.merge();
        m_largeObjects.insert(merged);
    }

    if (nextLargeObject) {
        LargeObject merged = nextLargeObject.merge();
        m_largeObjects.insert(merged);
    }

    return largeObject;
}

void* Heap::allocateLarge(std::lock_guard<StaticMutex>&, size_t size)
{
    BASSERT(size <= largeMax);
    BASSERT(size >= largeMin);
    BASSERT(size == roundUpToMultipleOf<largeAlignment>(size));

    LargeObject largeObject = m_largeObjects.take(size);
    if (!largeObject)
        largeObject = m_vmHeap.allocateLargeObject(size);

    if (largeObject.vmState().hasVirtual()) {
        m_isAllocatingPages = true;
        // We commit before we split in order to avoid split/merge commit/decommit churn.
        vmAllocatePhysicalPagesSloppy(largeObject.begin(), largeObject.size());
        largeObject.setVMState(VMState::Physical);
    }

    largeObject = splitAndAllocate(largeObject, size);

    return largeObject.begin();
}

void* Heap::allocateLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t size, size_t unalignedSize)
{
    BASSERT(size <= largeMax);
    BASSERT(size >= largeMin);
    BASSERT(size == roundUpToMultipleOf<largeAlignment>(size));
    BASSERT(unalignedSize <= largeMax);
    BASSERT(unalignedSize >= largeMin);
    BASSERT(unalignedSize == roundUpToMultipleOf<largeAlignment>(unalignedSize));
    BASSERT(alignment <= largeChunkSize / 2);
    BASSERT(alignment >= largeAlignment);
    BASSERT(isPowerOfTwo(alignment));

    LargeObject largeObject = m_largeObjects.take(alignment, size, unalignedSize);
    if (!largeObject)
        largeObject = m_vmHeap.allocateLargeObject(alignment, size, unalignedSize);

    if (largeObject.vmState().hasVirtual()) {
        m_isAllocatingPages = true;
        // We commit before we split in order to avoid split/merge commit/decommit churn.
        vmAllocatePhysicalPagesSloppy(largeObject.begin(), largeObject.size());
        largeObject.setVMState(VMState::Physical);
    }

    largeObject = splitAndAllocate(largeObject, alignment, size);

    return largeObject.begin();
}

void Heap::deallocateLarge(std::lock_guard<StaticMutex>&, const LargeObject& largeObject)
{
    BASSERT(!largeObject.isFree());
    largeObject.setFree(true);
    
    LargeObject merged = largeObject.merge();
    m_largeObjects.insert(merged);
    m_scavenger.run();
}

void Heap::deallocateLarge(std::lock_guard<StaticMutex>& lock, void* object)
{
    LargeObject largeObject(object);
    deallocateLarge(lock, largeObject);
}

} // namespace bmalloc
