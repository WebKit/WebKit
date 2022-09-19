//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Suballocation.h:
//    Defines class interface for BufferBlock and Suballocation and other related classes.
//

#ifndef LIBANGLE_RENDERER_VULKAN_SUBALLOCATION_H_
#define LIBANGLE_RENDERER_VULKAN_SUBALLOCATION_H_

#include "common/debug.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/serial_utils.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

namespace rx
{
class RendererVk;

namespace vk
{
class Context;

// BufferBlock
class BufferBlock final : angle::NonCopyable
{
  public:
    BufferBlock();
    BufferBlock(BufferBlock &&other);
    ~BufferBlock();

    void destroy(RendererVk *renderer);
    angle::Result init(Context *context,
                       Buffer &buffer,
                       vma::VirtualBlockCreateFlags flags,
                       DeviceMemory &deviceMemory,
                       VkMemoryPropertyFlags memoryPropertyFlags,
                       VkDeviceSize size);
    void initWithoutVirtualBlock(Context *context,
                                 Buffer &buffer,
                                 DeviceMemory &deviceMemory,
                                 VkMemoryPropertyFlags memoryPropertyFlags,
                                 VkDeviceSize size);

    BufferBlock &operator=(BufferBlock &&other);

    const Buffer &getBuffer() const { return mBuffer; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }
    DeviceMemory &getDeviceMemory() { return mDeviceMemory; }
    BufferSerial getBufferSerial() const { return mSerial; }

    VkMemoryPropertyFlags getMemoryPropertyFlags() const;
    VkDeviceSize getMemorySize() const;

    VkResult allocate(VkDeviceSize size,
                      VkDeviceSize alignment,
                      VmaVirtualAllocation *allocationOut,
                      VkDeviceSize *offsetOut);
    void free(VmaVirtualAllocation allocation, VkDeviceSize offset);
    VkBool32 isEmpty();

    bool hasVirtualBlock() const { return mVirtualBlock.valid(); }
    bool isHostVisible() const;
    bool isCoherent() const;
    bool isMapped() const;
    VkResult map(const VkDevice device);
    void unmap(const VkDevice device);
    uint8_t *getMappedMemory() const;

    // This should be called whenever this found to be empty. The total number of count of empty is
    // returned.
    int32_t getAndIncrementEmptyCounter();
    void calculateStats(vma::StatInfo *pStatInfo) const;

    void onNewDescriptorSet(const SharedDescriptorSetCacheKey &sharedCacheKey)
    {
        mDescriptorSetCacheManager.addKey(sharedCacheKey);
    }

  private:
    mutable std::mutex mVirtualBlockMutex;
    VirtualBlock mVirtualBlock;

    Buffer mBuffer;
    DeviceMemory mDeviceMemory;
    VkMemoryPropertyFlags mMemoryPropertyFlags;
    VkDeviceSize mSize;
    uint8_t *mMappedMemory;
    BufferSerial mSerial;
    // Heuristic information for pruneEmptyBuffer. This tracks how many times (consecutively) this
    // buffer block is found to be empty when pruneEmptyBuffer is called. This gets reset whenever
    // it becomes non-empty.
    int32_t mCountRemainsEmpty;
    // Manages the descriptorSet cache that created with this BufferBlock.
    DescriptorSetCacheManager mDescriptorSetCacheManager;
};
using BufferBlockPointerVector = std::vector<std::unique_ptr<BufferBlock>>;

// BufferSuballocation
class BufferSuballocation final : angle::NonCopyable
{
  public:
    BufferSuballocation();

    BufferSuballocation(BufferSuballocation &&other);
    BufferSuballocation &operator=(BufferSuballocation &&other);

    void destroy(RendererVk *renderer);

    void init(VkDevice device,
              BufferBlock *block,
              VmaVirtualAllocation allocation,
              VkDeviceSize offset,
              VkDeviceSize size);
    void initWithEntireBuffer(Context *context,
                              Buffer &buffer,
                              DeviceMemory &deviceMemory,
                              VkMemoryPropertyFlags memoryPropertyFlags,
                              VkDeviceSize size);

    const Buffer &getBuffer() const;
    VkDeviceSize getSize() const;
    const DeviceMemory &getDeviceMemory() const;
    VkMemoryMapFlags getMemoryPropertyFlags() const;
    bool isHostVisible() const;
    bool isCoherent() const;
    bool isMapped() const;
    uint8_t *getMappedMemory() const;
    void flush(const VkDevice &device);
    void invalidate(const VkDevice &device);
    VkDeviceSize getOffset() const;
    bool valid() const;
    VkResult map(Context *context);
    BufferSerial getBlockSerial() const;
    uint8_t *getBlockMemory() const;
    VkDeviceSize getBlockMemorySize() const;
    bool isSuballocated() const { return mBufferBlock->hasVirtualBlock(); }
    BufferBlock *getBufferBlock() const { return mBufferBlock; }

