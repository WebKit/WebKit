//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_format_utils:
//   Helper for Vulkan format code.

#include "libANGLE/renderer/vulkan/vk_format_utils.h"

#include "libANGLE/Texture.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/load_functions_table.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_caps_utils.h"

namespace rx
{
namespace
{
void FillTextureFormatCaps(RendererVk *renderer,
                           angle::FormatID formatID,
                           gl::TextureCaps *outTextureCaps)
{
    const VkPhysicalDeviceLimits &physicalDeviceLimits =
        renderer->getPhysicalDeviceProperties().limits;
    bool hasColorAttachmentFeatureBit =
        renderer->hasImageFormatFeatureBits(formatID, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
    bool hasDepthAttachmentFeatureBit = renderer->hasImageFormatFeatureBits(
        formatID, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    outTextureCaps->texturable =
        renderer->hasImageFormatFeatureBits(formatID, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    outTextureCaps->filterable = renderer->hasImageFormatFeatureBits(
        formatID, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    outTextureCaps->blendable =
        renderer->hasImageFormatFeatureBits(formatID, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);

    // For renderbuffer and texture attachments we require transfer and sampling for
    // GLES 2.0 CopyTexImage support. Sampling is also required for other features like
    // blits and EGLImages.
    outTextureCaps->textureAttachment =
        outTextureCaps->texturable &&
        (hasColorAttachmentFeatureBit || hasDepthAttachmentFeatureBit);
    outTextureCaps->renderbuffer = outTextureCaps->textureAttachment;

    if (outTextureCaps->renderbuffer)
    {
        if (hasColorAttachmentFeatureBit)
        {
            vk_gl::AddSampleCounts(physicalDeviceLimits.framebufferColorSampleCounts,
                                   &outTextureCaps->sampleCounts);
        }
        if (hasDepthAttachmentFeatureBit)
        {
            // Some drivers report different depth and stencil sample counts.  We'll AND those
            // counts together, limiting all depth and/or stencil formats to the lower number of
            // sample counts.
            vk_gl::AddSampleCounts((physicalDeviceLimits.framebufferDepthSampleCounts &
                                    physicalDeviceLimits.framebufferStencilSampleCounts),
                                   &outTextureCaps->sampleCounts);
        }
    }
}

bool HasFullBufferFormatSupport(RendererVk *renderer, angle::FormatID formatID)
{
    // Note: GL_EXT_texture_buffer support uses the same vkBufferFormat that is determined by
    // Format::initBufferFallback, which uses this function.  That relies on the fact that formats
    // required for GL_EXT_texture_buffer all have mandatory VERTEX_BUFFER feature support in
    // Vulkan.  If this function is changed to test for more features in such a way that makes any
    // of those formats use a fallback format, the implementation of GL_EXT_texture_buffer must be
    // modified not to use vkBufferFormat.
    return renderer->hasBufferFormatFeatureBits(formatID, VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT);
}

using SupportTest = bool (*)(RendererVk *renderer, angle::FormatID formatID);

template <class FormatInitInfo>
int FindSupportedFormat(RendererVk *renderer,
                        const FormatInitInfo *info,
                        size_t skip,
                        int numInfo,
                        SupportTest hasSupport)
{
    ASSERT(numInfo > 0);
    const int last = numInfo - 1;

    for (int i = static_cast<int>(skip); i < last; ++i)
    {
        ASSERT(info[i].format != angle::FormatID::NONE);
        if (hasSupport(renderer, info[i].format))
            return i;
    }

    if (skip > 0 && !hasSupport(renderer, info[last].format))
    {
        // We couldn't find a valid fallback, try again without skip
        return FindSupportedFormat(renderer, info, 0, numInfo, hasSupport);
    }

    ASSERT(info[last].format != angle::FormatID::NONE);
    ASSERT(hasSupport(renderer, info[last].format));
    return last;
}

bool HasNonFilterableTextureFormatSupport(RendererVk *renderer, angle::FormatID formatID)
{
    constexpr uint32_t kBitsColor =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    constexpr uint32_t kBitsDepth = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return renderer->hasImageFormatFeatureBits(formatID, kBitsColor) ||
           renderer->hasImageFormatFeatureBits(formatID, kBitsDepth);
}
}  // anonymous namespace

namespace vk
{
// Format implementation.
Format::Format()
    : mIntendedFormatID(angle::FormatID::NONE),
      mIntendedGLFormat(GL_NONE),
      mActualSampleOnlyImageFormatID(angle::FormatID::NONE),
      mActualRenderableImageFormatID(angle::FormatID::NONE),
      mActualBufferFormatID(angle::FormatID::NONE),
      mActualCompressedBufferFormatID(angle::FormatID::NONE),
      mImageInitializerFunction(nullptr),
      mTextureLoadFunctions(),
      mRenderableTextureLoadFunctions(),
      mVertexLoadFunction(nullptr),
      mCompressedVertexLoadFunction(nullptr),
      mVertexLoadRequiresConversion(false),
      mCompressedVertexLoadRequiresConversion(false),
      mVkBufferFormatIsPacked(false),
      mVkFormatIsInt(false),
      mVkFormatIsUnsigned(false)
{}

void Format::initImageFallback(RendererVk *renderer, const ImageFormatInitInfo *info, int numInfo)
{
    size_t skip                 = renderer->getFeatures().forceFallbackFormat.enabled ? 1 : 0;
    SupportTest testFunction    = HasNonRenderableTextureFormatSupport;
    const angle::Format &format = angle::Format::Get(info[0].format);
    if (format.isInt() || (format.isFloat() && format.redBits >= 32))
    {
        // Integer formats don't support filtering in GL, so don't test for it.
        // Filtering of 32-bit float textures is not supported on Android, and
        // it's enabled by the extension OES_texture_float_linear, which is
        // enabled automatically by examining format capabilities.
        testFunction = HasNonFilterableTextureFormatSupport;
    }

    int i = FindSupportedFormat(renderer, info, skip, static_cast<uint32_t>(numInfo), testFunction);
    mActualSampleOnlyImageFormatID = info[i].format;
    mImageInitializerFunction      = info[i].initializer;

    // Set renderable format.
    if (testFunction != HasNonFilterableTextureFormatSupport && !format.isSnorm() &&
        !format.isBlock)
    {
        // Rendering to SNORM textures is not supported on Android, and it's
        // enabled by the extension EXT_render_snorm.
        // Compressed textures also need to perform this check.
        testFunction = HasFullTextureFormatSupport;
        i = FindSupportedFormat(renderer, info, skip, static_cast<uint32_t>(numInfo), testFunction);
        mActualRenderableImageFormatID = info[i].format;
    }
}

void Format::initBufferFallback(RendererVk *renderer,
                                const BufferFormatInitInfo *info,
                                int numInfo,
                                int compressedStartIndex)
{
    {
        size_t skip = renderer->getFeatures().forceFallbackFormat.enabled ? 1 : 0;
        int i       = FindSupportedFormat(renderer, info, skip, compressedStartIndex,
                                          HasFullBufferFormatSupport);

        mActualBufferFormatID         = info[i].format;
        mVkBufferFormatIsPacked       = info[i].vkFormatIsPacked;
        mVertexLoadFunction           = info[i].vertexLoadFunction;
        mVertexLoadRequiresConversion = info[i].vertexLoadRequiresConversion;
    }

    if (renderer->getFeatures().compressVertexData.enabled && compressedStartIndex < numInfo)
    {
        int i = FindSupportedFormat(renderer, info, compressedStartIndex, numInfo,
                                    HasFullBufferFormatSupport);

        mActualCompressedBufferFormatID         = info[i].format;
        mVkCompressedBufferFormatIsPacked       = info[i].vkFormatIsPacked;
        mCompressedVertexLoadFunction           = info[i].vertexLoadFunction;
        mCompressedVertexLoadRequiresConversion = info[i].vertexLoadRequiresConversion;
    }
}

size_t Format::getVertexInputAlignment(bool compressed) const
{
    const angle::Format &bufferFormat = getActualBufferFormat(compressed);
    size_t pixelBytes                 = bufferFormat.pixelBytes;
    return mVkBufferFormatIsPacked ? pixelBytes : (pixelBytes / bufferFormat.channelCount);
}

bool HasEmulatedImageChannels(const angle::Format &intendedFormat,
                              const angle::Format &actualFormat)
{
    return (intendedFormat.alphaBits == 0 && actualFormat.alphaBits > 0) ||
           (intendedFormat.blueBits == 0 && actualFormat.blueBits > 0) ||
           (intendedFormat.greenBits == 0 && actualFormat.greenBits > 0) ||
           (intendedFormat.depthBits == 0 && actualFormat.depthBits > 0) ||
           (intendedFormat.stencilBits == 0 && actualFormat.stencilBits > 0);
}

bool HasEmulatedImageFormat(angle::FormatID intendedFormatID, angle::FormatID actualFormatID)
{
    return actualFormatID != intendedFormatID;
}

bool operator==(const Format &lhs, const Format &rhs)
{
    return &lhs == &rhs;
}

bool operator!=(const Format &lhs, const Format &rhs)
{
    return &lhs != &rhs;
}

// FormatTable implementation.
FormatTable::FormatTable() {}

FormatTable::~FormatTable() {}

void FormatTable::initialize(RendererVk *renderer, gl::TextureCapsMap *outTextureCapsMap)
{
    for (size_t formatIndex = 0; formatIndex < angle::kNumANGLEFormats; ++formatIndex)
    {
        Format &format                           = mFormatData[formatIndex];
        const auto intendedFormatID              = static_cast<angle::FormatID>(formatIndex);
        const angle::Format &intendedAngleFormat = angle::Format::Get(intendedFormatID);

        format.initialize(renderer, intendedAngleFormat);
        format.mIntendedFormatID = intendedFormatID;

        if (!format.valid())
        {
            continue;
        }

        // No sample-able or render-able formats, so nothing left to do. This includes skipping the
        // rest of the loop for buffer-only formats, since they are not texturable.
        if (format.mActualSampleOnlyImageFormatID == angle::FormatID::NONE)
        {
            continue;
        }

        bool transcodeEtcToBc = false;
        if (renderer->getFeatures().supportsComputeTranscodeEtcToBc.enabled &&
            IsETCFormat(intendedFormatID) &&
            !angle::Format::Get(format.mActualSampleOnlyImageFormatID).isBlock)
        {
            // Check BC format support
            angle::FormatID bcFormat = GetTranscodeBCFormatID(intendedFormatID);
            if (HasNonRenderableTextureFormatSupport(renderer, bcFormat))
            {
                format.mActualSampleOnlyImageFormatID = bcFormat;
                transcodeEtcToBc                      = true;
            }
        }

        if (format.mActualRenderableImageFormatID == angle::FormatID::NONE)
        {
            // If renderable format was not set, it means there is no fallback format for
            // renderable. We populate this the same formatID as sampleOnly formatID so that
            // getActualFormatID() will be simpler.
            format.mActualRenderableImageFormatID = format.mActualSampleOnlyImageFormatID;
        }

        gl::TextureCaps textureCaps;
        FillTextureFormatCaps(renderer, format.mActualSampleOnlyImageFormatID, &textureCaps);

        if (textureCaps.texturable)
        {
            format.mTextureLoadFunctions = GetLoadFunctionsMap(
                format.mIntendedGLFormat,
                transcodeEtcToBc ? intendedFormatID : format.mActualSampleOnlyImageFormatID);
        }

        if (format.mActualRenderableImageFormatID == format.mActualSampleOnlyImageFormatID)
        {
            outTextureCapsMap->set(intendedFormatID, textureCaps);
            format.mRenderableTextureLoadFunctions = format.mTextureLoadFunctions;
        }
        else
        {
            FillTextureFormatCaps(renderer, format.mActualRenderableImageFormatID, &textureCaps);
            outTextureCapsMap->set(intendedFormatID, textureCaps);
            if (textureCaps.texturable)
            {
                format.mRenderableTextureLoadFunctions = GetLoadFunctionsMap(
                    format.mIntendedGLFormat, format.mActualRenderableImageFormatID);
            }
        }
    }
}

size_t GetImageCopyBufferAlignment(angle::FormatID actualFormatID)
{
    // vkCmdCopyBufferToImage must have an offset that is a multiple of 4 as well as a multiple
    // of the texel size (if uncompressed) or pixel block size (if compressed).
    // https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkBufferImageCopy.html
    //
    // We need lcm(4, texelSize) (lcm = least common multiplier).  For compressed images,
    // |texelSize| would contain the block size.  Since 4 is constant, this can be calculated as:
    //
    //                      | texelSize             texelSize % 4 == 0
    //                      | 4 * texelSize         texelSize % 4 == 1
    // lcm(4, texelSize) = <
    //                      | 2 * texelSize         texelSize % 4 == 2
    //                      | 4 * texelSize         texelSize % 4 == 3
    //
    // This means:
    //
    // - texelSize % 2 != 0 gives a 4x multiplier
    // - else texelSize % 4 != 0 gives a 2x multiplier
    // - else there's no multiplier.
    //
    const angle::Format &actualFormat = angle::Format::Get(actualFormatID);

    ASSERT(actualFormat.pixelBytes != 0);
    const size_t texelSize  = actualFormat.pixelBytes;
    const size_t multiplier = texelSize % 2 != 0 ? 4 : texelSize % 4 != 0 ? 2 : 1;
    const size_t alignment  = multiplier * texelSize;

    return alignment;
}

size_t GetValidImageCopyBufferAlignment(angle::FormatID intendedFormatID,
                                        angle::FormatID actualFormatID)
{
    constexpr size_t kMinimumAlignment = 16;
    return (intendedFormatID == angle::FormatID::NONE)
               ? kMinimumAlignment
               : GetImageCopyBufferAlignment(actualFormatID);
}

VkImageUsageFlags GetMaximalImageUsageFlags(RendererVk *renderer, angle::FormatID formatID)
{
    constexpr VkFormatFeatureFlags kImageUsageFeatureBits =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    VkFormatFeatureFlags featureBits =
        renderer->getImageFormatFeatureBits(formatID, kImageUsageFeatureBits);
    VkImageUsageFlags imageUsageFlags = 0;
    if (featureBits & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (featureBits & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
        imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (featureBits & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        imageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (featureBits & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        imageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (featureBits & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)
        imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (featureBits & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
        imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    return imageUsageFlags;
}
}  // namespace vk

bool HasFullTextureFormatSupport(RendererVk *renderer, angle::FormatID formatID)
{
    constexpr uint32_t kBitsColor = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

    // In OpenGL ES, all renderable formats except 32-bit floating-point support blending.
    // 32-bit floating-point case validation is handled by ANGLE's frontend.
    uint32_t kBitsColorFull = kBitsColor;
    switch (formatID)
    {
        case angle::FormatID::R32_FLOAT:
        case angle::FormatID::R32G32_FLOAT:
        case angle::FormatID::R32G32B32A32_FLOAT:
            break;
        default:
            kBitsColorFull |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
            break;
    }

    constexpr uint32_t kBitsDepth = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return renderer->hasImageFormatFeatureBits(formatID, kBitsColorFull) ||
           renderer->hasImageFormatFeatureBits(formatID, kBitsDepth);
}

bool HasNonRenderableTextureFormatSupport(RendererVk *renderer, angle::FormatID formatID)
{
    constexpr uint32_t kBitsColor =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    constexpr uint32_t kBitsDepth = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return renderer->hasImageFormatFeatureBits(formatID, kBitsColor) ||
           renderer->hasImageFormatFeatureBits(formatID, kBitsDepth);
}

// Checks if it is a ETC texture format
bool IsETCFormat(angle::FormatID formatID)
{
    return formatID >= angle::FormatID::EAC_R11G11_SNORM_BLOCK &&
           formatID <= angle::FormatID::ETC2_R8G8B8_UNORM_BLOCK;
}
// Checks if it is a BC texture format
bool IsBCFormat(angle::FormatID formatID)
{
    return formatID >= angle::FormatID::BC1_RGBA_UNORM_BLOCK &&
           formatID <= angle::FormatID::BC7_RGBA_UNORM_SRGB_BLOCK;
}

static constexpr angle::FormatID kEtcToBcFormatMapping[] = {
    angle::FormatID::BC5_RG_SNORM_BLOCK,         // EAC_R11G11_SNORM
    angle::FormatID::BC5_RG_UNORM_BLOCK,         // EAC_R11G11_UNORM
    angle::FormatID::BC4_RED_SNORM_BLOCK,        // EAC_R11_SNORM
    angle::FormatID::BC4_RED_UNORM_BLOCK,        // EAC_R11_UNORM_BLOCK
    angle::FormatID::BC1_RGB_UNORM_BLOCK,        // ETC1_LOSSY_DECODE_R8G8B8_UNORM
    angle::FormatID::BC1_RGB_UNORM_BLOCK,        // ETC1_R8G8B8_UNORM
    angle::FormatID::BC1_RGBA_UNORM_SRGB_BLOCK,  // ETC2_R8G8B8A1_SRGB
    angle::FormatID::BC1_RGBA_UNORM_BLOCK,       // ETC2_R8G8B8A1_UNORM
    angle::FormatID::BC3_RGBA_UNORM_SRGB_BLOCK,  // ETC2_R8G8B8A8_SRGB
    angle::FormatID::BC3_RGBA_UNORM_BLOCK,       // ETC2_R8G8B8A8_UNORM
    angle::FormatID::BC1_RGB_UNORM_SRGB_BLOCK,   // ETC2_R8G8B8_SRGB
    angle::FormatID::BC1_RGB_UNORM_BLOCK,        // ETC2_R8G8B8_UNORM
};

angle::FormatID GetTranscodeBCFormatID(angle::FormatID formatID)
{
    ASSERT(IsETCFormat(formatID));
    return kEtcToBcFormatMapping[static_cast<uint32_t>(formatID) -
                                 static_cast<uint32_t>(angle::FormatID::EAC_R11G11_SNORM_BLOCK)];
}

GLenum GetSwizzleStateComponent(const gl::SwizzleState &swizzleState, GLenum component)
{
    switch (component)
    {
        case GL_RED:
            return swizzleState.swizzleRed;
        case GL_GREEN:
            return swizzleState.swizzleGreen;
        case GL_BLUE:
            return swizzleState.swizzleBlue;
        case GL_ALPHA:
            return swizzleState.swizzleAlpha;
        default:
            return component;
    }
}

gl::SwizzleState ApplySwizzle(const gl::SwizzleState &formatSwizzle,
                              const gl::SwizzleState &toApply)
{
    gl::SwizzleState result;

    result.swizzleRed   = GetSwizzleStateComponent(formatSwizzle, toApply.swizzleRed);
    result.swizzleGreen = GetSwizzleStateComponent(formatSwizzle, toApply.swizzleGreen);
    result.swizzleBlue  = GetSwizzleStateComponent(formatSwizzle, toApply.swizzleBlue);
    result.swizzleAlpha = GetSwizzleStateComponent(formatSwizzle, toApply.swizzleAlpha);

    return result;
}

gl::SwizzleState GetFormatSwizzle(const angle::Format &angleFormat, const bool sized)
{
    gl::SwizzleState internalSwizzle;

    if (angleFormat.isLUMA())
    {
        GLenum swizzleRGB, swizzleA;
        if (angleFormat.luminanceBits > 0)
        {
            swizzleRGB = GL_RED;
            swizzleA   = (angleFormat.alphaBits > 0 ? GL_GREEN : GL_ONE);
        }
        else
        {
            swizzleRGB = GL_ZERO;
            swizzleA   = GL_RED;
        }
        internalSwizzle.swizzleRed   = swizzleRGB;
        internalSwizzle.swizzleGreen = swizzleRGB;
        internalSwizzle.swizzleBlue  = swizzleRGB;
        internalSwizzle.swizzleAlpha = swizzleA;
    }
    else
    {
        if (angleFormat.hasDepthOrStencilBits())
        {
            // In OES_depth_texture/ARB_depth_texture, depth
            // textures are treated as luminance.
            // If the internalformat was not sized, use OES_depth_texture behavior
            bool hasGB = (angleFormat.depthBits > 0) && !sized;

            internalSwizzle.swizzleRed   = GL_RED;
            internalSwizzle.swizzleGreen = hasGB ? GL_RED : GL_ZERO;
            internalSwizzle.swizzleBlue  = hasGB ? GL_RED : GL_ZERO;
            internalSwizzle.swizzleAlpha = GL_ONE;
        }
        else
        {
            // Color bits are all zero for blocked formats
            if (!angleFormat.isBlock)
            {
                // Set any missing channel to default in case the emulated format has that channel.
                internalSwizzle.swizzleRed   = angleFormat.redBits > 0 ? GL_RED : GL_ZERO;
                internalSwizzle.swizzleGreen = angleFormat.greenBits > 0 ? GL_GREEN : GL_ZERO;
                internalSwizzle.swizzleBlue  = angleFormat.blueBits > 0 ? GL_BLUE : GL_ZERO;
                internalSwizzle.swizzleAlpha = angleFormat.alphaBits > 0 ? GL_ALPHA : GL_ONE;
            }
        }
    }

    return internalSwizzle;
}
}  // namespace rx
