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
#include "vulkan/vulkan_core.h"

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
                                  CLMemoryImpl::Ptr *subBufferOut) override;

    angle::Result map(uint8_t *&ptrOut, size_t offset = 0);
    void unmap() { unmapImpl(); }

    VkBufferUsageFlags getVkUsageFlags();
    VkMemoryPropertyFlags getVkMemPropertyFlags();
    virtual size_t getSize() const = 0;

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

    vk::BufferHelper &getBuffer() { return mBuffer; }
    CLBufferVk *getParent() { return static_cast<CLBufferVk *>(mParent); }

    angle::Result create(void *hostPtr);

    bool isSubBuffer() const { return mParent != nullptr; }

    bool isCurrentlyInUse() const override;
    size_t getSize() const override { return mMemory.getSize(); }

  private:
    angle::Result mapImpl() override;
    void unmapImpl() override;

    angle::Result setDataImpl(const uint8_t *data, size_t size, size_t offset);

    vk::BufferHelper mBuffer;
    VkBufferCreateInfo mDefaultBufferCreateInfo;
};

class CLImageVk : public CLMemoryVk
{
  public:
    CLImageVk(const cl::Image &image);
    ~CLImageVk() override;

    vk::ImageHelper &getImage() { return mImage; }
    vk::BufferHelper &getStagingBuffer() { return mStagingBuffer; }

    angle::Result create(void *hostPtr);

    bool isCurrentlyInUse() const override;
    bool isMapped() { return mMappedMemory != nullptr; }
    bool containsHostMemExtension();

    angle::Result createStagingBuffer(size_t size);
    angle::Result copyStagingFrom(void *ptr, size_t offset, size_t size);
    VkImageUsageFlags getVkImageUsageFlags();
    VkImageType getVkImageType(const cl::ImageDescriptor &desc);
    size_t getSize() const override { return mImageSize; }

  private:
    angle::Result mapImpl() override;
    void unmapImpl() override;
    angle::Result setDataImpl(const uint8_t *data, size_t size, size_t offset);

    vk::BufferHelper mStagingBuffer;
    vk::ImageHelper mImage;
    VkExtent3D mExtent;
    angle::FormatID mFormat;
    uint32_t mArrayLayers;
    size_t mImageSize;
    size_t mNumberChannels;
    size_t mBytesPerChannel;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLMEMORYVK_H_
