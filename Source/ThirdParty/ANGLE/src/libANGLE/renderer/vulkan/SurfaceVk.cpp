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

// Special value for currentExtent if surface size is determined by the swapchain's extent.  See
// the VkSurfaceCapabilitiesKHR spec for more details.
constexpr uint32_t kSurfaceSizedBySwapchain = 0xFFFFFFFFu;

// Special value for ImagePresentOperation::imageIndex meaning that VK_EXT_swapchain_maintenance1 is
// supported and fence is used instead of queueSerial.
constexpr uint32_t kInvalidImageIndex = std::numeric_limits<uint32_t>::max();

GLint GetSampleCount(const egl::Config *config)
{
    GLint samples = 1;
    if (config->sampleBuffers && config->samples > 1)
    {
        samples = config->samples;
    }
    return samples;
}

vk::PresentMode GetDesiredPresentMode(const std::vector<vk::PresentMode> &presentModes,
                                      EGLint interval)
{
    ASSERT(!presentModes.empty());

    // If v-sync is enabled, use FIFO, which throttles you to the display rate and is guaranteed to
    // always be supported.
    if (interval > 0)
    {
        return vk::PresentMode::FifoKHR;
    }

    // Otherwise, choose either of the following, if available, in order specified here:
    //
    // - Mailbox is similar to triple-buffering.
    // - Immediate is similar to single-buffering.
    //
    // If neither is supported, we fallback to FIFO.

    bool mailboxAvailable   = false;
    bool immediateAvailable = false;
    bool sharedPresent      = false;

    for (vk::PresentMode presentMode : presentModes)
    {
        switch (presentMode)
        {
            case vk::PresentMode::MailboxKHR:
                mailboxAvailable = true;
                break;
            case vk::PresentMode::ImmediateKHR:
                immediateAvailable = true;
                break;
            case vk::PresentMode::SharedDemandRefreshKHR:
                sharedPresent = true;
                break;
            default:
                break;
        }
    }

    if (mailboxAvailable)
    {
        return vk::PresentMode::MailboxKHR;
    }

    if (immediateAvailable)
    {
        return vk::PresentMode::ImmediateKHR;
    }

    if (sharedPresent)
    {
        return vk::PresentMode::SharedDemandRefreshKHR;
    }

    // Note again that VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available.
    return vk::PresentMode::FifoKHR;
}

uint32_t GetMinImageCount(const VkSurfaceCapabilitiesKHR &surfaceCaps)
{
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
    uint32_t minImageCount = std::max(3u, surfaceCaps.minImageCount);

    // Make sure we don't exceed maxImageCount.
    if (surfaceCaps.maxImageCount > 0 && minImageCount > surfaceCaps.maxImageCount)
    {
        minImageCount = surfaceCaps.maxImageCount;
    }

    return minImageCount;
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

bool NeedsInputAttachmentUsage(const angle::FeaturesVk &features)
{
    return features.supportsShaderFramebufferFetch.enabled ||
           features.supportsShaderFramebufferFetchNonCoherent.enabled ||
           features.emulateAdvancedBlendEquations.enabled;
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
    bool isDepthOrStencilFormat        = textureFormat.hasDepthOrStencilBits();
    VkImageUsageFlags usage = isDepthOrStencilFormat ? kSurfaceVkDepthStencilImageUsageFlags
                                                     : kSurfaceVkColorImageUsageFlags;

    RendererVk *rendererVk = displayVk->getRenderer();
    // If shaders may be fetching from this, we need this image to be an input
    if (NeedsInputAttachmentUsage(rendererVk->getFeatures()))
    {
        usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }

    VkExtent3D extents = {std::max(static_cast<uint32_t>(width), 1u),
                          std::max(static_cast<uint32_t>(height), 1u), 1u};

    angle::FormatID renderableFormatId = vkFormat.getActualRenderableImageFormatID();
    // For devices that don't support creating swapchain images with RGB8, emulate with RGBA8.
    if (rendererVk->getFeatures().overrideSurfaceFormatRGB8ToRGBA8.enabled &&
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

VkColorSpaceKHR MapEglColorSpaceToVkColorSpace(RendererVk *renderer, EGLenum EGLColorspace)
{
    switch (EGLColorspace)
    {
        case EGL_NONE:
            if (renderer->getFeatures().mapUnspecifiedColorSpaceToPassThrough.enabled)
            {
                return VK_COLOR_SPACE_PASS_THROUGH_EXT;
            }
            return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        case EGL_GL_COLORSPACE_LINEAR:
        case EGL_GL_COLORSPACE_SRGB_KHR:
            return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        case EGL_GL_COLORSPACE_DISPLAY_P3_LINEAR_EXT:
            return VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT;
        case EGL_GL_COLORSPACE_DISPLAY_P3_EXT:
        case EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT:
            return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
        case EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT:
            return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
        case EGL_GL_COLORSPACE_SCRGB_EXT:
            return VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT;
        case EGL_GL_COLORSPACE_BT2020_LINEAR_EXT:
            return VK_COLOR_SPACE_BT2020_LINEAR_EXT;
        case EGL_GL_COLORSPACE_BT2020_PQ_EXT:
            return VK_COLOR_SPACE_HDR10_ST2084_EXT;
        case EGL_GL_COLORSPACE_BT2020_HLG_EXT:
            return VK_COLOR_SPACE_HDR10_HLG_EXT;
        default:
            UNREACHABLE();
            return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
}

angle::Result LockSurfaceImpl(DisplayVk *displayVk,
                              vk::ImageHelper *image,
                              vk::BufferHelper &lockBufferHelper,
                              EGLint width,
                              EGLint height,
                              EGLint usageHint,
                              bool preservePixels,
                              uint8_t **bufferPtrOut,
                              EGLint *bufferPitchOut)
{
    const gl::InternalFormat &internalFormat =
        gl::GetSizedInternalFormatInfo(image->getActualFormat().glInternalFormat);
    GLuint rowStride = image->getActualFormat().pixelBytes * width;
    VkDeviceSize bufferSize =
        (static_cast<VkDeviceSize>(rowStride) * static_cast<VkDeviceSize>(height));

    if (!lockBufferHelper.valid() || (lockBufferHelper.getSize() != bufferSize))
    {
        lockBufferHelper.destroy(displayVk->getRenderer());

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext              = nullptr;
        bufferCreateInfo.flags              = 0;
        bufferCreateInfo.size               = bufferSize;
        bufferCreateInfo.usage =
            (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        bufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        bufferCreateInfo.queueFamilyIndexCount = 0;
        bufferCreateInfo.pQueueFamilyIndices   = 0;

        VkMemoryPropertyFlags memoryFlags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        ANGLE_TRY(lockBufferHelper.init(displayVk, bufferCreateInfo, memoryFlags));

        uint8_t *bufferPtr = nullptr;
        ANGLE_TRY(lockBufferHelper.map(displayVk, &bufferPtr));
    }

    if (lockBufferHelper.valid())
    {
        if (preservePixels)
        {
            gl::LevelIndex sourceLevelGL(0);
            const VkClearColorValue *clearColor;
            if (image->removeStagedClearUpdatesAndReturnColor(sourceLevelGL, &clearColor))
            {
                ASSERT(!image->hasStagedUpdatesForSubresource(sourceLevelGL, 0, 1));
                angle::Color<uint8_t> color((uint8_t)(clearColor->float32[0] * 255.0),
                                            (uint8_t)(clearColor->float32[1] * 255.0),
                                            (uint8_t)(clearColor->float32[2] * 255.0),
                                            (uint8_t)(clearColor->float32[3] * 255.0));
                lockBufferHelper.fillWithColor(color, internalFormat);
            }
            else
            {
                gl::Box sourceArea(0, 0, 0, width, height, 1);
                ANGLE_TRY(image->copySurfaceImageToBuffer(displayVk, sourceLevelGL, 1, 0,
                                                          sourceArea, &lockBufferHelper));
            }
        }

        *bufferPitchOut = rowStride;
        *bufferPtrOut   = lockBufferHelper.getMappedMemory();
    }
    return angle::Result::Continue;
}

angle::Result UnlockSurfaceImpl(DisplayVk *displayVk,
                                vk::ImageHelper *image,
                                vk::BufferHelper &lockBufferHelper,
                                EGLint width,
                                EGLint height,
                                bool preservePixels)
{
    if (preservePixels)
    {
        ASSERT(image->valid());

        gl::Box destArea(0, 0, 0, width, height, 1);
        gl::LevelIndex destLevelGL(0);

        ANGLE_TRY(image->copyBufferToSurfaceImage(displayVk, destLevelGL, 1, 0, destArea,
                                                  &lockBufferHelper));
    }

    return angle::Result::Continue;
}

// Converts an EGL rectangle, which is relative to the bottom-left of the surface,
// to a VkRectLayerKHR, relative to Vulkan framebuffer-space, with top-left origin.
// No rotation is done to these damage rectangles per the Vulkan spec.
// The bottomLeftOrigin parameter is true on Android which assumes VkRectLayerKHR to
// have a bottom-left origin.
VkRectLayerKHR ToVkRectLayer(const EGLint *eglRect,
                             EGLint width,
                             EGLint height,
                             bool bottomLeftOrigin)
{
    VkRectLayerKHR rect;
    // Make sure the damage rects are within swapchain bounds.
    rect.offset.x = gl::clamp(eglRect[0], 0, width);

    if (bottomLeftOrigin)
    {
        // EGL rectangles are already specified with a bottom-left origin, therefore the conversion
        // is trivial as we just get its Y coordinate as it is
        rect.offset.y = gl::clamp(eglRect[1], 0, height);
    }
    else
    {
        rect.offset.y =
            gl::clamp(height - gl::clamp(eglRect[1], 0, height) - gl::clamp(eglRect[3], 0, height),
                      0, height);
    }
    rect.extent.width  = gl::clamp(eglRect[2], 0, width - rect.offset.x);
    rect.extent.height = gl::clamp(eglRect[3], 0, height - rect.offset.y);
    rect.layer         = 0;
    return rect;
}

angle::Result GetPresentModes(DisplayVk *displayVk,
                              VkPhysicalDevice physicalDevice,
                              VkSurfaceKHR surface,
                              std::vector<vk::PresentMode> *outPresentModes)
{

    uint32_t presentModeCount = 0;
    ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                                                      &presentModeCount, nullptr));
    ASSERT(presentModeCount > 0);

    std::vector<VkPresentModeKHR> vkPresentModes(presentModeCount);
    ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfacePresentModesKHR(
                                physicalDevice, surface, &presentModeCount, vkPresentModes.data()));

    outPresentModes->resize(presentModeCount);
    std::transform(begin(vkPresentModes), end(vkPresentModes), begin(*outPresentModes),
                   vk::ConvertVkPresentModeToPresentMode);

    return angle::Result::Continue;
}

angle::Result NewSemaphore(vk::Context *context,
                           vk::Recycler<vk::Semaphore> *semaphoreRecycler,
                           vk::Semaphore *semaphoreOut)
{
    if (semaphoreRecycler->empty())
    {
        ANGLE_VK_TRY(context, semaphoreOut->init(context->getDevice()));
    }
    else
    {
        semaphoreRecycler->fetch(semaphoreOut);
    }
    return angle::Result::Continue;
}

VkResult NewFence(VkDevice device, vk::Recycler<vk::Fence> *fenceRecycler, vk::Fence *fenceOut)
{
    VkResult result = VK_SUCCESS;
    if (fenceRecycler->empty())
    {
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags             = 0;
        result                            = fenceOut->init(device, fenceCreateInfo);
    }
    else
    {
        fenceRecycler->fetch(fenceOut);
        ASSERT(fenceOut->getStatus(device) == VK_NOT_READY);
    }
    return result;
}

void RecycleUsedFence(VkDevice device, vk::Recycler<vk::Fence> *fenceRecycler, vk::Fence &&fence)
{
    // Reset fence now to mitigate Intel driver bug, when accessing fence after Swapchain
    // destruction causes crash.
    VkResult result = fence.reset(device);
    if (result != VK_SUCCESS)
    {
        ERR() << "Fence reset failed: " << result << "! Destroying fence...";
        fence.destroy(device);
        return;
    }
    fenceRecycler->recycle(std::move(fence));
}

void AssociateQueueSerialWithPresentHistory(uint32_t imageIndex,
                                            QueueSerial queueSerial,
                                            std::deque<impl::ImagePresentOperation> *presentHistory)
{
    // Walk the list backwards and find the entry for the given image index.  That's the last
    // present with that image.  Associate the QueueSerial with that present operation.
    for (size_t historyIndex = 0; historyIndex < presentHistory->size(); ++historyIndex)
    {
        impl::ImagePresentOperation &presentOperation =
            (*presentHistory)[presentHistory->size() - historyIndex - 1];
        // Must not use this function when VK_EXT_swapchain_maintenance1 is supported.
        ASSERT(!presentOperation.fence.valid());
        ASSERT(presentOperation.imageIndex != kInvalidImageIndex);

        if (presentOperation.imageIndex == imageIndex)
        {
            ASSERT(!presentOperation.queueSerial.valid());
            presentOperation.queueSerial = queueSerial;
            return;
        }
    }
}

bool HasAnyOldSwapchains(const std::deque<impl::ImagePresentOperation> &presentHistory)
{
    // Used to validate that swapchain clean up data can only be carried by the first present
    // operation of a swapchain.  That operation is already removed from history when this call is
    // made, so this verifies that no clean up data exists in the history.
    for (const impl::ImagePresentOperation &presentOperation : presentHistory)
    {
        if (!presentOperation.oldSwapchains.empty())
        {
            return true;
        }
    }

    return false;
}

bool IsCompatiblePresentMode(vk::PresentMode mode,
                             VkPresentModeKHR *compatibleModes,
                             size_t compatibleModesCount)
{
    VkPresentModeKHR vkMode              = vk::ConvertPresentModeToVkPresentMode(mode);
    VkPresentModeKHR *compatibleModesEnd = compatibleModes + compatibleModesCount;
    return std::find(compatibleModes, compatibleModesEnd, vkMode) != compatibleModesEnd;
}

void TryAcquireNextImageUnlocked(VkDevice device,
                                 VkSwapchainKHR swapchain,
                                 impl::ImageAcquireOperation *acquire)
{
    // Check if need to acquire before taking the lock, in case it's unnecessary.
    if (!acquire->needToAcquireNextSwapchainImage)
    {
        return;
    }

    impl::UnlockedTryAcquireData *tryAcquire = &acquire->unlockedTryAcquireData;
    impl::UnlockedTryAcquireResult *result   = &acquire->unlockedTryAcquireResult;

    std::lock_guard<std::mutex> lock(tryAcquire->mutex);

    // Check again under lock if acquire is still needed.  Another thread might have done it before
    // the lock is taken.
    if (!acquire->needToAcquireNextSwapchainImage)
    {
        return;
    }

    result->result     = VK_SUCCESS;
    result->imageIndex = std::numeric_limits<uint32_t>::max();

    // Get a semaphore to signal.
    result->acquireSemaphore = tryAcquire->acquireImageSemaphores.front().getHandle();

    // Try to acquire an image.
    if (result->result == VK_SUCCESS)
    {
        result->result =
            vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, result->acquireSemaphore,
                                  VK_NULL_HANDLE, &result->imageIndex);
    }

    // Don't process the results.  It will be done later when the share group lock is held.

    // The contents of *result can now be used by any thread.
    acquire->needToAcquireNextSwapchainImage = false;
}

// Checks whether a call to TryAcquireNextImageUnlocked has been made whose result is pending
// processing.  This function is called when the share group lock is taken, so no need for
// UnlockedTryAcquireData::mutex.
bool NeedToProcessAcquireNextImageResult(const impl::UnlockedTryAcquireResult &result)
{
    // TryAcquireNextImageUnlocked always creates a new acquire semaphore, use that as indication
    // that there's something to process.
    return result.acquireSemaphore != VK_NULL_HANDLE;
}

bool AreAllFencesSignaled(VkDevice device, const std::vector<vk::Fence> &fences)
{
    for (const vk::Fence &fence : fences)
    {
        if (fence.getStatus(device) != VK_SUCCESS)
        {
            return false;
        }
    }
    return true;
}
}  // namespace

