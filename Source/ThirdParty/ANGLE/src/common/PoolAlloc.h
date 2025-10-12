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

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

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

#include <memory>
#include <utility>
#include <vector>

#include "common/angleutils.h"
#include "common/log_utils.h"
#include "common/mathutil.h"
#include "common/span.h"
#ifdef ANGLE_PLATFORM_APPLE
#    if __has_include(<WebKitAdditions/ANGLEAllocProfile.h>)
#        include <WebKitAdditions/ANGLEAllocProfile.h>
#    endif
#endif

#if !defined(ANGLE_ALLOC_PROFILE)
#define ANGLE_ALLOC_PROFILE(kind, ...)
#define ANGLE_ALLOC_PROFILE_ALIGNMENT(x) (x)
#endif

namespace angle
{

// Allocator that allocates memory aligned to kAlignment and releases it when the instance is
// destroyed.
class PoolAllocator : angle::NonCopyable
{
  public:
    PoolAllocator();
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
    static constexpr size_t kAlignment = ANGLE_ALLOC_PROFILE_ALIGNMENT(sizeof(void *));
    Span<uint8_t> allocateSingleObject(size_t size);
    class Segment;
    std::vector<Segment> mSingleObjectSegments;  // Large objects.

#if !defined(ANGLE_DISABLE_POOL_ALLOC)
    static constexpr size_t kSegmentSize = 32768;
    bool allocateNewPoolSegment();

    Span<uint8_t> mCurrentPool;  // The unused part of memory in last entry of mPoolSegments.
    std::vector<Segment> mPoolSegments;    // List of currently in use memory allocations.
    std::vector<Segment> mUnusedSegments;  // List of unused allocations after reset().
#endif

#if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)
    void addGuard(Span<uint8_t> guardData);

    std::vector<Span<uint8_t>> mGuards;  // Guards, memory which is asserted to stay prestine.
#endif
    bool mLocked = false;
};

inline void *PoolAllocator::allocate(size_t size)
{
    ASSERT(!mLocked);
    Span<uint8_t> data;

    size_t extent = size;
#if !defined(ANGLE_DISABLE_POOL_ALLOC)
    // Allocate with kAlignment granularity to keep the next allocation aligned.
    extent = rx::roundUpPow2(extent, kAlignment);
#endif
#if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)
    // Add space for guard block before. Add space for guard block after if there is no alignment
    // padding, else use the padding as the guard block after.
    extent += kAlignment + (extent == size ? kAlignment : 0);
#endif
#if !defined(ANGLE_DISABLE_POOL_ALLOC)
    if (extent <= mCurrentPool.size())
    {
        data         = mCurrentPool.first(extent);
        mCurrentPool = mCurrentPool.subspan(extent);
        ANGLE_ALLOC_PROFILE(LOCAL_BUMP_ALLOCATION, data, false);
    }
    else if (extent < kSegmentSize)
    {
        if (ANGLE_UNLIKELY(!allocateNewPoolSegment()))
        {
            return nullptr;
        }
        data         = mCurrentPool.first(extent);
        mCurrentPool = mCurrentPool.subspan(extent);
        ANGLE_ALLOC_PROFILE(LOCAL_BUMP_ALLOCATION, data, false);
    }
    else
#endif
    {
        data = allocateSingleObject(extent);
        if (data.empty())
        {
            return nullptr;
        }
    }

#if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)
    addGuard(data.first(kAlignment));
    data = data.subspan(kAlignment);
    addGuard(data.subspan(size));
#endif
    return data.first(size).data();
}

}  // namespace angle

#endif  // COMMON_POOLALLOC_H_
