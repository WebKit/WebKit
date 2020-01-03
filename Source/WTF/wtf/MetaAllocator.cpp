/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/MetaAllocator.h>

#include <wtf/DataLog.h>
#include <wtf/FastMalloc.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessID.h>

namespace WTF {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MetaAllocatorHandle);

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MetaAllocatorFreeSpace);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MetaAllocatorFreeSpace);

MetaAllocator::~MetaAllocator()
{
    for (FreeSpaceNode* node = m_freeSpaceSizeMap.first(); node;) {
        FreeSpaceNode* next = node->successor();
        m_freeSpaceSizeMap.remove(node);
        freeFreeSpaceNode(node);
        node = next;
    }
#ifndef NDEBUG
    ASSERT(!m_mallocBalance);
#endif
}

void MetaAllocatorTracker::notify(MetaAllocatorHandle* handle)
{
    m_allocations.insert(handle);
}

void MetaAllocatorTracker::release(MetaAllocatorHandle* handle)
{
    m_allocations.remove(handle);
}

ALWAYS_INLINE void MetaAllocator::release(MetaAllocatorHandle* handle)
{
    LockHolder locker(&m_lock);
    if (handle->sizeInBytes()) {
        void* start = handle->start().untaggedPtr();
        size_t sizeInBytes = handle->sizeInBytes();
        decrementPageOccupancy(start, sizeInBytes);
        addFreeSpaceFromReleasedHandle(FreeSpacePtr(start), sizeInBytes);
    }

    if (UNLIKELY(!!m_tracker))
        m_tracker->release(handle);
}

MetaAllocatorHandle::MetaAllocatorHandle(MetaAllocator* allocator, void* start, size_t sizeInBytes, void* ownerUID)
    : m_allocator(allocator)
    , m_start(start)
    , m_end(reinterpret_cast<char*>(start) + sizeInBytes)
    , m_ownerUID(ownerUID)
{
    ASSERT(allocator);
    ASSERT(start);
    ASSERT(sizeInBytes);
}

MetaAllocatorHandle::~MetaAllocatorHandle()
{
    ASSERT(m_allocator);
    m_allocator->release(this);
}

void MetaAllocatorHandle::shrink(size_t newSizeInBytes)
{
    size_t sizeInBytes = this->sizeInBytes();
    ASSERT(newSizeInBytes <= sizeInBytes);

    LockHolder locker(&m_allocator->m_lock);

    newSizeInBytes = m_allocator->roundUp(newSizeInBytes);
    
    ASSERT(newSizeInBytes <= sizeInBytes);

    if (newSizeInBytes == sizeInBytes)
        return;

    uintptr_t freeStart = m_start.untaggedPtr<uintptr_t>() + newSizeInBytes;
    size_t freeSize = sizeInBytes - newSizeInBytes;
    uintptr_t freeEnd = freeStart + freeSize;
    
    uintptr_t firstCompletelyFreePage = (freeStart + m_allocator->m_pageSize - 1) & ~(m_allocator->m_pageSize - 1);
    if (firstCompletelyFreePage < freeEnd)
        m_allocator->decrementPageOccupancy(reinterpret_cast<void*>(firstCompletelyFreePage), freeSize - (firstCompletelyFreePage - freeStart));

    m_allocator->addFreeSpaceFromReleasedHandle(MetaAllocator::FreeSpacePtr(freeStart), freeSize);

    m_end = m_start + newSizeInBytes;
}

void MetaAllocatorHandle::dump(PrintStream& out) const
{
    out.print(RawPointer(start().untaggedPtr()), "...", RawPointer(end().untaggedPtr()));
}

MetaAllocator::MetaAllocator(size_t allocationGranule, size_t pageSize)
    : m_allocationGranule(allocationGranule)
    , m_pageSize(pageSize)
    , m_bytesAllocated(0)
    , m_bytesReserved(0)
    , m_bytesCommitted(0)
    , m_tracker(0)
#ifndef NDEBUG
    , m_mallocBalance(0)
#endif
#if ENABLE(META_ALLOCATOR_PROFILE)
    , m_numAllocations(0)
    , m_numFrees(0)
#endif
{
    for (m_logPageSize = 0; m_logPageSize < 32; ++m_logPageSize) {
        if (static_cast<size_t>(1) << m_logPageSize == m_pageSize)
            break;
    }
    
    ASSERT(static_cast<size_t>(1) << m_logPageSize == m_pageSize);
    
    for (m_logAllocationGranule = 0; m_logAllocationGranule < 32; ++m_logAllocationGranule) {
        if (static_cast<size_t>(1) << m_logAllocationGranule == m_allocationGranule)
            break;
    }
    
    ASSERT(static_cast<size_t>(1) << m_logAllocationGranule == m_allocationGranule);
}

