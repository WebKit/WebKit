//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryVk.cpp: Implements the class methods for CLMemoryVk.

#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLMemory.h"
#include "libANGLE/cl_utils.h"

namespace rx
{

CLMemoryVk::CLMemoryVk(const cl::Memory &memory)
    : CLMemoryImpl(memory),
      mContext(&memory.getContext().getImpl<CLContextVk>()),
      mRenderer(mContext->getRenderer()),
      mMapPtr(nullptr),
      mMapCount(0),
      mParent(nullptr)
{}

CLMemoryVk::~CLMemoryVk()
{
    mContext->mAssociatedObjects->mMemories.erase(mMemory.getNative());
}

angle::Result CLMemoryVk::createSubBuffer(const cl::Buffer &buffer,
                                          cl::MemFlags flags,
                                          size_t size,
                                          CLMemoryImpl::Ptr *subBufferOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLMemoryVk::getMapPtr(uint8_t **mapPtrOut)
{
    if (mMapPtr == nullptr && mapPtrOut != nullptr)
    {
        ANGLE_TRY(map());
        *mapPtrOut = mMapPtr;
    }
    return angle::Result::Continue;
}

VkBufferUsageFlags CLMemoryVk::getVkUsageFlags()
{
    VkBufferUsageFlags usageFlags =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (mMemory.getFlags().isSet(CL_MEM_WRITE_ONLY))
    {
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    else if (mMemory.getFlags().isSet(CL_MEM_READ_ONLY))
    {
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    else
    {
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    return usageFlags;
}

VkMemoryPropertyFlags CLMemoryVk::getVkMemPropertyFlags()
{
    // TODO: http://anglebug.com/42267018
    VkMemoryPropertyFlags propFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (mMemory.getFlags().isSet(CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR |
                                 CL_MEM_COPY_HOST_PTR))
    {
        propFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    return propFlags;
}

angle::Result CLMemoryVk::copyTo(void *dst, size_t srcOffset, size_t size)
{
    ANGLE_TRY(map());
    void *src = reinterpret_cast<void *>(mMapPtr + srcOffset);
    std::memcpy(dst, src, size);
    ANGLE_TRY(unmap());
    return angle::Result::Continue;
}

angle::Result CLMemoryVk::copyTo(CLMemoryVk *dst, size_t srcOffset, size_t dstOffset, size_t size)
{
    ANGLE_TRY(map());
    ANGLE_TRY(dst->map());
    uint8_t *ptr = nullptr;
    ANGLE_TRY(dst->getMapPtr(&ptr));
    ANGLE_TRY(copyTo(reinterpret_cast<void *>(ptr + dstOffset), srcOffset, size));
    ANGLE_TRY(unmap());
    ANGLE_TRY(dst->unmap());
    return angle::Result::Continue;
}

angle::Result CLMemoryVk::copyFrom(const void *src, size_t srcOffset, size_t size)
{
    ANGLE_TRY(map());
    void *dst = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(mMapPtr) + srcOffset);
    std::memcpy(dst, src, size);
    ANGLE_TRY(unmap());
    return angle::Result::Continue;
}

CLBufferVk::CLBufferVk(const cl::Buffer &buffer) : CLMemoryVk(buffer)
{
    if (buffer.isSubBuffer())
    {
        mParent = &buffer.getParent()->getImpl<CLBufferVk>();
    }
    mDefaultBufferCreateInfo             = {};
    mDefaultBufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    mDefaultBufferCreateInfo.size        = buffer.getSize();
    mDefaultBufferCreateInfo.usage       = getVkUsageFlags();
    mDefaultBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
}

CLBufferVk::~CLBufferVk()
{
    ANGLE_CL_IMPL_TRY(unmap());
    mBuffer.destroy(mRenderer);
}

angle::Result CLBufferVk::createSubBuffer(const cl::Buffer &buffer,
                                          cl::MemFlags flags,
                                          size_t size,
                                          CLMemoryImpl::Ptr *subBufferOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLBufferVk::create(void *hostPtr)
{
    if (!isSubBuffer())
    {
        VkBufferCreateInfo createInfo  = mDefaultBufferCreateInfo;
        createInfo.size                = getSize();
        VkMemoryPropertyFlags memFlags = getVkMemPropertyFlags();
        if (IsError(mBuffer.init(mContext, createInfo, memFlags)))
        {
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
        }
        if (mMemory.getFlags().isSet(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR))
        {
            ASSERT(hostPtr);
            ANGLE_CL_IMPL_TRY_ERROR(setDataImpl(static_cast<uint8_t *>(hostPtr), getSize(), 0),
                                    CL_OUT_OF_RESOURCES);
        }
    }
    return angle::Result::Continue;
}

angle::Result CLBufferVk::map()
{
    if (mParent != nullptr)
    {
        ANGLE_TRY(mParent->getMapPtr(&mMapPtr));
        if (mMapPtr != nullptr)
        {
            mMapPtr += mBuffer.getOffset();
            return angle::Result::Continue;
        }
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    if (!IsError(mBuffer.map(mContext, &mMapPtr)))
    {
        return angle::Result::Continue;
    }

    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLBufferVk::unmap()
{
    if (mParent != nullptr)
    {
        ANGLE_CL_IMPL_TRY_ERROR(mParent->unmap(), CL_OUT_OF_RESOURCES);
        return angle::Result::Continue;
    }
    mBuffer.unmap(mRenderer);
    mMapPtr = nullptr;
    return angle::Result::Continue;
}

angle::Result CLBufferVk::setDataImpl(const uint8_t *data, size_t size, size_t offset)
{
    // buffer cannot be in use state
    ASSERT(mBuffer.valid());
    ASSERT(!isCurrentlyInUse());
    ASSERT(size + offset <= getSize());
    ASSERT(data != nullptr);

    // Assuming host visible buffers for now
    // TODO: http://anglebug.com/42267019
    if (!mBuffer.isHostVisible())
    {
        UNIMPLEMENTED();
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(mBuffer.mapWithOffset(mContext, &mapPointer, offset));
    ASSERT(mapPointer != nullptr);

    std::memcpy(mapPointer, data, size);
    mBuffer.unmap(mRenderer);

    return angle::Result::Continue;
}

bool CLBufferVk::isCurrentlyInUse() const
{
    return !mRenderer->hasResourceUseFinished(mBuffer.getResourceUse());
}

}  // namespace rx
