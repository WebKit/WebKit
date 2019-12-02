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

namespace rx
{

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

    if (!HasFullTextureFormatSupport(renderer, bufferFormatProperties.format))
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

    const vk::Format &vkFormat       = renderer->getFormat(internalFormat);
    const angle::Format &imageFormat = vkFormat.actualImageFormat();
    bool isDepthOrStencilFormat      = imageFormat.depthBits > 0 || imageFormat.stencilBits > 0;
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        (imageFormat.redBits > 0 ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
        (isDepthOrStencilFormat ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0);

    VkExternalFormatANDROID externalFormat = {};
    externalFormat.sType                   = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    externalFormat.externalFormat          = 0;

    if (bufferFormatProperties.format == VK_FORMAT_UNDEFINED)
    {
        externalFormat.externalFormat = bufferFormatProperties.externalFormat;
    }

    VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
    externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageCreateInfo.pNext = &externalFormat;
    externalMemoryImageCreateInfo.handleTypes =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkExtent3D vkExtents;
    gl_vk::GetExtent(mSize, &vkExtents);

    mImage = new vk::ImageHelper();
    ANGLE_TRY(mImage->initExternal(displayVk, gl::TextureType::_2D, vkExtents, vkFormat, 1, usage,
                                   vk::ImageLayout::ExternalPreInitialized,
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
    ANGLE_TRY(mImage->initExternalMemory(displayVk, renderer->getMemoryProperties(),
                                         externalMemoryRequirements, &dedicatedAllocInfo,
                                         VK_QUEUE_FAMILY_FOREIGN_EXT, flags));

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