RefPtr<MetaAllocatorHandle> MetaAllocator::allocate(size_t sizeInBytes, void* ownerUID)
{
    LockHolder locker(&m_lock);

    if (!sizeInBytes)
        return nullptr;
    
    sizeInBytes = roundUp(sizeInBytes);

    FreeSpacePtr start = findAndRemoveFreeSpace(sizeInBytes);
    if (!start) {
        size_t requestedNumberOfPages = (sizeInBytes + m_pageSize - 1) >> m_logPageSize;
        size_t numberOfPages = requestedNumberOfPages;
        
        start = allocateNewSpace(numberOfPages);
        if (!start)
            return nullptr;
        
        ASSERT(numberOfPages >= requestedNumberOfPages);
        
        size_t roundedUpSize = numberOfPages << m_logPageSize;
        
        ASSERT(roundedUpSize >= sizeInBytes);
        
        m_bytesReserved += roundedUpSize;
        
        if (roundedUpSize > sizeInBytes) {
            FreeSpacePtr freeSpaceStart = start + sizeInBytes;
            size_t freeSpaceSize = roundedUpSize - sizeInBytes;
            addFreeSpace(freeSpaceStart, freeSpaceSize);
        }
    }
    incrementPageOccupancy(start.untaggedPtr(), sizeInBytes);
    m_bytesAllocated += sizeInBytes;
#if ENABLE(META_ALLOCATOR_PROFILE)
    m_numAllocations++;
#endif

    auto handle = adoptRef(*new MetaAllocatorHandle(this, start.untaggedPtr(), sizeInBytes, ownerUID));

    if (UNLIKELY(!!m_tracker))
        m_tracker->notify(handle.ptr());

    return handle;
}

MetaAllocator::Statistics MetaAllocator::currentStatistics()
{
    LockHolder locker(&m_lock);
    Statistics result;
    result.bytesAllocated = m_bytesAllocated;
    result.bytesReserved = m_bytesReserved;
    result.bytesCommitted = m_bytesCommitted;
    return result;
}

MetaAllocator::FreeSpacePtr MetaAllocator::findAndRemoveFreeSpace(size_t sizeInBytes)
{
    FreeSpaceNode* node = m_freeSpaceSizeMap.findLeastGreaterThanOrEqual(sizeInBytes);
    
    if (!node)
        return 0;
    
    size_t nodeSizeInBytes = node->sizeInBytes();
    ASSERT(nodeSizeInBytes >= sizeInBytes);

    m_freeSpaceSizeMap.remove(node);

    FreeSpacePtr result;

    if (nodeSizeInBytes == sizeInBytes) {
        // Easy case: perfect fit, so just remove the node entirely.
        result = node->m_start;
        
        m_freeSpaceStartAddressMap.remove(node->m_start);
        m_freeSpaceEndAddressMap.remove(node->m_end);
        freeFreeSpaceNode(node);
    } else {
        // Try to be a good citizen and ensure that the returned chunk of memory
        // straddles as few pages as possible, but only insofar as doing so will
        // not increase fragmentation. The intuition is that minimizing
        // fragmentation is a strictly higher priority than minimizing the number
        // of committed pages, since in the long run, smaller fragmentation means
        // fewer committed pages and fewer failures in general.
        
        uintptr_t nodeStartAsInt = node->m_start.untaggedPtr<uintptr_t>();
        uintptr_t firstPage = nodeStartAsInt >> m_logPageSize;
        uintptr_t lastPage = (nodeStartAsInt + nodeSizeInBytes - 1) >> m_logPageSize;

        uintptr_t lastPageForLeftAllocation = (nodeStartAsInt + sizeInBytes - 1) >> m_logPageSize;
        uintptr_t firstPageForRightAllocation = (nodeStartAsInt + nodeSizeInBytes - sizeInBytes) >> m_logPageSize;
        
        if (lastPageForLeftAllocation - firstPage + 1 <= lastPage - firstPageForRightAllocation + 1) {
            // Allocate in the left side of the returned chunk, and slide the node to the right.
            result = node->m_start;
            
            m_freeSpaceStartAddressMap.remove(node->m_start);

            node->m_start += sizeInBytes;

            m_freeSpaceSizeMap.insert(node);
            m_freeSpaceStartAddressMap.add(node->m_start, node);
        } else {
            // Allocate in the right size of the returned chunk, and slide the node to the left;

            result = node->m_end - sizeInBytes;

            m_freeSpaceEndAddressMap.remove(node->m_end);

            node->m_end = result;

            m_freeSpaceSizeMap.insert(node);
            m_freeSpaceEndAddressMap.add(result, node);
        }
    }
    
#if ENABLE(META_ALLOCATOR_PROFILE)
    dumpProfile();
#endif

    return result;
}

