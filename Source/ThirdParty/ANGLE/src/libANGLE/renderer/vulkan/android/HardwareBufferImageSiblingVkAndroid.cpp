//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// HardwareBufferImageSiblingVkAndroid.cpp: Implements HardwareBufferImageSiblingVkAndroid.

#include "libANGLE/renderer/vulkan/android/HardwareBufferImageSiblingVkAndroid.h"

#include "common/android_util.h"

#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

#include <android/hardware_buffer.h>

namespace rx
{

namespace
{
VkImageTiling AhbDescUsageToVkImageTiling(const AHardwareBuffer_Desc &ahbDescription)
{
    if ((ahbDescription.usage & AHARDWAREBUFFER_USAGE_CPU_READ_MASK) != 0 ||
        (ahbDescription.usage & AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK) != 0)
    {
        return VK_IMAGE_TILING_LINEAR;
    }

    return VK_IMAGE_TILING_OPTIMAL;
}
}  // namespace

HardwareBufferImageSiblingVkAndroid::HardwareBufferImageSiblingVkAndroid(EGLClientBuffer buffer)
    : mBuffer(buffer),
      mFormat(GL_NONE),
      mRenderable(false),
      mTextureable(false),
      mSamples(0),
      mImage(nullptr)
{}

HardwareBufferImageSiblingVkAndroid::~HardwareBufferImageSiblingVkAndroid() {}

// Static
egl::Error HardwareBufferImageSiblingVkAndroid::ValidateHardwareBuffer(RendererVk *renderer,
                                                                       EGLClientBuffer buffer)
{
    struct ANativeWindowBuffer *windowBuffer =
        angle::android::ClientBufferToANativeWindowBuffer(buffer);
    struct AHardwareBuffer *hardwareBuffer =
        angle::android::ANativeWindowBufferToAHardwareBuffer(windowBuffer);

    VkAndroidHardwareBufferFormatPropertiesANDROID bufferFormatProperties = {};
    bufferFormatProperties.sType =
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
    bufferFormatProperties.pNext = nullptr;

    VkAndroidHardwareBufferPropertiesANDROID bufferProperties = {};
    bufferProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    bufferProperties.pNext = &bufferFormatProperties;

    VkDevice device = renderer->getDevice();
    VkResult result =
        vkGetAndroidHardwareBufferPropertiesANDROID(device, hardwareBuffer, &bufferProperties);
    if (result != VK_SUCCESS)
    {
        return egl::EglBadParameter() << "Failed to query AHardwareBuffer properties";
    }

    if (bufferFormatProperties.format == VK_FORMAT_UNDEFINED)
    {
        ASSERT(bufferFormatProperties.externalFormat != 0);
        // We must have an external format, check that it supports texture sampling
        if (!(bufferFormatProperties.formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        {
            return egl::EglBadParameter()
                   << "Sampling from AHardwareBuffer externalFormat 0x" << std::hex
                   << bufferFormatProperties.externalFormat << " is unsupported ";
        }
    }
    else if (!HasFullTextureFormatSupport(renderer, bufferFormatProperties.format))
    {
        return egl::EglBadParameter()
               << "AHardwareBuffer format does not support enough features to use as a texture.";
    }

    return egl::NoError();
}

egl::Error HardwareBufferImageSiblingVkAndroid::initialize(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    return angle::ToEGL(initImpl(displayVk), displayVk, EGL_BAD_PARAMETER);
}

// Map AHB usage flags to VkImageUsageFlags using this table from the Vulkan spec
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap10.html#memory-external-android-hardware-buffer-usage
VkImageUsageFlags AhbDescUsageToVkImageUsage(const AHardwareBuffer_Desc &ahbDescription,
                                             bool isDepthOrStencilFormat)
{
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if ((ahbDescription.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) != 0)
    {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }

    if ((ahbDescription.usage & AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER) != 0)
    {
        if (isDepthOrStencilFormat)
        {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else
        {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    if ((ahbDescription.usage & AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP) != 0)
    {
        usage |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    if ((ahbDescription.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) != 0)
    {
        usage |= VK_IMAGE_CREATE_PROTECTED_BIT;
    }

    return usage;
}

angle::Result HardwareBufferImageSiblingVkAndroid::initImpl(DisplayVk *displayVk)
{
    RendererVk *renderer = displayVk->getRenderer();

    struct ANativeWindowBuffer *windowBuffer =
        angle::android::ClientBufferToANativeWindowBuffer(mBuffer);

    int pixelFormat = 0;
    angle::android::GetANativeWindowBufferProperties(windowBuffer, &mSize.width, &mSize.height,
                                                     &mSize.depth, &pixelFormat);
    GLenum internalFormat = angle::android::NativePixelFormatToGLInternalFormat(pixelFormat);
    mFormat               = gl::Format(internalFormat);

    struct AHardwareBuffer *hardwareBuffer =
        angle::android::ANativeWindowBufferToAHardwareBuffer(windowBuffer);

    VkAndroidHardwareBufferFormatPropertiesANDROID bufferFormatProperties;
    bufferFormatProperties.sType =
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
    bufferFormatProperties.pNext = nullptr;

    VkAndroidHardwareBufferPropertiesANDROID bufferProperties = {};
    bufferProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    bufferProperties.pNext = &bufferFormatProperties;

    VkDevice device = renderer->getDevice();
    ANGLE_VK_TRY(displayVk, vkGetAndroidHardwareBufferPropertiesANDROID(device, hardwareBuffer,
                                                                        &bufferProperties));

    VkExternalFormatANDROID externalFormat = {};
    externalFormat.sType                   = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    externalFormat.externalFormat          = 0;

    const vk::Format &vkFormat         = renderer->getFormat(internalFormat);
    const vk::Format &externalVkFormat = renderer->getFormat(angle::FormatID::NONE);
    const angle::Format &imageFormat   = vkFormat.actualImageFormat();
    bool isDepthOrStencilFormat        = imageFormat.hasDepthOrStencilBits();

    // Query AHB description and do the following -
    // 1. Derive VkImageTiling mode based on AHB usage flags
    // 2. Map AHB usage flags to VkImageUsageFlags
    AHardwareBuffer_Desc ahbDescription;
    AHardwareBuffer_describe(hardwareBuffer, &ahbDescription);
    VkImageTiling imageTilingMode = AhbDescUsageToVkImageTiling(ahbDescription);
    VkImageUsageFlags usage = AhbDescUsageToVkImageUsage(ahbDescription, isDepthOrStencilFormat);

    if (bufferFormatProperties.format == VK_FORMAT_UNDEFINED)
    {
        externalFormat.externalFormat = bufferFormatProperties.externalFormat;

        // VkImageCreateInfo struct: If the pNext chain includes a VkExternalFormatANDROID structure
        // whose externalFormat member is not 0, usage must not include any usages except
        // VK_IMAGE_USAGE_SAMPLED_BIT
        usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};

    externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageCreateInfo.pNext = &externalFormat;
    externalMemoryImageCreateInfo.handleTypes =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkExtent3D vkExtents;
    gl_vk::GetExtent(mSize, &vkExtents);

    mImage = new vk::ImageHelper();

    mImage->setTilingMode(imageTilingMode);
    ANGLE_TRY(mImage->initExternal(
        displayVk, gl::TextureType::_2D, vkExtents,
        bufferFormatProperties.format == VK_FORMAT_UNDEFINED ? externalVkFormat : vkFormat, 1,
        usage, vk::kVkImageCreateFlagsNone, vk::ImageLayout::ExternalPreInitialized,
        &externalMemoryImageCreateInfo, 0, 0, 1, 1));

    VkImportAndroidHardwareBufferInfoANDROID importHardwareBufferInfo = {};
    importHardwareBufferInfo.sType  = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    importHardwareBufferInfo.buffer = hardwareBuffer;

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
    dedicatedAllocInfo.sType  = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.pNext  = &importHardwareBufferInfo;
    dedicatedAllocInfo.image  = mImage->getImage().getHandle();
    dedicatedAllocInfo.buffer = VK_NULL_HANDLE;

    VkMemoryRequirements externalMemoryRequirements = {};
    externalMemoryRequirements.size                 = bufferProperties.allocationSize;
    externalMemoryRequirements.alignment            = 0;
    externalMemoryRequirements.memoryTypeBits       = bufferProperties.memoryTypeBits;

    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (bufferFormatProperties.format == VK_FORMAT_UNDEFINED)
    {
        // Note from Vulkan spec: Since GL_OES_EGL_image_external does not require the same sampling
        // and conversion calculations as Vulkan does, achieving identical results between APIs may
        // not be possible on some implementations.
        ANGLE_VK_CHECK(displayVk, renderer->getFeatures().supportsYUVSamplerConversion.enabled,
                       VK_ERROR_FEATURE_NOT_PRESENT);
        ASSERT(externalFormat.pNext == nullptr);
        VkSamplerYcbcrConversionCreateInfo yuvConversionInfo = {};
        yuvConversionInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
        yuvConversionInfo.pNext         = &externalFormat;
        yuvConversionInfo.format        = VK_FORMAT_UNDEFINED;
        yuvConversionInfo.xChromaOffset = bufferFormatProperties.suggestedXChromaOffset;
        yuvConversionInfo.yChromaOffset = bufferFormatProperties.suggestedYChromaOffset;
        yuvConversionInfo.ycbcrModel    = bufferFormatProperties.suggestedYcbcrModel;
        yuvConversionInfo.ycbcrRange    = bufferFormatProperties.suggestedYcbcrRange;
        yuvConversionInfo.chromaFilter  = VK_FILTER_LINEAR;
        yuvConversionInfo.components    = bufferFormatProperties.samplerYcbcrConversionComponents;

        ANGLE_TRY(mImage->initExternalMemory(
            displayVk, renderer->getMemoryProperties(), externalMemoryRequirements,
            &yuvConversionInfo, &dedicatedAllocInfo, VK_QUEUE_FAMILY_FOREIGN_EXT, flags));
    }
    else
    {
        ANGLE_TRY(mImage->initExternalMemory(
            displayVk, renderer->getMemoryProperties(), externalMemoryRequirements, nullptr,
            &dedicatedAllocInfo, VK_QUEUE_FAMILY_FOREIGN_EXT, flags));
    }

    constexpr uint32_t kColorRenderableRequiredBits        = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    constexpr uint32_t kDepthStencilRenderableRequiredBits = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    mRenderable =
        renderer->hasImageFormatFeatureBits(vkFormat.vkImageFormat, kColorRenderableRequiredBits) ||
        renderer->hasImageFormatFeatureBits(vkFormat.vkImageFormat,
                                            kDepthStencilRenderableRequiredBits);

    constexpr uint32_t kTextureableRequiredBits =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    mTextureable =
        renderer->hasImageFormatFeatureBits(vkFormat.vkImageFormat, kTextureableRequiredBits);

    return angle::Result::Continue;
}

void HardwareBufferImageSiblingVkAndroid::onDestroy(const egl::Display *display)
{
    ASSERT(mImage == nullptr);
}

gl::Format HardwareBufferImageSiblingVkAndroid::getFormat() const
{
    return mFormat;
}

bool HardwareBufferImageSiblingVkAndroid::isRenderable(const gl::Context *context) const
{
    return mRenderable;
}

bool HardwareBufferImageSiblingVkAndroid::isTexturable(const gl::Context *context) const
{
    return mTextureable;
}

gl::Extents HardwareBufferImageSiblingVkAndroid::getSize() const
{
    return mSize;
}

size_t HardwareBufferImageSiblingVkAndroid::getSamples() const
{
    return mSamples;
}

// ExternalImageSiblingVk interface
vk::ImageHelper *HardwareBufferImageSiblingVkAndroid::getImage() const
{
    return mImage;
}

void HardwareBufferImageSiblingVkAndroid::release(RendererVk *renderer)
{
    if (mImage != nullptr)
    {
        mImage->releaseImage(renderer);
        mImage->releaseStagingBuffer(renderer);
        SafeDelete(mImage);
    }
}

}  // namespace rx
