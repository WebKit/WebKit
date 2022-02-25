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
#include "libANGLE/renderer/vulkan/android/AHBFunctions.h"
#include "libANGLE/renderer/vulkan/android/DisplayVkAndroid.h"

namespace rx
{

namespace
{
VkImageTiling AhbDescUsageToVkImageTiling(const AHardwareBuffer_Desc &ahbDescription)
{
    // A note about the choice of OPTIMAL here.

    // When running Android on certain GPUs, there are problems creating Vulkan
    // image siblings of AHardwareBuffers because it's currently assumed that
    // the underlying driver can create linear tiling images that have input
    // attachment usage, which isn't supported on NVIDIA for example, resulting
    // in failure to create the image siblings. Yet, we don't currently take
    // advantage of linear elsewhere in ANGLE. To maintain maximum
    // compatibility on Android for such drivers, use optimal tiling for image
    // siblings.
    //
    // Note that while we have switched to optimal unconditionally in this path
    // versus linear, it's possible that previously compatible linear usages
    // might become uncompatible after switching to optimal. However, from what
    // we've seen on Samsung/NVIDIA/Intel/AMD GPUs so far, formats generally
    // have more possible usages in optimal tiling versus linear tiling:
    //
    // http://vulkan.gpuinfo.org/displayreport.php?id=10804#formats_linear
    // http://vulkan.gpuinfo.org/displayreport.php?id=10804#formats_optimal
    //
    // http://vulkan.gpuinfo.org/displayreport.php?id=10807#formats_linear
    // http://vulkan.gpuinfo.org/displayreport.php?id=10807#formats_optimal
    //
    // http://vulkan.gpuinfo.org/displayreport.php?id=10809#formats_linear
    // http://vulkan.gpuinfo.org/displayreport.php?id=10809#formats_optimal
    //
    // http://vulkan.gpuinfo.org/displayreport.php?id=10787#formats_linear
    // http://vulkan.gpuinfo.org/displayreport.php?id=10787#formats_optimal
    //
    // Also, as an aside, in terms of what's generally expected from the Vulkan
    // ICD in Android when determining AHB compatibility, if the vendor wants
    // to declare a particular combination of format/tiling/usage/etc as not
    // supported AHB-wise, it's up to the ICD vendor to zero out bits in
    // supportedHandleTypes in the vkGetPhysicalDeviceImageFormatProperties2
    // query:
    //
    // ``` *
    // [VUID-VkImageCreateInfo-pNext-00990](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#VUID-VkImageCreateInfo-pNext-00990)
    // If the pNext chain includes a VkExternalMemoryImageCreateInfo structure,
    // its handleTypes member must only contain bits that are also in
    // VkExternalImageFormatProperties::externalMemoryProperties.compatibleHandleTypes,
    // as returned by vkGetPhysicalDeviceImageFormatProperties2 with format,
    // imageType, tiling, usage, and flags equal to those in this structure,
    // and with a VkPhysicalDeviceExternalImageFormatInfo structure included in
    // the pNext chain, with a handleType equal to any one of the handle types
    // specified in VkExternalMemoryImageCreateInfo::handleTypes ```

    return VK_IMAGE_TILING_OPTIMAL;
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

    return usage;
}
}  // namespace

HardwareBufferImageSiblingVkAndroid::HardwareBufferImageSiblingVkAndroid(EGLClientBuffer buffer)
    : mBuffer(buffer),
      mFormat(GL_NONE),
      mRenderable(false),
      mTextureable(false),
      mYUV(false),
      mSamples(0),
      mImage(nullptr)
{}

HardwareBufferImageSiblingVkAndroid::~HardwareBufferImageSiblingVkAndroid() {}

// Static
egl::Error HardwareBufferImageSiblingVkAndroid::ValidateHardwareBuffer(
    RendererVk *renderer,
    EGLClientBuffer buffer,
    const egl::AttributeMap &attribs)
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
    else
    {
        angle::FormatID formatID = vk::GetFormatIDFromVkFormat(bufferFormatProperties.format);
        if (!HasFullTextureFormatSupport(renderer, formatID))
        {
            return egl::EglBadParameter()
                   << "AHardwareBuffer format " << bufferFormatProperties.format
                   << " does not support enough features to use as a texture.";
        }
    }