void MetaAllocator::addFreeSpaceFromReleasedHandle(FreeSpacePtr start, size_t sizeInBytes)
{
#if ENABLE(META_ALLOCATOR_PROFILE)
    m_numFrees++;
#endif
    m_bytesAllocated -= sizeInBytes;
    addFreeSpace(start, sizeInBytes);
}

void MetaAllocator::addFreshFreeSpace(void* start, size_t sizeInBytes)
{
    LockHolder locker(&m_lock);
    m_bytesReserved += sizeInBytes;
    addFreeSpace(FreeSpacePtr(start), sizeInBytes);
}

size_t MetaAllocator::debugFreeSpaceSize()
{
#ifndef NDEBUG
    LockHolder locker(&m_lock);
    size_t result = 0;
    for (FreeSpaceNode* node = m_freeSpaceSizeMap.first(); node; node = node->successor())
        result += node->sizeInBytes();
    return result;
#else
    CRASH();
    return 0;
#endif
}

void MetaAllocator::addFreeSpace(FreeSpacePtr start, size_t sizeInBytes)
{
    FreeSpacePtr end = start + sizeInBytes;

    HashMap<FreeSpacePtr, FreeSpaceNode*>::iterator leftNeighbor = m_freeSpaceEndAddressMap.find(start);
    HashMap<FreeSpacePtr, FreeSpaceNode*>::iterator rightNeighbor = m_freeSpaceStartAddressMap.find(end);

    if (leftNeighbor != m_freeSpaceEndAddressMap.end()) {
        // We have something we can coalesce with on the left. Remove it from the tree, and
        // remove its end from the end address map.
        
        ASSERT(leftNeighbor->value->m_end == leftNeighbor->key);

        FreeSpaceNode* leftNode = leftNeighbor->value;

        FreeSpacePtr leftEnd = leftNode->m_end;

        ASSERT(leftEnd == start);
        
        m_freeSpaceSizeMap.remove(leftNode);
        m_freeSpaceEndAddressMap.remove(leftEnd);
        
        // Now check if there is also something to coalesce with on the right.
        if (rightNeighbor != m_freeSpaceStartAddressMap.end()) {
            // Freeing something in the middle of free blocks. Coalesce both left and
            // right, whilst removing the right neighbor from the maps.
            
            ASSERT(rightNeighbor->value->m_start == rightNeighbor->key);
            
            FreeSpaceNode* rightNode = rightNeighbor->value;
            FreeSpacePtr rightStart = rightNeighbor->key;
            size_t rightSize = rightNode->sizeInBytes();
            FreeSpacePtr rightEnd = rightNode->m_end;

            ASSERT(rightStart == end);
            ASSERT(leftNode->m_start + (leftNode->sizeInBytes() + sizeInBytes + rightSize) == rightEnd);

            m_freeSpaceSizeMap.remove(rightNode);
            m_freeSpaceStartAddressMap.remove(rightStart);
            m_freeSpaceEndAddressMap.remove(rightEnd);
            
            freeFreeSpaceNode(rightNode);

            leftNode->m_end += (sizeInBytes + rightSize);

            m_freeSpaceSizeMap.insert(leftNode);
            m_freeSpaceEndAddressMap.add(rightEnd, leftNode);
        } else {
            leftNode->m_end += sizeInBytes;

            m_freeSpaceSizeMap.insert(leftNode);
            m_freeSpaceEndAddressMap.add(end, leftNode);
        }
    } else {
        // Cannot coalesce with left; try to see if we can coalesce with right.
        
        if (rightNeighbor != m_freeSpaceStartAddressMap.end()) {
            FreeSpaceNode* rightNode = rightNeighbor->value;
            FreeSpacePtr rightStart = rightNeighbor->key;

            ASSERT(rightStart == end);
            ASSERT(start + (sizeInBytes + rightNode->sizeInBytes()) == rightNode->m_end);

            m_freeSpaceSizeMap.remove(rightNode);
            m_freeSpaceStartAddressMap.remove(rightStart);

            rightNode->m_start = start;

            m_freeSpaceSizeMap.insert(rightNode);
            m_freeSpaceStartAddressMap.add(start, rightNode);
        } else {
            // Nothing to coalesce with, so create a new free space node and add it.
            
            FreeSpaceNode* node = allocFreeSpaceNode();

            node->m_start = start;
            node->m_end = start + sizeInBytes;

            m_freeSpaceSizeMap.insert(node);
            m_freeSpaceStartAddressMap.add(start, node);
            m_freeSpaceEndAddressMap.add(end, node);
        }
    }
    
#if ENABLE(META_ALLOCATOR_PROFILE)
    dumpProfile();
#endif
}

