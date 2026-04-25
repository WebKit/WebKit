//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_cl_utils:
//    Helper functions for the Vulkan Renderer in translation of vk state from/to cl state.
//

#include "libANGLE/renderer/vulkan/vk_cl_utils.h"
#include "vulkan/vulkan_core.h"

namespace rx
{
namespace cl_vk
{

// Give two cl::BufferRect regions, calculate a series of the buffer copy regions that can be used
// in vulkan copy buffer command.
std::vector<VkBufferCopy> CalculateRectCopyRegions(const cl::BufferRect &srcRect,
                                                   const cl::BufferRect &dstRect)
{
    // For copying the buffer rect region should be the same
    ASSERT(srcRect.getExtents() == dstRect.getExtents());
    std::vector<VkBufferCopy> copyRegions;

    for (size_t slice = 0; slice < srcRect.mSize.depth; slice++)
    {
        for (size_t row = 0; row < srcRect.mSize.height; row++)
        {
            VkBufferCopy copyRegion = {};
            copyRegion.size         = srcRect.mSize.width * srcRect.mElementSize;
            copyRegion.srcOffset    = srcRect.getRowOffset(slice, row);
            copyRegion.dstOffset    = dstRect.getRowOffset(slice, row);
            copyRegions.push_back(copyRegion);
        }
    }
    return copyRegions;
}

// Given an image and rect region to copy in to a buffer, calculate the VKBufferImageCopy struct to
// be using in VkCmd's.
VkBufferImageCopy CalculateBufferImageCopyRegion(const size_t bufferOffset,
                                                 const uint32_t rowPitch,
                                                 const uint32_t slicePitch,
                                                 const cl::Offset &origin,
                                                 const cl::Extents &region,
                                                 CLImageVk *imageVk)
{
    VkBufferImageCopy copyRegion{
        .bufferOffset = bufferOffset,
        .bufferRowLength =
            rowPitch == 0 ? 0 : rowPitch / static_cast<uint32_t>(imageVk->getElementSize()),
        .bufferImageHeight = rowPitch == 0 ? 0 : slicePitch / rowPitch,
        .imageSubresource = imageVk->getSubresourceLayersForCopy(origin, region, imageVk->getType(),
                                                                 ImageCopyWith::Buffer),
        .imageOffset      = GetOffset(imageVk->getOffsetForCopy(origin)),
        .imageExtent      = GetExtent(imageVk->getExtentForCopy(region))};

    return copyRegion;
}

VkExtent3D GetExtent(const cl::Extents &extent)
{
    VkExtent3D vkExtent{};

    vkExtent.width  = static_cast<uint32_t>(extent.width);
    vkExtent.height = static_cast<uint32_t>(extent.height);
    vkExtent.depth  = static_cast<uint32_t>(extent.depth);

    return vkExtent;
}

VkOffset3D GetOffset(const cl::Offset &offset)
{
    VkOffset3D vkOffset{};

    vkOffset.x = static_cast<uint32_t>(offset.x);
    vkOffset.y = static_cast<uint32_t>(offset.y);
    vkOffset.z = static_cast<uint32_t>(offset.z);

    return vkOffset;
}

VkImageType GetImageType(cl::MemObjectType memObjectType)
{
    switch (memObjectType)
    {
        case cl::MemObjectType::Image1D:
        case cl::MemObjectType::Image1D_Array:
        case cl::MemObjectType::Image1D_Buffer:
            return VK_IMAGE_TYPE_1D;
        case cl::MemObjectType::Image2D:
        case cl::MemObjectType::Image2D_Array:
            return VK_IMAGE_TYPE_2D;
        case cl::MemObjectType::Image3D:
            return VK_IMAGE_TYPE_3D;
        default:
            // We will need to implement all the texture types for ES3+.
            UNIMPLEMENTED();
            return VK_IMAGE_TYPE_MAX_ENUM;
    }
}

VkImageViewType GetImageViewType(cl::MemObjectType memObjectType)
{
    switch (memObjectType)
    {
        case cl::MemObjectType::Image1D:
            return VK_IMAGE_VIEW_TYPE_1D;
        case cl::MemObjectType::Image1D_Array:
            return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case cl::MemObjectType::Image2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case cl::MemObjectType::Image2D_Array:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case cl::MemObjectType::Image3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case cl::MemObjectType::Image1D_Buffer:
            // Image1D_Buffer has an associated buffer view and not an image view, returning max
            // enum here.
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        default:
            UNIMPLEMENTED();
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }
}

VkMemoryPropertyFlags GetMemoryPropertyFlags(cl::MemFlags memFlags)
{
    VkMemoryPropertyFlags propFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (memFlags.intersects(CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR))
    {
        propFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }

    if (memFlags.intersects(CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR))
    {
        propFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    return propFlags;
}

VkBufferUsageFlags GetBufferUsageFlags(cl::MemFlags memFlags, bool physicalAddressing)
{
    // The buffer usage flags don't particularly affect the buffer in any known drivers, use all the
    // bits that ANGLE needs.
    VkBufferUsageFlags usageFlags =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

    if (physicalAddressing)
    {
        // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT specifies that the buffer can be used to
        // retrieve a buffer device address via vkGetBufferDeviceAddress and use that address to
        // access the buffer's memory from a shader.
        usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    return usageFlags;
}

// Added Vulkan component swizzle mapper
VkComponentMapping GetChannelComponentMapping(cl_channel_order order)
{
    // Default swizzle values
    VkComponentMapping m{};
    m.r = VK_COMPONENT_SWIZZLE_R;
    m.g = VK_COMPONENT_SWIZZLE_G;
    m.b = VK_COMPONENT_SWIZZLE_B;
    m.a = VK_COMPONENT_SWIZZLE_A;

    switch (order)
    {
        case CL_A:
            // A-only: rgb = 0, a = R
            m.r = VK_COMPONENT_SWIZZLE_ZERO;
            m.g = VK_COMPONENT_SWIZZLE_ZERO;
            m.b = VK_COMPONENT_SWIZZLE_ZERO;
            m.a = VK_COMPONENT_SWIZZLE_R;
            break;
        case CL_LUMINANCE:
            // L replicated to RGB, alpha = 1
            m.r = VK_COMPONENT_SWIZZLE_R;
            m.g = VK_COMPONENT_SWIZZLE_R;
            m.b = VK_COMPONENT_SWIZZLE_R;
            m.a = VK_COMPONENT_SWIZZLE_ONE;
            break;
        case CL_INTENSITY:
            // I replicated to RGBA
            m.r = VK_COMPONENT_SWIZZLE_R;
            m.g = VK_COMPONENT_SWIZZLE_R;
            m.b = VK_COMPONENT_SWIZZLE_R;
            m.a = VK_COMPONENT_SWIZZLE_R;
            break;
        // ARGB re-mapping
        case CL_ARGB:
            // Storage is RGBA, backing format is VK_FORMAT_R8G8B8A8
            m.r = VK_COMPONENT_SWIZZLE_G;
            m.g = VK_COMPONENT_SWIZZLE_B;
            m.b = VK_COMPONENT_SWIZZLE_A;
            m.a = VK_COMPONENT_SWIZZLE_R;
            break;
        default:
            break;
    }

    return m;
}

}  // namespace cl_vk
}  // namespace rx