SurfaceVk::SurfaceVk(const egl::SurfaceState &surfaceState) : SurfaceImpl(surfaceState) {}

SurfaceVk::~SurfaceVk() {}

void SurfaceVk::destroy(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    RendererVk *renderer = displayVk->getRenderer();

    mColorRenderTarget.destroy(renderer);
    mDepthStencilRenderTarget.destroy(renderer);
}

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
    ANGLE_TRY(image.initMemoryAndNonZeroFillIfNeeded(
        displayVk, hasProtectedContent, renderer->getMemoryProperties(), flags,
        vk::MemoryAllocationType::OffscreenSurfaceAttachmentImage));

    imageViews.init(renderer);

    return angle::Result::Continue;
}

void OffscreenSurfaceVk::AttachmentImage::destroy(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    RendererVk *renderer = displayVk->getRenderer();
    // Front end must ensure all usage has been submitted.
    imageViews.release(renderer, image.getResourceUse());
    image.releaseImage(renderer);
    image.releaseStagedUpdates(renderer);
}

OffscreenSurfaceVk::OffscreenSurfaceVk(const egl::SurfaceState &surfaceState, RendererVk *renderer)
    : SurfaceVk(surfaceState),
      mWidth(mState.attributes.getAsInt(EGL_WIDTH, 0)),
      mHeight(mState.attributes.getAsInt(EGL_HEIGHT, 0)),
      mColorAttachment(this),
      mDepthStencilAttachment(this)
{
    mColorRenderTarget.init(&mColorAttachment.image, &mColorAttachment.imageViews, nullptr, nullptr,
                            {}, gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    mDepthStencilRenderTarget.init(&mDepthStencilAttachment.image,
                                   &mDepthStencilAttachment.imageViews, nullptr, nullptr, {},
                                   gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
}

OffscreenSurfaceVk::~OffscreenSurfaceVk() {}

egl::Error OffscreenSurfaceVk::initialize(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    angle::Result result = initializeImpl(displayVk);
    return angle::ToEGL(result, EGL_BAD_SURFACE);
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
                                nullptr, {}, gl::LevelIndex(0), 0, 1,
                                RenderTargetTransience::Default);
    }

    if (config->depthStencilFormat != GL_NONE)
    {
        ANGLE_TRY(mDepthStencilAttachment.initialize(
            displayVk, mWidth, mHeight, renderer->getFormat(config->depthStencilFormat), samples,
            robustInit, mState.hasProtectedContent()));
        mDepthStencilRenderTarget.init(&mDepthStencilAttachment.image,
                                       &mDepthStencilAttachment.imageViews, nullptr, nullptr, {},
                                       gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    }

    return angle::Result::Continue;
}

void OffscreenSurfaceVk::destroy(const egl::Display *display)
{
    mColorAttachment.destroy(display);
    mDepthStencilAttachment.destroy(display);

    if (mLockBufferHelper.valid())
    {
        mLockBufferHelper.destroy(vk::GetImpl(display)->getRenderer());
    }

    // Call parent class to destroy any resources parent owns.
    SurfaceVk::destroy(display);
}

egl::Error OffscreenSurfaceVk::unMakeCurrent(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    angle::Result result = contextVk->onSurfaceUnMakeCurrent(this);

    return angle::ToEGL(result, EGL_BAD_CURRENT_SURFACE);
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
                                                     GLenum binding,
                                                     const gl::ImageIndex &imageIndex)
{
    ContextVk *contextVk = vk::GetImpl(context);

    switch (binding)
    {
        case GL_BACK:
            ASSERT(mColorAttachment.image.valid());
            mColorAttachment.image.stageRobustResourceClear(imageIndex);
            ANGLE_TRY(mColorAttachment.image.flushAllStagedUpdates(contextVk));
            break;

        case GL_DEPTH:
        case GL_STENCIL:
            ASSERT(mDepthStencilAttachment.image.valid());
            mDepthStencilAttachment.image.stageRobustResourceClear(imageIndex);
            ANGLE_TRY(mDepthStencilAttachment.image.flushAllStagedUpdates(contextVk));
            break;

        default:
            UNREACHABLE();
            break;
    }
    return angle::Result::Continue;
}

vk::ImageHelper *OffscreenSurfaceVk::getColorAttachmentImage()
{
    return &mColorAttachment.image;
}

egl::Error OffscreenSurfaceVk::lockSurface(const egl::Display *display,
                                           EGLint usageHint,
                                           bool preservePixels,
                                           uint8_t **bufferPtrOut,
                                           EGLint *bufferPitchOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "OffscreenSurfaceVk::lockSurface");

    vk::ImageHelper *image = &mColorAttachment.image;
    ASSERT(image->valid());

    angle::Result result =
        LockSurfaceImpl(vk::GetImpl(display), image, mLockBufferHelper, getWidth(), getHeight(),
                        usageHint, preservePixels, bufferPtrOut, bufferPitchOut);
    return angle::ToEGL(result, EGL_BAD_ACCESS);
}

egl::Error OffscreenSurfaceVk::unlockSurface(const egl::Display *display, bool preservePixels)
{
    vk::ImageHelper *image = &mColorAttachment.image;
    ASSERT(image->valid());
    ASSERT(mLockBufferHelper.valid());

    return angle::ToEGL(UnlockSurfaceImpl(vk::GetImpl(display), image, mLockBufferHelper,
                                          getWidth(), getHeight(), preservePixels),
                        EGL_BAD_ACCESS);
}

EGLint OffscreenSurfaceVk::origin() const
{
    return EGL_UPPER_LEFT_KHR;
}

egl::Error OffscreenSurfaceVk::attachToFramebuffer(const gl::Context *context,
                                                   gl::Framebuffer *framebuffer)
{
    return egl::NoError();
}

egl::Error OffscreenSurfaceVk::detachFromFramebuffer(const gl::Context *context,
                                                     gl::Framebuffer *framebuffer)
{
    return egl::NoError();
}

