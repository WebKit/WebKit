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
void FillTextureFormatCaps(RendererVk *renderer, VkFormat format, gl::TextureCaps *outTextureCaps)
{
    const VkPhysicalDeviceLimits &physicalDeviceLimits =
        renderer->getPhysicalDeviceProperties().limits;
    bool hasColorAttachmentFeatureBit =
        renderer->hasImageFormatFeatureBits(format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
    bool hasDepthAttachmentFeatureBit =
        renderer->hasImageFormatFeatureBits(format, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    outTextureCaps->texturable =
        renderer->hasImageFormatFeatureBits(format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    outTextureCaps->filterable = renderer->hasImageFormatFeatureBits(
        format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    outTextureCaps->blendable =
        renderer->hasImageFormatFeatureBits(format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);

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

bool HasFullBufferFormatSupport(RendererVk *renderer, VkFormat vkFormat)
{
    return renderer->hasBufferFormatFeatureBits(vkFormat, VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT);
}

using SupportTest = bool (*)(RendererVk *renderer, VkFormat vkFormat);

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
        if (hasSupport(renderer, info[i].vkFormat))
            return i;
    }

    if (skip > 0 && !hasSupport(renderer, info[last].vkFormat))
    {
        // We couldn't find a valid fallback, try again without skip
        return FindSupportedFormat(renderer, info, 0, numInfo, hasSupport);
    }

    ASSERT(info[last].format != angle::FormatID::NONE);
    ASSERT(hasSupport(renderer, info[last].vkFormat));
    return last;
}

}  // anonymous namespace

namespace vk
{

// Format implementation.
Format::Format()
    : intendedFormatID(angle::FormatID::NONE),
      internalFormat(GL_NONE),
      actualImageFormatID(angle::FormatID::NONE),
      vkImageFormat(VK_FORMAT_UNDEFINED),
      actualBufferFormatID(angle::FormatID::NONE),
      vkBufferFormat(VK_FORMAT_UNDEFINED),
      imageInitializerFunction(nullptr),
      textureLoadFunctions(),
      vertexLoadRequiresConversion(false),
      vkBufferFormatIsPacked(false),
      vkFormatIsInt(false),
      vkFormatIsUnsigned(false)
{}

void Format::initImageFallback(RendererVk *renderer, const ImageFormatInitInfo *info, int numInfo)
{
    size_t skip                 = renderer->getFeatures().forceFallbackFormat.enabled ? 1 : 0;
    SupportTest testFunction    = HasFullTextureFormatSupport;
    const angle::Format &format = angle::Format::Get(info[0].format);
    if (format.isInt() || (format.isFloat() && format.redBits >= 32))
    {
        // Integer formats don't support filtering in GL, so don't test for it.
        // Filtering of 32-bit float textures is not supported on Android, and
        // it's enabled by the extension OES_texture_float_linear, which is
        // enabled automatically by examining format capabilities.
        testFunction = HasNonFilterableTextureFormatSupport;
    }
    if (format.isSnorm() || format.isBlock)
    {
        // Rendering to SNORM textures is not supported on Android, and it's
        // enabled by the extension EXT_render_snorm.
        // Compressed textures also need to perform this check.
        testFunction = HasNonRenderableTextureFormatSupport;
    }
    int i = FindSupportedFormat(renderer, info, skip, static_cast<uint32_t>(numInfo), testFunction);

    actualImageFormatID      = info[i].format;
    vkImageFormat            = info[i].vkFormat;
    imageInitializerFunction = info[i].initializer;
}

void Format::initBufferFallback(RendererVk *renderer, const BufferFormatInitInfo *info, int numInfo)
{
    size_t skip = renderer->getFeatures().forceFallbackFormat.enabled ? 1 : 0;
    int i       = FindSupportedFormat(renderer, info, skip, static_cast<uint32_t>(numInfo),
                                HasFullBufferFormatSupport);

    actualBufferFormatID         = info[i].format;
    vkBufferFormat               = info[i].vkFormat;
    vkBufferFormatIsPacked       = info[i].vkFormatIsPacked;
    vertexLoadFunction           = info[i].vertexLoadFunction;
    vertexLoadRequiresConversion = info[i].vertexLoadRequiresConversion;
}

size_t Format::getImageCopyBufferAlignment() const
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
    const angle::Format &format = actualImageFormat();

    ASSERT(format.pixelBytes != 0);
    const size_t texelSize  = format.pixelBytes;
    const size_t multiplier = texelSize % 2 != 0 ? 4 : texelSize % 4 != 0 ? 2 : 1;
    const size_t alignment  = multiplier * texelSize;

    return alignment;
}

bool Format::hasEmulatedImageChannels() const
{
    const angle::Format &angleFmt   = intendedFormat();
    const angle::Format &textureFmt = actualImageFormat();

    return (angleFmt.alphaBits == 0 && textureFmt.alphaBits > 0) ||
           (angleFmt.blueBits == 0 && textureFmt.blueBits > 0) ||
           (angleFmt.greenBits == 0 && textureFmt.greenBits > 0) ||
           (angleFmt.depthBits == 0 && textureFmt.depthBits > 0) ||
           (angleFmt.stencilBits == 0 && textureFmt.stencilBits > 0);
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

void FormatTable::initialize(RendererVk *renderer,
                             gl::TextureCapsMap *outTextureCapsMap,
                             std::vector<GLenum> *outCompressedTextureFormats)
{
    for (size_t formatIndex = 0; formatIndex < angle::kNumANGLEFormats; ++formatIndex)
    {
        vk::Format &format               = mFormatData[formatIndex];
        const auto formatID              = static_cast<angle::FormatID>(formatIndex);
        const angle::Format &angleFormat = angle::Format::Get(formatID);

        format.initialize(renderer, angleFormat);
        const GLenum internalFormat = format.internalFormat;
        format.intendedFormatID     = formatID;

        if (!format.valid())
        {
            continue;
        }

        gl::TextureCaps textureCaps;
        FillTextureFormatCaps(renderer, format.vkImageFormat, &textureCaps);
        outTextureCapsMap->set(formatID, textureCaps);

        if (textureCaps.texturable)
        {
            format.textureLoadFunctions =
                GetLoadFunctionsMap(internalFormat, format.actualImageFormatID);
        }

        if (angleFormat.isBlock)
        {
            outCompressedTextureFormats->push_back(internalFormat);
        }
    }
}

VkImageUsageFlags GetMaximalImageUsageFlags(RendererVk *renderer, VkFormat format)
{
    constexpr VkFormatFeatureFlags kImageUsageFeatureBits =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    VkFormatFeatureFlags featureBits =
        renderer->getImageFormatFeatureBits(format, kImageUsageFeatureBits);
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

bool HasFullTextureFormatSupport(RendererVk *renderer, VkFormat vkFormat)
{
    constexpr uint32_t kBitsColor = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    constexpr uint32_t kBitsDepth = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return renderer->hasImageFormatFeatureBits(vkFormat, kBitsColor) ||
           renderer->hasImageFormatFeatureBits(vkFormat, kBitsDepth);
}

bool HasNonFilterableTextureFormatSupport(RendererVk *renderer, VkFormat vkFormat)
{
    constexpr uint32_t kBitsColor =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    constexpr uint32_t kBitsDepth = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return renderer->hasImageFormatFeatureBits(vkFormat, kBitsColor) ||
           renderer->hasImageFormatFeatureBits(vkFormat, kBitsDepth);
}

bool HasNonRenderableTextureFormatSupport(RendererVk *renderer, VkFormat vkFormat)
{
    constexpr uint32_t kBitsColor =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    constexpr uint32_t kBitsDepth = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return renderer->hasImageFormatFeatureBits(vkFormat, kBitsColor) ||
           renderer->hasImageFormatFeatureBits(vkFormat, kBitsDepth);
}

size_t GetVertexInputAlignment(const vk::Format &format)
{
    const angle::Format &bufferFormat = format.actualBufferFormat();
    size_t pixelBytes                 = bufferFormat.pixelBytes;
    return format.vkBufferFormatIsPacked ? pixelBytes : (pixelBytes / bufferFormat.channelCount);
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

// Places the swizzle obtained by applying second after first into out.
void ComposeSwizzleState(const gl::SwizzleState &first,
                         const gl::SwizzleState &second,
                         gl::SwizzleState *out)
{
    out->swizzleRed   = GetSwizzleStateComponent(first, second.swizzleRed);
    out->swizzleGreen = GetSwizzleStateComponent(first, second.swizzleGreen);
    out->swizzleBlue  = GetSwizzleStateComponent(first, second.swizzleBlue);
    out->swizzleAlpha = GetSwizzleStateComponent(first, second.swizzleAlpha);
}

void MapSwizzleState(const ContextVk *contextVk,
                     const vk::Format &format,
                     const bool sized,
                     const gl::SwizzleState &swizzleState,
                     gl::SwizzleState *swizzleStateOut)
{
    const angle::Format &angleFormat = format.intendedFormat();

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
            bool hasRed = angleFormat.depthBits > 0;
            // In OES_depth_texture/ARB_depth_texture, depth
            // textures are treated as luminance.
            // If the internalformat was not sized, use OES_depth_texture behavior
            bool hasGB = hasRed && !sized;

            internalSwizzle.swizzleRed   = hasRed ? GL_RED : GL_ZERO;
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
    ComposeSwizzleState(internalSwizzle, swizzleState, swizzleStateOut);
}
}  // namespace rx
