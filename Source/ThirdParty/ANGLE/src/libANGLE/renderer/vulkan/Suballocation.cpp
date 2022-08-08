//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Suballocation.cpp:
//    Implements class methods for BufferBlock and Suballocation and other related classes
//

// #include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/renderer/vulkan/Suballocation.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_mem_alloc_wrapper.h"

namespace rx
{
namespace vk
{
// BufferBlock implementation.
BufferBlock::BufferBlock() : mMemoryPropertyFlags(0), mSize(0), mMappedMemory(nullptr) {}

BufferBlock::BufferBlock(BufferBlock &&other)
    : mVirtualBlock(std::move(other.mVirtualBlock)),
      mBuffer(std::move(other.mBuffer)),
      mDeviceMemory(std::move(other.mDeviceMemory)),
      mMemoryPropertyFlags(other.mMemoryPropertyFlags),
      mSize(other.mSize),
      mMappedMemory(other.mMappedMemory),
      mSerial(other.mSerial),
      mCountRemainsEmpty(0)
{}

BufferBlock &BufferBlock::operator=(BufferBlock &&other)
{
    std::swap(mVirtualBlock, other.mVirtualBlock);
    std::swap(mBuffer, other.mBuffer);
    std::swap(mDeviceMemory, other.mDeviceMemory);
    std::swap(mMemoryPropertyFlags, other.mMemoryPropertyFlags);
    std::swap(mSize, other.mSize);
    std::swap(mMappedMemory, other.mMappedMemory);
    std::swap(mSerial, other.mSerial);
    std::swap(mCountRemainsEmpty, other.mCountRemainsEmpty);
    return *this;
}

BufferBlock::~BufferBlock()
{
    ASSERT(!mVirtualBlock.valid());
    ASSERT(!mBuffer.valid());
    ASSERT(!mDeviceMemory.valid());
    ASSERT(mDescriptorSetCacheManager.empty());
}

void BufferBlock::destroy(RendererVk *renderer)
{
    VkDevice device = renderer->getDevice();

    mDescriptorSetCacheManager.destroyKeys(renderer);
    if (mMappedMemory)
    {
        unmap(device);
    }

    mVirtualBlock.destroy(device);
    mBuffer.destroy(device);
    mDeviceMemory.destroy(device);
}

angle::Result BufferBlock::init(Context *context,
                                Buffer &buffer,
                                vma::VirtualBlockCreateFlags flags,
                                DeviceMemory &deviceMemory,
                                VkMemoryPropertyFlags memoryPropertyFlags,
                                VkDeviceSize size)
{
    RendererVk *renderer = context->getRenderer();
    ASSERT(!mVirtualBlock.valid());
    ASSERT(!mBuffer.valid());
    ASSERT(!mDeviceMemory.valid());

    mVirtualBlockMutex.init(renderer->isAsyncCommandQueueEnabled());
    ANGLE_VK_TRY(context, mVirtualBlock.init(renderer->getDevice(), flags, size));

    mBuffer              = std::move(buffer);
    mDeviceMemory        = std::move(deviceMemory);
    mMemoryPropertyFlags = memoryPropertyFlags;
    mSize                = size;
    mMappedMemory        = nullptr;
    mSerial              = renderer->getResourceSerialFactory().generateBufferSerial();

    return angle::Result::Continue;
}

void BufferBlock::initWithoutVirtualBlock(Context *context,
                                          Buffer &buffer,
                                          DeviceMemory &deviceMemory,
                                          VkMemoryPropertyFlags memoryPropertyFlags,
                                          VkDeviceSize size)
{
    RendererVk *renderer = context->getRenderer();
    ASSERT(!mVirtualBlock.valid());
    ASSERT(!mBuffer.valid());
    ASSERT(!mDeviceMemory.valid());

    mBuffer              = std::move(buffer);
    mDeviceMemory        = std::move(deviceMemory);
    mMemoryPropertyFlags = memoryPropertyFlags;
    mSize                = size;
    mMappedMemory        = nullptr;
    mSerial              = renderer->getResourceSerialFactory().generateBufferSerial();
}

VkResult BufferBlock::map(const VkDevice device)
{
    ASSERT(mMappedMemory == nullptr);
    return mDeviceMemory.map(device, 0, mSize, 0, &mMappedMemory);
}

void BufferBlock::unmap(const VkDevice device)
{
    mDeviceMemory.unmap(device);
    mMappedMemory = nullptr;
}

void BufferBlock::free(VmaVirtualAllocation allocation, VkDeviceSize offset)
{
    std::lock_guard<ConditionalMutex> lock(mVirtualBlockMutex);
    mVirtualBlock.free(allocation, offset);
}

int32_t BufferBlock::getAndIncrementEmptyCounter()
{
    return ++mCountRemainsEmpty;
}

void BufferBlock::calculateStats(vma::StatInfo *pStatInfo) const
{
    std::lock_guard<ConditionalMutex> lock(mVirtualBlockMutex);
    mVirtualBlock.calculateStats(pStatInfo);
}

// BufferSuballocation implementation.
VkResult BufferSuballocation::map(Context *context)
{
    return mBufferBlock->map(context->getDevice());
}

// SharedBufferSuballocationGarbage implementation.
bool SharedBufferSuballocationGarbage::destroyIfComplete(RendererVk *renderer,
                                                         Serial completedSerial)
{
    if (mLifetime.isCurrentlyInUse(completedSerial))
    {
        return false;
    }

    mBuffer.destroy(renderer->getDevice());
    mSuballocation.destroy(renderer);
    mLifetime.release();
    return true;
}
}  // namespace vk
}  // namespace rx