    if (attribs.getAsInt(EGL_PROTECTED_CONTENT_EXT, EGL_FALSE) == EGL_TRUE)
    {
        int width       = 0;
        int height      = 0;
        int depth       = 0;
        int pixelFormat = 0;
        uint64_t usage  = 0;
        angle::android::GetANativeWindowBufferProperties(windowBuffer, &width, &height, &depth,
                                                         &pixelFormat, &usage);
        if ((usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) == 0)
        {
            return egl::EglBadAccess()
                   << "EGL_PROTECTED_CONTENT_EXT attribute does not match protected state "
                      "of EGLCleintBuffer.";
        }
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
    const AHBFunctions &functions = static_cast<DisplayVkAndroid *>(displayVk)->getAHBFunctions();
    ANGLE_VK_CHECK(displayVk, functions.valid(), VK_ERROR_INITIALIZATION_FAILED);

    RendererVk *renderer = displayVk->getRenderer();

    struct ANativeWindowBuffer *windowBuffer =
        angle::android::ClientBufferToANativeWindowBuffer(mBuffer);

    int pixelFormat = 0;
    angle::android::GetANativeWindowBufferProperties(windowBuffer, &mSize.width, &mSize.height,
                                                     &mSize.depth, &pixelFormat, &mUsage);
    GLenum internalFormat = angle::android::NativePixelFormatToGLInternalFormat(pixelFormat);
    mFormat               = gl::Format(internalFormat);

    struct AHardwareBuffer *hardwareBuffer =
        angle::android::ANativeWindowBufferToAHardwareBuffer(windowBuffer);

    functions.acquire(hardwareBuffer);
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
    const angle::Format &imageFormat   = vkFormat.getActualRenderableImageFormat();
    bool isDepthOrStencilFormat        = imageFormat.hasDepthOrStencilBits();

    // Query AHB description and do the following -
    // 1. Derive VkImageTiling mode based on AHB usage flags
    // 2. Map AHB usage flags to VkImageUsageFlags
    AHardwareBuffer_Desc ahbDescription;
    functions.describe(hardwareBuffer, &ahbDescription);
    VkImageTiling imageTilingMode = AhbDescUsageToVkImageTiling(ahbDescription);
    VkImageUsageFlags usage = AhbDescUsageToVkImageUsage(ahbDescription, isDepthOrStencilFormat);

    if (bufferFormatProperties.format == VK_FORMAT_UNDEFINED)
    {
        ANGLE_VK_CHECK(displayVk, bufferFormatProperties.externalFormat != 0, VK_ERROR_UNKNOWN);
        externalFormat.externalFormat = bufferFormatProperties.externalFormat;

        // VkImageCreateInfo struct: If the pNext chain includes a VkExternalFormatANDROID structure
        // whose externalFormat member is not 0, usage must not include any usages except
        // VK_IMAGE_USAGE_SAMPLED_BIT
        usage = VK_IMAGE_USAGE_SAMPLED_BIT;

        // If the pNext chain includes a VkExternalFormatANDROID structure whose externalFormat
        // member is not 0, tiling must be VK_IMAGE_TILING_OPTIMAL
        imageTilingMode = VK_IMAGE_TILING_OPTIMAL;
    }

    VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
    externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageCreateInfo.pNext = &externalFormat;
    externalMemoryImageCreateInfo.handleTypes =
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkExtent3D vkExtents;
    gl_vk::GetExtent(mSize, &vkExtents);

    const uint32_t layerCount = mSize.depth;
    vkExtents.depth           = 1;

    mImage = new vk::ImageHelper();

    // disable robust init for this external image.
    bool robustInitEnabled = false;

    mImage->setTilingMode(imageTilingMode);
    VkImageCreateFlags imageCreateFlags = vk::kVkImageCreateFlagsNone;
    if (hasProtectedContent())
    {
        imageCreateFlags |= VK_IMAGE_CREATE_PROTECTED_BIT;
    }

    const vk::Format &format =
        bufferFormatProperties.format == VK_FORMAT_UNDEFINED ? externalVkFormat : vkFormat;
    const gl::TextureType textureType =
        layerCount > 1 ? gl::TextureType::_2DArray : gl::TextureType::_2D;

    VkImageFormatListCreateInfoKHR imageFormatListInfoStorage;
    vk::ImageHelper::ImageListFormats imageListFormatsStorage;
    const void *imageCreateInfoPNext = vk::ImageHelper::DeriveCreateInfoPNext(
        displayVk, format.getActualRenderableImageFormatID(), &externalMemoryImageCreateInfo,
        &imageFormatListInfoStorage, &imageListFormatsStorage, &imageCreateFlags);

    ANGLE_TRY(mImage->initExternal(displayVk, textureType, vkExtents, format.getIntendedFormatID(),
                                   format.getActualRenderableImageFormatID(), 1, usage,
                                   imageCreateFlags, vk::ImageLayout::ExternalPreInitialized,
                                   imageCreateInfoPNext, gl::LevelIndex(0), 1, layerCount,
                                   robustInitEnabled, hasProtectedContent()));

    VkImportAndroidHardwareBufferInfoANDROID importHardwareBufferInfo = {};
    importHardwareBufferInfo.sType  = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    importHardwareBufferInfo.buffer = hardwareBuffer;

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
    dedicatedAllocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.pNext          = &importHardwareBufferInfo;
    dedicatedAllocInfo.image          = mImage->getImage().getHandle();
    dedicatedAllocInfo.buffer         = VK_NULL_HANDLE;
    const void *dedicatedAllocInfoPtr = &dedicatedAllocInfo;

    VkMemoryRequirements externalMemoryRequirements = {};
    externalMemoryRequirements.size                 = bufferProperties.allocationSize;
    externalMemoryRequirements.alignment            = 0;
    externalMemoryRequirements.memoryTypeBits       = bufferProperties.memoryTypeBits;

    const VkMemoryPropertyFlags flags =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        (hasProtectedContent() ? VK_MEMORY_PROPERTY_PROTECTED_BIT : 0);

    if (bufferFormatProperties.format == VK_FORMAT_UNDEFINED)
    {
        // Note from Vulkan spec: Since GL_OES_EGL_image_external does not require the same sampling
        // and conversion calculations as Vulkan does, achieving identical results between APIs may
        // not be possible on some implementations.
        ANGLE_VK_CHECK(displayVk, renderer->getFeatures().supportsYUVSamplerConversion.enabled,
                       VK_ERROR_FEATURE_NOT_PRESENT);
        ASSERT(externalFormat.pNext == nullptr);

        // Update the SamplerYcbcrConversionCache key
        mImage->updateYcbcrConversionDesc(
            renderer, bufferFormatProperties.externalFormat,
            bufferFormatProperties.suggestedYcbcrModel, bufferFormatProperties.suggestedYcbcrRange,
            bufferFormatProperties.suggestedXChromaOffset,
            bufferFormatProperties.suggestedYChromaOffset, VK_FILTER_NEAREST,
            bufferFormatProperties.samplerYcbcrConversionComponents, angle::FormatID::NONE);
        mYUV = true;
    }

    ANGLE_TRY(mImage->initExternalMemory(displayVk, renderer->getMemoryProperties(),
                                         externalMemoryRequirements, 1, &dedicatedAllocInfoPtr,
                                         VK_QUEUE_FAMILY_FOREIGN_EXT, flags));

    constexpr uint32_t kColorRenderableRequiredBits        = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    constexpr uint32_t kDepthStencilRenderableRequiredBits = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    mRenderable = renderer->hasImageFormatFeatureBits(vkFormat.getActualRenderableImageFormatID(),
                                                      kColorRenderableRequiredBits) ||
                  renderer->hasImageFormatFeatureBits(vkFormat.getActualRenderableImageFormatID(),
                                                      kDepthStencilRenderableRequiredBits);

    constexpr uint32_t kTextureableRequiredBits =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    mTextureable = renderer->hasImageFormatFeatureBits(vkFormat.getActualRenderableImageFormatID(),
                                                       kTextureableRequiredBits);

    return angle::Result::Continue;
}

void HardwareBufferImageSiblingVkAndroid::onDestroy(const egl::Display *display)
{
    const AHBFunctions &functions = GetImplAs<DisplayVkAndroid>(display)->getAHBFunctions();
    ASSERT(functions.valid());

    functions.release(angle::android::ANativeWindowBufferToAHardwareBuffer(
        angle::android::ClientBufferToANativeWindowBuffer(mBuffer)));

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

bool HardwareBufferImageSiblingVkAndroid::isYUV() const
{
    return mYUV;
}

bool HardwareBufferImageSiblingVkAndroid::hasProtectedContent() const
{
    return ((mUsage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) != 0);
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
        // TODO: Handle the case where the EGLImage is used in two contexts not in the same share
        // group.  https://issuetracker.google.com/169868803
        mImage->releaseImage(renderer);
        mImage->releaseStagedUpdates(renderer);
        SafeDelete(mImage);
    }
}

}  // namespace rx
