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

#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/renderer_utils.h"

#include <array>

namespace gl
{
class TextureCapsMap;
}  // namespace gl

namespace rx
{

namespace vk
{

struct Format final : private angle::NonCopyable
{
    Format();

    // This is an auto-generated method in vk_format_table_autogen.cpp.
    void initialize(VkPhysicalDevice physicalDevice, const angle::Format &angleFormat);

    const angle::Format &textureFormat() const;
    const angle::Format &bufferFormat() const;

    GLenum internalFormat;
    angle::Format::ID textureFormatID;
    VkFormat vkTextureFormat;
    angle::Format::ID bufferFormatID;
    VkFormat vkBufferFormat;
    InitializeTextureDataFunction dataInitializerFunction;
    LoadFunctionMap loadFunctions;
};

class FormatTable final : angle::NonCopyable
{
  public:
    FormatTable();
    ~FormatTable();

    // Also initializes the TextureCapsMap.
    void initialize(VkPhysicalDevice physicalDevice, gl::TextureCapsMap *textureCapsMap);

    const Format &operator[](GLenum internalFormat) const;

  private:
    // The table data is indexed by angle::Format::ID.
    std::array<Format, angle::kNumANGLEFormats> mFormatData;
};

// TODO(jmadill): This is temporary. Figure out how to handle format conversions.
VkFormat GetNativeVertexFormat(gl::VertexFormatType vertexFormat);

}  // namespace vk

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_FORMAT_H_