void MetaAllocator::incrementPageOccupancy(void* address, size_t sizeInBytes)
{
    uintptr_t firstPage = reinterpret_cast<uintptr_t>(address) >> m_logPageSize;
    uintptr_t lastPage = (reinterpret_cast<uintptr_t>(address) + sizeInBytes - 1) >> m_logPageSize;

    uintptr_t currentPageStart = 0;
    size_t count = 0;
    auto flushNeedPages = [&] {
        if (!currentPageStart)
            return;
        notifyNeedPage(reinterpret_cast<void*>(currentPageStart << m_logPageSize), count);
        currentPageStart = 0;
        count = 0;
    };
    
    for (uintptr_t page = firstPage; page <= lastPage; ++page) {
        auto result = m_pageOccupancyMap.add(page, 1);
        if (result.isNewEntry) {
            m_bytesCommitted += m_pageSize;
            if (!currentPageStart)
                currentPageStart = page;
            ++count;
        } else {
            result.iterator->value++;
            flushNeedPages();
        }
    }
    flushNeedPages();
}

void MetaAllocator::decrementPageOccupancy(void* address, size_t sizeInBytes)
{
    uintptr_t firstPage = reinterpret_cast<uintptr_t>(address) >> m_logPageSize;
    uintptr_t lastPage = (reinterpret_cast<uintptr_t>(address) + sizeInBytes - 1) >> m_logPageSize;

    uintptr_t currentPageStart = 0;
    size_t count = 0;
    auto flushFreePages = [&] {
        if (!currentPageStart)
            return;
        notifyPageIsFree(reinterpret_cast<void*>(currentPageStart << m_logPageSize), count);
        currentPageStart = 0;
        count = 0;
    };
    
    for (uintptr_t page = firstPage; page <= lastPage; ++page) {
        HashMap<uintptr_t, size_t>::iterator iter = m_pageOccupancyMap.find(page);
        ASSERT(iter != m_pageOccupancyMap.end());
        if (!--(iter->value)) {
            m_pageOccupancyMap.remove(iter);
            m_bytesCommitted -= m_pageSize;
            if (!currentPageStart)
                currentPageStart = page;
            ++count;
        } else
            flushFreePages();
    }
    flushFreePages();
}

bool MetaAllocator::isInAllocatedMemory(const AbstractLocker&, void* address)
{
    ASSERT(m_lock.isLocked());
    uintptr_t page = reinterpret_cast<uintptr_t>(address) >> m_logPageSize;
    return m_pageOccupancyMap.contains(page);
}

size_t MetaAllocator::roundUp(size_t sizeInBytes)
{
    if (std::numeric_limits<size_t>::max() - m_allocationGranule <= sizeInBytes)
        CRASH();
    return (sizeInBytes + m_allocationGranule - 1) & ~(m_allocationGranule - 1);
}

MetaAllocator::FreeSpaceNode* MetaAllocator::allocFreeSpaceNode()
{
#ifndef NDEBUG
    m_mallocBalance++;
#endif
    return new (NotNull, MetaAllocatorFreeSpaceMalloc::malloc(sizeof(FreeSpaceNode))) FreeSpaceNode();
}

void MetaAllocator::freeFreeSpaceNode(FreeSpaceNode* node)
{
#ifndef NDEBUG
    m_mallocBalance--;
#endif
    MetaAllocatorFreeSpaceMalloc::free(node);
}

#if ENABLE(META_ALLOCATOR_PROFILE)
void MetaAllocator::dumpProfile()
{
    dataLogF(
        "%d: MetaAllocator(%p): num allocations = %u, num frees = %u, allocated = %lu, reserved = %lu, committed = %lu\n",
        getCurrentProcessID(), this, m_numAllocations, m_numFrees, m_bytesAllocated, m_bytesReserved, m_bytesCommitted);
}
#endif

} // namespace WTF


