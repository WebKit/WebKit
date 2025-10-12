//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PoolAlloc.cpp:
//    Implements the PoolAllocator.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "common/PoolAlloc.h"

#include <stdint.h>

#include "common/aligned_memory.h"
#include "common/platform.h"

#if defined(ANGLE_WITH_ASAN)
#    include <sanitizer/asan_interface.h>
#endif

namespace angle
{

// The PoolAllocator memory is aligned by starting with an aligned pointer
// and reserving aligned size amount of memory.
// This is, as opposed to aligning the current pointer and reserving the
// exact amount.
// The layout is:
//   [client][pad][client][pad]...
// With ANGLE_POOL_ALLOC_GUARD_BLOCKS, the layout is:
//   [guard][client][pad/guard][guard][client][pad/guard]
// ANGLE_POOL_ALLOC_GUARD_BLOCKS asserts that guards and pads are not overwritten
// by the client.

#if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)
constexpr uint8_t kGuardFillValue = 0xfe;
#endif

class PoolAllocator::Segment
{
  public:
    Segment() = default;
    explicit Segment(Span<uint8_t> data) : mData(data) {}
    Segment(Segment &&other) : mData(std::exchange(other.mData, {})) {}
    ~Segment()
    {
        uint8_t *data = mData.data();
        if (data != nullptr)
        {
            size_t size = mData.size();
            ANGLE_UNUSED_VARIABLE(size);
            ANGLE_ALLOC_PROFILE(POOL_DEALLOCATION, data, size, size > kSegmentSize);
            AlignedFree(data);
        }
    }
    Segment &operator=(Segment &&other)
    {
        mData = std::exchange(other.mData, {});
        return *this;
    }
    static Segment Allocate(size_t size)
    {
        ANGLE_ALLOC_PROFILE(POOL_ALLOCATION, size, size > kSegmentSize);
        uint8_t *result = reinterpret_cast<uint8_t *>(AlignedAlloc(size, kAlignment));
        if (ANGLE_UNLIKELY(result == nullptr))
        {
            return {};
        }
        return Segment{{result, size}};
    }
    uint8_t *data() const { return mData.data(); }

  private:
    Span<uint8_t> mData;
};

PoolAllocator::PoolAllocator() = default;

PoolAllocator::~PoolAllocator()
{
    reset();
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

#if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)

void PoolAllocator::addGuard(Span<uint8_t> guardData)
{
    memset(guardData.data(), kGuardFillValue, guardData.size());
    mGuards.push_back(guardData);
}

#endif

void PoolAllocator::reset()
{
#if defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)
    for (Span<uint8_t> guard : mGuards)
    {
        for (uint8_t value : guard)
        {
            ASSERT(value == kGuardFillValue);
        }
    }
    mGuards.clear();
#endif
#if !defined(ANGLE_DISABLE_POOL_ALLOC)
    mCurrentPool    = {};
    mUnusedSegments = std::exchange(mPoolSegments, {});
#    if defined(ANGLE_WITH_ASAN)
    for (auto &segment : mUnusedSegments)
    {
        // Clear any container annotations left over from when the memory
        // was last used. (crbug.com/1419798)
        __asan_unpoison_memory_region(segment.data(), kSegmentSize);
    }
#    endif
#endif
    mSingleObjectSegments.clear();
}

#if !defined(ANGLE_DISABLE_POOL_ALLOC)

bool PoolAllocator::allocateNewPoolSegment()
{
    Segment segment;
    if (!mUnusedSegments.empty())
    {
        segment = std::move(mUnusedSegments.back());
        mUnusedSegments.pop_back();
    }
    else
    {
        segment = Segment::Allocate(kSegmentSize);
        if (ANGLE_UNLIKELY(segment.data() == nullptr))
        {
            return false;
        }
    }

    mCurrentPool = {segment.data(), kSegmentSize};
    mPoolSegments.push_back(std::move(segment));
    return true;
}

#endif

Span<uint8_t> PoolAllocator::allocateSingleObject(size_t size)
{
    Segment segment = Segment::Allocate(size);
    if (ANGLE_UNLIKELY(segment.data() == nullptr))
    {
        return {};
    }
    Span<uint8_t> result{segment.data(), size};
    mSingleObjectSegments.push_back(std::move(segment));
    ANGLE_ALLOC_PROFILE(LOCAL_BUMP_ALLOCATION, result, true);
    return result;
}

}  // namespace angle
