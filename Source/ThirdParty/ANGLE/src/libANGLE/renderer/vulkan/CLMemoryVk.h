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

    angle::Result map(uint8_t *&ptrOut, size_t offset = 0);
    void unmap() { unmapImpl(); }

    VkBufferUsageFlags getVkUsageFlags();
    VkMemoryPropertyFlags getVkMemPropertyFlags();
    size_t getSize() const { return mMemory.getSize(); }

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
    bool isMapped() const { return mMappedMemory != nullptr; }

  protected:
    CLMemoryVk(const cl::Memory &memory);

    virtual angle::Result mapImpl() = 0;
    virtual void unmapImpl()        = 0;

    CLContextVk *mContext;
    vk::Renderer *mRenderer;
    vk::Allocation mAllocation;
    angle::SimpleMutex mMapLock;
    uint8_t *mMappedMemory;
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

    bool isCurrentlyInUse() const override;

  private:
    angle::Result mapImpl() override;
    void unmapImpl() override;

    angle::Result setDataImpl(const uint8_t *data, size_t size, size_t offset);

    vk::BufferHelper mBuffer;
    VkBufferCreateInfo mDefaultBufferCreateInfo;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLMEMORYVK_H_
