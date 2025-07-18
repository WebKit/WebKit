//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PoolAlloc.h:
//    Defines the class interface for PoolAllocator.
//

#ifndef COMMON_POOLALLOC_H_
#define COMMON_POOLALLOC_H_

#if !defined(NDEBUG)
#    define ANGLE_POOL_ALLOC_GUARD_BLOCKS  // define to enable guard block checking
#endif

//
// This header defines an allocator that can be used to efficiently
// allocate a large number of small requests for heap memory, with the
// intention that they are not individually deallocated, but rather
// collectively deallocated at one time.
//
// This simultaneously
//
// * Makes each individual allocation much more efficient; the
//     typical allocation is trivial.
// * Completely avoids the cost of doing individual deallocation.
// * Saves the trouble of tracking down and plugging a large class of leaks.
//
// Individual classes can use this allocator by supplying their own
// new and delete methods.
//

#include <stdint.h>

#include "common/angleutils.h"
#include "common/log_utils.h"
#include "common/mathutil.h"

#if !defined(ANGLE_DISABLE_POOL_ALLOC)
#    include "common/span.h"
#else
#    include <memory>
#    include <vector>
#endif

namespace angle
{

// Allocator that allocates aligned memory and releases it when the instance is destroyed.
class PoolAllocator : angle::NonCopyable
{
  public:
    PoolAllocator(size_t alignment = sizeof(void *));
    ~PoolAllocator();

    // Returns aligned pointer to 'numBytes' of memory or nullptr on allocation failure.
    void *allocate(size_t numBytes);

    // Marks all allocated memory as unused. The memory will be reused.
    void reset();

    // Catch unwanted allocations.
    // TODO(jmadill): Remove this when we remove the global allocator.
    void lock();
    void unlock();

  private:
    size_t mAlignment;  // all returned allocations will be aligned at
                        // this granularity, which will be a power of 2
#if !defined(ANGLE_DISABLE_POOL_ALLOC)
    bool allocateNewPage(size_t numBytes);
    size_t adjustAllocationExtent(size_t alignedSize) const;
    void addGuard(Span<uint8_t> alignedData, size_t size);
    Span<uint8_t> bump(size_t alignedSize);

    Span<uint8_t> mMemory;
    class PageHeader;
    PageHeader *mUnusedPages = nullptr;
    PageHeader *mPages       = nullptr;
#    if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)
    std::vector<Span<uint8_t>> mGuards;
#    endif
#else  // !defined(ANGLE_DISABLE_POOL_ALLOC)
    std::vector<std::unique_ptr<uint8_t[]>> mAllocations;
#endif

    bool mLocked = false;
};

#if !defined(ANGLE_DISABLE_POOL_ALLOC)

#    if !defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)

inline size_t PoolAllocator::adjustAllocationExtent(size_t alignedSize) const
{
    return alignedSize;
}

inline void PoolAllocator::addGuard(Span<uint8_t> alignedData, size_t size) {}

#    endif

inline Span<uint8_t> PoolAllocator::bump(size_t alignedSize)
{
    Span<uint8_t> result = mMemory.first(alignedSize);
    mMemory              = mMemory.subspan(alignedSize);
    return result;
}

inline void *PoolAllocator::allocate(size_t size)
{
    ASSERT(!mLocked);
    size_t alignedSize = rx::roundUpPow2(size, mAlignment);
    size_t extent      = adjustAllocationExtent(alignedSize);
    if (extent > mMemory.size())
    {
        if (ANGLE_UNLIKELY(!allocateNewPage(extent)))
        {
            return nullptr;
        }
    }
    addGuard({}, 0);
    Span<uint8_t> result = bump(alignedSize);
    addGuard(result, size);
    return result.data();
}

#endif  // !defined(ANGLE_DISABLE_POOL_ALLOC)

}  // namespace angle

#endif  // COMMON_POOLALLOC_H_
