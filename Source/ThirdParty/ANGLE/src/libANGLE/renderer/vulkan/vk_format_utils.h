//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_format_utils:
//   Helper for Vulkan format code.

#ifndef LIBANGLE_RENDERER_VULKAN_VK_FORMAT_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_FORMAT_UTILS_H_

#include "common/vulkan/vk_headers.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/copyvertex.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "platform/FeaturesVk_autogen.h"

#include <array>

namespace gl
{
struct SwizzleState;
class TextureCapsMap;
}  // namespace gl

namespace rx
{
class RendererVk;
class ContextVk;

namespace vk
{
// VkFormat values in range [0, kNumVkFormats) are used as indices in various tables.
constexpr uint32_t kNumVkFormats = 185;

enum ImageAccess
{
    SampleOnly,
    Renderable,
};

struct ImageFormatInitInfo final
{
    angle::FormatID format;
    InitializeTextureDataFunction initializer;
};

struct BufferFormatInitInfo final
{
    angle::FormatID format;
    bool vkFormatIsPacked;
    VertexCopyFunction vertexLoadFunction;
    bool vertexLoadRequiresConversion;
};

VkFormat GetVkFormatFromFormatID(angle::FormatID actualFormatID);
angle::FormatID GetFormatIDFromVkFormat(VkFormat vkFormat);

// Returns buffer alignment for image-copy operations (to or from a buffer).
size_t GetImageCopyBufferAlignment(angle::FormatID actualFormatID);
size_t GetValidImageCopyBufferAlignment(angle::FormatID intendedFormatID,
                                        angle::FormatID actualFormatID);
bool HasEmulatedImageChannels(const angle::Format &intendedFormat,
                              const angle::Format &actualFormat);
// Returns true if the image has a different image format than intended.
bool HasEmulatedImageFormat(angle::FormatID intendedFormatID, angle::FormatID actualFormatID);

// Describes a Vulkan format. For more information on formats in the Vulkan back-end please see
// https://chromium.googlesource.com/angle/angle/+/main/src/libANGLE/renderer/vulkan/doc/FormatTablesAndEmulation.md
class Format final : private angle::NonCopyable
{
  public:
    Format();

    bool valid() const { return mIntendedGLFormat != 0; }
    GLenum getIntendedGLFormat() const { return mIntendedGLFormat; }

    // The intended format is the front-end format. For Textures this usually correponds to a
    // GLenum in the headers. Buffer formats don't always have a corresponding GLenum type.
    // Some Surface formats and unsized types also don't have a corresponding GLenum.
    angle::FormatID getIntendedFormatID() const { return mIntendedFormatID; }
    const angle::Format &getIntendedFormat() const { return angle::Format::Get(mIntendedFormatID); }

    // The actual Image format is used to implement the front-end format for Texture/Renderbuffers.
    const angle::Format &getActualImageFormat(ImageAccess access) const
    {
        return angle::Format::Get(getActualImageFormatID(access));
    }

    angle::FormatID getActualRenderableImageFormatID() const
    {
        return mActualRenderableImageFormatID;
    }
    const angle::Format &getActualRenderableImageFormat() const
    {
        return angle::Format::Get(mActualRenderableImageFormatID);
    }
    VkFormat getActualRenderableImageVkFormat() const
    {
        return GetVkFormatFromFormatID(mActualRenderableImageFormatID);
    }

    angle::FormatID getActualImageFormatID(ImageAccess access) const
    {
        return ImageAccess::Renderable == access ? mActualRenderableImageFormatID
                                                 : mActualSampleOnlyImageFormatID;
    }
    VkFormat getActualImageVkFormat(ImageAccess access) const
    {
        return GetVkFormatFromFormatID(getActualImageFormatID(access));
    }

    LoadImageFunctionInfo getTextureLoadFunction(ImageAccess access, GLenum type) const
    {
        return ImageAccess::Renderable == access ? mRenderableTextureLoadFunctions(type)
                                                 : mTextureLoadFunctions(type);
    }

    // The actual Buffer format is used to implement the front-end format for Buffers.  This format
    // is used by vertex buffers as well as texture buffers.  Note that all formats required for
    // GL_EXT_texture_buffer have mandatory support for vertex buffers in Vulkan, so they won't be
    // using an emulated format.
    const angle::Format &getActualBufferFormat(bool compressed) const
    {
        return angle::Format::Get(compressed ? mActualCompressedBufferFormatID
                                             : mActualBufferFormatID);
    }

    VkFormat getActualBufferVkFormat(bool compressed) const
    {
        return GetVkFormatFromFormatID(compressed ? mActualCompressedBufferFormatID
                                                  : mActualBufferFormatID);
    }

    VertexCopyFunction getVertexLoadFunction(bool compressed) const
    {
        return compressed ? mCompressedVertexLoadFunction : mVertexLoadFunction;
    }

