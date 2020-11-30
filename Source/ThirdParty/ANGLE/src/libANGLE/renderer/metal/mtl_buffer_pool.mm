//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_buffer_pool.mm:
//    Implements the class methods for BufferPool.
//

#include "libANGLE/renderer/metal/mtl_buffer_pool.h"

#include "libANGLE/renderer/metal/ContextMtl.h"

namespace rx
{

namespace mtl
{

// BufferPool implementation.
BufferPool::BufferPool(bool alwaysAllocNewBuffer)
    : mInitialSize(0),
      mBuffer(nullptr),
      mNextAllocationOffset(0),
      mSize(0),
      mAlignment(1),
      mBuffersAllocated(0),
      mMaxBuffers(0),
      mAlwaysAllocateNewBuffer(alwaysAllocNewBuffer)

{}

void BufferPool::initialize(ContextMtl *contextMtl,
                            size_t initialSize,
                            size_t alignment,
                            size_t maxBuffers)
{
    destroy(contextMtl);

    mInitialSize = initialSize;
    mSize        = 0;

    mMaxBuffers = maxBuffers;

    updateAlignment(contextMtl, alignment);
}

BufferPool::~BufferPool() {}

angle::Result BufferPool::allocateNewBuffer(ContextMtl *contextMtl)
{
    if (mMaxBuffers > 0 && mBuffersAllocated >= mMaxBuffers)
    {
        // We reach the max number of buffers allowed.
        // Try to deallocate old and smaller size inflight buffers.
        releaseInFlightBuffers(contextMtl);
    }

    if (mMaxBuffers > 0 && mBuffersAllocated >= mMaxBuffers)
    {
        // If we reach this point, it means there was no buffer deallocated inside
        // releaseInFlightBuffers() thus, the number of buffers allocated still exceeds number
        // allowed.
        ASSERT(!mBufferFreeList.empty());

        // Reuse the buffer in free list:
        if (mBufferFreeList.front()->isBeingUsedByGPU(contextMtl))
        {
            contextMtl->flushCommandBufer();
            // Force the GPU to finish its rendering and make the old buffer available.
            contextMtl->cmdQueue().ensureResourceReadyForCPU(mBufferFreeList.front());
        }

        mBuffer = mBufferFreeList.front();
        mBufferFreeList.erase(mBufferFreeList.begin());

        return angle::Result::Continue;
    }

    ANGLE_TRY(Buffer::MakeBuffer(contextMtl, mSize, nullptr, &mBuffer));

    ASSERT(mBuffer);

    mBuffersAllocated++;

    return angle::Result::Continue;
}

angle::Result BufferPool::allocate(ContextMtl *contextMtl,
                                   size_t sizeInBytes,
                                   uint8_t **ptrOut,
                                   BufferRef *bufferOut,
                                   size_t *offsetOut,
                                   bool *newBufferAllocatedOut)
{
    size_t sizeToAllocate = roundUp(sizeInBytes, mAlignment);

    angle::base::CheckedNumeric<size_t> checkedNextWriteOffset = mNextAllocationOffset;
    checkedNextWriteOffset += sizeToAllocate;

    if (!mBuffer || !checkedNextWriteOffset.IsValid() ||
        checkedNextWriteOffset.ValueOrDie() >= mSize || mAlwaysAllocateNewBuffer)
    {
        if (mBuffer)
        {
            ANGLE_TRY(commit(contextMtl));
        }

        if (sizeToAllocate > mSize)
        {
            mSize = std::max(mInitialSize, sizeToAllocate);

            // Clear the free list since the free buffers are now too small.
            destroyBufferList(contextMtl, &mBufferFreeList);
        }

        // The front of the free list should be the oldest. Thus if it is in use the rest of the
        // free list should be in use as well.
        if (mBufferFreeList.empty() || mBufferFreeList.front()->isBeingUsedByGPU(contextMtl))
        {
            ANGLE_TRY(allocateNewBuffer(contextMtl));
        }
        else
        {
            mBuffer = mBufferFreeList.front();
            mBufferFreeList.erase(mBufferFreeList.begin());
        }

        ASSERT(mBuffer->size() == mSize);

        mNextAllocationOffset = 0;

        if (newBufferAllocatedOut != nullptr)
        {
            *newBufferAllocatedOut = true;
        }
    }
    else if (newBufferAllocatedOut != nullptr)
    {
        *newBufferAllocatedOut = false;
    }

    ASSERT(mBuffer != nullptr);

    if (bufferOut != nullptr)
    {
        *bufferOut = mBuffer;
    }

    // Optionally map() the buffer if possible
    if (ptrOut)
    {
        *ptrOut = mBuffer->map(contextMtl) + mNextAllocationOffset;
    }

    if (offsetOut)
    {
        *offsetOut = static_cast<size_t>(mNextAllocationOffset);
    }
    mNextAllocationOffset += static_cast<uint32_t>(sizeToAllocate);
    return angle::Result::Continue;
}

angle::Result BufferPool::commit(ContextMtl *contextMtl)
{
    if (mBuffer)
    {
        mBuffer->unmap(contextMtl);

        mInFlightBuffers.push_back(mBuffer);
        mBuffer = nullptr;
    }

    mNextAllocationOffset = 0;

    return angle::Result::Continue;
}

void BufferPool::releaseInFlightBuffers(ContextMtl *contextMtl)
{
    for (auto &toRelease : mInFlightBuffers)
    {
        // If the dynamic buffer was resized we cannot reuse the retained buffer.
        if (toRelease->size() < mSize)
        {
            toRelease = nullptr;
            mBuffersAllocated--;
        }
        else
        {
            mBufferFreeList.push_back(toRelease);
        }
    }

    mInFlightBuffers.clear();
}

void BufferPool::destroyBufferList(ContextMtl *contextMtl, std::vector<BufferRef> *buffers)
{
    ASSERT(mBuffersAllocated >= buffers->size());
    mBuffersAllocated -= buffers->size();
    buffers->clear();
}

void BufferPool::destroy(ContextMtl *contextMtl)
{
    destroyBufferList(contextMtl, &mInFlightBuffers);
    destroyBufferList(contextMtl, &mBufferFreeList);

    reset();

    if (mBuffer)
    {
        mBuffer->unmap(contextMtl);

        mBuffer = nullptr;
    }
}

void BufferPool::updateAlignment(ContextMtl *contextMtl, size_t alignment)
{
    ASSERT(alignment > 0);

    // NOTE(hqle): May check additional platform limits.

    mAlignment = alignment;
}

void BufferPool::reset()
{
    mSize                    = 0;
    mNextAllocationOffset    = 0;
    mMaxBuffers              = 0;
    mAlwaysAllocateNewBuffer = false;
    mBuffersAllocated        = 0;
}
}
}
