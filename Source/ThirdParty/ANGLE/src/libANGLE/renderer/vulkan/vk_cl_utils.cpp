//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_cl_utils:
//    Helper functions for the Vulkan Renderer in translation of vk state from/to cl state.
//

#include "libANGLE/renderer/vulkan/vk_cl_utils.h"

namespace rx
{
namespace cl_vk
{

VkExtent3D GetExtentFromDescriptor(cl::ImageDescriptor desc)
{
    VkExtent3D extent{};

    extent.width  = static_cast<uint32_t>(desc.width);
    extent.height = static_cast<uint32_t>(desc.height);
    extent.depth  = static_cast<uint32_t>(desc.depth);

    // user can supply random values for height and depth for formats that dont need them
    switch (desc.type)
    {
        case cl::MemObjectType::Image1D:
        case cl::MemObjectType::Image1D_Array:
        case cl::MemObjectType::Image1D_Buffer:
            extent.height = 1;
            extent.depth  = 1;
            break;
        case cl::MemObjectType::Image2D:
        case cl::MemObjectType::Image2D_Array:
            extent.depth = 1;
            break;
        default:
            break;
    }
    return extent;
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
    // TODO: http://anglebug.com/8588
    VkMemoryPropertyFlags propFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (memFlags.isSet(CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR))
    {
        propFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    return propFlags;
}

}  // namespace cl_vk
}  // namespace rx
