//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common/MemoryBuffer.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <utility>

#include "common/debug.h"
#include "common/unsafe_buffers.h"

namespace angle
{

// MemoryBuffer implementation.
MemoryBuffer::~MemoryBuffer()
{
    destroy();
}

void MemoryBuffer::destroy()
{
    free(std::exchange(mData, nullptr));
    mSize     = 0;
    mCapacity = 0;
#if defined(ANGLE_ENABLE_ASSERTS)
    mTotalAllocatedBytes = 0;
    mTotalCopiedBytes    = 0;
#endif  // ANGLE_ENABLE_ASSERTS
}

bool MemoryBuffer::resize(size_t newSize)
{
    if (!reserve(newSize))
    {
        return false;
    }
    mSize = newSize;
    return true;
}

bool MemoryBuffer::reserve(size_t newCapacity)
{
    if (newCapacity <= mCapacity)
    {
        // Can already accommodate newCapacity, nothing to do.
        return true;
    }

    uint8_t *newMemory = static_cast<uint8_t *>(malloc(newCapacity));
    if (newMemory == nullptr)
    {
        return false;
    }

// Book keeping
#if defined(ANGLE_ENABLE_ASSERTS)
    mTotalAllocatedBytes += newCapacity;
#endif  // ANGLE_ENABLE_ASSERTS

    if (mSize > 0)
    {
        // Copy the intersection of the old data and the new data.
        // SAFETY: Relies on correct size allocation above.
        ANGLE_UNSAFE_BUFFERS(memcpy(newMemory, mData, mSize));
// Book keeping
#if defined(ANGLE_ENABLE_ASSERTS)
        mTotalCopiedBytes += mSize;
#endif  // ANGLE_ENABLE_ASSERTS
    }

    free(std::exchange(mData, newMemory));
    mCapacity = newCapacity;
    return true;
}

bool MemoryBuffer::clearAndReserve(size_t newCapacity)
{
    clear();
    return reserve(newCapacity);
}

void MemoryBuffer::fill(uint8_t datum)
{
    if (!empty())
    {
        // SAFETY: `mData` is always valid for `mSize`.
        ANGLE_UNSAFE_BUFFERS(std::fill(mData, mData + mSize, datum));
    }
}

MemoryBuffer::MemoryBuffer(MemoryBuffer &&other)
{
    *this = std::move(other);
}

MemoryBuffer &MemoryBuffer::operator=(MemoryBuffer &&other)
{
    std::swap(mSize, other.mSize);
    std::swap(mCapacity, other.mCapacity);
    std::swap(mData, other.mData);
    return *this;
}

namespace
{
static constexpr uint32_t kDefaultScratchBufferLifetime = 1000u;

}  // anonymous namespace

// ScratchBuffer implementation.
ScratchBuffer::ScratchBuffer() : ScratchBuffer(kDefaultScratchBufferLifetime) {}

ScratchBuffer::ScratchBuffer(uint32_t lifetime) : mLifetime(lifetime), mResetCounter(lifetime) {}

ScratchBuffer::~ScratchBuffer() {}

ScratchBuffer::ScratchBuffer(ScratchBuffer &&other)
{
    *this = std::move(other);
}

ScratchBuffer &ScratchBuffer::operator=(ScratchBuffer &&other)
{
    std::swap(mLifetime, other.mLifetime);
    std::swap(mResetCounter, other.mResetCounter);
    std::swap(mScratchMemory, other.mScratchMemory);
    return *this;
}

bool ScratchBuffer::get(size_t requestedSize, MemoryBuffer **memoryBufferOut)
{
    return getImpl(requestedSize, memoryBufferOut, Optional<uint8_t>::Invalid());
}

bool ScratchBuffer::getInitialized(size_t requestedSize,
                                   MemoryBuffer **memoryBufferOut,
                                   uint8_t initValue)
{
    return getImpl(requestedSize, memoryBufferOut, Optional<uint8_t>(initValue));
}

bool ScratchBuffer::getImpl(size_t requestedSize,
                            MemoryBuffer **memoryBufferOut,
                            Optional<uint8_t> initValue)
{
    mScratchMemory.setSizeToCapacity();

    if (mScratchMemory.size() == requestedSize)
    {
        mResetCounter    = mLifetime;
        *memoryBufferOut = &mScratchMemory;
        return true;
    }

    if (mScratchMemory.size() > requestedSize)
    {
        tick();
    }

    if (mScratchMemory.size() < requestedSize)
    {
        if (!mScratchMemory.resize(requestedSize))
        {
            return false;
        }
        mResetCounter = mLifetime;
        if (initValue.valid())
        {
            mScratchMemory.fill(initValue.value());
        }
    }

    ASSERT(mScratchMemory.size() >= requestedSize);

    *memoryBufferOut = &mScratchMemory;
    return true;
}

void ScratchBuffer::tick()
{
    if (mResetCounter > 0)
    {
        --mResetCounter;
        if (mResetCounter == 0)
        {
            destroy();
        }
    }
}

void ScratchBuffer::clear()
{
    mResetCounter = mLifetime;
    mScratchMemory.clear();
}

void ScratchBuffer::destroy()
{
    mScratchMemory.destroy();
}

}  // namespace angle