  private:
    // Only used by DynamicBuffer where DynamicBuffer does the actual suballocation and pass the
    // offset/size to this object. Since DynamicBuffer does not have a VMA virtual allocator, they
    // will be ignored at destroy time. The offset/size is set here mainly for easy retrieval when
    // the BufferHelper object is passed around.
    friend class BufferHelper;
    void setOffsetAndSize(VkDeviceSize offset, VkDeviceSize size);

    BufferBlock *mBufferBlock;
    VmaVirtualAllocation mAllocation;
    VkDeviceSize mOffset;
    VkDeviceSize mSize;
};

class SharedBufferSuballocationGarbage
{
  public:
    SharedBufferSuballocationGarbage() = default;
    SharedBufferSuballocationGarbage(SharedBufferSuballocationGarbage &&other)
        : mLifetime(std::move(other.mLifetime)),
          mSuballocation(std::move(other.mSuballocation)),
          mBuffer(std::move(other.mBuffer))
    {}
    SharedBufferSuballocationGarbage(SharedResourceUse &&use,
                                     BufferSuballocation &&suballocation,
                                     Buffer &&buffer)
        : mLifetime(std::move(use)),
          mSuballocation(std::move(suballocation)),
          mBuffer(std::move(buffer))
    {}
    ~SharedBufferSuballocationGarbage() = default;

    bool destroyIfComplete(RendererVk *renderer, Serial completedSerial);
    bool usedInRecordedCommands() const { return mLifetime.usedInRecordedCommands(); }
    VkDeviceSize getSize() const { return mSuballocation.getSize(); }
    bool isSuballocated() const { return mSuballocation.isSuballocated(); }