namespace impl
{
SwapchainCleanupData::SwapchainCleanupData() = default;
SwapchainCleanupData::~SwapchainCleanupData()
{
    ASSERT(swapchain == VK_NULL_HANDLE);
    ASSERT(fences.empty());
    ASSERT(semaphores.empty());
}

SwapchainCleanupData::SwapchainCleanupData(SwapchainCleanupData &&other)
    : swapchain(other.swapchain),
      fences(std::move(other.fences)),
      semaphores(std::move(other.semaphores))
{
    other.swapchain = VK_NULL_HANDLE;
}

VkResult SwapchainCleanupData::getFencesStatus(VkDevice device) const
{
    // From VkSwapchainPresentFenceInfoEXT documentation:
    //   Fences associated with presentations to the same swapchain on the same VkQueue must be
    //   signaled in the same order as the present operations.
    ASSERT(!fences.empty());
    VkResult result = fences.back().getStatus(device);
    ASSERT(result != VK_SUCCESS || AreAllFencesSignaled(device, fences));
    return result;
}

void SwapchainCleanupData::waitFences(VkDevice device, uint64_t timeout) const
{
    if (!fences.empty())
    {
        VkResult result = fences.back().wait(device, timeout);
        ASSERT(result != VK_SUCCESS || AreAllFencesSignaled(device, fences));
    }
}

void SwapchainCleanupData::destroy(VkDevice device,
                                   vk::Recycler<vk::Fence> *fenceRecycler,
                                   vk::Recycler<vk::Semaphore> *semaphoreRecycler)
{
    for (vk::Fence &fence : fences)
    {
        RecycleUsedFence(device, fenceRecycler, std::move(fence));
    }
    fences.clear();

    for (vk::Semaphore &semaphore : semaphores)
    {
        semaphoreRecycler->recycle(std::move(semaphore));
    }
    semaphores.clear();

    if (swapchain)
    {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

ImagePresentOperation::ImagePresentOperation() : imageIndex(kInvalidImageIndex) {}
ImagePresentOperation::~ImagePresentOperation()
{
    ASSERT(!fence.valid());
    ASSERT(!semaphore.valid());
    ASSERT(oldSwapchains.empty());
}

ImagePresentOperation::ImagePresentOperation(ImagePresentOperation &&other)
    : fence(std::move(other.fence)),
      semaphore(std::move(other.semaphore)),
      imageIndex(other.imageIndex),
      queueSerial(other.queueSerial),
      oldSwapchains(std::move(other.oldSwapchains))
{}

ImagePresentOperation &ImagePresentOperation::operator=(ImagePresentOperation &&other)
{
    std::swap(fence, other.fence);
    std::swap(semaphore, other.semaphore);
    std::swap(imageIndex, other.imageIndex);
    std::swap(queueSerial, other.queueSerial);
    std::swap(oldSwapchains, other.oldSwapchains);
    return *this;
}

void ImagePresentOperation::destroy(VkDevice device,
                                    vk::Recycler<vk::Fence> *fenceRecycler,
                                    vk::Recycler<vk::Semaphore> *semaphoreRecycler)
{
    // fence is only used when VK_EXT_swapchain_maintenance1 is supported.
    if (fence.valid())
    {
        RecycleUsedFence(device, fenceRecycler, std::move(fence));
    }

    ASSERT(semaphore.valid());
    semaphoreRecycler->recycle(std::move(semaphore));

    // Destroy old swapchains (relevant only when VK_EXT_swapchain_maintenance1 is not supported).
    for (SwapchainCleanupData &oldSwapchain : oldSwapchains)
    {
        oldSwapchain.destroy(device, fenceRecycler, semaphoreRecycler);
    }
    oldSwapchains.clear();
}

SwapchainImage::SwapchainImage()  = default;
SwapchainImage::~SwapchainImage() = default;

SwapchainImage::SwapchainImage(SwapchainImage &&other)
    : image(std::move(other.image)),
      imageViews(std::move(other.imageViews)),
      framebuffer(std::move(other.framebuffer)),
      fetchFramebuffer(std::move(other.fetchFramebuffer)),
      framebufferResolveMS(std::move(other.framebufferResolveMS)),
      frameNumber(other.frameNumber)
{}

ImageAcquireOperation::ImageAcquireOperation() : needToAcquireNextSwapchainImage(false) {}
}  // namespace impl

using namespace impl;

WindowSurfaceVk::WindowSurfaceVk(const egl::SurfaceState &surfaceState, EGLNativeWindowType window)
    : SurfaceVk(surfaceState),
      mNativeWindowType(window),
      mSurface(VK_NULL_HANDLE),
      mSupportsProtectedSwapchain(false),
      mSwapchain(VK_NULL_HANDLE),
      mSwapchainPresentMode(vk::PresentMode::FifoKHR),
      mDesiredSwapchainPresentMode(vk::PresentMode::FifoKHR),
      mMinImageCount(0),
      mPreTransform(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR),
      mEmulatedPreTransform(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR),
      mCompositeAlpha(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR),
      mCurrentSwapchainImageIndex(0),
      mDepthStencilImageBinding(this, kAnySurfaceImageSubjectIndex),
      mColorImageMSBinding(this, kAnySurfaceImageSubjectIndex),
      mFrameCount(1),
      mBufferAgeQueryFrameNumber(0)
{
    // Initialize the color render target with the multisampled targets.  If not multisampled, the
    // render target will be updated to refer to a swapchain image on every acquire.
    mColorRenderTarget.init(&mColorImageMS, &mColorImageMSViews, nullptr, nullptr, {},
                            gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    mDepthStencilRenderTarget.init(&mDepthStencilImage, &mDepthStencilImageViews, nullptr, nullptr,
                                   {}, gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    mDepthStencilImageBinding.bind(&mDepthStencilImage);
    mColorImageMSBinding.bind(&mColorImageMS);
    mSwapchainStatus.isPending = false;
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
    (void)renderer->waitForPresentToBeSubmitted(&mSwapchainStatus);
    (void)finish(displayVk);

    if (!needsAcquireImageOrProcessResult() && !mSwapchainImages.empty())
    {
        // swapchain image doesn't own ANI semaphore. Release ANI semaphore from image so that it
        // can destroy cleanly without hitting assertion..
        // Only single swapchain image may have semaphore associated.
        ASSERT(mCurrentSwapchainImageIndex < mSwapchainImages.size());
        mSwapchainImages[mCurrentSwapchainImageIndex].image->resetAcquireNextImageSemaphore();
    }

    if (mLockBufferHelper.valid())
    {
        mLockBufferHelper.destroy(renderer);
    }

    for (impl::ImagePresentOperation &presentOperation : mPresentHistory)
    {
        if (presentOperation.fence.valid())
        {
            (void)presentOperation.fence.wait(device, renderer->getMaxFenceWaitTimeNs());
        }
        presentOperation.destroy(device, &mPresentFenceRecycler, &mPresentSemaphoreRecycler);
    }
    mPresentHistory.clear();

    destroySwapChainImages(displayVk);

    if (mSwapchain)
    {
        vkDestroySwapchainKHR(device, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    for (vk::Semaphore &semaphore : mAcquireOperation.unlockedTryAcquireData.acquireImageSemaphores)
    {
        semaphore.destroy(device);
    }
    for (SwapchainCleanupData &oldSwapchain : mOldSwapchains)
    {
        oldSwapchain.waitFences(device, renderer->getMaxFenceWaitTimeNs());
        oldSwapchain.destroy(device, &mPresentFenceRecycler, &mPresentSemaphoreRecycler);
    }
    mOldSwapchains.clear();

    mPresentSemaphoreRecycler.destroy(device);
    mPresentFenceRecycler.destroy(device);

    // Call parent class to destroy any resources parent owns.
    SurfaceVk::destroy(display);

    // Destroy the surface without holding the EGL lock.  This works around a specific deadlock
    // in Android.  On this platform:
    //
    // - For EGL applications, parts of surface creation and destruction are handled by the
    //   platform, and parts of it are done by the native EGL driver.  Namely, on surface
    //   destruction, native_window_api_disconnect is called outside the EGL driver.
    // - For Vulkan applications, vkDestroySurfaceKHR takes full responsibility for destroying
    //   the surface, including calling native_window_api_disconnect.
    //
    // Unfortunately, native_window_api_disconnect may use EGL sync objects and can lead to
    // calling into the EGL driver.  For ANGLE, this is particularly problematic because it is
    // simultaneously a Vulkan application and the EGL driver, causing `vkDestroySurfaceKHR` to
    // call back into ANGLE and attempt to reacquire the EGL lock.
    //
    // Since there are no users of the surface when calling vkDestroySurfaceKHR, it is safe for
    // ANGLE to destroy it without holding the EGL lock, effectively simulating the situation
    // for EGL applications, where native_window_api_disconnect is called after the EGL driver
    // has returned.
    if (mSurface)
    {
        egl::Display::GetCurrentThreadUnlockedTailCall()->add(
            [surface = mSurface, instance](void *resultOut) {
                ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::destroy:vkDestroySurfaceKHR");
                ANGLE_UNUSED_VARIABLE(resultOut);
                vkDestroySurfaceKHR(instance, surface, nullptr);
            });
        mSurface = VK_NULL_HANDLE;
    }
}

egl::Error WindowSurfaceVk::initialize(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    angle::Result result = initializeImpl(displayVk);
    if (result == angle::Result::Incomplete)
    {
        return angle::ToEGL(result, EGL_BAD_MATCH);
    }
    else
    {
        return angle::ToEGL(result, EGL_BAD_SURFACE);
    }
}

egl::Error WindowSurfaceVk::unMakeCurrent(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    angle::Result result = contextVk->onSurfaceUnMakeCurrent(this);
    // Even though all swap chain images are tracked individually, the semaphores are not
    // tracked by ResourceUse. This propagates context's queue serial to surface when it
    // detaches from context so that surface will always wait until context is finished.
    mUse.merge(contextVk->getSubmittedResourceUse());

    return angle::ToEGL(result, EGL_BAD_CURRENT_SURFACE);
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

    ANGLE_TRY(GetPresentModes(displayVk, physicalDevice, mSurface, &mPresentModes));

    // Select appropriate present mode based on vsync parameter. Default to 1 (FIFO), though it
    // will get clamped to the min/max values specified at display creation time.
    setSwapInterval(mState.getPreferredSwapInterval());

    // Ensure that the format and colorspace pair is supported.
    const vk::Format &format = renderer->getFormat(mState.config->renderTargetFormat);
    VkFormat nativeFormat    = format.getActualRenderableImageVkFormat();
    // For devices that don't support creating swapchain images with RGB8, emulate with RGBA8.
    if (renderer->getFeatures().overrideSurfaceFormatRGB8ToRGBA8.enabled &&
        nativeFormat == VK_FORMAT_R8G8B8_UNORM)
    {
        nativeFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }
    VkColorSpaceKHR colorSpace = MapEglColorSpaceToVkColorSpace(
        renderer, static_cast<EGLenum>(mState.attributes.get(EGL_GL_COLORSPACE, EGL_NONE)));
    if (!displayVk->isSurfaceFormatColorspacePairSupported(mSurface, nativeFormat, colorSpace))
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

    // Single buffer, if supported
    if ((mState.attributes.getAsInt(EGL_RENDER_BUFFER, EGL_BACK_BUFFER) == EGL_SINGLE_BUFFER) &&
        supportsPresentMode(vk::PresentMode::SharedDemandRefreshKHR))
    {
        std::vector<vk::PresentMode> presentModes = {vk::PresentMode::SharedDemandRefreshKHR};
        mDesiredSwapchainPresentMode              = GetDesiredPresentMode(presentModes, 0);
    }

    ANGLE_TRY(createSwapChain(displayVk, extents, VK_NULL_HANDLE));

    // Create the semaphores that will be used for vkAcquireNextImageKHR.
    for (vk::Semaphore &semaphore : mAcquireOperation.unlockedTryAcquireData.acquireImageSemaphores)
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
    if (needsAcquireImageOrProcessResult())
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
    ASSERT(!mSwapchainStatus.isPending);

    // If no present operation has been done on the new swapchain, it can be destroyed right away.
    // This means that a new swapchain was created, but before any of its images were presented,
    // it's asked to be recreated.  This can happen for example if vkQueuePresentKHR returns
    // OUT_OF_DATE, the swapchain is recreated and the following vkAcquireNextImageKHR again
    // returns OUT_OF_DATE.  Otherwise, keep the current swapchain as the old swapchain to be
    // scheduled for destruction.
    //
    // The old(er) swapchains still need to be kept to be scheduled for destruction.
    VkSwapchainKHR swapchainToDestroy = VK_NULL_HANDLE;

    if (mPresentHistory.empty())
    {
        // Destroy the current (never-used) swapchain.
        swapchainToDestroy = mSwapchain;
    }

    // Place all present operation into mOldSwapchains. That gets scheduled for destruction when the
    // semaphore of the first image of the next swapchain can be recycled or when fences are
    // signaled (when VK_EXT_swapchain_maintenance1 is supported).
    SwapchainCleanupData cleanupData;

    // If the swapchain is not being immediately destroyed, schedule it for destruction.
    if (swapchainToDestroy == VK_NULL_HANDLE)
    {
        cleanupData.swapchain = mSwapchain;
    }

    for (impl::ImagePresentOperation &presentOperation : mPresentHistory)
    {
        // fence is only used when VK_EXT_swapchain_maintenance1 is supported.
        if (presentOperation.fence.valid())
        {
            cleanupData.fences.emplace_back(std::move(presentOperation.fence));
        }

        ASSERT(presentOperation.semaphore.valid());
        cleanupData.semaphores.emplace_back(std::move(presentOperation.semaphore));

        // Accumulate any previous swapchains that are pending destruction too.
        for (SwapchainCleanupData &oldSwapchain : presentOperation.oldSwapchains)
        {
            mOldSwapchains.emplace_back(std::move(oldSwapchain));
        }
        presentOperation.oldSwapchains.clear();
    }
    mPresentHistory.clear();

    // If too many old swapchains have accumulated, wait idle and destroy them.  This is to prevent
    // failures due to too many swapchains allocated.
    //
    // Note: Nvidia has been observed to fail creation of swapchains after 20 are allocated on
    // desktop, or less than 10 on Quadro P400.
    static constexpr size_t kMaxOldSwapchains = 5;
    if (mOldSwapchains.size() > kMaxOldSwapchains)
    {
        mUse.merge(contextVk->getSubmittedResourceUse());
        ANGLE_TRY(finish(contextVk));
        for (SwapchainCleanupData &oldSwapchain : mOldSwapchains)
        {
            oldSwapchain.waitFences(contextVk->getDevice(),
                                    contextVk->getRenderer()->getMaxFenceWaitTimeNs());
            oldSwapchain.destroy(contextVk->getDevice(), &mPresentFenceRecycler,
                                 &mPresentSemaphoreRecycler);
        }
        mOldSwapchains.clear();
    }

    if (cleanupData.swapchain != VK_NULL_HANDLE || !cleanupData.fences.empty() ||
        !cleanupData.semaphores.empty())
    {
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

    // On Android, vkCreateSwapchainKHR destroys lastSwapchain, which is incorrect.  Wait idle in
    // that case as a workaround.
    if (lastSwapchain &&
        contextVk->getRenderer()->getFeatures().waitIdleBeforeSwapchainRecreation.enabled)
    {
        mUse.merge(contextVk->getSubmittedResourceUse());
        ANGLE_TRY(finish(contextVk));
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
            mSwapchainImages[index].image = std::make_unique<vk::ImageHelper>();
            mSwapchainImageBindings[index].bind(mSwapchainImages[index].image.get());
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
    if (renderer->getFeatures().overrideSurfaceFormatRGB8ToRGBA8.enabled &&
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

    // If shaders may be fetching from this, we need this image to be an input
    if (NeedsInputAttachmentUsage(renderer->getFeatures()))
    {
        imageUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.flags = mState.hasProtectedContent() ? VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR : 0;
    swapchainInfo.surface         = mSurface;
    swapchainInfo.minImageCount   = mMinImageCount;
    swapchainInfo.imageFormat     = vk::GetVkFormatFromFormatID(actualFormatID);
    swapchainInfo.imageColorSpace = MapEglColorSpaceToVkColorSpace(
        renderer, static_cast<EGLenum>(mState.attributes.get(EGL_GL_COLORSPACE, EGL_NONE)));
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
    swapchainInfo.presentMode = vk::ConvertPresentModeToVkPresentMode(mDesiredSwapchainPresentMode);
    swapchainInfo.clipped     = VK_TRUE;
    swapchainInfo.oldSwapchain = lastSwapchain;

#if defined(ANGLE_PLATFORM_WINDOWS)
    // On some AMD drivers we need to explicitly enable the extension and set
    // it to "disallowed" mode in order to avoid seeing impossible-to-handle
    // extension-specific error codes from swapchain functions.
    VkSurfaceFullScreenExclusiveInfoEXT fullscreen = {};
    fullscreen.sType               = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
    fullscreen.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;

    VkSurfaceFullScreenExclusiveWin32InfoEXT fullscreenWin32 = {};
    fullscreenWin32.sType    = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
    fullscreenWin32.hmonitor = MonitorFromWindow((HWND)mNativeWindowType, MONITOR_DEFAULTTONEAREST);

    if (renderer->getFeatures().supportsFullScreenExclusive.enabled &&
        renderer->getFeatures().forceDisableFullScreenExclusive.enabled)
    {
        vk::AddToPNextChain(&swapchainInfo, &fullscreen);
        vk::AddToPNextChain(&swapchainInfo, &fullscreenWin32);
    }
#endif

    if (context->getFeatures().supportsSwapchainMaintenance1.enabled)
    {
        swapchainInfo.flags |= VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_EXT;
    }

    if (isSharedPresentModeDesired())
    {
        swapchainInfo.minImageCount = 1;

        // This feature is by default disabled, and only affects Android platform wsi behavior
        // transparent to angle internal tracking for shared present.
        if (renderer->getFeatures().forceContinuousRefreshOnSharedPresent.enabled)
        {
            swapchainInfo.presentMode = VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
        }
    }

    // Get the list of compatible present modes to avoid unnecessary swapchain recreation.
    VkSwapchainPresentModesCreateInfoEXT compatibleModesInfo = {};
    if (renderer->getFeatures().supportsSwapchainMaintenance1.enabled)
    {
        VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo2 = {};
        surfaceInfo2.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
        surfaceInfo2.surface = mSurface;

        VkSurfacePresentModeEXT surfacePresentMode = {};
        surfacePresentMode.sType                   = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT;
        surfacePresentMode.presentMode             = swapchainInfo.presentMode;
        vk::AddToPNextChain(&surfaceInfo2, &surfacePresentMode);

        VkSurfaceCapabilities2KHR surfaceCaps2 = {};
        surfaceCaps2.sType                     = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

        mCompatiblePresentModes.resize(kMaxCompatiblePresentModes);

        VkSurfacePresentModeCompatibilityEXT compatibleModes = {};
        compatibleModes.sType            = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT;
        compatibleModes.presentModeCount = kMaxCompatiblePresentModes;
        compatibleModes.pPresentModes    = mCompatiblePresentModes.data();
        vk::AddToPNextChain(&surfaceCaps2, &compatibleModes);

        ANGLE_VK_TRY(context, vkGetPhysicalDeviceSurfaceCapabilities2KHR(
                                  renderer->getPhysicalDevice(), &surfaceInfo2, &surfaceCaps2));

        mCompatiblePresentModes.resize(compatibleModes.presentModeCount);

        // The implementation must always return the given preesnt mode as compatible with itself.
        ASSERT(IsCompatiblePresentMode(mDesiredSwapchainPresentMode, mCompatiblePresentModes.data(),
                                       mCompatiblePresentModes.size()));

        if (compatibleModes.presentModeCount > 1)
        {
            compatibleModesInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT;
            compatibleModesInfo.presentModeCount = compatibleModes.presentModeCount;
            compatibleModesInfo.pPresentModes    = mCompatiblePresentModes.data();

            vk::AddToPNextChain(&swapchainInfo, &compatibleModesInfo);
        }
    }
    else
    {
        // Without VK_EXT_swapchain_maintenance1, each present mode can be considered only
        // compatible with itself.
        mCompatiblePresentModes.resize(1);
        mCompatiblePresentModes[0] = swapchainInfo.presentMode;
    }

    // TODO: Once EGL_SWAP_BEHAVIOR_PRESERVED_BIT is supported, the contents of the old swapchain
    // need to carry over to the new one.  http://anglebug.com/2942
    VkSwapchainKHR newSwapChain = VK_NULL_HANDLE;
    ANGLE_VK_TRY(context, vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &newSwapChain));
    mSwapchain            = newSwapChain;
    mSwapchainPresentMode = mDesiredSwapchainPresentMode;

    // If frame timestamp was enabled for the surface, [re]enable it when [re]creating the swapchain
    if (renderer->getFeatures().supportsTimestampSurfaceAttribute.enabled &&
        mState.timestampsEnabled)
    {
        // The implementation of "vkGetPastPresentationTimingGOOGLE" on Android calls into the
        // appropriate ANativeWindow API that enables frame timestamps.
        uint32_t count = 0;
        ANGLE_VK_TRY(context,
                     vkGetPastPresentationTimingGOOGLE(device, mSwapchain, &count, nullptr));
    }

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
        VkImageUsageFlags usage = kSurfaceVkColorImageUsageFlags;
        if (NeedsInputAttachmentUsage(renderer->getFeatures()))
        {
            usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }

        // Create a multisampled image that will be rendered to, and then resolved to a swapchain
        // image.  The actual VkImage is created with rotated coordinates to make it easier to do
        // the resolve.  The ImageHelper::mExtents will have non-rotated extents in order to fit
        // with the rest of ANGLE, (e.g. which calculates the Vulkan scissor with non-rotated
        // values and then rotates the final rectangle).
        ANGLE_TRY(mColorImageMS.initMSAASwapchain(
            context, gl::TextureType::_2D, vkExtents, Is90DegreeRotation(getPreTransform()), format,
            samples, usage, gl::LevelIndex(0), 1, 1, robustInit, mState.hasProtectedContent()));
        ANGLE_TRY(mColorImageMS.initMemoryAndNonZeroFillIfNeeded(
            context, mState.hasProtectedContent(), renderer->getMemoryProperties(),
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk::MemoryAllocationType::SwapchainMSAAImage));

        // Initialize the color render target with the multisampled targets.  If not multisampled,
        // the render target will be updated to refer to a swapchain image on every acquire.
        mColorRenderTarget.init(&mColorImageMS, &mColorImageMSViews, nullptr, nullptr, {},
                                gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    }

    ANGLE_TRY(resizeSwapchainImages(context, imageCount));

    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex)
    {
        SwapchainImage &member = mSwapchainImages[imageIndex];

        ASSERT(member.image);
        member.image->init2DWeakReference(context, swapchainImages[imageIndex], extents,
                                          Is90DegreeRotation(getPreTransform()), intendedFormatID,
                                          actualFormatID, 0, 1, robustInit);
        member.imageViews.init(renderer);
        member.frameNumber = 0;
    }

    // Initialize depth/stencil if requested.
    if (mState.config->depthStencilFormat != GL_NONE)
    {
        const vk::Format &dsFormat = renderer->getFormat(mState.config->depthStencilFormat);

        const VkImageUsageFlags dsUsage = kSurfaceVkDepthStencilImageUsageFlags;

        ANGLE_TRY(mDepthStencilImage.init(context, gl::TextureType::_2D, vkExtents, dsFormat,
                                          samples, dsUsage, gl::LevelIndex(0), 1, 1, robustInit,
                                          mState.hasProtectedContent()));
        ANGLE_TRY(mDepthStencilImage.initMemoryAndNonZeroFillIfNeeded(
            context, mState.hasProtectedContent(), renderer->getMemoryProperties(),
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vk::MemoryAllocationType::SwapchainDepthStencilImage));

        mDepthStencilRenderTarget.init(&mDepthStencilImage, &mDepthStencilImageViews, nullptr,
                                       nullptr, {}, gl::LevelIndex(0), 0, 1,
                                       RenderTargetTransience::Default);

        // We will need to pass depth/stencil image views to the RenderTargetVk in the future.
    }

    // Need to acquire a new image before the swapchain can be used.
    mAcquireOperation.needToAcquireNextSwapchainImage = true;

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
                                                          bool presentOutOfDate,
                                                          bool *swapchainRecreatedOut)
{
    *swapchainRecreatedOut = false;

    bool swapIntervalChanged =
        !IsCompatiblePresentMode(mDesiredSwapchainPresentMode, mCompatiblePresentModes.data(),
                                 mCompatiblePresentModes.size());
    presentOutOfDate = presentOutOfDate || swapIntervalChanged;

    // If there's no change, early out.
    if (!contextVk->getRenderer()->getFeatures().perFrameWindowSizeQuery.enabled &&
        !presentOutOfDate)
    {
        return angle::Result::Continue;
    }

    // Get the latest surface capabilities.
    ANGLE_TRY(queryAndAdjustSurfaceCaps(contextVk, &mSurfaceCaps));

    if (contextVk->getRenderer()->getFeatures().perFrameWindowSizeQuery.enabled)
    {
        // On Android, rotation can cause the minImageCount to change
        uint32_t minImageCount = GetMinImageCount(mSurfaceCaps);
        if (mMinImageCount != minImageCount)
        {
            presentOutOfDate = true;
            mMinImageCount   = minImageCount;
        }

        if (!presentOutOfDate)
        {
            // This device generates neither VK_ERROR_OUT_OF_DATE_KHR nor VK_SUBOPTIMAL_KHR.  Check
            // for whether the size and/or rotation have changed since the swapchain was created.
            uint32_t swapchainWidth  = getWidth();
            uint32_t swapchainHeight = getHeight();
            presentOutOfDate         = mSurfaceCaps.currentTransform != mPreTransform ||
                               mSurfaceCaps.currentExtent.width != swapchainWidth ||
                               mSurfaceCaps.currentExtent.height != swapchainHeight;
        }
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

    *swapchainRecreatedOut = true;
    return recreateSwapchain(contextVk, newSwapchainExtents);
}

void WindowSurfaceVk::releaseSwapchainImages(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    mColorRenderTarget.release(contextVk);
    mDepthStencilRenderTarget.release(contextVk);

    if (mDepthStencilImage.valid())
    {
        mDepthStencilImageViews.release(renderer, mDepthStencilImage.getResourceUse());
        mDepthStencilImage.releaseImageFromShareContexts(renderer, contextVk, {});
        mDepthStencilImage.releaseStagedUpdates(renderer);
    }

    if (mColorImageMS.valid())
    {
        mColorImageMSViews.release(renderer, mColorImageMS.getResourceUse());
        mColorImageMS.releaseImageFromShareContexts(renderer, contextVk, {});
        mColorImageMS.releaseStagedUpdates(renderer);
        contextVk->addGarbage(&mFramebufferMS);
    }

    mSwapchainImageBindings.clear();

    for (SwapchainImage &swapchainImage : mSwapchainImages)
    {
        ASSERT(swapchainImage.image);
        swapchainImage.imageViews.release(renderer, swapchainImage.image->getResourceUse());
        // swapchain image must not have ANI semaphore assigned here, since acquired image must be
        // presented before swapchain recreation.
        swapchainImage.image->resetImageWeakReference();
        swapchainImage.image->destroy(renderer);

        contextVk->addGarbage(&swapchainImage.framebuffer);
        if (swapchainImage.fetchFramebuffer.valid())
        {
            contextVk->addGarbage(&swapchainImage.fetchFramebuffer);
        }
        if (swapchainImage.framebufferResolveMS.valid())
        {
            contextVk->addGarbage(&swapchainImage.framebufferResolveMS);
        }
    }

    mSwapchainImages.clear();
}

angle::Result WindowSurfaceVk::finish(vk::Context *context)
{
    RendererVk *renderer = context->getRenderer();

    mUse.merge(mDepthStencilImage.getResourceUse());
    mUse.merge(mColorImageMS.getResourceUse());
    for (SwapchainImage &swapchainImage : mSwapchainImages)
    {
        mUse.merge(swapchainImage.image->getResourceUse());
    }

    return renderer->finishResourceUse(context, mUse);
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
        ASSERT(swapchainImage.image);
        // swapchain image must not have ANI semaphore assigned here, because it should be released
        // in the destroy() prior to calling this method.
        // We don't own the swapchain image handles, so we just remove our reference to it.
        swapchainImage.image->resetImageWeakReference();
        swapchainImage.image->destroy(renderer);
        swapchainImage.imageViews.destroy(device);
        swapchainImage.framebuffer.destroy(device);
        if (swapchainImage.fetchFramebuffer.valid())
        {
            swapchainImage.fetchFramebuffer.destroy(device);
        }
        if (swapchainImage.framebufferResolveMS.valid())
        {
            swapchainImage.framebufferResolveMS.destroy(device);
        }
    }

    mSwapchainImages.clear();
}

egl::Error WindowSurfaceVk::prepareSwap(const gl::Context *context)
{
    if (!mAcquireOperation.needToAcquireNextSwapchainImage)
    {
        return egl::NoError();
    }

    DisplayVk *displayVk = vk::GetImpl(context->getDisplay());
    RendererVk *renderer = displayVk->getRenderer();

    bool swapchainRecreated = false;
    angle::Result result = prepareForAcquireNextSwapchainImage(context, false, &swapchainRecreated);
    if (result != angle::Result::Continue)
    {
        return angle::ToEGL(result, EGL_BAD_SURFACE);
    }
    if (swapchainRecreated || isSharedPresentMode())
    {
        // If swapchain is recreated or it is a shred present mode, acquire the image right away;
        // it's not going to block.
        result = doDeferredAcquireNextImageWithUsableSwapchain(context);
        return angle::ToEGL(result, EGL_BAD_SURFACE);
    }

    // Call vkAcquireNextImageKHR without holding the share group lock.  The following are accessed
    // by this function:
    //
    // - mAcquireOperation.needToAcquireNextSwapchainImage, which is atomic
    // - Contents of mAcquireOperation.unlockedTryAcquireData and
    //   mAcquireOperation.unlockedTryAcquireResult, which are protected by
    //   unlockedTryAcquireData.mutex
    // - context->getDevice(), which doesn't need external synchronization
    // - mSwapchain
    //
    // The latter two are also protected by unlockedTryAcquireData.mutex during this call.  Note
    // that due to the presence of needToAcquireNextSwapchainImage, the threads may be in either of
    // these states:
    //
    // 1. Calling eglPrepareSwapBuffersANGLE; in this case, they are accessing mSwapchain protected
    //    by the aforementioned mutex
    // 2. Calling doDeferredAcquireNextImage() through an EGL/GL call
    //    * If needToAcquireNextSwapchainImage is true, these variables are protected by the same
    //      mutex in the same TryAcquireNextImageUnlocked call.
    //    * If needToAcquireNextSwapchainImage is false, these variables are not protected by this
    //      mutex.  However, in this case no thread could be calling TryAcquireNextImageUnlocked
    //      because needToAcquireNextSwapchainImage is false (and hence there is no data race).
    //      Note that needToAcquireNextSwapchainImage's atomic store and load ensure
    //      availability/visibility of changes to these variables between threads.
    //
    // The result of this call is processed in doDeferredAcquireNextImage() by whoever ends up
    // calling it (likely the eglSwapBuffers call that follows)

    egl::Display::GetCurrentThreadUnlockedTailCall()->add(
        [device = renderer->getDevice(), swapchain = mSwapchain,
         acquire = &mAcquireOperation](void *resultOut) {
            ANGLE_TRACE_EVENT0("gpu.angle", "Acquire Swap Image Before Swap");
            ANGLE_UNUSED_VARIABLE(resultOut);
            TryAcquireNextImageUnlocked(device, swapchain, acquire);
        });

    return egl::NoError();
}

egl::Error WindowSurfaceVk::swapWithDamage(const gl::Context *context,
                                           const EGLint *rects,
                                           EGLint n_rects)
{
    const angle::Result result = swapImpl(context, rects, n_rects, nullptr);
    return angle::ToEGL(result, EGL_BAD_SURFACE);
}

egl::Error WindowSurfaceVk::swap(const gl::Context *context)
{
    // When in shared present mode, eglSwapBuffers is unnecessary except for mode change.  When mode
    // change is not expected, the eglSwapBuffers call is forwarded to the context as a glFlush.
    // This allows the context to skip it if there's nothing to flush.  Otherwise control is bounced
    // back swapImpl().
    //
    // Some apps issue eglSwapBuffers after glFlush unnecessary, causing the CPU throttling logic to
    // effectively wait for the just submitted commands.
    if (isSharedPresentMode() && mSwapchainPresentMode == mDesiredSwapchainPresentMode)
    {
        const angle::Result result = vk::GetImpl(context)->flush(context);
        return angle::ToEGL(result, EGL_BAD_SURFACE);
    }

    const angle::Result result = swapImpl(context, nullptr, 0, nullptr);
    return angle::ToEGL(result, EGL_BAD_SURFACE);
}

angle::Result WindowSurfaceVk::computePresentOutOfDate(vk::Context *context,
                                                       VkResult result,
                                                       bool *presentOutOfDate)
{
    // If OUT_OF_DATE is returned, it's ok, we just need to recreate the swapchain before
    // continuing.  We do the same when VK_SUBOPTIMAL_KHR is returned to avoid visual degradation
    // and handle device rotation / screen resize.
    *presentOutOfDate = result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR;
    if (!*presentOutOfDate)
    {
        ANGLE_VK_TRY(context, result);
    }
    return angle::Result::Continue;
}

vk::Framebuffer &WindowSurfaceVk::chooseFramebuffer(const SwapchainResolveMode swapchainResolveMode)
{
    if (isMultiSampled())
    {
        return swapchainResolveMode == SwapchainResolveMode::Enabled
                   ? mSwapchainImages[mCurrentSwapchainImageIndex].framebufferResolveMS
                   : mFramebufferMS;
    }

    // Choose which framebuffer to use based on fetch, so it will have a matching renderpass
    ASSERT(swapchainResolveMode == SwapchainResolveMode::Disabled);
    return mFramebufferFetchMode == FramebufferFetchMode::Enabled
               ? mSwapchainImages[mCurrentSwapchainImageIndex].fetchFramebuffer
               : mSwapchainImages[mCurrentSwapchainImageIndex].framebuffer;
}

angle::Result WindowSurfaceVk::prePresentSubmit(ContextVk *contextVk,
                                                const vk::Semaphore &presentSemaphore)
{
    RendererVk *renderer = contextVk->getRenderer();

    SwapchainImage &image               = mSwapchainImages[mCurrentSwapchainImageIndex];
    vk::Framebuffer &currentFramebuffer = chooseFramebuffer(SwapchainResolveMode::Disabled);

    // Make sure deferred clears are applied, if any.
    if (mColorImageMS.valid())
    {
        ANGLE_TRY(mColorImageMS.flushStagedUpdates(contextVk, gl::LevelIndex(0), gl::LevelIndex(1),
                                                   0, 1, {}));
    }
    else
    {
        ANGLE_TRY(image.image->flushStagedUpdates(contextVk, gl::LevelIndex(0), gl::LevelIndex(1),
                                                  0, 1, {}));
    }

    // If user calls eglSwapBuffer without use it, image may already in Present layout (if swap
    // without any draw) or Undefined (first time present). In this case, if
    // acquireNextImageSemaphore has not been waited, we must add to context will force the
    // semaphore wait so that it will be in unsignaled state and ready to use for ANI call.
    if (image.image->getAcquireNextImageSemaphore().valid())
    {
        ASSERT(!renderer->getFeatures().supportsPresentation.enabled ||
               image.image->getCurrentImageLayout() == vk::ImageLayout::Present ||
               image.image->getCurrentImageLayout() == vk::ImageLayout::Undefined);
        contextVk->addWaitSemaphore(image.image->getAcquireNextImageSemaphore().getHandle(),
                                    vk::kSwapchainAcquireImageWaitStageFlags);
        image.image->resetAcquireNextImageSemaphore();
    }

    // We can only do present related optimization if this is the last renderpass that touches the
    // swapchain image. MSAA resolve and overlay will insert another renderpass which disqualifies
    // the optimization.
    bool imageResolved = false;
    if (currentFramebuffer.valid())
    {
        ANGLE_TRY(contextVk->optimizeRenderPassForPresent(
            currentFramebuffer.getHandle(), &image.imageViews, image.image.get(), &mColorImageMS,
            mSwapchainPresentMode, &imageResolved));
    }

    // Because the color attachment defers layout changes until endRenderPass time, we must call
    // finalize the layout transition in the renderpass before we insert layout change to
    // ImageLayout::Present bellow.
    contextVk->finalizeImageLayout(image.image.get(), {});
    contextVk->finalizeImageLayout(&mColorImageMS, {});

    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper({}, &commandBufferHelper));

    if (mColorImageMS.valid() && !imageResolved)
    {
        // Transition the multisampled image to TRANSFER_SRC for resolve.
        vk::CommandBufferAccess access;
        access.onImageTransferRead(VK_IMAGE_ASPECT_COLOR_BIT, &mColorImageMS);
        access.onImageTransferWrite(gl::LevelIndex(0), 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                    image.image.get());

        ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper(access, &commandBufferHelper));

        VkImageResolve resolveRegion                = {};
        resolveRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        resolveRegion.srcSubresource.mipLevel       = 0;
        resolveRegion.srcSubresource.baseArrayLayer = 0;
        resolveRegion.srcSubresource.layerCount     = 1;
        resolveRegion.srcOffset                     = {};
        resolveRegion.dstSubresource                = resolveRegion.srcSubresource;
        resolveRegion.dstOffset                     = {};
        resolveRegion.extent                        = image.image->getRotatedExtents();

        mColorImageMS.resolve(image.image.get(), resolveRegion,
                              &commandBufferHelper->getCommandBuffer());
        contextVk->getPerfCounters().swapchainResolveOutsideSubpass++;
    }

    if (renderer->getFeatures().supportsPresentation.enabled)
    {
        // This does nothing if it's already in the requested layout
        image.image->recordReadBarrier(contextVk, VK_IMAGE_ASPECT_COLOR_BIT,
                                       vk::ImageLayout::Present, commandBufferHelper);
    }

    // The overlay is drawn after this.  This ensures that drawing the overlay does not interfere
    // with other functionality, especially counters used to validate said functionality.
    const bool shouldDrawOverlay = overlayHasEnabledWidget(contextVk);

    ANGLE_TRY(contextVk->flushImpl(shouldDrawOverlay ? nullptr : &presentSemaphore, nullptr,
                                   RenderPassClosureReason::EGLSwapBuffers));

    if (shouldDrawOverlay)
    {
        updateOverlay(contextVk);
        ANGLE_TRY(drawOverlay(contextVk, &image));
        ANGLE_TRY(contextVk->flushImpl(&presentSemaphore, nullptr,
                                       RenderPassClosureReason::AlreadySpecifiedElsewhere));
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

    // Clean up whatever present is already finished. Do this before allocating new semaphore/fence
    // to reduce number of allocations.
    ANGLE_TRY(cleanUpPresentHistory(contextVk));

    // Get a new semaphore to use for present.
    vk::Semaphore presentSemaphore;
    ANGLE_TRY(NewSemaphore(contextVk, &mPresentSemaphoreRecycler, &presentSemaphore));

    // Make a submission before present to flush whatever's pending.  In the very least, a
    // submission is necessary to make sure the present semaphore is signaled.
    ANGLE_TRY(prePresentSubmit(contextVk, presentSemaphore));

    QueueSerial swapSerial = contextVk->getLastSubmittedQueueSerial();

    if (!contextVk->getFeatures().supportsSwapchainMaintenance1.enabled)
    {
        // Associate swapSerial of this present with the previous present of the same imageIndex.
        // Completion of swapSerial implies that current ANI semaphore was waited.  See
        // doc/PresentSemaphores.md for details.
        AssociateQueueSerialWithPresentHistory(mCurrentSwapchainImageIndex, swapSerial,
                                               &mPresentHistory);
    }

    VkPresentInfoKHR presentInfo   = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext              = pNextChain;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = presentSemaphore.ptr();
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
            vkRects[i] = ToVkRectLayer(
                eglRects + i * 4, width, height,
                contextVk->getFeatures().bottomLeftOriginPresentRegionRectangles.enabled);
        }
        presentRegion.pRectangles = vkRects.data();

        presentRegions.sType          = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR;
        presentRegions.swapchainCount = 1;
        presentRegions.pRegions       = &presentRegion;

        vk::AddToPNextChain(&presentInfo, &presentRegions);
    }

    VkSwapchainPresentFenceInfoEXT presentFenceInfo = {};
    VkSwapchainPresentModeInfoEXT presentModeInfo   = {};
    vk::Fence presentFence;
    VkPresentModeKHR presentMode;
    if (contextVk->getFeatures().supportsSwapchainMaintenance1.enabled)
    {
        ANGLE_VK_TRY(contextVk,
                     NewFence(contextVk->getDevice(), &mPresentFenceRecycler, &presentFence));

        presentFenceInfo.sType          = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
        presentFenceInfo.swapchainCount = 1;
        presentFenceInfo.pFences        = presentFence.ptr();

        vk::AddToPNextChain(&presentInfo, &presentFenceInfo);

        // Update the present mode if necessary and possible
        if (mSwapchainPresentMode != mDesiredSwapchainPresentMode &&
            IsCompatiblePresentMode(mDesiredSwapchainPresentMode, mCompatiblePresentModes.data(),
                                    mCompatiblePresentModes.size()))
        {
            presentMode = vk::ConvertPresentModeToVkPresentMode(mDesiredSwapchainPresentMode);

            presentModeInfo.sType          = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT;
            presentModeInfo.swapchainCount = 1;
            presentModeInfo.pPresentModes  = &presentMode;

            vk::AddToPNextChain(&presentInfo, &presentModeInfo);

            mSwapchainPresentMode = mDesiredSwapchainPresentMode;
        }
    }

    // The ANI semaphore must have been submitted and waited.
    ASSERT(!mSwapchainImages[mCurrentSwapchainImageIndex]
                .image->getAcquireNextImageSemaphore()
                .valid());

    renderer->queuePresent(contextVk, contextVk->getPriority(), presentInfo, &mSwapchainStatus);

    // Set FrameNumber for the presented image.
    mSwapchainImages[mCurrentSwapchainImageIndex].frameNumber = mFrameCount++;

    // Place the semaphore in the present history.  Schedule pending old swapchains to be destroyed
    // at the same time the semaphore for this present can be destroyed.
    mPresentHistory.emplace_back();
    mPresentHistory.back().semaphore = std::move(presentSemaphore);
    if (contextVk->getFeatures().supportsSwapchainMaintenance1.enabled)
    {
        mPresentHistory.back().imageIndex = kInvalidImageIndex;
        mPresentHistory.back().fence      = std::move(presentFence);
        ANGLE_TRY(cleanUpOldSwapchains(contextVk));
    }
    else
    {
        // Image index is used to associate swapSerial in the next present.
        mPresentHistory.back().imageIndex    = mCurrentSwapchainImageIndex;
        mPresentHistory.back().oldSwapchains = std::move(mOldSwapchains);
    }

    ANGLE_TRY(
        computePresentOutOfDate(contextVk, mSwapchainStatus.lastPresentResult, presentOutOfDate));

    // Now apply CPU throttle if needed
    ANGLE_TRY(throttleCPU(vk::GetImpl(renderer->getDisplay()), swapSerial));

    contextVk->resetPerFramePerfCounters();

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::throttleCPU(DisplayVk *displayVk,
                                           const QueueSerial &currentSubmitSerial)
{
    // Wait on the oldest serial and replace it with the newest as the circular buffer moves
    // forward.
    QueueSerial swapSerial = mSwapHistory.front();
    mSwapHistory.front()   = currentSubmitSerial;
    mSwapHistory.next();

    if (swapSerial.valid() && !displayVk->getRenderer()->hasQueueSerialFinished(swapSerial))
    {
        // Make this call after unlocking the EGL lock.   The RendererVk::finishQueueSerial is
        // necessarily thread-safe because it can get called from any number of GL commands, which
        // don't necessarily hold the EGL lock.
        //
        // As this is an unlocked tail call, it must not access anything else in RendererVk.  The
        // display passed to |finishQueueSerial| is a |vk::Context|, and the only possible
        // modification to it is through |handleError()|.
        egl::Display::GetCurrentThreadUnlockedTailCall()->add(
            [displayVk, swapSerial](void *resultOut) {
                ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::throttleCPU");
                ANGLE_UNUSED_VARIABLE(resultOut);
                (void)displayVk->getRenderer()->finishQueueSerial(displayVk, swapSerial);
            });
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::cleanUpPresentHistory(vk::Context *context)
{
    const VkDevice device = context->getDevice();

    while (!mPresentHistory.empty())
    {
        impl::ImagePresentOperation &presentOperation = mPresentHistory.front();

        // If there is no fence associated with the history, check queueSerial.
        if (!presentOperation.fence.valid())
        {
            // |kInvalidImageIndex| is only possible when |VkSwapchainPresentFenceInfoEXT| is used,
            // in which case |fence| is always valid.
            ASSERT(presentOperation.imageIndex != kInvalidImageIndex);
            // If queueSerial already assigned, check if it is finished.
            if (!presentOperation.queueSerial.valid() ||
                !context->getRenderer()->hasQueueSerialFinished(presentOperation.queueSerial))
            {
                // Not yet
                break;
            }
        }
        // Otherwise check to see if the fence is signaled.
        else
        {
            VkResult result = presentOperation.fence.getStatus(device);
            if (result == VK_NOT_READY)
            {
                // Not yet
                break;
            }

            ANGLE_VK_TRY(context, result);
        }

        presentOperation.destroy(device, &mPresentFenceRecycler, &mPresentSemaphoreRecycler);
        mPresentHistory.pop_front();
    }

    // The present history can grow indefinitely if a present operation is done on an index that's
    // never presented in the future.  In that case, there's no queueSerial associated with that
    // present operation.  Move the offending entry to last, so the resources associated with the
    // rest of the present operations can be duly freed.
    if (mPresentHistory.size() > mSwapchainImages.size() * 2 &&
        !mPresentHistory.front().fence.valid() && !mPresentHistory.front().queueSerial.valid())
    {
        impl::ImagePresentOperation presentOperation = std::move(mPresentHistory.front());
        mPresentHistory.pop_front();

        // |kInvalidImageIndex| is only possible when |VkSwapchainPresentFenceInfoEXT| is used, in
        // which case |fence| is always valid.
        ASSERT(presentOperation.imageIndex != kInvalidImageIndex);

        // Move clean up data to the next (now first) present operation, if any.  Note that there
        // cannot be any clean up data on the rest of the present operations, because the first
        // present already gathers every old swapchain to clean up.
        ASSERT(!HasAnyOldSwapchains(mPresentHistory));
        mPresentHistory.front().oldSwapchains = std::move(presentOperation.oldSwapchains);

        // Put the present operation at the end of the queue so it's revisited after the rest of the
        // present operations are cleaned up.
        mPresentHistory.push_back(std::move(presentOperation));
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::cleanUpOldSwapchains(vk::Context *context)
{
    const VkDevice device = context->getDevice();

    ASSERT(context->getFeatures().supportsSwapchainMaintenance1.enabled);

    while (!mOldSwapchains.empty())
    {
        impl::SwapchainCleanupData &oldSwapchain = mOldSwapchains.front();
        VkResult result                          = oldSwapchain.getFencesStatus(device);
        if (result == VK_NOT_READY)
        {
            break;
        }
        ANGLE_VK_TRY(context, result);
        oldSwapchain.destroy(device, &mPresentFenceRecycler, &mPresentSemaphoreRecycler);
        mOldSwapchains.pop_front();
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::swapImpl(const gl::Context *context,
                                        const EGLint *rects,
                                        EGLint n_rects,
                                        const void *pNextChain)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::swapImpl");

    ContextVk *contextVk = vk::GetImpl(context);

    // prepareSwap() has already called vkAcquireNextImageKHR if necessary, but its results need to
    // be processed now if not already.  doDeferredAcquireNextImage() will
    // automatically skip the prepareForAcquireNextSwapchainImage() and vkAcquireNextImageKHR calls
    // in that case.  The swapchain recreation path in
    // doDeferredAcquireNextImageWithUsableSwapchain() is acceptable because it only happens if
    // previous vkAcquireNextImageKHR failed.
    if (needsAcquireImageOrProcessResult())
    {
        ANGLE_TRY(doDeferredAcquireNextImage(context, false));
    }

    bool presentOutOfDate = false;
    ANGLE_TRY(present(contextVk, rects, n_rects, pNextChain, &presentOutOfDate));

    if (!presentOutOfDate)
    {
        // Defer acquiring the next swapchain image since the swapchain is not out-of-date.
        deferAcquireNextImage();
    }
    else
    {
        // Immediately try to acquire the next image, which will recognize the out-of-date
        // swapchain (potentially because of a rotation change), and recreate it.
        ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, "Out-of-Date Swapbuffer");
        ANGLE_TRY(doDeferredAcquireNextImage(context, presentOutOfDate));
    }

    RendererVk *renderer = contextVk->getRenderer();
    DisplayVk *displayVk = vk::GetImpl(context->getDisplay());
    ANGLE_TRY(renderer->syncPipelineCacheVk(displayVk, context));

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::onSharedPresentContextFlush(const gl::Context *context)
{
    return swapImpl(context, nullptr, 0, nullptr);
}

bool WindowSurfaceVk::hasStagedUpdates() const
{
    return !needsAcquireImageOrProcessResult() &&
           mSwapchainImages[mCurrentSwapchainImageIndex].image->hasStagedUpdatesInAllocatedLevels();
}

void WindowSurfaceVk::setTimestampsEnabled(bool enabled)
{
    // The frontend has already cached the state, nothing to do.
    ASSERT(IsAndroid());
}

void WindowSurfaceVk::deferAcquireNextImage()
{
    mAcquireOperation.needToAcquireNextSwapchainImage = true;

    // Set gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0 via subject-observer message-passing
    // to the front-end Surface, Framebuffer, and Context classes.  The DIRTY_BIT_COLOR_ATTACHMENT_0
    // is processed before all other dirty bits.  However, since the attachments of the default
    // framebuffer cannot change, this bit will be processed before all others.  It will cause
    // WindowSurfaceVk::getAttachmentRenderTarget() to be called (which will acquire the next image)
    // before any RenderTargetVk accesses.  The processing of other dirty bits as well as other
    // setup for draws and reads will then access a properly-updated RenderTargetVk.
    onStateChange(angle::SubjectMessage::SwapchainImageChanged);
}

angle::Result WindowSurfaceVk::prepareForAcquireNextSwapchainImage(const gl::Context *context,
                                                                   bool presentOutOfDate,
                                                                   bool *swapchainRecreatedOut)
{
    ASSERT(!NeedToProcessAcquireNextImageResult(mAcquireOperation.unlockedTryAcquireResult));

    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    // TODO(jmadill): Expose in CommandQueueInterface, or manage in CommandQueue. b/172704839
    if (renderer->isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(renderer->waitForPresentToBeSubmitted(&mSwapchainStatus));
        VkResult result = mSwapchainStatus.lastPresentResult;

        // Now that we have the result from the last present need to determine if it's out of date
        // or not.
        ANGLE_TRY(computePresentOutOfDate(contextVk, result, &presentOutOfDate));
    }

    return checkForOutOfDateSwapchain(contextVk, presentOutOfDate, swapchainRecreatedOut);
}

angle::Result WindowSurfaceVk::doDeferredAcquireNextImage(const gl::Context *context,
                                                          bool presentOutOfDate)
{
    bool swapchainRecreated = false;
    // prepareForAcquireNextSwapchainImage() may recreate Swapchain even if there is an image
    // acquired. Avoid this, by skipping the prepare call.
    if (!NeedToProcessAcquireNextImageResult(mAcquireOperation.unlockedTryAcquireResult))
    {
        ANGLE_TRY(
            prepareForAcquireNextSwapchainImage(context, presentOutOfDate, &swapchainRecreated));
    }
    return doDeferredAcquireNextImageWithUsableSwapchain(context);
}

angle::Result WindowSurfaceVk::doDeferredAcquireNextImageWithUsableSwapchain(
    const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

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
            bool swapchainRecreated = false;
            ANGLE_TRY(checkForOutOfDateSwapchain(contextVk, true, &swapchainRecreated));
            ASSERT(swapchainRecreated);
            // Try one more time and bail if we fail
            result = acquireNextSwapchainImage(contextVk);
        }
        ANGLE_VK_TRY(contextVk, result);
    }

    // Auto-invalidate the contents of the surface.  According to EGL, on swap:
    //
    // - When EGL_BUFFER_DESTROYED is specified, the contents of the color image can be
    //   invalidated.
    //    * This is disabled when buffer age has been queried to work around a dEQP test bug.
    // - Depth/Stencil can always be invalidated
    //
    // In all cases, when in shared present mode, swap is implicit and the swap behavior
    // doesn't apply so no invalidation is done.
    if (!isSharedPresentMode())
    {
        if (mState.swapBehavior == EGL_BUFFER_DESTROYED && mBufferAgeQueryFrameNumber == 0)
        {
            mSwapchainImages[mCurrentSwapchainImageIndex].image->invalidateSubresourceContent(
                contextVk, gl::LevelIndex(0), 0, 1, nullptr);
            if (mColorImageMS.valid())
            {
                mColorImageMS.invalidateSubresourceContent(contextVk, gl::LevelIndex(0), 0, 1,
                                                           nullptr);
            }
        }
        if (mDepthStencilImage.valid())
        {
            mDepthStencilImage.invalidateSubresourceContent(contextVk, gl::LevelIndex(0), 0, 1,
                                                            nullptr);
            mDepthStencilImage.invalidateSubresourceStencilContent(contextVk, gl::LevelIndex(0), 0,
                                                                   1, nullptr);
        }
    }

    return angle::Result::Continue;
}

bool WindowSurfaceVk::skipAcquireNextSwapchainImageForSharedPresentMode() const
{
    if (isSharedPresentMode())
    {
        ASSERT(mSwapchainImages.size());
        const SwapchainImage &image = mSwapchainImages[0];
        if (image.image->valid() &&
            image.image->getCurrentImageLayout() == vk::ImageLayout::SharedPresent)
        {
            return true;
        }
    }

    return false;
}

// This method will either return VK_SUCCESS or VK_ERROR_*.  Thus, it is appropriate to ASSERT that
// the return value won't be VK_SUBOPTIMAL_KHR.
VkResult WindowSurfaceVk::acquireNextSwapchainImage(vk::Context *context)
{
    VkDevice device = context->getDevice();

    if (skipAcquireNextSwapchainImageForSharedPresentMode())
    {
        ASSERT(!NeedToProcessAcquireNextImageResult(mAcquireOperation.unlockedTryAcquireResult));
        // This will check for OUT_OF_DATE when in single image mode. and prevent
        // re-AcquireNextImage.
        VkResult result = vkGetSwapchainStatusKHR(device, mSwapchain);
        if (ANGLE_UNLIKELY(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR))
        {
            return result;
        }
        // Note that an acquire is no longer needed.
        mAcquireOperation.needToAcquireNextSwapchainImage = false;
        return VK_SUCCESS;
    }

    // If calling vkAcquireNextImageKHR is necessary, do so first.
    if (mAcquireOperation.needToAcquireNextSwapchainImage)
    {
        TryAcquireNextImageUnlocked(context->getDevice(), mSwapchain, &mAcquireOperation);
    }

    // If the result of vkAcquireNextImageKHR is not yet processed, do so now.
    if (NeedToProcessAcquireNextImageResult(mAcquireOperation.unlockedTryAcquireResult))
    {
        return postProcessUnlockedTryAcquire(context);
    }

    return VK_SUCCESS;
}

VkResult WindowSurfaceVk::postProcessUnlockedTryAcquire(vk::Context *context)
{
    const VkResult result = mAcquireOperation.unlockedTryAcquireResult.result;
    const VkSemaphore acquireImageSemaphore =
        mAcquireOperation.unlockedTryAcquireResult.acquireSemaphore;
    mAcquireOperation.unlockedTryAcquireResult.acquireSemaphore = VK_NULL_HANDLE;

    // VK_SUBOPTIMAL_KHR is ok since we still have an Image that can be presented successfully
    if (ANGLE_UNLIKELY(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR))
    {
        // vkAcquireNextImageKHR still needs to be called after swapchain recreation:
        mAcquireOperation.needToAcquireNextSwapchainImage = true;
        return result;
    }

    mCurrentSwapchainImageIndex = mAcquireOperation.unlockedTryAcquireResult.imageIndex;
    ASSERT(!isSharedPresentMode() || mCurrentSwapchainImageIndex == 0);

    SwapchainImage &image = mSwapchainImages[mCurrentSwapchainImageIndex];

    // Let Image keep the ani semaphore so that it can add to the semaphore wait list if it is
    // being used. Image's barrier code will move the semaphore into CommandBufferHelper object
    // and then added to waitSemaphores when commands gets flushed and submitted. Since all
    // image use after ANI must go through barrier code, this approach is very robust. And since
    // this is tracked bny ImageHelper object, it also ensures it only added to command that
    // image is actually being referenced, thus avoid potential bugs.
    image.image->setAcquireNextImageSemaphore(acquireImageSemaphore);

    // Single Image Mode
    if (isSharedPresentMode())
    {
        ASSERT(image.image->valid() &&
               image.image->getCurrentImageLayout() != vk::ImageLayout::SharedPresent);
        rx::RendererVk *rendererVk = context->getRenderer();
        rx::vk::PrimaryCommandBuffer primaryCommandBuffer;
        auto protectionType = vk::ConvertProtectionBoolToType(mState.hasProtectedContent());
        if (rendererVk->getCommandBufferOneOff(context, protectionType, &primaryCommandBuffer) ==
            angle::Result::Continue)
        {
            VkSemaphore semaphore;
            // Note return errors is early exit may leave new Image and Swapchain in unknown state.
            image.image->recordWriteBarrierOneOff(context, vk::ImageLayout::SharedPresent,
                                                  &primaryCommandBuffer, &semaphore);
            ASSERT(semaphore == acquireImageSemaphore);
            if (primaryCommandBuffer.end() != VK_SUCCESS)
            {
                mDesiredSwapchainPresentMode = vk::PresentMode::FifoKHR;
                return VK_ERROR_OUT_OF_DATE_KHR;
            }
            QueueSerial queueSerial;
            if (rendererVk->queueSubmitOneOff(
                    context, std::move(primaryCommandBuffer), protectionType,
                    egl::ContextPriority::Medium, semaphore,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    vk::SubmitPolicy::EnsureSubmitted, &queueSerial) != angle::Result::Continue)
            {
                mDesiredSwapchainPresentMode = vk::PresentMode::FifoKHR;
                return VK_ERROR_OUT_OF_DATE_KHR;
            }
            mUse.setQueueSerial(queueSerial);
        }
    }

    // The semaphore will be waited on in the next flush.
    mAcquireOperation.unlockedTryAcquireData.acquireImageSemaphores.next();

    // Update RenderTarget pointers to this swapchain image if not multisampling.  Note: a possible
    // optimization is to defer the |vkAcquireNextImageKHR| call itself to |present()| if
    // multisampling, as the swapchain image is essentially unused until then.
    if (!mColorImageMS.valid())
    {
        mColorRenderTarget.updateSwapchainImage(image.image.get(), &image.imageViews, nullptr,
                                                nullptr);
    }

    // Notify the owning framebuffer there may be staged updates.
    if (image.image->hasStagedUpdatesInAllocatedLevels())
    {
        onStateChange(angle::SubjectMessage::SwapchainImageChanged);
    }

    ASSERT(!needsAcquireImageOrProcessResult());

    return VK_SUCCESS;
}

bool WindowSurfaceVk::needsAcquireImageOrProcessResult() const
{
    // Go down the acquireNextSwapchainImage() path if either vkAcquireNextImageKHR needs to be
    // called, or its results processed
    return mAcquireOperation.needToAcquireNextSwapchainImage ||
           NeedToProcessAcquireNextImageResult(mAcquireOperation.unlockedTryAcquireResult);
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
    // Don't let setSwapInterval change presentation mode if using SHARED present.
    if (isSharedPresentMode())
    {
        return;
    }

    const EGLint minSwapInterval = mState.config->minSwapInterval;
    const EGLint maxSwapInterval = mState.config->maxSwapInterval;
    ASSERT(minSwapInterval == 0 || minSwapInterval == 1);
    ASSERT(maxSwapInterval == 0 || maxSwapInterval == 1);

    interval = gl::clamp(interval, minSwapInterval, maxSwapInterval);

    mDesiredSwapchainPresentMode = GetDesiredPresentMode(mPresentModes, interval);

    // minImageCount may vary based on the Present Mode
    mMinImageCount = GetMinImageCount(mSurfaceCaps);

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
    return angle::ToEGL(result, EGL_BAD_SURFACE);
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
    return angle::ToEGL(result, EGL_BAD_SURFACE);
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

angle::Result WindowSurfaceVk::getCurrentFramebuffer(
    ContextVk *contextVk,
    FramebufferFetchMode fetchMode,
    const vk::RenderPass &compatibleRenderPass,
    const SwapchainResolveMode swapchainResolveMode,
    vk::MaybeImagelessFramebuffer *framebufferOut)
{
    // FramebufferVk dirty-bit processing should ensure that a new image was acquired.
    ASSERT(!needsAcquireImageOrProcessResult());

    // Track the new fetch mode
    mFramebufferFetchMode = fetchMode;

    vk::Framebuffer &currentFramebuffer = chooseFramebuffer(swapchainResolveMode);
    if (currentFramebuffer.valid())
    {
        // Validation layers should detect if the render pass is really compatible.
        framebufferOut->setHandle(currentFramebuffer.getHandle());
        return angle::Result::Continue;
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    uint32_t attachmentCount                = mDepthStencilImage.valid() ? 2u : 1u;

    const gl::Extents rotatedExtents      = mColorRenderTarget.getRotatedExtents();
    std::array<VkImageView, 3> imageViews = {};

    if (mDepthStencilImage.valid())
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(mDepthStencilRenderTarget.getImageView(contextVk, &imageView));
        imageViews[1] = imageView->getHandle();
    }

    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.flags           = 0;
    framebufferInfo.renderPass      = compatibleRenderPass.getHandle();
    framebufferInfo.attachmentCount = attachmentCount;
    framebufferInfo.pAttachments    = imageViews.data();
    framebufferInfo.width           = static_cast<uint32_t>(rotatedExtents.width);
    framebufferInfo.height          = static_cast<uint32_t>(rotatedExtents.height);
    framebufferInfo.layers          = 1;

    if (isMultiSampled())
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(mColorRenderTarget.getImageView(contextVk, &imageView));
        imageViews[0] = imageView->getHandle();

        if (swapchainResolveMode == SwapchainResolveMode::Enabled)
        {
            SwapchainImage &swapchainImage = mSwapchainImages[mCurrentSwapchainImageIndex];

            framebufferInfo.attachmentCount = attachmentCount + 1;

            ANGLE_TRY(swapchainImage.imageViews.getLevelLayerDrawImageView(
                contextVk, *swapchainImage.image, vk::LevelIndex(0), 0,
                gl::SrgbWriteControlMode::Default, &imageView));
            imageViews[attachmentCount] = imageView->getHandle();

            ANGLE_VK_TRY(contextVk, swapchainImage.framebufferResolveMS.init(contextVk->getDevice(),
                                                                             framebufferInfo));
        }
        else
        {
            // If multisampled, there is only a single color image and framebuffer.
            ANGLE_VK_TRY(contextVk, mFramebufferMS.init(contextVk->getDevice(), framebufferInfo));
        }
    }
    else
    {
        SwapchainImage &swapchainImage = mSwapchainImages[mCurrentSwapchainImageIndex];

        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(swapchainImage.imageViews.getLevelLayerDrawImageView(
            contextVk, *swapchainImage.image, vk::LevelIndex(0), 0,
            gl::SrgbWriteControlMode::Default, &imageView));

        imageViews[0] = imageView->getHandle();

        if (fetchMode == FramebufferFetchMode::Enabled)
        {
            ANGLE_VK_TRY(contextVk, swapchainImage.fetchFramebuffer.init(contextVk->getDevice(),
                                                                         framebufferInfo));
        }
        else
        {
            ANGLE_VK_TRY(contextVk,
                         swapchainImage.framebuffer.init(contextVk->getDevice(), framebufferInfo));
        }
    }

    ASSERT(currentFramebuffer.valid());
    framebufferOut->setHandle(currentFramebuffer.getHandle());
    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::initializeContents(const gl::Context *context,
                                                  GLenum binding,
                                                  const gl::ImageIndex &imageIndex)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (needsAcquireImageOrProcessResult())
    {
        // Acquire the next image (previously deferred).  Some tests (e.g.
        // GenerateMipmapWithRedefineBenchmark.Run/vulkan_webgl) cause this path to be taken,
        // because of dirty-object processing.
        ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, "Initialize Swap Image");
        ANGLE_TRY(doDeferredAcquireNextImage(context, false));
    }

    ASSERT(mSwapchainImages.size() > 0);
    ASSERT(mCurrentSwapchainImageIndex < mSwapchainImages.size());

    switch (binding)
    {
        case GL_BACK:
        {
            vk::ImageHelper *image =
                isMultiSampled() ? &mColorImageMS
                                 : mSwapchainImages[mCurrentSwapchainImageIndex].image.get();
            image->stageRobustResourceClear(imageIndex);
            ANGLE_TRY(image->flushAllStagedUpdates(contextVk));
            break;
        }
        case GL_DEPTH:
        case GL_STENCIL:
            ASSERT(mDepthStencilImage.valid());
            mDepthStencilImage.stageRobustResourceClear(gl::ImageIndex::Make2D(0));
            ANGLE_TRY(mDepthStencilImage.flushAllStagedUpdates(contextVk));
            break;
        default:
            UNREACHABLE();
            break;
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
            ->set(validationMessageCount);
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
        contextVk, *image->image, vk::LevelIndex(0), 0, gl::SrgbWriteControlMode::Default,
        &imageView));
    ANGLE_TRY(overlayVk->onPresent(contextVk, image->image.get(), imageView,
                                   Is90DegreeRotation(getPreTransform())));

    return angle::Result::Continue;
}

egl::Error WindowSurfaceVk::setAutoRefreshEnabled(bool enabled)
{
    if (enabled && !supportsPresentMode(vk::PresentMode::SharedContinuousRefreshKHR))
    {
        return egl::EglBadMatch();
    }

    vk::PresentMode newDesiredSwapchainPresentMode =
        enabled ? vk::PresentMode::SharedContinuousRefreshKHR
                : vk::PresentMode::SharedDemandRefreshKHR;
    // Auto refresh is only applicable in shared present mode
    if (isSharedPresentModeDesired() &&
        (mDesiredSwapchainPresentMode != newDesiredSwapchainPresentMode))
    {
        // In cases where the user switches to single buffer and have yet to call eglSwapBuffer,
        // enabling/disabling auto refresh should only change mDesiredSwapchainPresentMode as we
        // have not yet actually switched to single buffer mode.
        mDesiredSwapchainPresentMode = newDesiredSwapchainPresentMode;

        // If auto refresh is updated and we are already in single buffer mode we may need to
        // recreate swapchain. We need the deferAcquireNextImage() call as unlike setRenderBuffer(),
        // the user does not have to call eglSwapBuffers after setting the auto refresh attribute
        if (isSharedPresentMode() &&
            !IsCompatiblePresentMode(mDesiredSwapchainPresentMode, mCompatiblePresentModes.data(),
                                     mCompatiblePresentModes.size()))
        {
            deferAcquireNextImage();
        }
    }

    return egl::NoError();
}

egl::Error WindowSurfaceVk::getBufferAge(const gl::Context *context, EGLint *age)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "getBufferAge");

    if (needsAcquireImageOrProcessResult())
    {
        // Acquire the current image if needed.
        egl::Error result =
            angle::ToEGL(doDeferredAcquireNextImage(context, false), EGL_BAD_SURFACE);
        if (result.isError())
        {
            return result;
        }
    }

    if (isMultiSampled())
    {
        *age = 0;
        return egl::NoError();
    }

    if (mBufferAgeQueryFrameNumber == 0)
    {
        ANGLE_VK_PERF_WARNING(vk::GetImpl(context), GL_DEBUG_SEVERITY_LOW,
                              "Querying age of a surface will make it retain its content");

        mBufferAgeQueryFrameNumber = mFrameCount;
    }
    if (age != nullptr)
    {
        uint64_t frameNumber = mSwapchainImages[mCurrentSwapchainImageIndex].frameNumber;
        if (frameNumber < mBufferAgeQueryFrameNumber)
        {
            *age = 0;  // Has not been used for rendering yet or since age was queried, no age.
        }
        else
        {
            *age = static_cast<EGLint>(mFrameCount - frameNumber);
        }
    }
    return egl::NoError();
}

bool WindowSurfaceVk::supportsPresentMode(vk::PresentMode presentMode) const
{
    return (std::find(mPresentModes.begin(), mPresentModes.end(), presentMode) !=
            mPresentModes.end());
}

egl::Error WindowSurfaceVk::setRenderBuffer(EGLint renderBuffer)
{
    if (renderBuffer == EGL_SINGLE_BUFFER)
    {
        vk::PresentMode presentMode = mState.autoRefreshEnabled
                                          ? vk::PresentMode::SharedContinuousRefreshKHR
                                          : vk::PresentMode::SharedDemandRefreshKHR;
        if (!supportsPresentMode(presentMode))
        {
            return egl::EglBadMatch();
        }
        mDesiredSwapchainPresentMode = presentMode;
    }
    else  // EGL_BACK_BUFFER
    {
        mDesiredSwapchainPresentMode = vk::PresentMode::FifoKHR;
    }
    return egl::NoError();
}

egl::Error WindowSurfaceVk::lockSurface(const egl::Display *display,
                                        EGLint usageHint,
                                        bool preservePixels,
                                        uint8_t **bufferPtrOut,
                                        EGLint *bufferPitchOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::lockSurface");

    vk::ImageHelper *image = mSwapchainImages[mCurrentSwapchainImageIndex].image.get();
    if (!image->valid())
    {
        mAcquireOperation.needToAcquireNextSwapchainImage = true;
        if (acquireNextSwapchainImage(vk::GetImpl(display)) != VK_SUCCESS)
        {
            return egl::EglBadAccess();
        }
    }
    image = mSwapchainImages[mCurrentSwapchainImageIndex].image.get();
    ASSERT(image->valid());

    angle::Result result =
        LockSurfaceImpl(vk::GetImpl(display), image, mLockBufferHelper, getWidth(), getHeight(),
                        usageHint, preservePixels, bufferPtrOut, bufferPitchOut);
    return angle::ToEGL(result, EGL_BAD_ACCESS);
}

egl::Error WindowSurfaceVk::unlockSurface(const egl::Display *display, bool preservePixels)
{
    vk::ImageHelper *image = mSwapchainImages[mCurrentSwapchainImageIndex].image.get();
    ASSERT(image->valid());
    ASSERT(mLockBufferHelper.valid());

    return angle::ToEGL(UnlockSurfaceImpl(vk::GetImpl(display), image, mLockBufferHelper,
                                          getWidth(), getHeight(), preservePixels),
                        EGL_BAD_ACCESS);
}

EGLint WindowSurfaceVk::origin() const
{
    return EGL_UPPER_LEFT_KHR;
}

egl::Error WindowSurfaceVk::attachToFramebuffer(const gl::Context *context,
                                                gl::Framebuffer *framebuffer)
{
    FramebufferVk *framebufferVk = GetImplAs<FramebufferVk>(framebuffer);
    ASSERT(!framebufferVk->getBackbuffer());
    framebufferVk->setBackbuffer(this);
    return egl::NoError();
}

egl::Error WindowSurfaceVk::detachFromFramebuffer(const gl::Context *context,
                                                  gl::Framebuffer *framebuffer)
{
    FramebufferVk *framebufferVk = GetImplAs<FramebufferVk>(framebuffer);
    ASSERT(framebufferVk->getBackbuffer() == this);
    framebufferVk->setBackbuffer(nullptr);
    return egl::NoError();
}

}  // namespace rx
