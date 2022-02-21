//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceVk.cpp:
//    Implements the class methods for SurfaceVk.
//

#include "libANGLE/renderer/vulkan/SurfaceVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Overlay.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/OverlayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/trace.h"

namespace rx
{

namespace
{
angle::SubjectIndex kAnySurfaceImageSubjectIndex = 0;

// Special value for currentExtent if surface size is determined by the
// swapchain's extent. See VkSurfaceCapabilitiesKHR spec for more details.
constexpr uint32_t kSurfaceSizedBySwapchain = 0xFFFFFFFFu;

GLint GetSampleCount(const egl::Config *config)
{
    GLint samples = 1;
    if (config->sampleBuffers && config->samples > 1)
    {
        samples = config->samples;
    }
    return samples;
}

VkPresentModeKHR GetDesiredPresentMode(const std::vector<VkPresentModeKHR> &presentModes,
                                       EGLint interval)
{
    ASSERT(!presentModes.empty());

    // If v-sync is enabled, use FIFO, which throttles you to the display rate and is guaranteed to
    // always be supported.
    if (interval > 0)
    {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // Otherwise, choose either of the following, if available, in order specified here:
    //
    // - Mailbox is similar to triple-buffering.
    // - Immediate is similar to single-buffering.
    //
    // If neither is supported, we fallback to FIFO.

    bool mailboxAvailable   = false;
    bool immediateAvailable = false;

    for (VkPresentModeKHR presentMode : presentModes)
    {
        switch (presentMode)
        {
            case VK_PRESENT_MODE_MAILBOX_KHR:
                mailboxAvailable = true;
                break;
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
                immediateAvailable = true;
                break;
            default:
                break;
        }
    }

    if (immediateAvailable)
    {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    if (mailboxAvailable)
    {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }

    // Note again that VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available.
    return VK_PRESENT_MODE_FIFO_KHR;
}

constexpr VkImageUsageFlags kSurfaceVkImageUsageFlags =
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
constexpr VkImageUsageFlags kSurfaceVkColorImageUsageFlags =
    kSurfaceVkImageUsageFlags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
constexpr VkImageUsageFlags kSurfaceVkDepthStencilImageUsageFlags =
    kSurfaceVkImageUsageFlags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

// If the device is rotated with any of the following transform flags, the swapchain width and
// height must be swapped (e.g. make a landscape window portrait).  This must also be done for all
// attachments used with the swapchain (i.e. depth, stencil, and multisample buffers).
constexpr VkSurfaceTransformFlagsKHR k90DegreeRotationVariants =
    VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR | VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR |
    VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR |
    VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR;

bool Is90DegreeRotation(VkSurfaceTransformFlagsKHR transform)
{
    return ((transform & k90DegreeRotationVariants) != 0);
}

angle::Result InitImageHelper(DisplayVk *displayVk,
                              EGLint width,
                              EGLint height,
                              const vk::Format &vkFormat,
                              GLint samples,
                              bool isRobustResourceInitEnabled,
                              bool hasProtectedContent,
                              vk::ImageHelper *imageHelper)
{
    const angle::Format &textureFormat = vkFormat.getActualRenderableImageFormat();
    bool isDepthOrStencilFormat   = textureFormat.depthBits > 0 || textureFormat.stencilBits > 0;
    const VkImageUsageFlags usage = isDepthOrStencilFormat ? kSurfaceVkDepthStencilImageUsageFlags
                                                           : kSurfaceVkColorImageUsageFlags;

    VkExtent3D extents = {std::max(static_cast<uint32_t>(width), 1u),
                          std::max(static_cast<uint32_t>(height), 1u), 1u};

    angle::FormatID renderableFormatId = vkFormat.getActualRenderableImageFormatID();
    RendererVk *rendererVk             = displayVk->getRenderer();
    // For devices that don't support creating swapchain images with RGB8, emulate with RGBA8.
    if (rendererVk->getFeatures().overrideSurfaceFormatRGB8toRGBA8.enabled &&
        renderableFormatId == angle::FormatID::R8G8B8_UNORM)
    {
        renderableFormatId = angle::FormatID::R8G8B8A8_UNORM;
    }

    VkImageCreateFlags imageCreateFlags =
        hasProtectedContent ? VK_IMAGE_CREATE_PROTECTED_BIT : vk::kVkImageCreateFlagsNone;
    ANGLE_TRY(imageHelper->initExternal(
        displayVk, gl::TextureType::_2D, extents, vkFormat.getIntendedFormatID(),
        renderableFormatId, samples, usage, imageCreateFlags, vk::ImageLayout::Undefined, nullptr,
        gl::LevelIndex(0), 1, 1, isRobustResourceInitEnabled, hasProtectedContent));

    return angle::Result::Continue;
}

VkColorSpaceKHR MapEglColorSpaceToVkColorSpace(EGLenum EGLColorspace)
{
    switch (EGLColorspace)
    {
        case EGL_NONE:
        case EGL_GL_COLORSPACE_LINEAR:
        case EGL_GL_COLORSPACE_SRGB_KHR:
        case EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT:
            return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        case EGL_GL_COLORSPACE_DISPLAY_P3_LINEAR_EXT:
            return VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT;
        case EGL_GL_COLORSPACE_DISPLAY_P3_EXT:
            return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
        case EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT:
            return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
        case EGL_GL_COLORSPACE_SCRGB_EXT:
            return VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT;
        default:
            UNREACHABLE();
            return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
}

angle::Result DoesSurfaceSupportFormatAndColorspace(DisplayVk *displayVk,
                                                    VkPhysicalDevice physicalDevice,
                                                    VkSurfaceKHR surface,
                                                    VkFormat format,
                                                    VkColorSpaceKHR colorSpace,
                                                    bool *surfaceFormatSupported)
{
    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo2 = {};
    surfaceInfo2.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
    surfaceInfo2.surface = surface;

    uint32_t surfaceFormatCount = 0;
    ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo2,
                                                                  &surfaceFormatCount, nullptr));

    std::vector<VkSurfaceFormat2KHR> surfaceFormats2(surfaceFormatCount);
    for (VkSurfaceFormat2KHR &surfaceFormat2 : surfaceFormats2)
    {
        surfaceFormat2.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
    }
    ANGLE_VK_TRY(displayVk,
                 vkGetPhysicalDeviceSurfaceFormats2KHR(
                     physicalDevice, &surfaceInfo2, &surfaceFormatCount, surfaceFormats2.data()));

    for (VkSurfaceFormat2KHR &surfaceFormat2 : surfaceFormats2)
    {
        if (surfaceFormat2.surfaceFormat.format == format &&
            surfaceFormat2.surfaceFormat.colorSpace == colorSpace)
        {
            *surfaceFormatSupported = true;
            return angle::Result::Continue;
        }
    }

    return angle::Result::Continue;
}

angle::Result DoesSurfaceSupportFormat(DisplayVk *displayVk,
                                       VkPhysicalDevice physicalDevice,
                                       VkSurfaceKHR surface,
                                       VkFormat format,
                                       bool *surfaceFormatSupported)
{
    uint32_t surfaceFormatCount = 0;
    ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                                                 &surfaceFormatCount, nullptr));

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    ANGLE_VK_TRY(displayVk,
                 vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount,
                                                      surfaceFormats.data()));

    if (surfaceFormatCount == 1u && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        // This is fine.
        *surfaceFormatSupported = true;
    }
    else
    {
        for (const VkSurfaceFormatKHR &surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == format)
            {
                *surfaceFormatSupported = true;
                return angle::Result::Continue;
            }
        }
    }

    return angle::Result::Continue;
}

}  // namespace

#if defined(ANGLE_ENABLE_OVERLAY)
constexpr bool kEnableOverlay = ANGLE_ENABLE_OVERLAY;
#else
constexpr bool kEnableOverlay = false;
#endif

SurfaceVk::SurfaceVk(const egl::SurfaceState &surfaceState) : SurfaceImpl(surfaceState) {}

SurfaceVk::~SurfaceVk() = default;

angle::Result SurfaceVk::getAttachmentRenderTarget(const gl::Context *context,
                                                   GLenum binding,
                                                   const gl::ImageIndex &imageIndex,
                                                   GLsizei samples,
                                                   FramebufferAttachmentRenderTarget **rtOut)
{
    ASSERT(samples == 0);

    if (binding == GL_BACK)
    {
        *rtOut = &mColorRenderTarget;
    }
    else
    {
        ASSERT(binding == GL_DEPTH || binding == GL_STENCIL || binding == GL_DEPTH_STENCIL);
        *rtOut = &mDepthStencilRenderTarget;
    }

    return angle::Result::Continue;
}

void SurfaceVk::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    // Forward the notification to parent class that the staging buffer changed.
    onStateChange(angle::SubjectMessage::SubjectChanged);
}

OffscreenSurfaceVk::AttachmentImage::AttachmentImage(SurfaceVk *surfaceVk)
    : imageObserverBinding(surfaceVk, kAnySurfaceImageSubjectIndex)
{
    imageObserverBinding.bind(&image);
}

OffscreenSurfaceVk::AttachmentImage::~AttachmentImage() = default;

angle::Result OffscreenSurfaceVk::AttachmentImage::initialize(DisplayVk *displayVk,
                                                              EGLint width,
                                                              EGLint height,
                                                              const vk::Format &vkFormat,
                                                              GLint samples,
                                                              bool isRobustResourceInitEnabled,
                                                              bool hasProtectedContent)
{
    ANGLE_TRY(InitImageHelper(displayVk, width, height, vkFormat, samples,
                              isRobustResourceInitEnabled, hasProtectedContent, &image));

    RendererVk *renderer        = displayVk->getRenderer();
    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (hasProtectedContent)
    {
        flags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;
    }
    ANGLE_TRY(
        image.initMemory(displayVk, hasProtectedContent, renderer->getMemoryProperties(), flags));

    imageViews.init(renderer);

    return angle::Result::Continue;
}

void OffscreenSurfaceVk::AttachmentImage::destroy(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    RendererVk *renderer = displayVk->getRenderer();
    // Front end must ensure all usage has been submitted.
    image.releaseImage(renderer);
    image.releaseStagingBuffer(renderer);
    imageViews.release(renderer);
}