  private:
    SharedResourceUse mLifetime;
    BufferSuballocation mSuballocation;
    Buffer mBuffer;
};
using SharedBufferSuballocationGarbageList = std::queue<SharedBufferSuballocationGarbage>;

// BufferBlock implementation.
ANGLE_INLINE VkMemoryPropertyFlags BufferBlock::getMemoryPropertyFlags() const
{
    return mMemoryPropertyFlags;
}

ANGLE_INLINE VkDeviceSize BufferBlock::getMemorySize() const
{
    return mSize;
}

ANGLE_INLINE VkBool32 BufferBlock::isEmpty()
{
    std::unique_lock<std::mutex> lock(mVirtualBlockMutex);
    return vma::IsVirtualBlockEmpty(mVirtualBlock.getHandle());
}

ANGLE_INLINE bool BufferBlock::isHostVisible() const
{
    return (mMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
}

ANGLE_INLINE bool BufferBlock::isCoherent() const
{
    return (mMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
}

ANGLE_INLINE bool BufferBlock::isMapped() const
{
    return mMappedMemory != nullptr;
}

ANGLE_INLINE uint8_t *BufferBlock::getMappedMemory() const
{
    ASSERT(mMappedMemory != nullptr);
    return mMappedMemory;
}

ANGLE_INLINE VkResult BufferBlock::allocate(VkDeviceSize size,
                                            VkDeviceSize alignment,
                                            VmaVirtualAllocation *allocationOut,
                                            VkDeviceSize *offsetOut)
{
    std::unique_lock<std::mutex> lock(mVirtualBlockMutex);
    mCountRemainsEmpty = 0;
    return mVirtualBlock.allocate(size, alignment, allocationOut, offsetOut);
}

// BufferSuballocation implementation.
ANGLE_INLINE BufferSuballocation::BufferSuballocation()
    : mBufferBlock(nullptr), mAllocation(VK_NULL_HANDLE), mOffset(0), mSize(0)
{}

ANGLE_INLINE BufferSuballocation::BufferSuballocation(BufferSuballocation &&other)
    : BufferSuballocation()
{
    *this = std::move(other);
}

ANGLE_INLINE BufferSuballocation &BufferSuballocation::operator=(BufferSuballocation &&other)
{
    std::swap(mBufferBlock, other.mBufferBlock);
    std::swap(mSize, other.mSize);
    std::swap(mAllocation, other.mAllocation);
    std::swap(mOffset, other.mOffset);
    return *this;
}

ANGLE_INLINE bool BufferSuballocation::valid() const
{
    return mBufferBlock != nullptr;
}

ANGLE_INLINE void BufferSuballocation::destroy(RendererVk *renderer)
{
    if (valid())
    {
        ASSERT(mBufferBlock);
        if (mBufferBlock->hasVirtualBlock())
        {
            mBufferBlock->free(mAllocation, mOffset);
            mBufferBlock = nullptr;
        }
        else
        {
            // When virtual block is invalid, this is the standalone buffer that are created by
            // BufferSuballocation::initWithEntireBuffer call. In this case, vmaBufferSuballocation
            // owns block, we must properly delete the block object.
            mBufferBlock->destroy(renderer);
            SafeDelete(mBufferBlock);
        }
        mAllocation = VK_NULL_HANDLE;
        mOffset     = 0;
        mSize       = 0;
    }
}

ANGLE_INLINE void BufferSuballocation::init(VkDevice device,
                                            BufferBlock *block,
                                            VmaVirtualAllocation allocation,
                                            VkDeviceSize offset,
                                            VkDeviceSize size)
{
    ASSERT(!valid());
    ASSERT(block != nullptr);
#if ANGLE_VMA_VERSION >= 3000000
    ASSERT(allocation != VK_NULL_HANDLE);
#endif  // ANGLE_VMA_VERSION >= 3000000
    ASSERT(offset != VK_WHOLE_SIZE);
    mBufferBlock = block;
    mAllocation  = allocation;
    mOffset      = offset;
    mSize        = size;
}

ANGLE_INLINE void BufferSuballocation::initWithEntireBuffer(
    Context *context,
    Buffer &buffer,
    DeviceMemory &deviceMemory,
    VkMemoryPropertyFlags memoryPropertyFlags,
    VkDeviceSize size)
{
    ASSERT(!valid());

    std::unique_ptr<BufferBlock> block = std::make_unique<BufferBlock>();
    block->initWithoutVirtualBlock(context, buffer, deviceMemory, memoryPropertyFlags, size);

    mBufferBlock = block.release();
    mAllocation  = VK_NULL_HANDLE;
    mOffset      = 0;
    mSize        = mBufferBlock->getMemorySize();
}

ANGLE_INLINE const Buffer &BufferSuballocation::getBuffer() const
{
    return mBufferBlock->getBuffer();
}

ANGLE_INLINE VkDeviceSize BufferSuballocation::getSize() const
{
    return mSize;
}

ANGLE_INLINE const DeviceMemory &BufferSuballocation::getDeviceMemory() const
{
    return mBufferBlock->getDeviceMemory();
}

ANGLE_INLINE VkMemoryMapFlags BufferSuballocation::getMemoryPropertyFlags() const
{
    return mBufferBlock->getMemoryPropertyFlags();
}

ANGLE_INLINE bool BufferSuballocation::isHostVisible() const
{
    return mBufferBlock->isHostVisible();
}
ANGLE_INLINE bool BufferSuballocation::isCoherent() const
{
    return mBufferBlock->isCoherent();
}
ANGLE_INLINE bool BufferSuballocation::isMapped() const
{
    return mBufferBlock->isMapped();
}
ANGLE_INLINE uint8_t *BufferSuballocation::getMappedMemory() const
{
    return mBufferBlock->getMappedMemory() + getOffset();
}

ANGLE_INLINE void BufferSuballocation::flush(const VkDevice &device)
{
    if (!isCoherent())
    {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory              = mBufferBlock->getDeviceMemory().getHandle();
        mappedRange.offset              = getOffset();
        mappedRange.size                = mSize;
        mBufferBlock->getDeviceMemory().flush(device, mappedRange);
    }
}

ANGLE_INLINE void BufferSuballocation::invalidate(const VkDevice &device)
{
    if (!isCoherent())
    {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory              = mBufferBlock->getDeviceMemory().getHandle();
        mappedRange.offset              = getOffset();
        mappedRange.size                = mSize;
        mBufferBlock->getDeviceMemory().invalidate(device, mappedRange);
    }
}

ANGLE_INLINE VkDeviceSize BufferSuballocation::getOffset() const
{
    return mOffset;
}

ANGLE_INLINE void BufferSuballocation::setOffsetAndSize(VkDeviceSize offset, VkDeviceSize size)
{
    mOffset = offset;
    mSize   = size;
}

ANGLE_INLINE uint8_t *BufferSuballocation::getBlockMemory() const
{
    return mBufferBlock->getMappedMemory();
}
ANGLE_INLINE VkDeviceSize BufferSuballocation::getBlockMemorySize() const
{
    return mBufferBlock->getMemorySize();
}
ANGLE_INLINE BufferSerial BufferSuballocation::getBlockSerial() const
{
    ASSERT(valid());
    return mBufferBlock->getBufferSerial();
}
}  // namespace vk
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SUBALLOCATION_H_
