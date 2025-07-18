//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PoolAlloc.cpp:
//    Implements the PoolAllocator.
//

#include "common/PoolAlloc.h"

#include <stdint.h>
#include <utility>

#include "common/platform.h"

#if defined(ANGLE_WITH_ASAN)
#    include <sanitizer/asan_interface.h>
#endif
#if !defined(ANGLE_DISABLE_POOL_ALLOC)
#    include "common/aligned_memory.h"
#endif

namespace angle
{

constexpr size_t kPoolAllocatorPageSize = 16 * 1024;

PoolAllocator::PoolAllocator(size_t alignment) : mAlignment(alignment)
{
    ASSERT(gl::isPow2(mAlignment) && mAlignment < kPoolAllocatorPageSize);
}

void PoolAllocator::lock()
{
    ASSERT(!mLocked);
    mLocked = true;
}

void PoolAllocator::unlock()
{
    ASSERT(mLocked);
    mLocked = false;
}

#if !defined(ANGLE_DISABLE_POOL_ALLOC)

namespace
{

inline Span<uint8_t> AllocatePageMemory(size_t size, size_t alignment)
{
    uint8_t *result = reinterpret_cast<uint8_t *>(AlignedAlloc(size, alignment));
    if (ANGLE_UNLIKELY(result == nullptr))
    {
        return {};
    }
    return {result, size};
}

inline void DeallocatePageMemory(uint8_t *memory)
{
    AlignedFree(memory);
}

#    if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)

constexpr uint8_t kGuardFillValue = 0xfe;
constexpr uint8_t kDataFillValue  = 0xcd;

#    endif

}  // anonymous namespace.

class PoolAllocator::PageHeader
{
  public:
    PageHeader(PageHeader *next, size_t size) : next(next), size(size) {}

    PageHeader *next;
    size_t size;
};

#    if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)

size_t PoolAllocator::adjustAllocationExtent(size_t alignedSize) const
{
    size_t adjustedSize = alignedSize;
    if (mAlignment != 1)
    {
        adjustedSize += 2 * mAlignment;
    }
    return adjustedSize;
}

void PoolAllocator::addGuard(Span<uint8_t> alignedData, size_t size)
{
    if (!alignedData.empty())
    {
        memset(alignedData.data(), kDataFillValue, alignedData.size());
        Span<uint8_t> alignmentGuard = alignedData.subspan(size);
        if (!alignmentGuard.empty())
        {
            mGuards.push_back(alignmentGuard);
        }
    }

    if (mAlignment != 1)
    {
        Span guard = bump(mAlignment);
        memset(guard.data(), kGuardFillValue, guard.size());
        mGuards.push_back(guard);
    }
}

#    endif

PoolAllocator::~PoolAllocator()
{
    reset();
    PageHeader *page = mUnusedPages;
    while (page)
    {
        PageHeader *next = page->next;
        DeallocatePageMemory(reinterpret_cast<uint8_t *>(page));
        page = next;
    }
}

void PoolAllocator::reset()
{
#    if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)
    for (Span<uint8_t> guard : mGuards)
    {
        uint8_t expectedValue = reinterpret_cast<uintptr_t>(guard.data()) % mAlignment != 0
                                    ? kDataFillValue
                                    : kGuardFillValue;
        for (uint8_t value : guard)
        {
            ASSERT(value == expectedValue);
        }
    }
    mGuards.clear();
#    endif
    mMemory          = {};
    PageHeader *page = std::exchange(mPages, nullptr);
    while (page)
    {
        PageHeader *next = page->next;
        if (page->size > kPoolAllocatorPageSize)
        {
            DeallocatePageMemory(reinterpret_cast<uint8_t *>(page));
        }
        else
        {
#    if defined(ANGLE_WITH_ASAN)
            // Clear any container annotations left over from when the memory
            // was last used. (crbug.com/1419798)
            __asan_unpoison_memory_region(page, page->size);
#    endif
            page->next   = mUnusedPages;
            mUnusedPages = page;
        }
        page = next;
    }
}

bool PoolAllocator::allocateNewPage(size_t numBytes)
{
    size_t headerSize = rx::roundUpPow2(sizeof(PageHeader), mAlignment);
    size_t pageExtent = rx::roundUpPow2(numBytes + headerSize, kPoolAllocatorPageSize);
    if (mUnusedPages != nullptr && mUnusedPages->size >= pageExtent)
    {
        PageHeader *page = mUnusedPages;
        mUnusedPages     = page->next;
        mMemory    = {reinterpret_cast<uint8_t *>(page) + headerSize, page->size - headerSize};
        page->next = mPages;
        mPages     = page;
        return true;
    }

    Span<uint8_t> memory =
        AllocatePageMemory(pageExtent, std::max(mAlignment, alignof(PageHeader)));
    if (ANGLE_UNLIKELY(memory.empty()))
    {
        return false;
    }
    mMemory = memory;
    mPages  = new (bump(headerSize).data()) PageHeader(mPages, pageExtent);
    return true;
}

#else  // !defined(ANGLE_DISABLE_POOL_ALLOC)

PoolAllocator::~PoolAllocator() = default;

void PoolAllocator::reset()
{
    mAllocations.clear();
}

void *PoolAllocator::allocate(size_t numBytes)
{
    ASSERT(!mLocked);
    uint8_t *alloc = new (std::nothrow) uint8_t[numBytes + mAlignment - 1];
    if (ANGLE_UNLIKELY(!alloc))
    {
        return nullptr;
    }
    mAllocations.push_back(std::unique_ptr<uint8_t[]>(alloc));
    uintptr_t intAlloc = reinterpret_cast<uintptr_t>(alloc);
    intAlloc           = rx::roundUpPow2<uintptr_t>(intAlloc, mAlignment);
    return reinterpret_cast<void *>(intAlloc);
}

#endif

}  // namespace angle