OffscreenSurfaceVk::OffscreenSurfaceVk(const egl::SurfaceState &surfaceState, RendererVk *renderer)
    : SurfaceVk(surfaceState),
      mWidth(mState.attributes.getAsInt(EGL_WIDTH, 0)),
      mHeight(mState.attributes.getAsInt(EGL_HEIGHT, 0)),
      mColorAttachment(this),
      mDepthStencilAttachment(this)
{
    mColorRenderTarget.init(&mColorAttachment.image, &mColorAttachment.imageViews, nullptr, nullptr,
                            gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    mDepthStencilRenderTarget.init(&mDepthStencilAttachment.image,
                                   &mDepthStencilAttachment.imageViews, nullptr, nullptr,
                                   gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
}

OffscreenSurfaceVk::~OffscreenSurfaceVk() {}

egl::Error OffscreenSurfaceVk::initialize(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    angle::Result result = initializeImpl(displayVk);
    return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
}

angle::Result OffscreenSurfaceVk::initializeImpl(DisplayVk *displayVk)
{
    RendererVk *renderer      = displayVk->getRenderer();
    const egl::Config *config = mState.config;

    renderer->reloadVolkIfNeeded();

    GLint samples = GetSampleCount(mState.config);
    ANGLE_VK_CHECK(displayVk, samples > 0, VK_ERROR_INITIALIZATION_FAILED);

    bool robustInit = mState.isRobustResourceInitEnabled();

    if (config->renderTargetFormat != GL_NONE)
    {
        ANGLE_TRY(mColorAttachment.initialize(displayVk, mWidth, mHeight,
                                              renderer->getFormat(config->renderTargetFormat),
                                              samples, robustInit, mState.hasProtectedContent()));
        mColorRenderTarget.init(&mColorAttachment.image, &mColorAttachment.imageViews, nullptr,
                                nullptr, gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    }

    if (config->depthStencilFormat != GL_NONE)
    {
        ANGLE_TRY(mDepthStencilAttachment.initialize(
            displayVk, mWidth, mHeight, renderer->getFormat(config->depthStencilFormat), samples,
            robustInit, mState.hasProtectedContent()));
        mDepthStencilRenderTarget.init(&mDepthStencilAttachment.image,
                                       &mDepthStencilAttachment.imageViews, nullptr, nullptr,
                                       gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    }

    return angle::Result::Continue;
}

void OffscreenSurfaceVk::destroy(const egl::Display *display)
{
    mColorAttachment.destroy(display);
    mDepthStencilAttachment.destroy(display);
}

FramebufferImpl *OffscreenSurfaceVk::createDefaultFramebuffer(const gl::Context *context,
                                                              const gl::FramebufferState &state)
{
    RendererVk *renderer = vk::GetImpl(context)->getRenderer();

    // Use a user FBO for an offscreen RT.
    return FramebufferVk::CreateUserFBO(renderer, state);
}

egl::Error OffscreenSurfaceVk::swap(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error OffscreenSurfaceVk::postSubBuffer(const gl::Context * /*context*/,
                                             EGLint /*x*/,
                                             EGLint /*y*/,
                                             EGLint /*width*/,
                                             EGLint /*height*/)
{
    return egl::NoError();
}

egl::Error OffscreenSurfaceVk::querySurfacePointerANGLE(EGLint /*attribute*/, void ** /*value*/)
{
    UNREACHABLE();
    return egl::EglBadCurrentSurface();
}

egl::Error OffscreenSurfaceVk::bindTexImage(const gl::Context * /*context*/,
                                            gl::Texture * /*texture*/,
                                            EGLint /*buffer*/)
{
    return egl::NoError();
}

egl::Error OffscreenSurfaceVk::releaseTexImage(const gl::Context * /*context*/, EGLint /*buffer*/)
{
    return egl::NoError();
}

egl::Error OffscreenSurfaceVk::getSyncValues(EGLuint64KHR * /*ust*/,
                                             EGLuint64KHR * /*msc*/,
                                             EGLuint64KHR * /*sbc*/)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error OffscreenSurfaceVk::getMscRate(EGLint * /*numerator*/, EGLint * /*denominator*/)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

void OffscreenSurfaceVk::setSwapInterval(EGLint /*interval*/) {}

EGLint OffscreenSurfaceVk::getWidth() const
{
    return mWidth;
}

EGLint OffscreenSurfaceVk::getHeight() const
{
    return mHeight;
}

EGLint OffscreenSurfaceVk::isPostSubBufferSupported() const
{
    return EGL_FALSE;
}

EGLint OffscreenSurfaceVk::getSwapBehavior() const
{
    return EGL_BUFFER_DESTROYED;
}

angle::Result OffscreenSurfaceVk::initializeContents(const gl::Context *context,
                                                     const gl::ImageIndex &imageIndex)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (mColorAttachment.image.valid())
    {
        mColorAttachment.image.stageRobustResourceClear(imageIndex);
        ANGLE_TRY(mColorAttachment.image.flushAllStagedUpdates(contextVk));
    }

    if (mDepthStencilAttachment.image.valid())
    {
        mDepthStencilAttachment.image.stageRobustResourceClear(imageIndex);
        ANGLE_TRY(mDepthStencilAttachment.image.flushAllStagedUpdates(contextVk));
    }
    return angle::Result::Continue;
}

vk::ImageHelper *OffscreenSurfaceVk::getColorAttachmentImage()
{
    return &mColorAttachment.image;
}

namespace impl
{
SwapchainCleanupData::SwapchainCleanupData() = default;
SwapchainCleanupData::~SwapchainCleanupData()
{
    ASSERT(swapchain == VK_NULL_HANDLE);
    ASSERT(semaphores.empty());
}

SwapchainCleanupData::SwapchainCleanupData(SwapchainCleanupData &&other)
    : swapchain(other.swapchain), semaphores(std::move(other.semaphores))
{
    other.swapchain = VK_NULL_HANDLE;
}

void SwapchainCleanupData::destroy(VkDevice device, vk::Recycler<vk::Semaphore> *semaphoreRecycler)
{
    if (swapchain)
    {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    for (vk::Semaphore &semaphore : semaphores)
    {
        semaphoreRecycler->recycle(std::move(semaphore));
    }
    semaphores.clear();
}

ImagePresentHistory::ImagePresentHistory() = default;
ImagePresentHistory::~ImagePresentHistory()
{
    ASSERT(!semaphore.valid());
    ASSERT(oldSwapchains.empty());
}

ImagePresentHistory::ImagePresentHistory(ImagePresentHistory &&other)
    : semaphore(std::move(other.semaphore)), oldSwapchains(std::move(other.oldSwapchains))
{}

ImagePresentHistory &ImagePresentHistory::operator=(ImagePresentHistory &&other)
{
    std::swap(semaphore, other.semaphore);
    std::swap(oldSwapchains, other.oldSwapchains);
    return *this;
}

SwapchainImage::SwapchainImage()  = default;
SwapchainImage::~SwapchainImage() = default;

SwapchainImage::SwapchainImage(SwapchainImage &&other)
    : image(std::move(other.image)),
      imageViews(std::move(other.imageViews)),
      framebuffer(std::move(other.framebuffer)),
      presentHistory(std::move(other.presentHistory))
{}
}  // namespace impl

using namespace impl;

WindowSurfaceVk::WindowSurfaceVk(const egl::SurfaceState &surfaceState, EGLNativeWindowType window)
    : SurfaceVk(surfaceState),
      mNativeWindowType(window),
      mSurface(VK_NULL_HANDLE),
      mSupportsProtectedSwapchain(false),
      mSwapchain(VK_NULL_HANDLE),
      mSwapchainPresentMode(VK_PRESENT_MODE_FIFO_KHR),
      mDesiredSwapchainPresentMode(VK_PRESENT_MODE_FIFO_KHR),
      mMinImageCount(0),
      mPreTransform(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR),
      mEmulatedPreTransform(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR),
      mCompositeAlpha(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR),
      mCurrentSwapchainImageIndex(0),
      mAcquireImageSemaphore(nullptr),
      mDepthStencilImageBinding(this, kAnySurfaceImageSubjectIndex),
      mColorImageMSBinding(this, kAnySurfaceImageSubjectIndex),
      mNeedToAcquireNextSwapchainImage(false),
      mFrameCount(1)
{
    // Initialize the color render target with the multisampled targets.  If not multisampled, the
    // render target will be updated to refer to a swapchain image on every acquire.
    mColorRenderTarget.init(&mColorImageMS, &mColorImageMSViews, nullptr, nullptr,
                            gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    mDepthStencilRenderTarget.init(&mDepthStencilImage, &mDepthStencilImageViews, nullptr, nullptr,
                                   gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    mDepthStencilImageBinding.bind(&mDepthStencilImage);
    mColorImageMSBinding.bind(&mColorImageMS);
}

WindowSurfaceVk::~WindowSurfaceVk()
{
    ASSERT(mSurface == VK_NULL_HANDLE);
    ASSERT(mSwapchain == VK_NULL_HANDLE);
}

void WindowSurfaceVk::destroy(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    RendererVk *renderer = displayVk->getRenderer();
    VkDevice device      = renderer->getDevice();
    VkInstance instance  = renderer->getInstance();

    // flush the pipe.
    (void)renderer->finish(displayVk, mState.hasProtectedContent());

    destroySwapChainImages(displayVk);

    if (mSwapchain)
    {
        vkDestroySwapchainKHR(device, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    for (vk::Semaphore &semaphore : mAcquireImageSemaphores)
    {
        semaphore.destroy(device);
    }
    for (SwapchainCleanupData &oldSwapchain : mOldSwapchains)
    {
        oldSwapchain.destroy(device, &mPresentSemaphoreRecycler);
    }
    mOldSwapchains.clear();

    if (mSurface)
    {
        vkDestroySurfaceKHR(instance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    mPresentSemaphoreRecycler.destroy(device);
}

egl::Error WindowSurfaceVk::initialize(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    angle::Result result = initializeImpl(displayVk);
    if (result == angle::Result::Incomplete)
    {
        return angle::ToEGL(result, displayVk, EGL_BAD_MATCH);
    }
    else
    {
        return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
    }
}

angle::Result WindowSurfaceVk::initializeImpl(DisplayVk *displayVk)
{
    RendererVk *renderer = displayVk->getRenderer();

    mColorImageMSViews.init(renderer);
    mDepthStencilImageViews.init(renderer);

    renderer->reloadVolkIfNeeded();

    gl::Extents windowSize;
    ANGLE_TRY(createSurfaceVk(displayVk, &windowSize));

    uint32_t presentQueue = 0;
    ANGLE_TRY(renderer->selectPresentQueueForSurface(displayVk, mSurface, &presentQueue));
    ANGLE_UNUSED_VARIABLE(presentQueue);

    const VkPhysicalDevice &physicalDevice = renderer->getPhysicalDevice();

    if (renderer->getFeatures().supportsSurfaceCapabilities2Extension.enabled)
    {
        VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo2 = {};
        surfaceInfo2.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
        surfaceInfo2.surface = mSurface;

        VkSurfaceCapabilities2KHR surfaceCaps2 = {};
        surfaceCaps2.sType                     = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

        VkSharedPresentSurfaceCapabilitiesKHR sharedPresentSurfaceCaps = {};
        if (renderer->getFeatures().supportsSharedPresentableImageExtension.enabled)
        {
            sharedPresentSurfaceCaps.sType =
                VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR;
            sharedPresentSurfaceCaps.sharedPresentSupportedUsageFlags =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            vk::AddToPNextChain(&surfaceCaps2, &sharedPresentSurfaceCaps);
        }

        VkSurfaceProtectedCapabilitiesKHR surfaceProtectedCaps = {};
        if (renderer->getFeatures().supportsSurfaceProtectedCapabilitiesExtension.enabled)
        {
            surfaceProtectedCaps.sType = VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR;

            vk::AddToPNextChain(&surfaceCaps2, &surfaceProtectedCaps);
        }

        ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfaceCapabilities2KHR(
                                    physicalDevice, &surfaceInfo2, &surfaceCaps2));

        mSurfaceCaps                = surfaceCaps2.surfaceCapabilities;
        mSupportsProtectedSwapchain = surfaceProtectedCaps.supportsProtected;
    }
    else
    {
        ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface,
                                                                          &mSurfaceCaps));
    }

    if (IsAndroid())
    {
        mSupportsProtectedSwapchain = true;
    }

    ANGLE_VK_CHECK(displayVk, (mState.hasProtectedContent() ? mSupportsProtectedSwapchain : true),
                   VK_ERROR_FEATURE_NOT_PRESENT);

    // Adjust width and height to the swapchain if necessary.
    uint32_t width  = mSurfaceCaps.currentExtent.width;
    uint32_t height = mSurfaceCaps.currentExtent.height;

    // TODO(jmadill): Support devices which don't support copy. We use this for ReadPixels.
    ANGLE_VK_CHECK(displayVk,
                   (mSurfaceCaps.supportedUsageFlags & kSurfaceVkColorImageUsageFlags) ==
                       kSurfaceVkColorImageUsageFlags,
                   VK_ERROR_INITIALIZATION_FAILED);

    EGLAttrib attribWidth  = mState.attributes.get(EGL_WIDTH, 0);
    EGLAttrib attribHeight = mState.attributes.get(EGL_HEIGHT, 0);

    if (mSurfaceCaps.currentExtent.width == kSurfaceSizedBySwapchain)
    {
        ASSERT(mSurfaceCaps.currentExtent.height == kSurfaceSizedBySwapchain);

        width  = (attribWidth != 0) ? static_cast<uint32_t>(attribWidth) : windowSize.width;
        height = (attribHeight != 0) ? static_cast<uint32_t>(attribHeight) : windowSize.height;
    }

    gl::Extents extents(static_cast<int>(width), static_cast<int>(height), 1);

    // Introduction to Android rotation and pre-rotation:
    //
    // Android devices have one native orientation, but a window may be displayed in a different
    // orientation.  This results in the window being "rotated" relative to the native orientation.
    // For example, the native orientation of a Pixel 4 is portrait (i.e. height > width).
    // However, many games want to be landscape (i.e. width > height).  Some applications will
    // adapt to whatever orientation the user places the device in (e.g. auto-rotation).
    //
    // A convention is used within ANGLE of referring to the "rotated" and "non-rotated" aspects of
    // a topic (e.g. a window's extents, a scissor, a viewport):
    //
    // - Non-rotated.  This refers to the way that the application views the window.  Rotation is
    //   an Android concept, not a GL concept.  An application may view its window as landscape or
    //   portrait, but not necessarily view its window as being rotated.  For example, an
    //   application will set a scissor and viewport in a manner consistent with its view of the
    //   window size (i.e. a non-rotated manner).
    //
    // - Rotated.  This refers to the way that Vulkan views the window.  If the window's
    //   orientation is the same as the native orientation, the rotated view will happen to be
    //   equivalent to the non-rotated view, but regardless of the window's orientation, ANGLE uses
    //   the "rotated" term as whatever the Vulkan view of the window is.
    //
    // Most of ANGLE is designed to work with the non-rotated view of the window.  This is
    // certainly true of the ANGLE front-end.  It is also true of most of the Vulkan back-end,
    // which is still translating GL to Vulkan.  Only part of the Vulkan back-end needs to
    // communicate directly to Vulkan in terms of the window's rotation.  For example, the viewport
    // and scissor calculations are done with non-rotated values; and then the final values are
    // rotated.
    //
    // ANGLE learns about the window's rotation from mSurfaceCaps.currentTransform.  If
    // currentTransform is non-IDENTITY, ANGLE must "pre-rotate" various aspects of its work
    // (e.g. rotate vertices in the vertex shaders, change scissor, viewport, and render-pass
    // renderArea).  The swapchain's transform is given the value of mSurfaceCaps.currentTransform.
    // That prevents SurfaceFlinger from doing a rotation blit for every frame (which is costly in
    // terms of performance and power).
    //
    // When a window is rotated 90 or 270 degrees, the aspect ratio changes.  The width and height
    // are swapped.  The x/y and width/height of various values in ANGLE must also be swapped
    // before communicating the values to Vulkan.
    if (renderer->getFeatures().enablePreRotateSurfaces.enabled)
    {
        // Use the surface's transform.  For many platforms, this will always be identity (ANGLE
        // does not need to do any pre-rotation).  However, when mSurfaceCaps.currentTransform is
        // not identity, the device has been rotated away from its natural orientation.  In such a
        // case, ANGLE must rotate all rendering in order to avoid the compositor
        // (e.g. SurfaceFlinger on Android) performing an additional rotation blit.  In addition,
        // ANGLE must create the swapchain with VkSwapchainCreateInfoKHR::preTransform set to the
        // value of mSurfaceCaps.currentTransform.
        mPreTransform = mSurfaceCaps.currentTransform;
    }
    else
    {
        // Default to identity transform.
        mPreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

        if ((mSurfaceCaps.supportedTransforms & mPreTransform) == 0)
        {
            mPreTransform = mSurfaceCaps.currentTransform;
        }
    }

    // Set emulated pre-transform if any emulated prerotation features are set.
    if (renderer->getFeatures().emulatedPrerotation90.enabled)
    {
        mEmulatedPreTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    }
    else if (renderer->getFeatures().emulatedPrerotation180.enabled)
    {
        mEmulatedPreTransform = VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR;
    }
    else if (renderer->getFeatures().emulatedPrerotation270.enabled)
    {
        mEmulatedPreTransform = VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;
    }

    // If prerotation is emulated, the window is physically rotated.  With real prerotation, the
    // surface reports the rotated sizes.  With emulated prerotation however, the surface reports
    // the actual window sizes.  Adjust the window extents to match what real prerotation would have
    // reported.
    if (Is90DegreeRotation(mEmulatedPreTransform))
    {
        ASSERT(mPreTransform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
        std::swap(extents.width, extents.height);
    }

    uint32_t presentModeCount = 0;
    ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface,
                                                                      &presentModeCount, nullptr));
    ASSERT(presentModeCount > 0);

    mPresentModes.resize(presentModeCount);
    ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfacePresentModesKHR(
                                physicalDevice, mSurface, &presentModeCount, mPresentModes.data()));

    // Select appropriate present mode based on vsync parameter. Default to 1 (FIFO), though it
    // will get clamped to the min/max values specified at display creation time.
    EGLint preferredSwapInterval = mState.getPreferredSwapInterval();
    if (renderer->getFeatures().disableFifoPresentMode.enabled)
    {
        preferredSwapInterval = 0;
    }
    setSwapInterval(preferredSwapInterval);

    const vk::Format &format = renderer->getFormat(mState.config->renderTargetFormat);
    VkFormat nativeFormat    = format.getActualRenderableImageVkFormat();
    RendererVk *rendererVk   = displayVk->getRenderer();
    // For devices that don't support creating swapchain images with RGB8, emulate with RGBA8.
    if (rendererVk->getFeatures().overrideSurfaceFormatRGB8toRGBA8.enabled &&
        nativeFormat == VK_FORMAT_R8G8B8_UNORM)
    {
        nativeFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }

    bool surfaceFormatSupported = false;
    VkColorSpaceKHR colorSpace  = MapEglColorSpaceToVkColorSpace(
        static_cast<EGLenum>(mState.attributes.get(EGL_GL_COLORSPACE, EGL_NONE)));

    if (renderer->getFeatures().supportsSurfaceCapabilities2Extension.enabled)
    {

        // If a non-linear colorspace was requested but the non-linear colorspace is
        // not supported in combination with the vulkan surface format, treat it as a non-fatal
        // error
        ANGLE_TRY(DoesSurfaceSupportFormatAndColorspace(displayVk, physicalDevice, mSurface,
                                                        nativeFormat, colorSpace,
                                                        &surfaceFormatSupported));
    }
    else if (colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
        // VK_KHR_get_surface_capabilities2 is required to query support for colorspaces
        // from VK_EXT_swapchain_colorspace
    }
    else
    {
        // If a non-linear colorspace was requested but the non-linear format is
        // not supported as a vulkan surface format, treat it as a non-fatal error
        ANGLE_TRY(DoesSurfaceSupportFormat(displayVk, physicalDevice, mSurface, nativeFormat,
                                           &surfaceFormatSupported));
    }

    if (!surfaceFormatSupported)
    {
        return angle::Result::Incomplete;
    }

    mCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((mSurfaceCaps.supportedCompositeAlpha & mCompositeAlpha) == 0)
    {
        mCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    ANGLE_VK_CHECK(displayVk, (mSurfaceCaps.supportedCompositeAlpha & mCompositeAlpha) != 0,
                   VK_ERROR_INITIALIZATION_FAILED);

    ANGLE_TRY(createSwapChain(displayVk, extents, VK_NULL_HANDLE));

    // Create the semaphores that will be used for vkAcquireNextImageKHR.
    for (vk::Semaphore &semaphore : mAcquireImageSemaphores)
    {
        ANGLE_VK_TRY(displayVk, semaphore.init(displayVk->getDevice()));
    }

    VkResult vkResult = acquireNextSwapchainImage(displayVk);
    ASSERT(vkResult != VK_SUBOPTIMAL_KHR);
    ANGLE_VK_TRY(displayVk, vkResult);

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::getAttachmentRenderTarget(const gl::Context *context,
                                                         GLenum binding,
                                                         const gl::ImageIndex &imageIndex,
                                                         GLsizei samples,
                                                         FramebufferAttachmentRenderTarget **rtOut)
{
    if (mNeedToAcquireNextSwapchainImage)
    {
        // Acquire the next image (previously deferred) before it is drawn to or read from.
        ContextVk *contextVk = vk::GetImpl(context);
        ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, "First Swap Image Use");
        ANGLE_TRY(doDeferredAcquireNextImage(context, false));
    }
    return SurfaceVk::getAttachmentRenderTarget(context, binding, imageIndex, samples, rtOut);
}

angle::Result WindowSurfaceVk::recreateSwapchain(ContextVk *contextVk, const gl::Extents &extents)
{
    // If mOldSwapchains is not empty, it means that a new swapchain was created, but before
    // any of its images were presented, it's asked to be recreated.  In this case, we can destroy
    // the current swapchain immediately (although the old swapchains still need to be kept to be
    // scheduled for destruction).  This can happen for example if vkQueuePresentKHR returns
    // OUT_OF_DATE, the swapchain is recreated and the following vkAcquireNextImageKHR again
    // returns OUT_OF_DATE.
    //
    // Otherwise, keep the current swapchain as the old swapchain to be scheduled for destruction
    // and create a new one.

    VkSwapchainKHR swapchainToDestroy = VK_NULL_HANDLE;

    if (!mOldSwapchains.empty())
    {
        // Keep the old swapchain, destroy the current (never-used) swapchain.
        swapchainToDestroy = mSwapchain;

        // Recycle present semaphores.
        for (SwapchainImage &swapchainImage : mSwapchainImages)
        {
            for (ImagePresentHistory &presentHistory : swapchainImage.presentHistory)
            {
                ASSERT(presentHistory.semaphore.valid());
                ASSERT(presentHistory.oldSwapchains.empty());

                mPresentSemaphoreRecycler.recycle(std::move(presentHistory.semaphore));
            }
        }
    }
    else
    {
        SwapchainCleanupData cleanupData;

        // Remember the current swapchain to be scheduled for destruction later.
        cleanupData.swapchain = mSwapchain;

        // Accumulate the semaphores to be destroyed at the same time as the swapchain.
        for (SwapchainImage &swapchainImage : mSwapchainImages)
        {
            for (ImagePresentHistory &presentHistory : swapchainImage.presentHistory)
            {
                ASSERT(presentHistory.semaphore.valid());
                cleanupData.semaphores.emplace_back(std::move(presentHistory.semaphore));

                // Accumulate any previous swapchains that are pending destruction too.
                for (SwapchainCleanupData &oldSwapchain : presentHistory.oldSwapchains)
                {
                    mOldSwapchains.emplace_back(std::move(oldSwapchain));
                }
                presentHistory.oldSwapchains.clear();
            }
        }

        // If too many old swapchains have accumulated, wait idle and destroy them.  This is to
        // prevent failures due to too many swapchains allocated.
        //
        // Note: Nvidia has been observed to fail creation of swapchains after 20 are allocated on
        // desktop, or less than 10 on Quadro P400.
        static constexpr size_t kMaxOldSwapchains = 5;
        if (mOldSwapchains.size() > kMaxOldSwapchains)
        {
            ANGLE_TRY(
                contextVk->getRenderer()->finish(contextVk, contextVk->hasProtectedContent()));
            for (SwapchainCleanupData &oldSwapchain : mOldSwapchains)
            {
                oldSwapchain.destroy(contextVk->getDevice(), &mPresentSemaphoreRecycler);
            }
            mOldSwapchains.clear();
        }

        mOldSwapchains.emplace_back(std::move(cleanupData));
    }

    // Recreate the swapchain based on the most recent one.
    VkSwapchainKHR lastSwapchain = mSwapchain;
    mSwapchain                   = VK_NULL_HANDLE;

    releaseSwapchainImages(contextVk);

    // If prerotation is emulated, adjust the window extents to match what real prerotation would
    // have reported.
    gl::Extents swapchainExtents = extents;
    if (Is90DegreeRotation(mEmulatedPreTransform))
    {
        ASSERT(mPreTransform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
        std::swap(swapchainExtents.width, swapchainExtents.height);
    }

    angle::Result result = createSwapChain(contextVk, swapchainExtents, lastSwapchain);

    // Notify the parent classes of the surface's new state.
    onStateChange(angle::SubjectMessage::SurfaceChanged);

    // If the most recent swapchain was never used, destroy it right now.
    if (swapchainToDestroy)
    {
        vkDestroySwapchainKHR(contextVk->getDevice(), swapchainToDestroy, nullptr);
    }

    return result;
}

angle::Result WindowSurfaceVk::newPresentSemaphore(vk::Context *context,
                                                   vk::Semaphore *semaphoreOut)
{
    if (mPresentSemaphoreRecycler.empty())
    {
        ANGLE_VK_TRY(context, semaphoreOut->init(context->getDevice()));
    }
    else
    {
        mPresentSemaphoreRecycler.fetch(semaphoreOut);
    }
    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::resizeSwapchainImages(vk::Context *context, uint32_t imageCount)
{
    if (static_cast<size_t>(imageCount) != mSwapchainImages.size())
    {
        mSwapchainImageBindings.clear();
        mSwapchainImages.resize(imageCount);

        // Update the image bindings. Because the observer binding class uses raw pointers we
        // need to first ensure the entire image vector is fully allocated before binding the
        // subject and observer together.
        for (uint32_t index = 0; index < imageCount; ++index)
        {
            mSwapchainImageBindings.push_back(
                angle::ObserverBinding(this, kAnySurfaceImageSubjectIndex));
        }

        for (uint32_t index = 0; index < imageCount; ++index)
        {
            mSwapchainImageBindings[index].bind(&mSwapchainImages[index].image);
        }
    }

    // At this point, if there was a previous swapchain, the previous present semaphores have all
    // been moved to mOldSwapchains to be scheduled for destruction, so all semaphore handles in
    // mSwapchainImages should be invalid.
    for (SwapchainImage &swapchainImage : mSwapchainImages)
    {
        for (ImagePresentHistory &presentHistory : swapchainImage.presentHistory)
        {
            ASSERT(!presentHistory.semaphore.valid());
            ANGLE_TRY(newPresentSemaphore(context, &presentHistory.semaphore));
        }
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::createSwapChain(vk::Context *context,
                                               const gl::Extents &extents,
                                               VkSwapchainKHR lastSwapchain)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::createSwapchain");

    ASSERT(mSwapchain == VK_NULL_HANDLE);

    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    const vk::Format &format         = renderer->getFormat(mState.config->renderTargetFormat);
    angle::FormatID actualFormatID   = format.getActualRenderableImageFormatID();
    angle::FormatID intendedFormatID = format.getIntendedFormatID();

    // For devices that don't support creating swapchain images with RGB8, emulate with RGBA8.
    if (renderer->getFeatures().overrideSurfaceFormatRGB8toRGBA8.enabled &&
        intendedFormatID == angle::FormatID::R8G8B8_UNORM)
    {
        actualFormatID = angle::FormatID::R8G8B8A8_UNORM;
    }

    gl::Extents rotatedExtents = extents;
    if (Is90DegreeRotation(getPreTransform()))
    {
        // The Surface is oriented such that its aspect ratio no longer matches that of the
        // device.  In this case, the width and height of the swapchain images must be swapped to
        // match the device's native orientation.  This must also be done for other attachments
        // used with the swapchain (e.g. depth buffer).  The width and height of the viewport,
        // scissor, and render-pass render area must also be swapped.  Then, when ANGLE rotates
        // gl_Position in the vertex shader, the rendering will look the same as if no
        // pre-rotation had been done.
        std::swap(rotatedExtents.width, rotatedExtents.height);
    }

    // We need transfer src for reading back from the backbuffer.
    VkImageUsageFlags imageUsageFlags = kSurfaceVkColorImageUsageFlags;

    // We need storage image for compute writes (debug overlay output).
    if (kEnableOverlay)
    {
        VkFormatFeatureFlags featureBits = renderer->getImageFormatFeatureBits(
            format.getActualRenderableImageFormatID(), VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
        if ((featureBits & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0)
        {
            imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
    }

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.flags = mState.hasProtectedContent() ? VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR : 0;
    swapchainInfo.surface         = mSurface;
    swapchainInfo.minImageCount   = mMinImageCount;
    swapchainInfo.imageFormat     = vk::GetVkFormatFromFormatID(actualFormatID);
    swapchainInfo.imageColorSpace = MapEglColorSpaceToVkColorSpace(
        static_cast<EGLenum>(mState.attributes.get(EGL_GL_COLORSPACE, EGL_NONE)));
    // Note: Vulkan doesn't allow 0-width/height swapchains.
    swapchainInfo.imageExtent.width     = std::max(rotatedExtents.width, 1);
    swapchainInfo.imageExtent.height    = std::max(rotatedExtents.height, 1);
    swapchainInfo.imageArrayLayers      = 1;
    swapchainInfo.imageUsage            = imageUsageFlags;
    swapchainInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices   = nullptr;
    swapchainInfo.preTransform          = mPreTransform;
    swapchainInfo.compositeAlpha        = mCompositeAlpha;
    swapchainInfo.presentMode           = mDesiredSwapchainPresentMode;
    swapchainInfo.clipped               = VK_TRUE;
    swapchainInfo.oldSwapchain          = lastSwapchain;

    if (mDesiredSwapchainPresentMode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR)
    {
        swapchainInfo.minImageCount = 1;
    }

    // On Android, vkCreateSwapchainKHR destroys lastSwapchain, which is incorrect.  Wait idle in
    // that case as a workaround.
    if (lastSwapchain && renderer->getFeatures().waitIdleBeforeSwapchainRecreation.enabled)
    {
        ANGLE_TRY(renderer->finish(context, mState.hasProtectedContent()));
    }

    // TODO(syoussefi): Once EGL_SWAP_BEHAVIOR_PRESERVED_BIT is supported, the contents of the old
    // swapchain need to carry over to the new one.  http://anglebug.com/2942
    VkSwapchainKHR newSwapChain = VK_NULL_HANDLE;
    ANGLE_VK_TRY(context, vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &newSwapChain));
    mSwapchain            = newSwapChain;
    mSwapchainPresentMode = mDesiredSwapchainPresentMode;

    // Initialize the swapchain image views.
    uint32_t imageCount = 0;
    ANGLE_VK_TRY(context, vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, nullptr));

    std::vector<VkImage> swapchainImages(imageCount);
    ANGLE_VK_TRY(context,
                 vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, swapchainImages.data()));

    // If multisampling is enabled, create a multisampled image which gets resolved just prior to
    // present.
    GLint samples = GetSampleCount(mState.config);
    ANGLE_VK_CHECK(context, samples > 0, VK_ERROR_INITIALIZATION_FAILED);

    VkExtent3D vkExtents;
    gl_vk::GetExtent(rotatedExtents, &vkExtents);

    bool robustInit = mState.isRobustResourceInitEnabled();

    if (samples > 1)
    {
        const VkImageUsageFlags usage = kSurfaceVkColorImageUsageFlags;

        // Create a multisampled image that will be rendered to, and then resolved to a swapchain
        // image.  The actual VkImage is created with rotated coordinates to make it easier to do
        // the resolve.  The ImageHelper::mExtents will have non-rotated extents in order to fit
        // with the rest of ANGLE, (e.g. which calculates the Vulkan scissor with non-rotated
        // values and then rotates the final rectangle).
        ANGLE_TRY(mColorImageMS.initMSAASwapchain(
            context, gl::TextureType::_2D, vkExtents, Is90DegreeRotation(getPreTransform()), format,
            samples, usage, gl::LevelIndex(0), 1, 1, robustInit, mState.hasProtectedContent()));
        ANGLE_TRY(mColorImageMS.initMemory(context, mState.hasProtectedContent(),
                                           renderer->getMemoryProperties(),
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

        // Initialize the color render target with the multisampled targets.  If not multisampled,
        // the render target will be updated to refer to a swapchain image on every acquire.
        mColorRenderTarget.init(&mColorImageMS, &mColorImageMSViews, nullptr, nullptr,
                                gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    }

    ANGLE_TRY(resizeSwapchainImages(context, imageCount));

    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex)
    {
        SwapchainImage &member = mSwapchainImages[imageIndex];

        member.image.init2DWeakReference(context, swapchainImages[imageIndex], extents,
                                         Is90DegreeRotation(getPreTransform()), intendedFormatID,
                                         actualFormatID, 1, robustInit);
        member.imageViews.init(renderer);
        member.mFrameNumber = 0;
    }

    // Initialize depth/stencil if requested.
    if (mState.config->depthStencilFormat != GL_NONE)
    {
        const vk::Format &dsFormat = renderer->getFormat(mState.config->depthStencilFormat);

        const VkImageUsageFlags dsUsage = kSurfaceVkDepthStencilImageUsageFlags;

        ANGLE_TRY(mDepthStencilImage.init(context, gl::TextureType::_2D, vkExtents, dsFormat,
                                          samples, dsUsage, gl::LevelIndex(0), 1, 1, robustInit,
                                          mState.hasProtectedContent()));
        ANGLE_TRY(mDepthStencilImage.initMemory(context, mState.hasProtectedContent(),
                                                renderer->getMemoryProperties(),
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

        mDepthStencilRenderTarget.init(&mDepthStencilImage, &mDepthStencilImageViews, nullptr,
                                       nullptr, gl::LevelIndex(0), 0, 1,
                                       RenderTargetTransience::Default);

        // We will need to pass depth/stencil image views to the RenderTargetVk in the future.
    }

    return angle::Result::Continue;
}

bool WindowSurfaceVk::isMultiSampled() const
{
    return mColorImageMS.valid();
}

angle::Result WindowSurfaceVk::queryAndAdjustSurfaceCaps(ContextVk *contextVk,
                                                         VkSurfaceCapabilitiesKHR *surfaceCaps)
{
    const VkPhysicalDevice &physicalDevice = contextVk->getRenderer()->getPhysicalDevice();
    ANGLE_VK_TRY(contextVk,
                 vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, surfaceCaps));
    if (surfaceCaps->currentExtent.width == kSurfaceSizedBySwapchain)
    {
        ASSERT(surfaceCaps->currentExtent.height == kSurfaceSizedBySwapchain);
        ASSERT(!IsAndroid());

        // vkGetPhysicalDeviceSurfaceCapabilitiesKHR does not provide useful extents for some
        // platforms (e.g. Fuschia).  Therefore, we must query the window size via a
        // platform-specific mechanism.  Add those extents to the surfaceCaps
        gl::Extents currentExtents;
        ANGLE_TRY(getCurrentWindowSize(contextVk, &currentExtents));
        surfaceCaps->currentExtent.width  = currentExtents.width;
        surfaceCaps->currentExtent.height = currentExtents.height;
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::checkForOutOfDateSwapchain(ContextVk *contextVk,
                                                          bool presentOutOfDate)
{
    bool swapIntervalChanged = mSwapchainPresentMode != mDesiredSwapchainPresentMode;
    presentOutOfDate         = presentOutOfDate || swapIntervalChanged;

    // If there's no change, early out.
    if (!contextVk->getRenderer()->getFeatures().perFrameWindowSizeQuery.enabled &&
        !presentOutOfDate)
    {
        return angle::Result::Continue;
    }

    // Get the latest surface capabilities.
    ANGLE_TRY(queryAndAdjustSurfaceCaps(contextVk, &mSurfaceCaps));

    if (contextVk->getRenderer()->getFeatures().perFrameWindowSizeQuery.enabled &&
        !presentOutOfDate)
    {
        // This device generates neither VK_ERROR_OUT_OF_DATE_KHR nor VK_SUBOPTIMAL_KHR.  Check for
        // whether the size and/or rotation have changed since the swapchain was created.
        uint32_t swapchainWidth  = getWidth();
        uint32_t swapchainHeight = getHeight();
        presentOutOfDate         = mSurfaceCaps.currentTransform != mPreTransform ||
                           mSurfaceCaps.currentExtent.width != swapchainWidth ||
                           mSurfaceCaps.currentExtent.height != swapchainHeight;
    }

    // If anything has changed, recreate the swapchain.
    if (!presentOutOfDate)
    {
        return angle::Result::Continue;
    }

    gl::Extents newSwapchainExtents(mSurfaceCaps.currentExtent.width,
                                    mSurfaceCaps.currentExtent.height, 1);

    if (contextVk->getFeatures().enablePreRotateSurfaces.enabled)
    {
        // Update the surface's transform, which can change even if the window size does not.
        mPreTransform = mSurfaceCaps.currentTransform;
    }

    return recreateSwapchain(contextVk, newSwapchainExtents);
}

void WindowSurfaceVk::releaseSwapchainImages(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    if (mDepthStencilImage.valid())
    {
        mDepthStencilImage.releaseImageFromShareContexts(renderer, contextVk);
        mDepthStencilImage.releaseStagingBuffer(renderer);
        mDepthStencilImageViews.release(renderer);
    }

    if (mColorImageMS.valid())
    {
        mColorImageMS.releaseImageFromShareContexts(renderer, contextVk);
        mColorImageMS.releaseStagingBuffer(renderer);
        mColorImageMSViews.release(renderer);
        contextVk->addGarbage(&mFramebufferMS);
    }

    mSwapchainImageBindings.clear();

    for (SwapchainImage &swapchainImage : mSwapchainImages)
    {
        // We don't own the swapchain image handles, so we just remove our reference to it.
        swapchainImage.image.resetImageWeakReference();
        swapchainImage.image.destroy(renderer);

        swapchainImage.imageViews.release(renderer);
        contextVk->addGarbage(&swapchainImage.framebuffer);

        // present history must have already been taken care of.
        for (ImagePresentHistory &presentHistory : swapchainImage.presentHistory)
        {
            ASSERT(!presentHistory.semaphore.valid());
            ASSERT(presentHistory.oldSwapchains.empty());
        }
    }

    mSwapchainImages.clear();
}

void WindowSurfaceVk::destroySwapChainImages(DisplayVk *displayVk)
{
    RendererVk *renderer = displayVk->getRenderer();
    VkDevice device      = displayVk->getDevice();

    mDepthStencilImage.destroy(renderer);
    mDepthStencilImageViews.destroy(device);
    mColorImageMS.destroy(renderer);
    mColorImageMSViews.destroy(device);
    mFramebufferMS.destroy(device);

    for (SwapchainImage &swapchainImage : mSwapchainImages)
    {
        // We don't own the swapchain image handles, so we just remove our reference to it.
        swapchainImage.image.resetImageWeakReference();
        swapchainImage.image.destroy(renderer);
        swapchainImage.imageViews.destroy(device);
        swapchainImage.framebuffer.destroy(device);

        for (ImagePresentHistory &presentHistory : swapchainImage.presentHistory)
        {
            ASSERT(presentHistory.semaphore.valid());

            mPresentSemaphoreRecycler.recycle(std::move(presentHistory.semaphore));
            for (SwapchainCleanupData &oldSwapchain : presentHistory.oldSwapchains)
            {
                oldSwapchain.destroy(device, &mPresentSemaphoreRecycler);
            }
            presentHistory.oldSwapchains.clear();
        }
    }

    mSwapchainImages.clear();
}

FramebufferImpl *WindowSurfaceVk::createDefaultFramebuffer(const gl::Context *context,
                                                           const gl::FramebufferState &state)
{
    RendererVk *renderer = vk::GetImpl(context)->getRenderer();
    return FramebufferVk::CreateDefaultFBO(renderer, state, this);
}

egl::Error WindowSurfaceVk::swapWithDamage(const gl::Context *context,
                                           const EGLint *rects,
                                           EGLint n_rects)
{
    DisplayVk *displayVk = vk::GetImpl(context->getDisplay());
    angle::Result result;
    result = swapImpl(context, rects, n_rects, nullptr);
    return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
}

egl::Error WindowSurfaceVk::swap(const gl::Context *context)
{
    DisplayVk *displayVk = vk::GetImpl(context->getDisplay());
    angle::Result result = swapImpl(context, nullptr, 0, nullptr);
    return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
}

angle::Result WindowSurfaceVk::computePresentOutOfDate(vk::Context *context,
                                                       VkResult result,
                                                       bool *presentOutOfDate)
{
    // If OUT_OF_DATE is returned, it's ok, we just need to recreate the swapchain before
    // continuing.
    // If VK_SUBOPTIMAL_KHR is returned it's because the device orientation changed and we should
    // recreate the swapchain with a new window orientation.
    if (context->getRenderer()->getFeatures().enablePreRotateSurfaces.enabled)
    {
        // Also check for VK_SUBOPTIMAL_KHR.
        *presentOutOfDate = ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR));
        if (!*presentOutOfDate)
        {
            ANGLE_VK_TRY(context, result);
        }
    }
    else
    {
        // We aren't quite ready for that so just ignore for now.
        *presentOutOfDate = result == VK_ERROR_OUT_OF_DATE_KHR;
        if (!*presentOutOfDate && result != VK_SUBOPTIMAL_KHR)
        {
            ANGLE_VK_TRY(context, result);
        }
    }
    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::present(ContextVk *contextVk,
                                       const EGLint *rects,
                                       EGLint n_rects,
                                       const void *pNextChain,
                                       bool *presentOutOfDate)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::present");
    RendererVk *renderer = contextVk->getRenderer();

    // Throttle the submissions to avoid getting too far ahead of the GPU.
    Serial *swapSerial = &mSwapHistory.front();
    mSwapHistory.next();

    {
        ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::present: Throttle CPU");
        ANGLE_TRY(renderer->finishToSerial(contextVk, *swapSerial));
    }

    SwapchainImage &image               = mSwapchainImages[mCurrentSwapchainImageIndex];
    vk::Framebuffer &currentFramebuffer = mSwapchainImages[mCurrentSwapchainImageIndex].framebuffer;
    updateOverlay(contextVk);
    bool overlayHasWidget = overlayHasEnabledWidget(contextVk);

    // Make sure deferred clears are applied, if any.
    ANGLE_TRY(
        image.image.flushStagedUpdates(contextVk, gl::LevelIndex(0), gl::LevelIndex(1), 0, 1, {}));

    // We can only do present related optimization if this is the last renderpass that touches the
    // swapchain image. MSAA resolve and overlay will insert another renderpass which disqualifies
    // the optimization.
    if (!mColorImageMS.valid() && !overlayHasWidget && currentFramebuffer.valid())
    {
        contextVk->optimizeRenderPassForPresent(currentFramebuffer.getHandle());
    }

    // Because the color attachment defers layout changes until endRenderPass time, we must call
    // finalize the layout transition in the renderpass before we insert layout change to
    // ImageLayout::Present bellow.
    contextVk->finalizeImageLayout(&image.image);

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBuffer({}, &commandBuffer));

    if (mColorImageMS.valid())
    {
        // Transition the multisampled image to TRANSFER_SRC for resolve.
        vk::CommandBufferAccess access;
        access.onImageTransferRead(VK_IMAGE_ASPECT_COLOR_BIT, &mColorImageMS);
        access.onImageTransferWrite(gl::LevelIndex(0), 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                    &image.image);

        ANGLE_TRY(contextVk->getOutsideRenderPassCommandBuffer(access, &commandBuffer));

        VkImageResolve resolveRegion                = {};
        resolveRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        resolveRegion.srcSubresource.mipLevel       = 0;
        resolveRegion.srcSubresource.baseArrayLayer = 0;
        resolveRegion.srcSubresource.layerCount     = 1;
        resolveRegion.srcOffset                     = {};
        resolveRegion.dstSubresource                = resolveRegion.srcSubresource;
        resolveRegion.dstOffset                     = {};
        resolveRegion.extent                        = image.image.getRotatedExtents();

        mColorImageMS.resolve(&image.image, resolveRegion, commandBuffer);
    }

    if (overlayHasWidget)
    {
        ANGLE_TRY(drawOverlay(contextVk, &image));
        ANGLE_TRY(contextVk->getOutsideRenderPassCommandBuffer({}, &commandBuffer));
    }

    // This does nothing if it's already in the requested layout
    image.image.recordReadBarrier(contextVk, VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::Present,
                                  commandBuffer);

    // Knowing that the kSwapHistorySize'th submission ago has finished, we can know that the
    // (kSwapHistorySize+1)'th present ago of this image is definitely finished and so its wait
    // semaphore can be reused.  See doc/PresentSemaphores.md for details.
    //
    // This also means the swapchain(s) scheduled to be deleted at the same time can be deleted.
    ImagePresentHistory &presentHistory = image.presentHistory.front();
    image.presentHistory.next();

    vk::Semaphore *presentSemaphore = &presentHistory.semaphore;
    ASSERT(presentSemaphore->valid());

    for (SwapchainCleanupData &oldSwapchain : presentHistory.oldSwapchains)
    {
        oldSwapchain.destroy(contextVk->getDevice(), &mPresentSemaphoreRecycler);
    }
    presentHistory.oldSwapchains.clear();

    // Schedule pending old swapchains to be destroyed at the same time the semaphore for this
    // present can be destroyed.
    presentHistory.oldSwapchains = std::move(mOldSwapchains);

    ANGLE_TRY(contextVk->flushAndGetSerial(presentSemaphore, swapSerial,
                                           RenderPassClosureReason::EGLSwapBuffers));

    VkPresentInfoKHR presentInfo   = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext              = pNextChain;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = presentSemaphore->ptr();
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &mSwapchain;
    presentInfo.pImageIndices      = &mCurrentSwapchainImageIndex;
    presentInfo.pResults           = nullptr;

    VkPresentRegionKHR presentRegion   = {};
    VkPresentRegionsKHR presentRegions = {};
    std::vector<VkRectLayerKHR> vkRects;
    if (contextVk->getFeatures().supportsIncrementalPresent.enabled && (n_rects > 0))
    {
        EGLint width  = getWidth();
        EGLint height = getHeight();

        const EGLint *eglRects       = rects;
        presentRegion.rectangleCount = n_rects;
        vkRects.resize(n_rects);
        for (EGLint i = 0; i < n_rects; i++)
        {
            VkRectLayerKHR &rect = vkRects[i];

            // Make sure the damage rects are within swapchain bounds.
            rect.offset.x      = gl::clamp(*eglRects++, 0, width);
            rect.offset.y      = gl::clamp(*eglRects++, 0, height);
            rect.extent.width  = gl::clamp(*eglRects++, 0, width - rect.offset.x);
            rect.extent.height = gl::clamp(*eglRects++, 0, height - rect.offset.y);
            rect.layer         = 0;
            // No rotation is done to these damage rectangles per the Vulkan spec.
        }
        presentRegion.pRectangles = vkRects.data();

        presentRegions.sType          = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR;
        presentRegions.pNext          = presentInfo.pNext;
        presentRegions.swapchainCount = 1;
        presentRegions.pRegions       = &presentRegion;

        presentInfo.pNext = &presentRegions;
    }

    ASSERT(mAcquireImageSemaphore == nullptr);

    VkResult result = renderer->queuePresent(contextVk, contextVk->getPriority(), presentInfo);

    // Set FrameNumber for the presented image.
    mSwapchainImages[mCurrentSwapchainImageIndex].mFrameNumber = mFrameCount++;

    ANGLE_TRY(computePresentOutOfDate(contextVk, result, presentOutOfDate));

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::swapImpl(const gl::Context *context,
                                        const EGLint *rects,
                                        EGLint n_rects,
                                        const void *pNextChain)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::swapImpl");

    ContextVk *contextVk = vk::GetImpl(context);

    if (mNeedToAcquireNextSwapchainImage)
    {
        // Acquire the next image (previously deferred). The image may not have been already
        // acquired if there was no rendering done at all to the default framebuffer in this frame,
        // for example if all rendering was done to FBOs.
        ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, "Acquire Swap Image Before Swap");
        ANGLE_TRY(doDeferredAcquireNextImage(context, false));
    }

    bool presentOutOfDate = false;
    ANGLE_TRY(present(contextVk, rects, n_rects, pNextChain, &presentOutOfDate));

    if (!presentOutOfDate)
    {
        // Defer acquiring the next swapchain image since the swapchain is not out-of-date.
        deferAcquireNextImage(context);
    }
    else
    {
        // Immediately try to acquire the next image, which will recognize the out-of-date
        // swapchain (potentially because of a rotation change), and recreate it.
        ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, "Out-of-Date Swapbuffer");
        ANGLE_TRY(doDeferredAcquireNextImage(context, presentOutOfDate));
    }

    return angle::Result::Continue;
}

void WindowSurfaceVk::deferAcquireNextImage(const gl::Context *context)
{
    mNeedToAcquireNextSwapchainImage = true;

    // Set gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0 via subject-observer message-passing
    // to the front-end Surface, Framebuffer, and Context classes.  The DIRTY_BIT_COLOR_ATTACHMENT_0
    // is processed before all other dirty bits.  However, since the attachments of the default
    // framebuffer cannot change, this bit will be processed before all others.  It will cause
    // WindowSurfaceVk::getAttachmentRenderTarget() to be called (which will acquire the next image)
    // before any RenderTargetVk accesses.  The processing of other dirty bits as well as other
    // setup for draws and reads will then access a properly-updated RenderTargetVk.
    onStateChange(angle::SubjectMessage::SubjectChanged);
}

angle::Result WindowSurfaceVk::doDeferredAcquireNextImage(const gl::Context *context,
                                                          bool presentOutOfDate)
{
    ContextVk *contextVk = vk::GetImpl(context);
    DisplayVk *displayVk = vk::GetImpl(context->getDisplay());

    // TODO(jmadill): Expose in CommandQueueInterface, or manage in CommandQueue. b/172704839
    if (contextVk->getRenderer()->isAsyncCommandQueueEnabled())
    {
        VkResult result = contextVk->getRenderer()->getLastPresentResult(mSwapchain);

        // Now that we have the result from the last present need to determine if it's out of date
        // or not.
        ANGLE_TRY(computePresentOutOfDate(contextVk, result, &presentOutOfDate));
    }

    ANGLE_TRY(checkForOutOfDateSwapchain(contextVk, presentOutOfDate));

    {
        // Note: TRACE_EVENT0 is put here instead of inside the function to workaround this issue:
        // http://anglebug.com/2927
        ANGLE_TRACE_EVENT0("gpu.angle", "acquireNextSwapchainImage");
        // Get the next available swapchain image.

        VkResult result = acquireNextSwapchainImage(contextVk);

        ASSERT(result != VK_SUBOPTIMAL_KHR);
        // If OUT_OF_DATE is returned, it's ok, we just need to recreate the swapchain before
        // continuing.
        if (ANGLE_UNLIKELY(result == VK_ERROR_OUT_OF_DATE_KHR))
        {
            ANGLE_TRY(checkForOutOfDateSwapchain(contextVk, true));
            // Try one more time and bail if we fail
            result = acquireNextSwapchainImage(contextVk);
        }
        ANGLE_VK_TRY(contextVk, result);
    }

    // Invalidate the color image if the swap behavior is EGL_BUFFER_DESTROYED.
    // Also invalidate the multi-sample color image.
    // In swapImpl(), optimizeRenderPassForPresent() does this for depth/stencil content.
    if (mState.swapBehavior == EGL_BUFFER_DESTROYED)
    {
        mSwapchainImages[mCurrentSwapchainImageIndex].image.invalidateSubresourceContent(
            contextVk, gl::LevelIndex(0), 0, 1);
    }
    mColorImageMS.invalidateSubresourceContent(contextVk, gl::LevelIndex(0), 0, 1);

    RendererVk *renderer = contextVk->getRenderer();
    ANGLE_TRY(renderer->syncPipelineCacheVk(displayVk, context));

    return angle::Result::Continue;
}

// This method will either return VK_SUCCESS or VK_ERROR_*.  Thus, it is appropriate to ASSERT that
// the return value won't be VK_SUBOPTIMAL_KHR.
VkResult WindowSurfaceVk::acquireNextSwapchainImage(vk::Context *context)
{
    VkDevice device = context->getDevice();

    if (mSwapchainPresentMode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR)
    {
        ASSERT(mSwapchainImages.size());
        SwapchainImage &image = mSwapchainImages[0];
        if (image.image.valid() &&
            (image.image.getCurrentImageLayout() == vk::ImageLayout::SharedPresent))
        {  // This will check for OUT_OF_DATE when in single image mode. and prevent
           // re-AcquireNextImage.
            return vkGetSwapchainStatusKHR(device, mSwapchain);
        }
    }

    const vk::Semaphore *acquireImageSemaphore = &mAcquireImageSemaphores.front();

    VkResult result =
        vkAcquireNextImageKHR(device, mSwapchain, UINT64_MAX, acquireImageSemaphore->getHandle(),
                              VK_NULL_HANDLE, &mCurrentSwapchainImageIndex);

    acquireImageSemaphore->valid();

    // VK_SUBOPTIMAL_KHR is ok since we still have an Image that can be presented successfully
    if (ANGLE_UNLIKELY(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR))
    {
        return result;
    }

    SwapchainImage &image = mSwapchainImages[mCurrentSwapchainImageIndex];

    // Single Image Mode
    if ((mSwapchainPresentMode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR) &&
        (image.image.getCurrentImageLayout() != vk::ImageLayout::SharedPresent))
    {
        rx::RendererVk *rendererVk = context->getRenderer();
        rx::vk::PrimaryCommandBuffer primaryCommandBuffer;
        if (rendererVk->getCommandBufferOneOff(context, mState.hasProtectedContent(),
                                               &primaryCommandBuffer) == angle::Result::Continue)
        {
            // Note return errors is early exit may leave new Image and Swapchain in unknown state.
            image.image.recordWriteBarrierOneOff(context, vk::ImageLayout::SharedPresent,
                                                 &primaryCommandBuffer);
            if (primaryCommandBuffer.end() != VK_SUCCESS)
            {
                mDesiredSwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
                return VK_ERROR_OUT_OF_DATE_KHR;
            }
            Serial serial;
            if (rendererVk->queueSubmitOneOff(
                    context, std::move(primaryCommandBuffer), false, egl::ContextPriority::Medium,
                    acquireImageSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, nullptr,
                    vk::SubmitPolicy::EnsureSubmitted, &serial) != angle::Result::Continue)
            {
                mDesiredSwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
                return VK_ERROR_OUT_OF_DATE_KHR;
            }
            acquireImageSemaphore = nullptr;
        }
    }

    // The semaphore will be waited on in the next flush.
    mAcquireImageSemaphores.next();
    mAcquireImageSemaphore = acquireImageSemaphore;

    // Update RenderTarget pointers to this swapchain image if not multisampling.  Note: a possible
    // optimization is to defer the |vkAcquireNextImageKHR| call itself to |present()| if
    // multisampling, as the swapchain image is essentially unused until then.
    if (!mColorImageMS.valid())
    {
        mColorRenderTarget.updateSwapchainImage(&image.image, &image.imageViews, nullptr, nullptr);
    }

    // Notify the owning framebuffer there may be staged updates.
    if (image.image.hasStagedUpdatesInAllocatedLevels())
    {
        onStateChange(angle::SubjectMessage::SubjectChanged);
    }

    // Note that an acquire is no longer needed.
    mNeedToAcquireNextSwapchainImage = false;

    return VK_SUCCESS;
}

egl::Error WindowSurfaceVk::postSubBuffer(const gl::Context *context,
                                          EGLint x,
                                          EGLint y,
                                          EGLint width,
                                          EGLint height)
{
    // TODO(jmadill)
    return egl::NoError();
}

egl::Error WindowSurfaceVk::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNREACHABLE();
    return egl::EglBadCurrentSurface();
}

egl::Error WindowSurfaceVk::bindTexImage(const gl::Context *context,
                                         gl::Texture *texture,
                                         EGLint buffer)
{
    return egl::NoError();
}

egl::Error WindowSurfaceVk::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    return egl::NoError();
}

egl::Error WindowSurfaceVk::getSyncValues(EGLuint64KHR * /*ust*/,
                                          EGLuint64KHR * /*msc*/,
                                          EGLuint64KHR * /*sbc*/)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error WindowSurfaceVk::getMscRate(EGLint * /*numerator*/, EGLint * /*denominator*/)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

void WindowSurfaceVk::setSwapInterval(EGLint interval)
{
    const EGLint minSwapInterval = mState.config->minSwapInterval;
    const EGLint maxSwapInterval = mState.config->maxSwapInterval;
    ASSERT(minSwapInterval == 0 || minSwapInterval == 1);
    ASSERT(maxSwapInterval == 0 || maxSwapInterval == 1);

    interval = gl::clamp(interval, minSwapInterval, maxSwapInterval);

    mDesiredSwapchainPresentMode = GetDesiredPresentMode(mPresentModes, interval);

    // - On mailbox, we need at least three images; one is being displayed to the user until the
    //   next v-sync, and the application alternatingly renders to the other two, one being
    //   recorded, and the other queued for presentation if v-sync happens in the meantime.
    // - On immediate, we need at least two images; the application alternates between the two
    //   images.
    // - On fifo, we use at least three images.  Triple-buffering allows us to present an image,
    //   have one in the queue, and record in another.  Note: on certain configurations (windows +
    //   nvidia + windowed mode), we could get away with a smaller number.
    //
    // For simplicity, we always allocate at least three images.
    mMinImageCount = std::max(3u, mSurfaceCaps.minImageCount);

    // Make sure we don't exceed maxImageCount.
    if (mSurfaceCaps.maxImageCount > 0 && mMinImageCount > mSurfaceCaps.maxImageCount)
    {
        mMinImageCount = mSurfaceCaps.maxImageCount;
    }

    // On the next swap, if the desired present mode is different from the current one, the
    // swapchain will be recreated.
}

EGLint WindowSurfaceVk::getWidth() const
{
    return static_cast<EGLint>(mColorRenderTarget.getExtents().width);
}

EGLint WindowSurfaceVk::getRotatedWidth() const
{
    return static_cast<EGLint>(mColorRenderTarget.getRotatedExtents().width);
}

EGLint WindowSurfaceVk::getHeight() const
{
    return static_cast<EGLint>(mColorRenderTarget.getExtents().height);
}

EGLint WindowSurfaceVk::getRotatedHeight() const
{
    return static_cast<EGLint>(mColorRenderTarget.getRotatedExtents().height);
}

egl::Error WindowSurfaceVk::getUserWidth(const egl::Display *display, EGLint *value) const
{
    DisplayVk *displayVk = vk::GetImpl(display);

    if (mSurfaceCaps.currentExtent.width == kSurfaceSizedBySwapchain)
    {
        // Surface has no intrinsic size; use current size.
        *value = getWidth();
        return egl::NoError();
    }

    VkSurfaceCapabilitiesKHR surfaceCaps;
    angle::Result result = getUserExtentsImpl(displayVk, &surfaceCaps);
    if (result == angle::Result::Continue)
    {
        // The EGL spec states that value is not written if there is an error
        ASSERT(surfaceCaps.currentExtent.width != kSurfaceSizedBySwapchain);
        *value = static_cast<EGLint>(surfaceCaps.currentExtent.width);
    }
    return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
}

egl::Error WindowSurfaceVk::getUserHeight(const egl::Display *display, EGLint *value) const
{
    DisplayVk *displayVk = vk::GetImpl(display);

    if (mSurfaceCaps.currentExtent.height == kSurfaceSizedBySwapchain)
    {
        // Surface has no intrinsic size; use current size.
        *value = getHeight();
        return egl::NoError();
    }

    VkSurfaceCapabilitiesKHR surfaceCaps;
    angle::Result result = getUserExtentsImpl(displayVk, &surfaceCaps);
    if (result == angle::Result::Continue)
    {
        // The EGL spec states that value is not written if there is an error
        ASSERT(surfaceCaps.currentExtent.height != kSurfaceSizedBySwapchain);
        *value = static_cast<EGLint>(surfaceCaps.currentExtent.height);
    }
    return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
}

angle::Result WindowSurfaceVk::getUserExtentsImpl(DisplayVk *displayVk,
                                                  VkSurfaceCapabilitiesKHR *surfaceCaps) const
{
    const VkPhysicalDevice &physicalDevice = displayVk->getRenderer()->getPhysicalDevice();

    ANGLE_VK_TRY(displayVk,
                 vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, surfaceCaps));

    // With real prerotation, the surface reports the rotated sizes.  With emulated prerotation,
    // adjust the window extents to match what real pre-rotation would have reported.
    if (Is90DegreeRotation(mEmulatedPreTransform))
    {
        std::swap(surfaceCaps->currentExtent.width, surfaceCaps->currentExtent.height);
    }

    return angle::Result::Continue;
}

EGLint WindowSurfaceVk::isPostSubBufferSupported() const
{
    // TODO(jmadill)
    return EGL_FALSE;
}

EGLint WindowSurfaceVk::getSwapBehavior() const
{
    // TODO(jmadill)
    return EGL_BUFFER_DESTROYED;
}

angle::Result WindowSurfaceVk::getCurrentFramebuffer(ContextVk *contextVk,
                                                     const vk::RenderPass &compatibleRenderPass,
                                                     vk::Framebuffer **framebufferOut)
{
    // FramebufferVk dirty-bit processing should ensure that a new image was acquired.
    ASSERT(!mNeedToAcquireNextSwapchainImage);

    vk::Framebuffer &currentFramebuffer =
        isMultiSampled() ? mFramebufferMS
                         : mSwapchainImages[mCurrentSwapchainImageIndex].framebuffer;

    if (currentFramebuffer.valid())
    {
        // Validation layers should detect if the render pass is really compatible.
        *framebufferOut = &currentFramebuffer;
        return angle::Result::Continue;
    }

    VkFramebufferCreateInfo framebufferInfo = {};

    const gl::Extents rotatedExtents      = mColorRenderTarget.getRotatedExtents();
    std::array<VkImageView, 2> imageViews = {};

    if (mDepthStencilImage.valid())
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(mDepthStencilRenderTarget.getImageView(contextVk, &imageView));
        imageViews[1] = imageView->getHandle();
    }

    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.flags           = 0;
    framebufferInfo.renderPass      = compatibleRenderPass.getHandle();
    framebufferInfo.attachmentCount = (mDepthStencilImage.valid() ? 2u : 1u);
    framebufferInfo.pAttachments    = imageViews.data();
    framebufferInfo.width           = static_cast<uint32_t>(rotatedExtents.width);
    framebufferInfo.height          = static_cast<uint32_t>(rotatedExtents.height);
    framebufferInfo.layers          = 1;

    if (isMultiSampled())
    {
        // If multisampled, there is only a single color image and framebuffer.
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(mColorRenderTarget.getImageView(contextVk, &imageView));
        imageViews[0] = imageView->getHandle();
        ANGLE_VK_TRY(contextVk, mFramebufferMS.init(contextVk->getDevice(), framebufferInfo));
    }
    else
    {
        for (SwapchainImage &swapchainImage : mSwapchainImages)
        {
            const vk::ImageView *imageView = nullptr;
            ANGLE_TRY(swapchainImage.imageViews.getLevelLayerDrawImageView(
                contextVk, swapchainImage.image, vk::LevelIndex(0), 0,
                gl::SrgbWriteControlMode::Default, &imageView));

            imageViews[0] = imageView->getHandle();
            ANGLE_VK_TRY(contextVk,
                         swapchainImage.framebuffer.init(contextVk->getDevice(), framebufferInfo));
        }
    }

    ASSERT(currentFramebuffer.valid());
    *framebufferOut = &currentFramebuffer;
    return angle::Result::Continue;
}

const vk::Semaphore *WindowSurfaceVk::getAndResetAcquireImageSemaphore()
{
    const vk::Semaphore *acquireSemaphore = mAcquireImageSemaphore;
    mAcquireImageSemaphore                = nullptr;
    return acquireSemaphore;
}

angle::Result WindowSurfaceVk::initializeContents(const gl::Context *context,
                                                  const gl::ImageIndex &imageIndex)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (mNeedToAcquireNextSwapchainImage)
    {
        // Acquire the next image (previously deferred).  Some tests (e.g.
        // GenerateMipmapWithRedefineBenchmark.Run/vulkan_webgl) cause this path to be taken,
        // because of dirty-object processing.
        ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, "Initialize Swap Image");
        ANGLE_TRY(doDeferredAcquireNextImage(context, false));
    }

    ASSERT(mSwapchainImages.size() > 0);
    ASSERT(mCurrentSwapchainImageIndex < mSwapchainImages.size());

    vk::ImageHelper *image =
        isMultiSampled() ? &mColorImageMS : &mSwapchainImages[mCurrentSwapchainImageIndex].image;
    image->stageRobustResourceClear(imageIndex);
    ANGLE_TRY(image->flushAllStagedUpdates(contextVk));

    if (mDepthStencilImage.valid())
    {
        mDepthStencilImage.stageRobustResourceClear(gl::ImageIndex::Make2D(0));
        ANGLE_TRY(mDepthStencilImage.flushAllStagedUpdates(contextVk));
    }

    return angle::Result::Continue;
}

void WindowSurfaceVk::updateOverlay(ContextVk *contextVk) const
{
    const gl::OverlayType *overlay = contextVk->getOverlay();

    // If overlay is disabled, nothing to do.
    if (!overlay->isEnabled())
    {
        return;
    }

    RendererVk *renderer = contextVk->getRenderer();

    uint32_t validationMessageCount = 0;
    std::string lastValidationMessage =
        renderer->getAndClearLastValidationMessage(&validationMessageCount);
    if (validationMessageCount)
    {
        overlay->getTextWidget(gl::WidgetId::VulkanLastValidationMessage)
            ->set(std::move(lastValidationMessage));
        overlay->getCountWidget(gl::WidgetId::VulkanValidationMessageCount)
            ->add(validationMessageCount);
    }

    contextVk->updateOverlayOnPresent();
}

ANGLE_INLINE bool WindowSurfaceVk::overlayHasEnabledWidget(ContextVk *contextVk) const
{
    const gl::OverlayType *overlay = contextVk->getOverlay();
    OverlayVk *overlayVk           = vk::GetImpl(overlay);
    return overlayVk && overlayVk->getEnabledWidgetCount() > 0;
}

angle::Result WindowSurfaceVk::drawOverlay(ContextVk *contextVk, SwapchainImage *image) const
{
    const gl::OverlayType *overlay = contextVk->getOverlay();
    OverlayVk *overlayVk           = vk::GetImpl(overlay);

    // Draw overlay
    const vk::ImageView *imageView = nullptr;
    ANGLE_TRY(image->imageViews.getLevelLayerDrawImageView(
        contextVk, image->image, vk::LevelIndex(0), 0, gl::SrgbWriteControlMode::Default,
        &imageView));
    ANGLE_TRY(overlayVk->onPresent(contextVk, &image->image, imageView,
                                   Is90DegreeRotation(getPreTransform())));

    return angle::Result::Continue;
}

egl::Error WindowSurfaceVk::getBufferAge(const gl::Context *context, EGLint *age)
{
    // Set age to 0 if swap behavior is EGL_BUFFER_DESTROYED.
    if (age != nullptr && mState.swapBehavior == EGL_BUFFER_DESTROYED)
    {
        *age = 0;
        return egl::NoError();
    }

    if (mNeedToAcquireNextSwapchainImage)
    {
        // Acquire the current image if needed.
        DisplayVk *displayVk = vk::GetImpl(context->getDisplay());
        egl::Error result =
            angle::ToEGL(doDeferredAcquireNextImage(context, false), displayVk, EGL_BAD_SURFACE);
        if (result.isError())
        {
            return result;
        }
    }

    if (age != nullptr)
    {
        uint64_t frameNumber = mSwapchainImages[mCurrentSwapchainImageIndex].mFrameNumber;
        if (frameNumber == 0)
        {
            *age = 0;  // Has not been used for rendering yet, no age.
        }
        else
        {
            *age = static_cast<EGLint>(mFrameCount - frameNumber);
        }
    }
    return egl::NoError();
}

egl::Error WindowSurfaceVk::setRenderBuffer(EGLint renderBuffer)
{
    if (std::find(mPresentModes.begin(), mPresentModes.end(),
                  VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR) == mPresentModes.end())
    {
        return egl::EglBadMatch();
    }

    if (renderBuffer == EGL_SINGLE_BUFFER)
    {
        mDesiredSwapchainPresentMode = VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR;
    }
    else  // EGL_BACK_BUFFER
    {
        mDesiredSwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    }
    return egl::NoError();
}

}  // namespace rx
