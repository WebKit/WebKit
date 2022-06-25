//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vma_allocator_wrapper.cpp:
//    Hides VMA functions so we can use separate warning sets.
//

#include "vk_mem_alloc_wrapper.h"

#include <vk_mem_alloc.h>

namespace vma
{
#define VALIDATE_BLOCK_CREATE_FLAG_BITS(x)                                                 \
    static_assert(static_cast<uint32_t>(x) ==                                              \
                      static_cast<uint32_t>(VMA_VIRTUAL_BLOCK_CREATE_##x##_ALGORITHM_BIT), \
                  "VMA enum mismatch")
VALIDATE_BLOCK_CREATE_FLAG_BITS(LINEAR);
VALIDATE_BLOCK_CREATE_FLAG_BITS(BUDDY);

VkResult InitAllocator(VkPhysicalDevice physicalDevice,
                       VkDevice device,
                       VkInstance instance,
                       uint32_t apiVersion,
                       VkDeviceSize preferredLargeHeapBlockSize,
                       VmaAllocator *pAllocator)
{
    VmaVulkanFunctions funcs                  = {};
    funcs.vkGetPhysicalDeviceProperties       = vkGetPhysicalDeviceProperties;
    funcs.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    funcs.vkAllocateMemory                    = vkAllocateMemory;
    funcs.vkFreeMemory                        = vkFreeMemory;
    funcs.vkMapMemory                         = vkMapMemory;
    funcs.vkUnmapMemory                       = vkUnmapMemory;
    funcs.vkFlushMappedMemoryRanges           = vkFlushMappedMemoryRanges;
    funcs.vkInvalidateMappedMemoryRanges      = vkInvalidateMappedMemoryRanges;
    funcs.vkBindBufferMemory                  = vkBindBufferMemory;
    funcs.vkBindImageMemory                   = vkBindImageMemory;
    funcs.vkGetBufferMemoryRequirements       = vkGetBufferMemoryRequirements;
    funcs.vkGetImageMemoryRequirements        = vkGetImageMemoryRequirements;
    funcs.vkCreateBuffer                      = vkCreateBuffer;
    funcs.vkDestroyBuffer                     = vkDestroyBuffer;
    funcs.vkCreateImage                       = vkCreateImage;
    funcs.vkDestroyImage                      = vkDestroyImage;
    funcs.vkCmdCopyBuffer                     = vkCmdCopyBuffer;
    {
#if !defined(ANGLE_SHARED_LIBVULKAN)
        // When the vulkan-loader is statically linked, we need to use the extension
        // functions defined in ANGLE's rx namespace. When it's dynamically linked
        // with volk, this will default to the function definitions with no namespace
        using rx::vkBindBufferMemory2KHR;
        using rx::vkBindImageMemory2KHR;
        using rx::vkGetBufferMemoryRequirements2KHR;
        using rx::vkGetImageMemoryRequirements2KHR;
        using rx::vkGetPhysicalDeviceMemoryProperties2KHR;
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
        funcs.vkGetBufferMemoryRequirements2KHR       = vkGetBufferMemoryRequirements2KHR;
        funcs.vkGetImageMemoryRequirements2KHR        = vkGetImageMemoryRequirements2KHR;
        funcs.vkBindBufferMemory2KHR                  = vkBindBufferMemory2KHR;
        funcs.vkBindImageMemory2KHR                   = vkBindImageMemory2KHR;
        funcs.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
    }

    VmaAllocatorCreateInfo allocatorInfo      = {};
    allocatorInfo.physicalDevice              = physicalDevice;
    allocatorInfo.device                      = device;
    allocatorInfo.instance                    = instance;
    allocatorInfo.pVulkanFunctions            = &funcs;
    allocatorInfo.vulkanApiVersion            = apiVersion;
    allocatorInfo.preferredLargeHeapBlockSize = preferredLargeHeapBlockSize;

    return vmaCreateAllocator(&allocatorInfo, pAllocator);
}

void DestroyAllocator(VmaAllocator allocator)
{
    vmaDestroyAllocator(allocator);
}

VkResult CreatePool(VmaAllocator allocator,
                    uint32_t memoryTypeIndex,
                    bool buddyAlgorithm,
                    VkDeviceSize blockSize,
                    VmaPool *pPool)
{
    VmaPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.memoryTypeIndex   = memoryTypeIndex;
    poolCreateInfo.flags             = VMA_POOL_CREATE_IGNORE_BUFFER_IMAGE_GRANULARITY_BIT;
    if (buddyAlgorithm)
    {
        poolCreateInfo.flags |= VMA_POOL_CREATE_BUDDY_ALGORITHM_BIT;
    }
    poolCreateInfo.blockSize     = blockSize;
    poolCreateInfo.maxBlockCount = -1;  // unlimited
    return vmaCreatePool(allocator, &poolCreateInfo, pPool);
}

void DestroyPool(VmaAllocator allocator, VmaPool pool)
{
    vmaDestroyPool(allocator, pool);
}

void FreeMemory(VmaAllocator allocator, VmaAllocation allocation)
{
    vmaFreeMemory(allocator, allocation);
}

VkResult CreateBuffer(VmaAllocator allocator,
                      const VkBufferCreateInfo *pBufferCreateInfo,
                      VkMemoryPropertyFlags requiredFlags,
                      VkMemoryPropertyFlags preferredFlags,
                      bool persistentlyMapped,
                      uint32_t *pMemoryTypeIndexOut,
                      VkBuffer *pBuffer,
                      VmaAllocation *pAllocation)
{
    VkResult result;
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.requiredFlags           = requiredFlags;
    allocationCreateInfo.preferredFlags          = preferredFlags;
    allocationCreateInfo.flags       = (persistentlyMapped) ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;
    VmaAllocationInfo allocationInfo = {};

    result = vmaCreateBuffer(allocator, pBufferCreateInfo, &allocationCreateInfo, pBuffer,
                             pAllocation, &allocationInfo);
    *pMemoryTypeIndexOut = allocationInfo.memoryType;

    return result;
}

VkResult FindMemoryTypeIndexForBufferInfo(VmaAllocator allocator,
                                          const VkBufferCreateInfo *pBufferCreateInfo,
                                          VkMemoryPropertyFlags requiredFlags,
                                          VkMemoryPropertyFlags preferredFlags,
                                          bool persistentlyMappedBuffers,
                                          uint32_t *pMemoryTypeIndexOut)
{
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.requiredFlags           = requiredFlags;
    allocationCreateInfo.preferredFlags          = preferredFlags;
    allocationCreateInfo.flags = (persistentlyMappedBuffers) ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;

    return vmaFindMemoryTypeIndexForBufferInfo(allocator, pBufferCreateInfo, &allocationCreateInfo,
                                               pMemoryTypeIndexOut);
}

void GetMemoryTypeProperties(VmaAllocator allocator,
                             uint32_t memoryTypeIndex,
                             VkMemoryPropertyFlags *pFlags)
{
    vmaGetMemoryTypeProperties(allocator, memoryTypeIndex, pFlags);
}

VkResult MapMemory(VmaAllocator allocator, VmaAllocation allocation, void **ppData)
{
    return vmaMapMemory(allocator, allocation, ppData);
}

void UnmapMemory(VmaAllocator allocator, VmaAllocation allocation)
{
    return vmaUnmapMemory(allocator, allocation);
}

void FlushAllocation(VmaAllocator allocator,
                     VmaAllocation allocation,
                     VkDeviceSize offset,
                     VkDeviceSize size)
{
    vmaFlushAllocation(allocator, allocation, offset, size);
}

void InvalidateAllocation(VmaAllocator allocator,
                          VmaAllocation allocation,
                          VkDeviceSize offset,
                          VkDeviceSize size)
{
    vmaInvalidateAllocation(allocator, allocation, offset, size);
}

void BuildStatsString(VmaAllocator allocator, char **statsString, VkBool32 detailedMap)
{
    vmaBuildStatsString(allocator, statsString, detailedMap);
}

void FreeStatsString(VmaAllocator allocator, char *statsString)
{
    vmaFreeStatsString(allocator, statsString);
}

// VmaVirtualBlock implementation
VkResult CreateVirtualBlock(VkDeviceSize size,
                            VirtualBlockCreateFlags flags,
                            VmaVirtualBlock *pVirtualBlock)
{
    VmaVirtualBlockCreateInfo virtualBlockCreateInfo = {};
    virtualBlockCreateInfo.size                      = size;
    virtualBlockCreateInfo.flags                     = (VmaVirtualBlockCreateFlagBits)flags;
    return vmaCreateVirtualBlock(&virtualBlockCreateInfo, pVirtualBlock);
}

void DestroyVirtualBlock(VmaVirtualBlock virtualBlock)
{
    vmaDestroyVirtualBlock(virtualBlock);
}

VkResult VirtualAllocate(VmaVirtualBlock virtualBlock,
                         VkDeviceSize size,
                         VkDeviceSize alignment,
                         VkDeviceSize *pOffset)
{
    VmaVirtualAllocationCreateInfo createInfo = {};
    createInfo.size                           = size;
    createInfo.alignment                      = alignment;
    createInfo.flags                          = 0;
    return vmaVirtualAllocate(virtualBlock, &createInfo, pOffset);
}

void VirtualFree(VmaVirtualBlock virtualBlock, VkDeviceSize offset)
{
    vmaVirtualFree(virtualBlock, offset);
}

VkBool32 IsVirtualBlockEmpty(VmaVirtualBlock virtualBlock)
{
    return vmaIsVirtualBlockEmpty(virtualBlock);
}

void GetVirtualAllocationInfo(VmaVirtualBlock virtualBlock,
                              VkDeviceSize offset,
                              VkDeviceSize *sizeOut,
                              void **pUserDataOut)
{
    VmaVirtualAllocationInfo virtualAllocInfo = {};
    vmaGetVirtualAllocationInfo(virtualBlock, offset, &virtualAllocInfo);
    *sizeOut      = virtualAllocInfo.size;
    *pUserDataOut = virtualAllocInfo.pUserData;
}

void ClearVirtualBlock(VmaVirtualBlock virtualBlock)
{
    vmaClearVirtualBlock(virtualBlock);
}

void SetVirtualAllocationUserData(VmaVirtualBlock virtualBlock,
                                  VkDeviceSize offset,
                                  void *pUserData)
{
    vmaSetVirtualAllocationUserData(virtualBlock, offset, pUserData);
}

void CalculateVirtualBlockStats(VmaVirtualBlock virtualBlock, StatInfo *pStatInfo)
{
    vmaCalculateVirtualBlockStats(virtualBlock, reinterpret_cast<VmaStatInfo *>(pStatInfo));
}

void BuildVirtualBlockStatsString(VmaVirtualBlock virtualBlock,
                                  char **ppStatsString,
                                  VkBool32 detailedMap)
{
    vmaBuildVirtualBlockStatsString(virtualBlock, ppStatsString, detailedMap);
}

void FreeVirtualBlockStatsString(VmaVirtualBlock virtualBlock, char *pStatsString)
{
    vmaFreeVirtualBlockStatsString(virtualBlock, pStatsString);
}
}  // namespace vma
