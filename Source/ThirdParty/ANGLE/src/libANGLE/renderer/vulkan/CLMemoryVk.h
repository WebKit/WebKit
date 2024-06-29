//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryVk.h: Defines the class interface for CLMemoryVk, implementing CLMemoryImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLMEMORYVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLMEMORYVK_H_

#include "common/SimpleMutex.h"

#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

#include "libANGLE/renderer/CLMemoryImpl.h"

#include "libANGLE/CLMemory.h"

namespace rx
{

class CLMemoryVk : public CLMemoryImpl
{
  public:
    ~CLMemoryVk() override;

    // TODO: http://anglebug.com/42267017
    angle::Result createSubBuffer(const cl::Buffer &buffer,
                                  cl::MemFlags flags,
                                  size_t size,
                                  CLMemoryImpl::Ptr *subBufferOut) override = 0;

    virtual vk::BufferHelper &getBuffer() = 0;
    angle::Result getMapPtr(uint8_t **mapPtrOut);

    VkBufferUsageFlags getVkUsageFlags();
    VkMemoryPropertyFlags getVkMemPropertyFlags();

    virtual angle::Result map()   = 0;
    virtual angle::Result unmap() = 0;
    angle::Result copyTo(void *ptr, size_t offset, size_t size);
    angle::Result copyTo(CLMemoryVk *dst, size_t srcOffset, size_t dstOffset, size_t size);
    angle::Result copyFrom(const void *ptr, size_t offset, size_t size);

    bool isWritable()
    {
        constexpr VkBufferUsageFlags kWritableUsage =
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        return (getVkUsageFlags() & kWritableUsage) != 0;
    }

    virtual bool isCurrentlyInUse() const = 0;
    virtual size_t getSize() const        = 0;

  protected:
    CLMemoryVk(const cl::Memory &memory);

    CLContextVk *mContext;
    vk::Renderer *mRenderer;
    vk::Allocation mAllocation;
    angle::SimpleMutex mMapLock;
    uint8_t *mMapPtr;
    uint32_t mMapCount;
    CLMemoryVk *mParent;
};

class CLBufferVk : public CLMemoryVk
{
  public:
    CLBufferVk(const cl::Buffer &buffer);
    ~CLBufferVk() override;

    angle::Result createSubBuffer(const cl::Buffer &buffer,
                                  cl::MemFlags flags,
                                  size_t size,
                                  CLMemoryImpl::Ptr *subBufferOut) override;

    vk::BufferHelper &getBuffer() override { return mBuffer; }
    CLBufferVk *getParent() { return static_cast<CLBufferVk *>(mParent); }

    angle::Result create(void *hostPtr);

    bool isSubBuffer() const { return mParent != nullptr; }

    angle::Result map() override;
    angle::Result unmap() override;

    bool isCurrentlyInUse() const override;
    size_t getSize() const override { return mMemory.getSize(); }

  private:
    angle::Result setDataImpl(const uint8_t *data, size_t size, size_t offset);

    vk::BufferHelper mBuffer;
    VkBufferCreateInfo mDefaultBufferCreateInfo;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLMEMORYVK_H_
