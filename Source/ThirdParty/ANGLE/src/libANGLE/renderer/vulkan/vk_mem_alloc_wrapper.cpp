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
VkResult InitAllocator(VkPhysicalDevice physicalDevice,
                       VkDevice device,
                       VkInstance instance,
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
        using rx::vkGetBufferMemoryRequirements2KHR;
        using rx::vkGetImageMemoryRequirements2KHR;
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
        funcs.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
        funcs.vkGetImageMemoryRequirements2KHR  = vkGetImageMemoryRequirements2KHR;
    }

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = physicalDevice;
    allocatorInfo.device                 = device;
    allocatorInfo.instance               = instance;
    allocatorInfo.pVulkanFunctions       = &funcs;

    return vmaCreateAllocator(&allocatorInfo, pAllocator);
}

void DestroyAllocator(VmaAllocator allocator)
{
    vmaDestroyAllocator(allocator);
}

void FreeMemory(VmaAllocator allocator, VmaAllocation allocation)
{
    vmaFreeMemory(allocator, allocation);
}

VkResult CreateBuffer(VmaAllocator allocator,
                      const VkBufferCreateInfo *pBufferCreateInfo,
                      VkMemoryPropertyFlags requiredFlags,
                      VkMemoryPropertyFlags preferredFlags,
                      bool persistentlyMappedBuffers,
                      uint32_t *pMemoryTypeIndexOut,
                      VkBuffer *pBuffer,
                      VmaAllocation *pAllocation)
{
    VkResult result;
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.requiredFlags           = requiredFlags;
    allocationCreateInfo.preferredFlags          = preferredFlags;
    allocationCreateInfo.flags = (persistentlyMappedBuffers) ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;
    VmaAllocationInfo allocationInfo = {};

    result = vmaCreateBuffer(allocator, pBufferCreateInfo, &allocationCreateInfo, pBuffer,
                             pAllocation, &allocationInfo);
    *pMemoryTypeIndexOut = allocationInfo.memoryType;

    return result;
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
}  // namespace vma
