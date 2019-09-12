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
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_caps_utils.h"

namespace rx
{
namespace
{
void AddSampleCounts(VkSampleCountFlags sampleCounts, gl::SupportedSampleSet *outSet)
{
    // The possible bits are VK_SAMPLE_COUNT_n_BIT = n, with n = 1 << b.  At the time of this
    // writing, b is in [0, 6], however, we test all 32 bits in case the enum is extended.
    for (unsigned int i = 0; i < 32; ++i)
    {
        if ((sampleCounts & (1 << i)) != 0)
        {
            outSet->insert(1 << i);
        }
    }
}

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
    outTextureCaps->textureAttachment =
        hasColorAttachmentFeatureBit || hasDepthAttachmentFeatureBit;
    outTextureCaps->renderbuffer = outTextureCaps->textureAttachment;

    if (outTextureCaps->renderbuffer)
    {
        if (hasColorAttachmentFeatureBit)
        {
            AddSampleCounts(physicalDeviceLimits.framebufferColorSampleCounts,
                            &outTextureCaps->sampleCounts);
        }
        if (hasDepthAttachmentFeatureBit)
        {
            AddSampleCounts(physicalDeviceLimits.framebufferDepthSampleCounts,
                            &outTextureCaps->sampleCounts);
            AddSampleCounts(physicalDeviceLimits.framebufferStencilSampleCounts,
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
                        int numInfo,
                        SupportTest hasSupport)
{
    ASSERT(numInfo > 0);
    const int last = numInfo - 1;

    for (int i = 0; i < last; ++i)
    {
        ASSERT(info[i].format != angle::FormatID::NONE);
        if (hasSupport(renderer, info[i].vkFormat))
            return i;
    }

    // List must contain a supported item.  We failed on all the others so the last one must be it.
    ASSERT(info[last].format != angle::FormatID::NONE);
    ASSERT(hasSupport(renderer, info[last].vkFormat));
    return last;
}

}  // anonymous namespace

namespace vk
{

// Format implementation.
Format::Format()
    : angleFormatID(angle::FormatID::NONE),
      internalFormat(GL_NONE),
      imageFormatID(angle::FormatID::NONE),
      vkImageFormat(VK_FORMAT_UNDEFINED),
      bufferFormatID(angle::FormatID::NONE),
      vkBufferFormat(VK_FORMAT_UNDEFINED),
      imageInitializerFunction(nullptr),
      textureLoadFunctions(),
      vertexLoadRequiresConversion(false),
      vkBufferFormatIsPacked(false),
      vkSupportsStorageBuffer(false),
      vkFormatIsInt(false),
      vkFormatIsUnsigned(false)
{}

void Format::initImageFallback(RendererVk *renderer, const ImageFormatInitInfo *info, int numInfo)
{
    size_t skip = renderer->getFeatures().forceFallbackFormat ? 1 : 0;
    int i = FindSupportedFormat(renderer, info + skip, numInfo - skip, HasFullTextureFormatSupport);
    i += skip;

    imageFormatID            = info[i].format;
    vkImageFormat            = info[i].vkFormat;
    imageInitializerFunction = info[i].initializer;
}

void Format::initBufferFallback(RendererVk *renderer, const BufferFormatInitInfo *info, int numInfo)
{
    size_t skip = renderer->getFeatures().forceFallbackFormat ? 1 : 0;
    int i = FindSupportedFormat(renderer, info + skip, numInfo - skip, HasFullBufferFormatSupport);
    i += skip;

    bufferFormatID               = info[i].format;
    vkBufferFormat               = info[i].vkFormat;
    vkBufferFormatIsPacked       = info[i].vkFormatIsPacked;
    vertexLoadFunction           = info[i].vertexLoadFunction;
    vertexLoadRequiresConversion = info[i].vertexLoadRequiresConversion;
}

size_t Format::getImageCopyBufferAlignment() const
{
    // vkCmdCopyBufferToImage must have an offset that is a multiple of 4 as well as a multiple
    // of the pixel block size.
    // https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkBufferImageCopy.html
    //
    // We need lcm(4, blockSize) (lcm = least common multiplier).  Since 4 is constant, this
    // can be calculated as:
    //
    //                      | blockSize             blockSize % 4 == 0
    //                      | 4 * blockSize         blockSize % 4 == 1
    // lcm(4, blockSize) = <
    //                      | 2 * blockSize         blockSize % 4 == 2
    //                      | 4 * blockSize         blockSize % 4 == 3
    //
    // This means:
    //
    // - blockSize % 2 != 0 gives a 4x multiplier
    // - else blockSize % 4 != 0 gives a 2x multiplier
    // - else there's no multiplier.
    //
    const angle::Format &format = imageFormat();

    if (!format.isBlock)
    {
        // Currently, 4 is sufficient for any known non-block format.
        return 4;
    }

    const size_t blockSize  = format.pixelBytes;
    const size_t multiplier = blockSize % 2 != 0 ? 4 : blockSize % 4 != 0 ? 2 : 1;
    const size_t alignment  = multiplier * blockSize;

    return alignment;
}

bool Format::hasEmulatedImageChannels() const
{
    const angle::Format &angleFmt   = angleFormat();
    const angle::Format &textureFmt = imageFormat();

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
        format.angleFormatID        = formatID;

        if (!format.valid())
        {
            continue;
        }

        format.vkSupportsStorageBuffer = renderer->hasBufferFormatFeatureBits(
            format.vkBufferFormat, VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT);

        gl::TextureCaps textureCaps;
        FillTextureFormatCaps(renderer, format.vkImageFormat, &textureCaps);
        outTextureCapsMap->set(formatID, textureCaps);

        if (textureCaps.texturable)
        {
            format.textureLoadFunctions = GetLoadFunctionsMap(internalFormat, format.imageFormatID);
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

size_t GetVertexInputAlignment(const vk::Format &format)
{
    const angle::Format &bufferFormat = format.bufferFormat();
    size_t pixelBytes                 = bufferFormat.pixelBytes;
    return format.vkBufferFormatIsPacked ? pixelBytes : (pixelBytes / bufferFormat.channelCount());
}

void MapSwizzleState(const vk::Format &format,
                     const gl::SwizzleState &swizzleState,
                     gl::SwizzleState *swizzleStateOut)
{
    const angle::Format &angleFormat = format.angleFormat();

    if (angleFormat.isBlock)
    {
        // No need to override swizzles for compressed images, as they are not emulated.
        // Either way, angleFormat.xBits (with x in {red, green, blue, alpha}) is zero for blocked
        // formats so the following code would incorrectly turn its swizzle to (0, 0, 0, 1).
        return;
    }

    switch (format.internalFormat)
    {
        case GL_LUMINANCE8_OES:
            swizzleStateOut->swizzleRed   = swizzleState.swizzleRed;
            swizzleStateOut->swizzleGreen = swizzleState.swizzleRed;
            swizzleStateOut->swizzleBlue  = swizzleState.swizzleRed;
            swizzleStateOut->swizzleAlpha = GL_ONE;
            break;
        case GL_LUMINANCE8_ALPHA8_OES:
            swizzleStateOut->swizzleRed   = swizzleState.swizzleRed;
            swizzleStateOut->swizzleGreen = swizzleState.swizzleRed;
            swizzleStateOut->swizzleBlue  = swizzleState.swizzleRed;
            swizzleStateOut->swizzleAlpha = swizzleState.swizzleGreen;
            break;
        case GL_ALPHA8_OES:
            swizzleStateOut->swizzleRed   = GL_ZERO;
            swizzleStateOut->swizzleGreen = GL_ZERO;
            swizzleStateOut->swizzleBlue  = GL_ZERO;
            swizzleStateOut->swizzleAlpha = swizzleState.swizzleRed;
            break;
        default:
            // Set any missing channel to default in case the emulated format has that channel.
            swizzleStateOut->swizzleRed =
                angleFormat.redBits > 0 ? swizzleState.swizzleRed : GL_ZERO;
            swizzleStateOut->swizzleGreen =
                angleFormat.greenBits > 0 ? swizzleState.swizzleGreen : GL_ZERO;
            swizzleStateOut->swizzleBlue =
                angleFormat.blueBits > 0 ? swizzleState.swizzleBlue : GL_ZERO;
            swizzleStateOut->swizzleAlpha =
                angleFormat.alphaBits > 0 ? swizzleState.swizzleAlpha : GL_ONE;
            break;
    }
}
}  // namespace rx