    bool getVertexLoadRequiresConversion(bool compressed) const
    {
        return compressed ? mCompressedVertexLoadRequiresConversion : mVertexLoadRequiresConversion;
    }

    // |intendedGLFormat| always correponds to a valid GLenum type. For types that don't have a
    // corresponding GLenum we do our best to specify a GLenum that is "close".
    const gl::InternalFormat &getInternalFormatInfo(GLenum type) const
    {
        return gl::GetInternalFormatInfo(mIntendedGLFormat, type);
    }

    bool hasRenderableImageFallbackFormat() const
    {
        return mActualSampleOnlyImageFormatID != mActualRenderableImageFormatID;
    }

    bool canCompressBufferData() const
    {
        return mActualCompressedBufferFormatID != angle::FormatID::NONE &&
               mActualBufferFormatID != mActualCompressedBufferFormatID;
    }

    // Returns the alignment for a buffer to be used with the vertex input stage in Vulkan. This
    // calculation is listed in the Vulkan spec at the end of the section 'Vertex Input
    // Description'.
    size_t getVertexInputAlignment(bool compressed) const;

  private:
    friend class FormatTable;

    // This is an auto-generated method in vk_format_table_autogen.cpp.
    void initialize(RendererVk *renderer, const angle::Format &intendedAngleFormat);

    // These are used in the format table init.
    void initImageFallback(RendererVk *renderer, const ImageFormatInitInfo *info, int numInfo);
    void initBufferFallback(RendererVk *renderer,
                            const BufferFormatInitInfo *fallbackInfo,
                            int numInfo,
                            int compressedStartIndex);

    angle::FormatID mIntendedFormatID;
    GLenum mIntendedGLFormat;
    angle::FormatID mActualSampleOnlyImageFormatID;
    angle::FormatID mActualRenderableImageFormatID;
    angle::FormatID mActualBufferFormatID;
    angle::FormatID mActualCompressedBufferFormatID;

    InitializeTextureDataFunction mImageInitializerFunction;
    LoadFunctionMap mTextureLoadFunctions;
    LoadFunctionMap mRenderableTextureLoadFunctions;
    VertexCopyFunction mVertexLoadFunction;
    VertexCopyFunction mCompressedVertexLoadFunction;

    bool mVertexLoadRequiresConversion;
    bool mCompressedVertexLoadRequiresConversion;
    bool mVkBufferFormatIsPacked;
    bool mVkCompressedBufferFormatIsPacked;
    bool mVkFormatIsInt;
    bool mVkFormatIsUnsigned;
};

bool operator==(const Format &lhs, const Format &rhs);
bool operator!=(const Format &lhs, const Format &rhs);

class FormatTable final : angle::NonCopyable
{
  public:
    FormatTable();
    ~FormatTable();

    // Also initializes the TextureCapsMap and the compressedTextureCaps in the Caps instance.
    void initialize(RendererVk *renderer, gl::TextureCapsMap *outTextureCapsMap);

    ANGLE_INLINE const Format &operator[](GLenum internalFormat) const
    {
        angle::FormatID formatID = angle::Format::InternalFormatToID(internalFormat);
        return mFormatData[static_cast<size_t>(formatID)];
    }

    ANGLE_INLINE const Format &operator[](angle::FormatID formatID) const
    {
        return mFormatData[static_cast<size_t>(formatID)];
    }

  private:
    // The table data is indexed by angle::FormatID.
    std::array<Format, angle::kNumANGLEFormats> mFormatData;
};

// This will return a reference to a VkFormatProperties with the feature flags supported
// if the format is a mandatory format described in section 31.3.3. Required Format Support
// of the Vulkan spec. If the vkFormat isn't mandatory, it will return a VkFormatProperties
// initialized to 0.
const VkFormatProperties &GetMandatoryFormatSupport(angle::FormatID formatID);

VkImageUsageFlags GetMaximalImageUsageFlags(RendererVk *renderer, angle::FormatID formatID);

}  // namespace vk

// Checks if a Vulkan format supports all the features needed to use it as a GL texture format.
bool HasFullTextureFormatSupport(RendererVk *renderer, angle::FormatID formatID);
// Checks if a Vulkan format supports all the features except rendering.
bool HasNonRenderableTextureFormatSupport(RendererVk *renderer, angle::FormatID formatID);
// Checks if it is a ETC texture format
bool IsETCFormat(angle::FormatID formatID);
// Checks if it is a BC texture format
bool IsBCFormat(angle::FormatID formatID);

angle::FormatID GetTranscodeBCFormatID(angle::FormatID formatID);

// Get the swizzle state based on format's requirements and emulations.
gl::SwizzleState GetFormatSwizzle(const angle::Format &angleFormat, const bool sized);

// Apply application's swizzle to the swizzle implied by format as received from GetFormatSwizzle.
gl::SwizzleState ApplySwizzle(const gl::SwizzleState &formatSwizzle,
                              const gl::SwizzleState &toApply);

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VK_FORMAT_UTILS_H_
