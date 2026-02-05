//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMMON_MEMORYBUFFER_H_
#define COMMON_MEMORYBUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "common/Optional.h"
#include "common/angleutils.h"
#include "common/debug.h"
#include "common/span.h"
#include "common/unsafe_buffers.h"

namespace angle
{

// MemoryBuffers are used in place of std::vector<uint8_t> when an uninitialized buffer
// as would be obtained via malloc is required.
class MemoryBuffer final : NonCopyable
{
  public:
    MemoryBuffer() = default;
    ~MemoryBuffer();

    MemoryBuffer(MemoryBuffer &&other);
    MemoryBuffer &operator=(MemoryBuffer &&other);

    // Free underlying memory. After this call, the MemoryBuffer has zero size
    // but can be reused given a subsequent resize()/reserve().
    void destroy();

    // Updates mSize to newSize. May cause a reallocation iff newSize > mCapacity.
    [[nodiscard]] bool resize(size_t newSize);

    // Updates mCapacity iff newCapacity > mCapacity. May cause a reallocation if
    // newCapacity > mCapacity.
    [[nodiscard]] bool reserve(size_t newCapacity);

    // Sets size to zero and updates mCapacity iff newCapacity > mCapacity. May cause
    // a reallocation if newCapacity is greater than mCapacity prior to the clear.
    [[nodiscard]] bool clearAndReserve(size_t newCapacity);

    // Sets size bound by capacity.
    void setSize(size_t size)
    {
        ASSERT(size <= mCapacity);
        mSize = size;
    }
    void setSizeToCapacity() { setSize(mCapacity); }

    // Sets current size to 0, but retains buffer for future use.
    void clear() { (void)resize(0); }

    size_t size() const { return mSize; }
    size_t capacity() const { return mCapacity; }
    bool empty() const { return mSize == 0; }

    const uint8_t *data() const { return mData; }
    uint8_t *data()
    {
        ASSERT(mData);
        return mData;
    }

    // Access entire buffer, although MemoryBuffer should be implicitly convertible to
    // any span implementation because it has both data() and size() methods.
    angle::Span<uint8_t> span()
    {
        // SAFETY: `mData` is valid for `mSize` bytes.
        return ANGLE_UNSAFE_BUFFERS(angle::Span<uint8_t>(mData, mSize));
    }
    angle::Span<const uint8_t> span() const
    {
        // SAFETY: `mData` is valid for `mSize` bytes.
        return ANGLE_UNSAFE_BUFFERS(angle::Span<uint8_t>(mData, mSize));
    }

    // Convenience methods for accessing portions of the buffer.
    angle::Span<uint8_t> first(size_t count) { return span().first(count); }
    angle::Span<uint8_t> last(size_t count) { return span().last(count); }
    angle::Span<uint8_t> subspan(size_t offset) { return span().subspan(offset); }
    angle::Span<uint8_t> subspan(size_t offset, size_t count)
    {
        return span().subspan(offset, count);
    }

    uint8_t &operator[](size_t pos)
    {
        ASSERT(mData && pos < mSize);
        // SAFETY: assert on previous line.
        return ANGLE_UNSAFE_BUFFERS(mData[pos]);
    }
    const uint8_t &operator[](size_t pos) const
    {
        ASSERT(mData && pos < mSize);
        // SAFETY: assert on previous line.
        return ANGLE_UNSAFE_BUFFERS(mData[pos]);
    }

    void fill(uint8_t datum);

    // Only used by unit tests
    // Validate total bytes allocated during a resize
    void assertTotalAllocatedBytes(size_t totalAllocatedBytes) const
    {
#if defined(ANGLE_ENABLE_ASSERTS)
        ASSERT(totalAllocatedBytes == mTotalAllocatedBytes);
#endif  // ANGLE_ENABLE_ASSERTS
    }
    // Validate total bytes copied during a resize
    void assertTotalCopiedBytes(size_t totalCopiedBytes) const
    {
#if defined(ANGLE_ENABLE_ASSERTS)
        ASSERT(totalCopiedBytes == mTotalCopiedBytes);
#endif  // ANGLE_ENABLE_ASSERTS
    }
    // Validate data buffer is no longer present. Needed because data() may
    // assert a non-null buffer.
    void assertDataBufferFreed() const { ASSERT(mData == nullptr); }

  private:
    size_t mSize     = 0;
    size_t mCapacity = 0;
    uint8_t *mData   = nullptr;
#if defined(ANGLE_ENABLE_ASSERTS)
    size_t mTotalAllocatedBytes = 0;
    size_t mTotalCopiedBytes    = 0;
#endif  // ANGLE_ENABLE_ASSERTS
};

class ScratchBuffer final : NonCopyable
{
  public:
    ScratchBuffer();
    // If we request a scratch buffer requesting a smaller size this many times, release the
    // scratch buffer. This ensures we don't have a degenerate case where we are stuck
    // hogging memory. Zero means eternal lifetime.
    ScratchBuffer(uint32_t lifetime);
    ~ScratchBuffer();

    ScratchBuffer(ScratchBuffer &&other);
    ScratchBuffer &operator=(ScratchBuffer &&other);

    // Returns true with a memory buffer of the requested size, or false on failure.
    bool get(size_t requestedSize, MemoryBuffer **memoryBufferOut);

    // Same as get, but ensures new values are initialized to a fixed constant.
    bool getInitialized(size_t requestedSize, MemoryBuffer **memoryBufferOut, uint8_t initValue);

    // Ticks the release counter for the scratch buffer. Also done implicitly in get(). Memory
    // will be returned to the system after tick expiration.
    void tick();

    // clear() the underlying MemoryBuffer, setting size to zero but retaining
    // any allocated memory.
    void clear();

    // destroy() the underlying MemoryBuffer, setting size to zero and freeing
    // any allocated memory.
    void destroy();

    MemoryBuffer *getMemoryBuffer() { return &mScratchMemory; }

  private:
    bool getImpl(size_t requestedSize, MemoryBuffer **memoryBufferOut, Optional<uint8_t> initValue);

    uint32_t mLifetime;
    uint32_t mResetCounter;
    MemoryBuffer mScratchMemory;
};

}  // namespace angle

#endif  // COMMON_MEMORYBUFFER_H_
