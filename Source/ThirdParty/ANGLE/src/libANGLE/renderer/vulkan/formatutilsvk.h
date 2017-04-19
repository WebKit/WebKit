//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Vk::Format:
//   Vulkan implementation of a storage format.

#ifndef LIBANGLE_RENDERER_VULKAN_FORMAT_H_
#define LIBANGLE_RENDERER_VULKAN_FORMAT_H_

#include <vulkan/vulkan.h>

#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/renderer_utils.h"

namespace rx
{

namespace vk
{

struct Format final : angle::NonCopyable
{
    constexpr Format(GLenum internalFormat,
                     angle::Format::ID formatID,
                     VkFormat native,
                     InitializeTextureDataFunction initFunction);

    static const Format &Get(GLenum internalFormat);

    const angle::Format &format() const;
    LoadFunctionMap getLoadFunctions() const;

    GLenum internalFormat;
    angle::Format::ID formatID;
    VkFormat native;
    InitializeTextureDataFunction dataInitializerFunction;
};

constexpr Format::Format(GLenum internalFormat,
                         angle::Format::ID formatID,
                         VkFormat native,
                         InitializeTextureDataFunction initFunction)
    : internalFormat(internalFormat),
      formatID(formatID),
      native(native),
      dataInitializerFunction(initFunction)
{
}

// TODO(jmadill): This is temporary. Figure out how to handle format conversions.
VkFormat GetNativeVertexFormat(gl::VertexFormatType vertexFormat);

}  // namespace vk

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_FORMAT_H_
