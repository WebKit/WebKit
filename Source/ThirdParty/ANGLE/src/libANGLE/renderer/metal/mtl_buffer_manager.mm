//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_buffer_manager.mm:
//    Implements the class methods for BufferManager.
//

#include "libANGLE/renderer/metal/mtl_buffer_manager.h"

#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"

namespace rx
{

namespace mtl
{

namespace
{

constexpr size_t Log2(size_t num)
{
    return num <= 1 ? 0 : (1 + Log2(num / 2));
}

constexpr size_t Log2Ceil(size_t num)
{
    size_t l    = Log2(num);
    size_t size = size_t(1) << l;
    return num == size ? l : l + 1;
}

#ifdef ANGLE_MTL_TRACK_BUFFER_MEM
const char *memUnitSuffix(size_t powerOf2)
{
    if (powerOf2 < 10)
    {
        return "b";
    }
    if (powerOf2 < 20)
    {
        return "k";
    }
    if (powerOf2 < 30)
    {
        return "M";
    }
    return "G";
}

size_t memUnitValue(size_t powerOf2)
{
    if (powerOf2 < 10)
    {
        return 1u << powerOf2;
    }
    if (powerOf2 < 20)
    {
        return 1u << (powerOf2 - 10);
    }
    if (powerOf2 < 30)
    {
        return 1u << (powerOf2 - 20);
    }
    return 1u << (powerOf2 - 30);
}
#endif  // ANGLE_MTL_TRACK_BUFFER_MEM

int storageModeToCacheIndex(MTLStorageMode storageMode)
{
    static_assert(MTLStorageModeShared == 0);
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    static_assert(MTLStorageModeManaged == 1);
#endif
    return static_cast<int>(storageMode);
}

}  // namespace

BufferManager::BufferManager()
#ifdef ANGLE_MTL_TRACK_BUFFER_MEM
    : mAllocations(kMaxSizePowerOf2, 0)
#endif
{}

void BufferManager::freeUnusedBuffers(ContextMtl *contextMtl)
{
    // Scan for the first buffer still in use.
    BufferList::iterator firstInUseIter =
        std::find_if(mInUseBuffers.begin(), mInUseBuffers.end(),
                     [&contextMtl](auto ref) { return ref->isBeingUsedByGPU(contextMtl); });

    // Move unused buffers to the free lists
    for (BufferList::iterator it = mInUseBuffers.begin(); it != firstInUseIter; ++it)
    {
        addBufferRefToFreeLists(*it);
    }
    mInUseBuffers.erase(mInUseBuffers.begin(), firstInUseIter);
}

void BufferManager::addBufferRefToFreeLists(mtl::BufferRef &bufferRef)
{
    int cacheIndex = storageModeToCacheIndex(bufferRef->storageMode());
    ASSERT(cacheIndex < kNumCachedStorageModes);
    const size_t bucketIndex = Log2Ceil(bufferRef->size());
    ASSERT(bucketIndex < kMaxSizePowerOf2);
    mFreeBuffers[cacheIndex][bucketIndex].push_back(bufferRef);
}

void BufferManager::returnBuffer(ContextMtl *contextMtl, BufferRef &bufferRef)
{
    int cacheIndex = storageModeToCacheIndex(bufferRef->storageMode());
    if (cacheIndex >= kNumCachedStorageModes)
    {
        return;  // Storage mode that we do not have a cache for.
    }
    if (bufferRef->isBeingUsedByGPU(contextMtl))
    {
        mInUseBuffers.push_back(bufferRef);
    }
    else
    {
        addBufferRefToFreeLists(bufferRef);
    }
}

angle::Result BufferManager::getBuffer(ContextMtl *contextMtl,
                                       MTLStorageMode storageMode,
                                       size_t size,
                                       BufferRef &bufferRef)
{
    freeUnusedBuffers(contextMtl);
    const size_t bucketIndex = Log2Ceil(size);
    const int cacheIndex     = storageModeToCacheIndex(storageMode);
    if (cacheIndex < kNumCachedStorageModes)
    {
        // Buffer has a storage mode that have a cache for.
        BufferList &freeBuffers = mFreeBuffers[cacheIndex][bucketIndex];
        if (!freeBuffers.empty())
        {
            bufferRef = freeBuffers.back();
            freeBuffers.pop_back();
            return angle::Result::Continue;
        }
    }

    // Create a new one
    mtl::BufferRef newBufferRef;

    size_t allocSize = size_t(1) << bucketIndex;
    ASSERT(allocSize >= size);
    ANGLE_TRY(mtl::Buffer::MakeBufferWithStorageMode(contextMtl, storageMode, allocSize, nullptr,
                                                     &newBufferRef));

#ifdef ANGLE_MTL_TRACK_BUFFER_MEM
    {
        mTotalMem += allocSize;
        mAllocations[bucketIndex]++;
        fprintf(stderr, "totalMem: %zu, ", mTotalMem);
        size_t numBuffers = 0;
        for (size_t i = 0; i < kMaxSizePowerOf2; ++i)
        {
            if (mAllocations[i])
            {
                numBuffers += mAllocations[i];
                fprintf(stderr, "%zu%s: %zu, ", memUnitValue(i), memUnitSuffix(i), mAllocations[i]);
            }
        }
        fprintf(stderr, " total: %zu\n", numBuffers);
    }
#endif

    bufferRef = newBufferRef;

    return angle::Result::Continue;
}

angle::Result BufferManager::queueBlitCopyDataToBuffer(ContextMtl *contextMtl,
                                                       const void *srcPtr,
                                                       size_t sizeToCopy,
                                                       size_t offset,
                                                       mtl::BufferRef &dstMetalBuffer)
{
    const uint8_t *src = reinterpret_cast<const uint8_t *>(srcPtr);

    for (size_t srcOffset = 0; srcOffset < sizeToCopy; srcOffset += kMaxStagingBufferSize)
    {
        size_t subSizeToCopy = std::min(kMaxStagingBufferSize, sizeToCopy - srcOffset);

        mtl::BufferRef bufferRef;
        // TODO(anglebug.com/7544): Here we pass DynamicDraw to get managed buffer for the
        // operation. This should be checked to see if this makes sense.
        auto storageMode = Buffer::getStorageModeForUsage(contextMtl, Buffer::Usage::DynamicDraw);
        ANGLE_TRY(getBuffer(contextMtl, storageMode, subSizeToCopy, bufferRef));

        // copy data to buffer
        uint8_t *ptr = bufferRef->mapWithOpt(contextMtl, false, true);
        std::copy(src + srcOffset, src + srcOffset + subSizeToCopy, ptr);
        bufferRef->unmapAndFlushSubset(contextMtl, 0, subSizeToCopy);

        // queue blit
        mtl::BlitCommandEncoder *blitEncoder = contextMtl->getBlitCommandEncoder();
        blitEncoder->copyBuffer(bufferRef, 0, dstMetalBuffer, offset + srcOffset, subSizeToCopy);

        returnBuffer(contextMtl, bufferRef);
    }
    return angle::Result::Continue;
}

}  // namespace mtl
}  // namespace rx
