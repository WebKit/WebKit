//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceVk.cpp:
//    Implements the class methods for SurfaceVk.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

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
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"
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

uint32_t GetMinImageCount(vk::Renderer *renderer,
                          const VkSurfaceCapabilitiesKHR &surfaceCaps,
                          vk::PresentMode presentMode)
{
    // - On mailbox, we need at least three images; one is being displayed to the user until the
    //   next v-sync, and the application alternatingly renders to the other two, one being
    //   recorded, and the other queued for presentation if v-sync happens in the meantime.
    // - On immediate, we need at least two images; the application alternates between the two
    //   images.
    // - On fifo, we use at least three images.  Triple-buffering allows us to present an image,
    //   have one in the queue, and record in another.  Note: on certain configurations (windows +
    //   nvidia + windowed mode), we could get away with a smaller number.

    // For simplicity, we always allocate at least three images, unless double buffer FIFO is
    // specifically preferred.
    const uint32_t imageCount =
        renderer->getFeatures().preferDoubleBufferSwapchainOnFifoMode.enabled &&
                presentMode == vk::PresentMode::FifoKHR
            ? 0x2u
            : 0x3u;

    uint32_t minImageCount = std::max(imageCount, surfaceCaps.minImageCount);
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

bool ColorNeedsInputAttachmentUsage(const angle::FeaturesVk &features)
{
    return features.supportsShaderFramebufferFetch.enabled ||
           features.supportsShaderFramebufferFetchNonCoherent.enabled ||
           features.emulateAdvancedBlendEquations.enabled;
}

bool DepthStencilNeedsInputAttachmentUsage(const angle::FeaturesVk &features)
{
    return features.supportsShaderFramebufferFetchDepthStencil.enabled;
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

    vk::Renderer *renderer = displayVk->getRenderer();
    // If shaders may be fetching from this, we need this image to be an input
    const bool isColorAndNeedsInputUsage =
        !isDepthOrStencilFormat && ColorNeedsInputAttachmentUsage(renderer->getFeatures());
    const bool isDepthStencilAndNeedsInputUsage =
        isDepthOrStencilFormat && DepthStencilNeedsInputAttachmentUsage(renderer->getFeatures());
    if (isColorAndNeedsInputUsage || isDepthStencilAndNeedsInputUsage)
    {
        usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }

    VkExtent3D extents = {std::max(static_cast<uint32_t>(width), 1u),
                          std::max(static_cast<uint32_t>(height), 1u), 1u};

    angle::FormatID renderableFormatId = vkFormat.getActualRenderableImageFormatID();
    // For devices that don't support creating swapchain images with RGB8, emulate with RGBA8.
    if (renderer->getFeatures().overrideSurfaceFormatRGB8ToRGBA8.enabled &&
        renderableFormatId == angle::FormatID::R8G8B8_UNORM)
    {
        renderableFormatId = angle::FormatID::R8G8B8A8_UNORM;
    }

    VkImageCreateFlags imageCreateFlags =
        hasProtectedContent ? VK_IMAGE_CREATE_PROTECTED_BIT : vk::kVkImageCreateFlagsNone;
    ANGLE_TRY(imageHelper->initExternal(
        displayVk, gl::TextureType::_2D, extents, vkFormat.getIntendedFormatID(),
        renderableFormatId, samples, usage, imageCreateFlags, vk::ImageLayout::Undefined, nullptr,
        gl::LevelIndex(0), 1, 1, isRobustResourceInitEnabled, hasProtectedContent,
        vk::YcbcrConversionDesc{}, nullptr));

    return angle::Result::Continue;
}

VkColorSpaceKHR MapEglColorSpaceToVkColorSpace(vk::Renderer *renderer, EGLenum EGLColorspace)
{
    switch (EGLColorspace)
    {
        case EGL_NONE:
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

angle::Result NewSemaphore(vk::ErrorContext *context,
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

void DestroyPresentHistory(vk::Renderer *renderer,
                           std::deque<impl::ImagePresentOperation> *presentHistory,
                           vk::Recycler<vk::Fence> *fenceRecycler,
                           vk::Recycler<vk::Semaphore> *semaphoreRecycler)
{
    VkDevice device = renderer->getDevice();
    for (impl::ImagePresentOperation &presentOperation : *presentHistory)
    {
        if (presentOperation.fence.valid())
        {
            (void)presentOperation.fence.wait(device, renderer->getMaxFenceWaitTimeNs());
        }
        presentOperation.destroy(device, fenceRecycler, semaphoreRecycler);
    }
    presentHistory->clear();
}

bool IsCompatiblePresentMode(vk::PresentMode mode,
                             VkPresentModeKHR *compatibleModes,
                             size_t compatibleModesCount)
{
    VkPresentModeKHR vkMode              = vk::ConvertPresentModeToVkPresentMode(mode);
    VkPresentModeKHR *compatibleModesEnd = compatibleModes + compatibleModesCount;
    return std::find(compatibleModes, compatibleModesEnd, vkMode) != compatibleModesEnd;
}

impl::SurfaceSizeState GetSizeState(const std::atomic<impl::SurfaceSizeState> &sizeState)
{
    return sizeState.load(std::memory_order_relaxed);
}

void SetSizeState(std::atomic<impl::SurfaceSizeState> *sizeState, impl::SurfaceSizeState value)
{
    sizeState->store(value, std::memory_order_relaxed);
}

// VK_SUBOPTIMAL_KHR is ok since we still have an Image that can be presented successfully
bool IsImageAcquireFailed(VkResult result)
{
    return ANGLE_UNLIKELY(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR);
}

// This function MUST only be called from a thread where Surface is current.
void AcquireNextImageUnlocked(VkDevice device,
                              VkSwapchainKHR swapchain,
                              impl::ImageAcquireOperation *acquire,
                              std::atomic<impl::SurfaceSizeState> *sizeState)
{
    ASSERT(acquire->state == impl::ImageAcquireState::Unacquired);
    ASSERT(*sizeState == impl::SurfaceSizeState::Unresolved);
    ASSERT(swapchain != VK_NULL_HANDLE);

    impl::UnlockedAcquireData *data     = &acquire->unlockedAcquireData;
    impl::UnlockedAcquireResult *result = &acquire->unlockedAcquireResult;

    result->imageIndex = std::numeric_limits<uint32_t>::max();

    // Get a semaphore to signal.
    result->acquireSemaphore = data->acquireImageSemaphores.front().getHandle();

    // Try to acquire an image.
    result->result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, result->acquireSemaphore,
                                           VK_NULL_HANDLE, &result->imageIndex);

    if (!IsImageAcquireFailed(result->result))
    {
        SetSizeState(sizeState, impl::SurfaceSizeState::Resolved);
    }

    // Result processing will be done later in the same thread.
    acquire->state = impl::ImageAcquireState::NeedToProcessResult;
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

SurfaceVk::SurfaceVk(const egl::SurfaceState &surfaceState)
    : SurfaceImpl(surfaceState),
      mWidth(mState.attributes.getAsInt(EGL_WIDTH, 0)),
      mHeight(mState.attributes.getAsInt(EGL_HEIGHT, 0))
{}

SurfaceVk::~SurfaceVk() {}

void SurfaceVk::destroy(const egl::Display *display)
{
    DisplayVk *displayVk   = vk::GetImpl(display);
    vk::Renderer *renderer = displayVk->getRenderer();

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

gl::Extents SurfaceVk::getSize() const
{
    return gl::Extents(mWidth, mHeight, 1);
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

    vk::Renderer *renderer      = displayVk->getRenderer();
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
    DisplayVk *displayVk   = vk::GetImpl(display);
    vk::Renderer *renderer = displayVk->getRenderer();
    // Front end must ensure all usage has been submitted.
    imageViews.release(renderer, image.getResourceUse());
    image.releaseImage(renderer);
    image.releaseStagedUpdates(renderer);
}

OffscreenSurfaceVk::OffscreenSurfaceVk(const egl::SurfaceState &surfaceState,
                                       vk::Renderer *renderer)
    : SurfaceVk(surfaceState), mColorAttachment(this), mDepthStencilAttachment(this)
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
    vk::Renderer *renderer    = displayVk->getRenderer();
    const egl::Config *config = mState.config;

    renderer->reloadVolkIfNeeded();

    GLint samples = GetSampleCount(mState.config);
    ANGLE_VK_CHECK(displayVk, samples > 0, VK_ERROR_INITIALIZATION_FAILED);

    bool robustInit = mState.isRobustResourceInitEnabled();

    EGLBoolean isLargestPbuffer =
        static_cast<EGLBoolean>(mState.attributes.get(EGL_LARGEST_PBUFFER, EGL_FALSE));
    if (isLargestPbuffer)
    {
        mWidth = std::min(mWidth, config->maxPBufferWidth);

        mHeight = std::min(mHeight, config->maxPBufferHeight);

        if (mWidth * mHeight > config->maxPBufferPixels)
        {
            mHeight = config->maxPBufferPixels / mWidth;
        }
    }

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

egl::Error OffscreenSurfaceVk::swap(const gl::Context *context, SurfaceSwapFeedback *feedback)
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
    return egl::Error(EGL_BAD_CURRENT_SURFACE);
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
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error OffscreenSurfaceVk::getMscRate(EGLint * /*numerator*/, EGLint * /*denominator*/)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

void OffscreenSurfaceVk::setSwapInterval(const egl::Display *display, EGLint /*interval*/) {}

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
        LockSurfaceImpl(vk::GetImpl(display), image, mLockBufferHelper, mWidth, mHeight, usageHint,
                        preservePixels, bufferPtrOut, bufferPitchOut);
    return angle::ToEGL(result, EGL_BAD_ACCESS);
}

egl::Error OffscreenSurfaceVk::unlockSurface(const egl::Display *display, bool preservePixels)
{
    vk::ImageHelper *image = &mColorAttachment.image;
    ASSERT(image->valid());
    ASSERT(mLockBufferHelper.valid());

    return angle::ToEGL(UnlockSurfaceImpl(vk::GetImpl(display), image, mLockBufferHelper, mWidth,
                                          mHeight, preservePixels),
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
      frameNumber(other.frameNumber)
{}
}  // namespace impl

using namespace impl;

WindowSurfaceVk::WindowSurfaceVk(const egl::SurfaceState &surfaceState, EGLNativeWindowType window)
    : SurfaceVk(surfaceState),
      mNativeWindowType(window),
      mSurface(VK_NULL_HANDLE),
      mSupportsProtectedSwapchain(false),
      mIsSurfaceSizedBySwapchain(false),
      mSizeState(SurfaceSizeState::InvalidSwapchain),
      mSwapchain(VK_NULL_HANDLE),
      mLastSwapchain(VK_NULL_HANDLE),
      mSwapchainPresentMode(vk::PresentMode::FifoKHR),
      mDesiredSwapchainPresentMode(vk::PresentMode::FifoKHR),
      mMinImageCount(0),
      mPreTransform(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR),
      mEmulatedPreTransform(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR),
      mCompositeAlpha(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR),
      mSurfaceColorSpace(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),
      mCurrentSwapchainImageIndex(0),
      mDepthStencilImageBinding(this, kAnySurfaceImageSubjectIndex),
      mColorImageMSBinding(this, kAnySurfaceImageSubjectIndex),
      mFrameCount(1),
      mPresentID(0),
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
    // Reserve enough room upfront to avoid storage re-allocation.
    mSwapchainImages.reserve(8);
    mSwapchainImageBindings.reserve(8);
}

WindowSurfaceVk::~WindowSurfaceVk()
{
    ASSERT(mSurface == VK_NULL_HANDLE);
    ASSERT(mSwapchain == VK_NULL_HANDLE);
    ASSERT(mLastSwapchain == VK_NULL_HANDLE);
}

void WindowSurfaceVk::destroy(const egl::Display *display)
{
    DisplayVk *displayVk   = vk::GetImpl(display);
    vk::Renderer *renderer = displayVk->getRenderer();
    VkDevice device        = renderer->getDevice();
    VkInstance instance    = renderer->getInstance();

    // flush the pipe.
    (void)finish(displayVk);

    if (mAcquireOperation.state == ImageAcquireState::Ready)
    {
        // swapchain image doesn't own ANI semaphore. Release ANI semaphore from image so that it
        // can destroy cleanly without hitting assertion..
        // Only single swapchain image may have semaphore associated.
        ASSERT(!mSwapchainImages.empty());
        ASSERT(mCurrentSwapchainImageIndex < mSwapchainImages.size());
        mSwapchainImages[mCurrentSwapchainImageIndex].image->resetAcquireNextImageSemaphore();
    }

    if (mLockBufferHelper.valid())
    {
        mLockBufferHelper.destroy(renderer);
    }

    DestroyPresentHistory(renderer, &mPresentHistory, &mPresentFenceRecycler,
                          &mPresentSemaphoreRecycler);

    destroySwapChainImages(displayVk);

    ASSERT(mSwapchain == mLastSwapchain || mSwapchain == VK_NULL_HANDLE);
    if (mLastSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, mLastSwapchain, nullptr);
        mSwapchain     = VK_NULL_HANDLE;
        mLastSwapchain = VK_NULL_HANDLE;
    }

    for (vk::Semaphore &semaphore : mAcquireOperation.unlockedAcquireData.acquireImageSemaphores)
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
    bool anyMatches      = false;
    angle::Result result = initializeImpl(displayVk, &anyMatches);
    if (result == angle::Result::Continue && !anyMatches)
    {
        return angle::ToEGL(angle::Result::Stop, EGL_BAD_MATCH);
    }
    return angle::ToEGL(result, EGL_BAD_SURFACE);
}

egl::Error WindowSurfaceVk::unMakeCurrent(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    angle::Result result = contextVk->onSurfaceUnMakeCurrent(this);

    return angle::ToEGL(result, EGL_BAD_CURRENT_SURFACE);
}

angle::FormatID WindowSurfaceVk::getIntendedFormatID(vk::Renderer *renderer)
{
    // Ensure that the format and colorspace pair is supported.
    const vk::Format &format = renderer->getFormat(mState.config->renderTargetFormat);
    return format.getIntendedFormatID();
}

angle::FormatID WindowSurfaceVk::getActualFormatID(vk::Renderer *renderer)
{
    // Ensure that the format and colorspace pair is supported.
    const vk::Format &format = renderer->getFormat(mState.config->renderTargetFormat);

    angle::FormatID actualFormatID   = format.getActualRenderableImageFormatID();
    angle::FormatID intendedFormatID = format.getIntendedFormatID();

    // For devices that don't support creating swapchain images with RGB8, emulate with RGBA8.
    if (renderer->getFeatures().overrideSurfaceFormatRGB8ToRGBA8.enabled &&
        intendedFormatID == angle::FormatID::R8G8B8_UNORM)
    {
        actualFormatID = angle::FormatID::R8G8B8A8_UNORM;
    }
    return actualFormatID;
}

bool WindowSurfaceVk::updateColorSpace(DisplayVk *displayVk)
{
    vk::Renderer *renderer = displayVk->getRenderer();

    VkFormat vkFormat = vk::GetVkFormatFromFormatID(renderer, getActualFormatID(renderer));

    EGLenum eglColorSpaceEnum =
        static_cast<EGLenum>(mState.attributes.get(EGL_GL_COLORSPACE, EGL_NONE));

    // If EGL did not specify color space, we will use VK_COLOR_SPACE_PASS_THROUGH_EXT if supported.
    if (eglColorSpaceEnum == EGL_NONE &&
        renderer->getFeatures().mapUnspecifiedColorSpaceToPassThrough.enabled &&
        displayVk->isSurfaceFormatColorspacePairSupported(mSurface, vkFormat,
                                                          VK_COLOR_SPACE_PASS_THROUGH_EXT))
    {
        mSurfaceColorSpace = VK_COLOR_SPACE_PASS_THROUGH_EXT;
        return true;
    }

    mSurfaceColorSpace = MapEglColorSpaceToVkColorSpace(renderer, eglColorSpaceEnum);
    return displayVk->isSurfaceFormatColorspacePairSupported(mSurface, vkFormat,
                                                             mSurfaceColorSpace);
}

angle::Result WindowSurfaceVk::initializeImpl(DisplayVk *displayVk, bool *anyMatchesOut)
{
    vk::Renderer *renderer = displayVk->getRenderer();

    mColorImageMSViews.init(renderer);
    mDepthStencilImageViews.init(renderer);

    renderer->reloadVolkIfNeeded();

    ANGLE_TRY(createSurfaceVk(displayVk));

    // Check if the selected queue created supports present to this surface.
    bool presentSupported = false;
    ANGLE_TRY(renderer->checkQueueForSurfacePresent(displayVk, mSurface, &presentSupported));
    if (!presentSupported)
    {
        return angle::Result::Continue;
    }

    const VkPhysicalDevice &physicalDevice = renderer->getPhysicalDevice();

    VkSurfaceCapabilitiesKHR surfaceCaps;

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

        surfaceCaps                 = surfaceCaps2.surfaceCapabilities;
        mSupportsProtectedSwapchain = surfaceProtectedCaps.supportsProtected;
    }
    else
    {
        ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface,
                                                                          &surfaceCaps));
    }

    if (IsAndroid())
    {
        mSupportsProtectedSwapchain = true;
    }

    ANGLE_VK_CHECK(displayVk, (mState.hasProtectedContent() ? mSupportsProtectedSwapchain : true),
                   VK_ERROR_FEATURE_NOT_PRESENT);

    ANGLE_VK_CHECK(displayVk,
                   (surfaceCaps.supportedUsageFlags & kSurfaceVkColorImageUsageFlags) ==
                       kSurfaceVkColorImageUsageFlags,
                   VK_ERROR_INITIALIZATION_FAILED);

    if (surfaceCaps.currentExtent.width == kSurfaceSizedBySwapchain)
    {
        ASSERT(surfaceCaps.currentExtent.height == kSurfaceSizedBySwapchain);
        ASSERT(!IsAndroid());

        mIsSurfaceSizedBySwapchain = true;
    }

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
    // ANGLE learns about the window's rotation from surfaceCaps.currentTransform.  If
    // currentTransform is non-IDENTITY, ANGLE must "pre-rotate" various aspects of its work
    // (e.g. rotate vertices in the vertex shaders, change scissor, viewport, and render-pass
    // renderArea).  The swapchain's transform is given the value of surfaceCaps.currentTransform.
    // That prevents SurfaceFlinger from doing a rotation blit for every frame (which is costly in
    // terms of performance and power).
    //
    // When a window is rotated 90 or 270 degrees, the aspect ratio changes.  The width and height
    // are swapped.  The x/y and width/height of various values in ANGLE must also be swapped
    // before communicating the values to Vulkan.

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

    ANGLE_TRY(GetPresentModes(displayVk, physicalDevice, mSurface, &mPresentModes));

    // Select appropriate present mode based on vsync parameter. Default to 1 (FIFO), though it
    // will get clamped to the min/max values specified at display creation time.
    setDesiredSwapInterval(mState.swapInterval);

    if (!updateColorSpace(displayVk))
    {
        return angle::Result::Continue;
    }

    // Android used to only advertise INHERIT bit, but might update to advertise OPAQUE bit as a
    // hint for RGBX backed VK_FORMAT_R8G8B8A8_* surface format. So here we would default to the
    // INHERTI bit if detecting Android and the client has explicitly requested alpha channel.
    mCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (IsAndroid() && mState.config->alphaSize != 0)
    {
        mCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }

    if ((surfaceCaps.supportedCompositeAlpha & mCompositeAlpha) == 0)
    {
        mCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    ANGLE_VK_CHECK(displayVk, (surfaceCaps.supportedCompositeAlpha & mCompositeAlpha) != 0,
                   VK_ERROR_INITIALIZATION_FAILED);

    // Single buffer, if supported
    if (mState.attributes.getAsInt(EGL_RENDER_BUFFER, EGL_BACK_BUFFER) == EGL_SINGLE_BUFFER)
    {
        if (supportsPresentMode(vk::PresentMode::SharedDemandRefreshKHR))
        {
            mSwapchainPresentMode = vk::PresentMode::SharedDemandRefreshKHR;
            setDesiredSwapchainPresentMode(vk::PresentMode::SharedDemandRefreshKHR);
        }
        else
        {
            WARN() << "Shared presentation mode requested, but not supported";
        }
    }

    mCompressionFlags    = VK_IMAGE_COMPRESSION_DISABLED_EXT;
    mFixedRateFlags      = 0;
    VkFormat imageFormat = vk::GetVkFormatFromFormatID(renderer, getActualFormatID(renderer));
    EGLenum surfaceCompressionRate = static_cast<EGLenum>(mState.attributes.get(
        EGL_SURFACE_COMPRESSION_EXT, EGL_SURFACE_COMPRESSION_FIXED_RATE_NONE_EXT));
    bool useFixedRateCompression =
        (surfaceCompressionRate != EGL_SURFACE_COMPRESSION_FIXED_RATE_NONE_EXT);
    bool fixedRateDefault =
        (surfaceCompressionRate == EGL_SURFACE_COMPRESSION_FIXED_RATE_DEFAULT_EXT);
    if (useFixedRateCompression)
    {
        ASSERT(renderer->getFeatures().supportsImageCompressionControl.enabled);
        ASSERT(renderer->getFeatures().supportsImageCompressionControlSwapchain.enabled);
        if (imageFormat == VK_FORMAT_R8G8B8A8_UNORM || imageFormat == VK_FORMAT_R8_UNORM ||
            imageFormat == VK_FORMAT_R5G6B5_UNORM_PACK16 ||
            imageFormat == VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16)
        {
            mCompressionFlags = fixedRateDefault ? VK_IMAGE_COMPRESSION_FIXED_RATE_DEFAULT_EXT
                                                 : VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT;
            mFixedRateFlags   = gl_vk::ConvertEGLFixedRateToVkFixedRate(surfaceCompressionRate,
                                                                        getActualFormatID(renderer));
        }
    }

    ANGLE_TRY(prepareSwapchainForAcquireNextImage(displayVk));
    ASSERT(mSwapchain != VK_NULL_HANDLE);

    // Create the semaphores that will be used for vkAcquireNextImageKHR.
    for (vk::Semaphore &semaphore : mAcquireOperation.unlockedAcquireData.acquireImageSemaphores)
    {
        ANGLE_VK_TRY(displayVk, semaphore.init(displayVk->getDevice()));
    }

    // Keep the image acquire deferred.  |mColorRenderTarget| will not be accessed until update in
    // the |acquireNextSwapchainImage| call.
    ASSERT(mAcquireOperation.state == ImageAcquireState::Unacquired);

    *anyMatchesOut = true;
    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::getAttachmentRenderTarget(const gl::Context *context,
                                                         GLenum binding,
                                                         const gl::ImageIndex &imageIndex,
                                                         GLsizei samples,
                                                         FramebufferAttachmentRenderTarget **rtOut)
{
    if (mAcquireOperation.state != ImageAcquireState::Ready)
    {
        // Acquire the next image (previously deferred) before it is drawn to or read from.
        ContextVk *contextVk = vk::GetImpl(context);
        ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, "First Swap Image Use");
        ANGLE_TRY(doDeferredAcquireNextImage(contextVk));
    }
    return SurfaceVk::getAttachmentRenderTarget(context, binding, imageIndex, samples, rtOut);
}

angle::Result WindowSurfaceVk::collectOldSwapchain(vk::ErrorContext *context,
                                                   VkSwapchainKHR swapchain)
{
    ASSERT(swapchain != VK_NULL_HANDLE);
    ASSERT(swapchain != mLastSwapchain);

    // If no present operation has been done on the new swapchain, it can be destroyed right away.
    // This means that a new swapchain was created, but before any of its images were presented,
    // it's asked to be recreated.  This can happen for example if vkQueuePresentKHR returns
    // OUT_OF_DATE, the swapchain is recreated and the following vkAcquireNextImageKHR again
    // returns OUT_OF_DATE.  Otherwise, keep the current swapchain as the old swapchain to be
    // scheduled for destruction.
    //
    // The old(er) swapchains still need to be kept to be scheduled for destruction.

    if (mPresentHistory.empty())
    {
        // Destroy the current (never-used) swapchain.
        vkDestroySwapchainKHR(context->getDevice(), swapchain, nullptr);
        return angle::Result::Continue;
    }

    // Place all present operation into mOldSwapchains. That gets scheduled for destruction when the
    // semaphore of the first image of the next swapchain can be recycled or when fences are
    // signaled (when VK_EXT_swapchain_maintenance1 is supported).
    SwapchainCleanupData cleanupData;

    // Schedule the swapchain for destruction.
    cleanupData.swapchain = swapchain;

    for (ImagePresentOperation &presentOperation : mPresentHistory)
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

    // Add new item now, before below calls that may fail.
    mOldSwapchains.emplace_back(std::move(cleanupData));

    // Try to cleanup old swapchains first, before checking the kMaxOldSwapchains limit.
    if (context->getFeatures().supportsSwapchainMaintenance1.enabled)
    {
        ANGLE_TRY(cleanUpOldSwapchains(context));
    }

    // If too many old swapchains have accumulated, wait idle and destroy them.  This is to prevent
    // failures due to too many swapchains allocated.
    //
    // Note: Nvidia has been observed to fail creation of swapchains after 20 are allocated on
    // desktop, or less than 10 on Quadro P400.
    static constexpr size_t kMaxOldSwapchains = 5;
    if (mOldSwapchains.size() > kMaxOldSwapchains)
    {
        ANGLE_TRY(finish(context));
        for (SwapchainCleanupData &oldSwapchain : mOldSwapchains)
        {
            oldSwapchain.waitFences(context->getDevice(),
                                    context->getRenderer()->getMaxFenceWaitTimeNs());
            oldSwapchain.destroy(context->getDevice(), &mPresentFenceRecycler,
                                 &mPresentSemaphoreRecycler);
        }
        mOldSwapchains.clear();
    }

    return angle::Result::Continue;
}

void WindowSurfaceVk::invalidateSwapchain(vk::Renderer *renderer)
{
    ASSERT(mSizeState != SurfaceSizeState::InvalidSwapchain);
    ASSERT(mSwapchain != VK_NULL_HANDLE);
    ASSERT(!mSwapchainImages[mCurrentSwapchainImageIndex]
                .image->getAcquireNextImageSemaphore()
                .valid());

    // Invalidate the current swapchain while keep the last handle to create the new swapchain.
    ASSERT(mSwapchain == mLastSwapchain);
    mSwapchain = VK_NULL_HANDLE;

    mAcquireOperation.state = ImageAcquireState::Unacquired;

    // Surface size is unresolved since new swapchain may have new size.
    setSizeState(SurfaceSizeState::InvalidSwapchain);

    releaseSwapchainImages(renderer);

    // Notify the parent classes of the surface's new state.
    onStateChange(angle::SubjectMessage::SurfaceChanged);
}

angle::Result WindowSurfaceVk::recreateSwapchain(vk::ErrorContext *context)
{
    // Swapchain must be already invalidated.
    ASSERT(mAcquireOperation.state == ImageAcquireState::Unacquired);
    ASSERT(mSwapchain == VK_NULL_HANDLE);

    // May happen in case if it is recreate after a previous failure.
    if (!mSwapchainImages.empty() || mDepthStencilImage.valid() || mColorImageMS.valid())
    {
        releaseSwapchainImages(context->getRenderer());
    }

    if (mLastSwapchain != VK_NULL_HANDLE)
    {
        // On Android, vkCreateSwapchainKHR may return VK_ERROR_NATIVE_WINDOW_IN_USE_KHR if use
        // mLastSwapchain as an oldSwapchain when in shared present mode.  Destroy the swapchain
        // now as a workaround.
        if (isSharedPresentMode() &&
            context->getFeatures().destroyOldSwapchainInSharedPresentMode.enabled)
        {
            ANGLE_TRY(finish(context));
            DestroyPresentHistory(context->getRenderer(), &mPresentHistory, &mPresentFenceRecycler,
                                  &mPresentSemaphoreRecycler);
            vkDestroySwapchainKHR(context->getDevice(), mLastSwapchain, nullptr);
            mLastSwapchain = VK_NULL_HANDLE;
        }
        // On Android, vkCreateSwapchainKHR destroys mLastSwapchain, which is incorrect.  Wait idle
        // in that case as a workaround.
        else if (context->getFeatures().waitIdleBeforeSwapchainRecreation.enabled)
        {
            ANGLE_TRY(finish(context));
        }
    }

    // Save the handle since it is going to be updated in the createSwapChain call below.
    VkSwapchainKHR oldSwapchain = mLastSwapchain;

    angle::Result result = createSwapChain(context);

    // oldSwapchain was retired in the createSwapChain call above and can be collected.
    if (oldSwapchain != VK_NULL_HANDLE && oldSwapchain != mLastSwapchain)
    {
        ANGLE_TRY(collectOldSwapchain(context, oldSwapchain));
    }

    return result;
}

void WindowSurfaceVk::createSwapchainImages(uint32_t imageCount)
{
    ASSERT(mSwapchainImages.empty());
    ASSERT(mSwapchainImageBindings.empty());

    // Because the observer binding class uses raw pointers we need to first ensure the entire image
    // vector is fully allocated before binding the subject and observer together.
    mSwapchainImages.resize(imageCount);
    mSwapchainImageBindings.resize(imageCount);

    for (uint32_t index = 0; index < imageCount; ++index)
    {
        mSwapchainImages[index].image  = std::make_unique<vk::ImageHelper>();
        mSwapchainImageBindings[index] = angle::ObserverBinding(this, kAnySurfaceImageSubjectIndex);
        mSwapchainImageBindings[index].bind(mSwapchainImages[index].image.get());
    }
}

angle::Result WindowSurfaceVk::createSwapChain(vk::ErrorContext *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::createSwapchain");

    ASSERT(mAcquireOperation.state == ImageAcquireState::Unacquired);
    ASSERT(mSizeState == SurfaceSizeState::InvalidSwapchain);
    ASSERT(mSwapchain == VK_NULL_HANDLE);

    vk::Renderer *renderer = context->getRenderer();
    VkDevice device        = renderer->getDevice();

    const angle::FormatID actualFormatID   = getActualFormatID(renderer);
    const angle::FormatID intendedFormatID = getIntendedFormatID(renderer);

    // Note: Vulkan doesn't allow 0-width/height swapchains and images.
    const gl::Extents nonZeroSurfaceExtents(std::max(mWidth, 1), std::max(mHeight, 1), 1);

    gl::Extents swapchainExtents = nonZeroSurfaceExtents;
    if (Is90DegreeRotation(getPreTransform()))
    {
        // The Surface is oriented such that its aspect ratio no longer matches that of the
        // device.  In this case, the width and height of the swapchain images must be swapped to
        // match the device's native orientation.  This must also be done for other attachments
        // used with the swapchain (e.g. depth buffer).  The width and height of the viewport,
        // scissor, and render-pass render area must also be swapped.  Then, when ANGLE rotates
        // gl_Position in the vertex shader, the rendering will look the same as if no
        // pre-rotation had been done.
        std::swap(swapchainExtents.width, swapchainExtents.height);
    }

    // We need transfer src for reading back from the backbuffer.
    VkImageUsageFlags imageUsageFlags = kSurfaceVkColorImageUsageFlags;

    // If shaders may be fetching from this, we need this image to be an input
    if (ColorNeedsInputAttachmentUsage(renderer->getFeatures()))
    {
        imageUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.flags = mState.hasProtectedContent() ? VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR : 0;
    swapchainInfo.surface = mSurface;
    swapchainInfo.minImageCount         = mMinImageCount;
    swapchainInfo.imageFormat           = vk::GetVkFormatFromFormatID(renderer, actualFormatID);
    swapchainInfo.imageColorSpace       = mSurfaceColorSpace;
    swapchainInfo.imageExtent.width     = swapchainExtents.width;
    swapchainInfo.imageExtent.height    = swapchainExtents.height;
    swapchainInfo.imageArrayLayers      = 1;
    swapchainInfo.imageUsage            = imageUsageFlags;
    swapchainInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices   = nullptr;
    swapchainInfo.preTransform          = mPreTransform;
    swapchainInfo.compositeAlpha        = mCompositeAlpha;
    swapchainInfo.presentMode  = vk::ConvertPresentModeToVkPresentMode(mSwapchainPresentMode);
    swapchainInfo.clipped      = VK_TRUE;
    swapchainInfo.oldSwapchain = mLastSwapchain;

    VkImageCompressionControlEXT compressionInfo = {};
    compressionInfo.sType                        = VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT;
    compressionInfo.flags                        = mCompressionFlags;
    compressionInfo.compressionControlPlaneCount = 1;
    compressionInfo.pFixedRateFlags              = &mFixedRateFlags;
    if (mCompressionFlags != VK_IMAGE_COMPRESSION_DISABLED_EXT)
    {
        vk::AddToPNextChain(&swapchainInfo, &compressionInfo);
    }

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

    if (renderer->getFeatures().supportsSwapchainMaintenance1.enabled)
    {
        swapchainInfo.flags |= VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_EXT;
    }

    ASSERT(!mCompatiblePresentModes.empty());
    VkSwapchainPresentModesCreateInfoEXT compatibleModesInfo = {};
    if (renderer->getFeatures().supportsSwapchainMaintenance1.enabled &&
        mCompatiblePresentModes.size() > 1)
    {
        compatibleModesInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT;
        compatibleModesInfo.presentModeCount =
            static_cast<uint32_t>(mCompatiblePresentModes.size());
        compatibleModesInfo.pPresentModes = mCompatiblePresentModes.data();

        vk::AddToPNextChain(&swapchainInfo, &compatibleModesInfo);
    }

    if (isSharedPresentMode())
    {
        swapchainInfo.minImageCount = 1;

        // This feature is by default disabled, and only affects Android platform wsi behavior
        // transparent to angle internal tracking for shared present.
        if (renderer->getFeatures().forceContinuousRefreshOnSharedPresent.enabled)
        {
            swapchainInfo.presentMode = VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
        }
    }

    // Old swapchain is retired regardless if the below call fails or not.
    mLastSwapchain = VK_NULL_HANDLE;

    // TODO: Once EGL_SWAP_BEHAVIOR_PRESERVED_BIT is supported, the contents of the old swapchain
    // need to carry over to the new one.  http://anglebug.com/42261637
    VkSwapchainKHR newSwapChain = VK_NULL_HANDLE;
    ANGLE_VK_TRY(context, vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &newSwapChain));
    mLastSwapchain = newSwapChain;

    // If frame timestamp was enabled for the surface, [re]enable it when [re]creating the swapchain
    if (renderer->getFeatures().supportsTimestampSurfaceAttribute.enabled &&
        mState.timestampsEnabled)
    {
        // The implementation of "vkGetPastPresentationTimingGOOGLE" on Android calls into the
        // appropriate ANativeWindow API that enables frame timestamps.
        uint32_t count = 0;
        ANGLE_VK_TRY(context,
                     vkGetPastPresentationTimingGOOGLE(device, newSwapChain, &count, nullptr));
    }

    // Initialize the swapchain image views.
    uint32_t imageCount = 0;
    ANGLE_VK_TRY(context, vkGetSwapchainImagesKHR(device, newSwapChain, &imageCount, nullptr));

    std::vector<VkImage> swapchainImages(imageCount);
    ANGLE_VK_TRY(context, vkGetSwapchainImagesKHR(device, newSwapChain, &imageCount,
                                                  swapchainImages.data()));

    // If multisampling is enabled, create a multisampled image which gets resolved just prior to
    // present.
    GLint samples = GetSampleCount(mState.config);
    ANGLE_VK_CHECK(context, samples > 0, VK_ERROR_INITIALIZATION_FAILED);

    VkExtent3D vkExtents;
    gl_vk::GetExtent(swapchainExtents, &vkExtents);

    bool robustInit = mState.isRobustResourceInitEnabled();

    if (samples > 1)
    {
        VkImageUsageFlags usage = kSurfaceVkColorImageUsageFlags;
        if (ColorNeedsInputAttachmentUsage(renderer->getFeatures()))
        {
            usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }

        // Create a multisampled image that will be rendered to, and then resolved to a swapchain
        // image.  The actual VkImage is created with rotated coordinates to make it easier to do
        // the resolve.  The ImageHelper::mExtents will have non-rotated extents in order to fit
        // with the rest of ANGLE, (e.g. which calculates the Vulkan scissor with non-rotated
        // values and then rotates the final rectangle).
        ANGLE_TRY(mColorImageMS.initMSAASwapchain(
            context, gl::TextureType::_2D, vkExtents, Is90DegreeRotation(getPreTransform()),
            intendedFormatID, actualFormatID, samples, usage, gl::LevelIndex(0), 1, 1, robustInit,
            mState.hasProtectedContent()));
        ANGLE_TRY(mColorImageMS.initMemoryAndNonZeroFillIfNeeded(
            context, mState.hasProtectedContent(), renderer->getMemoryProperties(),
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk::MemoryAllocationType::SwapchainMSAAImage));

        // Initialize the color render target with the multisampled targets.  If not multisampled,
        // the render target will be updated to refer to a swapchain image on every acquire.
        mColorRenderTarget.init(&mColorImageMS, &mColorImageMSViews, nullptr, nullptr, {},
                                gl::LevelIndex(0), 0, 1, RenderTargetTransience::Default);
    }

    createSwapchainImages(imageCount);

    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex)
    {
        SwapchainImage &member = mSwapchainImages[imageIndex];

        // Convert swapchain create flags to image create flags
        const VkImageCreateFlags createFlags =
            (swapchainInfo.flags & VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR) != 0
                ? VK_IMAGE_CREATE_PROTECTED_BIT
                : 0;

        ASSERT(member.image);
        member.image->init2DWeakReference(
            context, swapchainImages[imageIndex], nonZeroSurfaceExtents,
            Is90DegreeRotation(getPreTransform()), intendedFormatID, actualFormatID, createFlags,
            imageUsageFlags, 1, robustInit);
        member.imageViews.init(renderer);
        member.frameNumber = 0;
    }

    // Initialize depth/stencil if requested.
    if (mState.config->depthStencilFormat != GL_NONE)
    {
        const vk::Format &dsFormat = renderer->getFormat(mState.config->depthStencilFormat);

        VkImageUsageFlags dsUsage = kSurfaceVkDepthStencilImageUsageFlags;
        if (DepthStencilNeedsInputAttachmentUsage(renderer->getFeatures()))
        {
            dsUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }

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

    // Assign swapchain after all initialization is finished.
    mSwapchain = newSwapChain;

    // Swapchain is now valid, but size is still unresolved until acquire next image.
    setSizeState(SurfaceSizeState::Unresolved);

    context->getPerfCounters().swapchainCreate++;

    return angle::Result::Continue;
}

bool WindowSurfaceVk::isMultiSampled() const
{
    return mColorImageMS.valid();
}

angle::Result WindowSurfaceVk::queryAndAdjustSurfaceCaps(
    vk::ErrorContext *context,
    vk::PresentMode presentMode,
    VkSurfaceCapabilitiesKHR *surfaceCapsOut,
    CompatiblePresentModes *compatiblePresentModesOut) const
{
    // We must not query compatible present modes while swapchain is valid, but must query otherwise
    ASSERT((compatiblePresentModesOut == nullptr && mSwapchain != VK_NULL_HANDLE) ||
           (compatiblePresentModesOut != nullptr && mSwapchain == VK_NULL_HANDLE));

    vk::Renderer *renderer                 = context->getRenderer();
    const VkPhysicalDevice &physicalDevice = renderer->getPhysicalDevice();

    if (renderer->getFeatures().supportsSurfaceMaintenance1.enabled)
    {
        VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo2 = {};
        surfaceInfo2.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
        surfaceInfo2.surface = mSurface;

        VkSurfacePresentModeEXT surfacePresentMode = {};
        surfacePresentMode.sType                   = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT;
        surfacePresentMode.presentMode = vk::ConvertPresentModeToVkPresentMode(presentMode);
        vk::AddToPNextChain(&surfaceInfo2, &surfacePresentMode);

        VkSurfaceCapabilities2KHR surfaceCaps2 = {};
        surfaceCaps2.sType                     = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

        VkSurfacePresentModeCompatibilityEXT compatibleModes = {};
        if (compatiblePresentModesOut != nullptr)
        {
            // Skip the query if VK_EXT_swapchain_maintenance1 is not supported since compatible
            // modes can't be used.
            if (renderer->getFeatures().supportsSwapchainMaintenance1.enabled)
            {
                compatiblePresentModesOut->resize(kCompatiblePresentModesSize);

                compatibleModes.sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT;
                compatibleModes.presentModeCount = kCompatiblePresentModesSize;
                compatibleModes.pPresentModes    = compatiblePresentModesOut->data();
                vk::AddToPNextChain(&surfaceCaps2, &compatibleModes);
            }
            else
            {
                compatiblePresentModesOut->resize(1);
                (*compatiblePresentModesOut)[0] = surfacePresentMode.presentMode;
            }
        }

        ANGLE_VK_TRY(context, vkGetPhysicalDeviceSurfaceCapabilities2KHR(
                                  physicalDevice, &surfaceInfo2, &surfaceCaps2));

        if (compatibleModes.pPresentModes != nullptr)
        {
            // http://anglebug.com/368647924: in case of multiple drivers vulkan loader causes
            // extension to be listed when not actually supported. kCompatiblePresentModesSize is
            // above max count to catch this case and work around.
            if (compatibleModes.presentModeCount == kCompatiblePresentModesSize)
            {
                compatiblePresentModesOut->resize(1);
                (*compatiblePresentModesOut)[0] = surfacePresentMode.presentMode;
            }
            else
            {
                compatiblePresentModesOut->resize(compatibleModes.presentModeCount);

                // The implementation must always return the given present mode as compatible with
                // itself.
                ASSERT(IsCompatiblePresentMode(presentMode, compatiblePresentModesOut->data(),
                                               compatiblePresentModesOut->size()));

                // On Android we expect VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR and
                // VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR to be compatible.
                ASSERT(!IsAndroid() || !IsSharedPresentMode(presentMode) ||
                       IsCompatiblePresentMode(
                           presentMode == vk::PresentMode::SharedDemandRefreshKHR
                               ? vk::PresentMode::SharedContinuousRefreshKHR
                               : vk::PresentMode::SharedDemandRefreshKHR,
                           compatiblePresentModesOut->data(), compatiblePresentModesOut->size()));
            }
        }

        *surfaceCapsOut = surfaceCaps2.surfaceCapabilities;
    }
    else
    {
        ANGLE_VK_TRY(context, vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface,
                                                                        surfaceCapsOut));
        if (compatiblePresentModesOut != nullptr)
        {
            // Without VK_EXT_surface_maintenance1, each present mode can be considered only
            // compatible with itself.
            compatiblePresentModesOut->resize(1);
            (*compatiblePresentModesOut)[0] = vk::ConvertPresentModeToVkPresentMode(presentMode);
        }
    }

    if (mIsSurfaceSizedBySwapchain)
    {
        // vkGetPhysicalDeviceSurfaceCapabilitiesKHR does not provide useful extents for some
        // platforms (e.g. Fuchsia).  Therefore, we must query the window size via a
        // platform-specific mechanism.  Add those extents to the surfaceCapsOut
        gl::Extents windowExtents;
        ANGLE_TRY(getCurrentWindowSize(context, &windowExtents));
        surfaceCapsOut->currentExtent.width  = windowExtents.width;
        surfaceCapsOut->currentExtent.height = windowExtents.height;
    }

    adjustSurfaceExtent(&surfaceCapsOut->currentExtent);

    return angle::Result::Continue;
}

void WindowSurfaceVk::adjustSurfaceExtent(VkExtent2D *extent) const
{
    ASSERT(extent->width != kSurfaceSizedBySwapchain);
    ASSERT(extent->height != kSurfaceSizedBySwapchain);

    // When screen is physically rotated and prerotation is emulated, the window is rotated along
    // with it.  With real prerotation, the window preserves the upright orientation, by counter
    // rotating relative to the screen physical rotation.  In both cases, surface reports the window
    // sizes.  Because with emulated prerotation window is physically rotated, the surface will
    // also report rotated sizes (relative to the upright orientation).  Adjust the window extents
    // to match what real prerotation would have reported.
    if (Is90DegreeRotation(mEmulatedPreTransform))
    {
        ASSERT(mPreTransform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
        std::swap(extent->width, extent->height);
    }
}

angle::Result WindowSurfaceVk::prepareSwapchainForAcquireNextImage(vk::ErrorContext *context)
{
    ASSERT(mAcquireOperation.state == ImageAcquireState::Unacquired);
    ASSERT(mSizeState != SurfaceSizeState::Resolved);

    vk::Renderer *renderer = context->getRenderer();

    const bool isSwapchainValid = (mSwapchain != VK_NULL_HANDLE);
    ASSERT(!isSwapchainValid || !skipAcquireNextSwapchainImageForSharedPresentMode());

    // Get the latest surface capabilities.  Also update the compatible present modes if recreate
    // was probably caused by the incompatible desired present mode.  Note, that we must not update
    // compatible present modes while swapchain is still valid, but must do it otherwise.
    VkSurfaceCapabilitiesKHR surfaceCaps;
    ANGLE_TRY(queryAndAdjustSurfaceCaps(context, mSwapchainPresentMode, &surfaceCaps,
                                        isSwapchainValid ? nullptr : &mCompatiblePresentModes));

    const uint32_t minImageCount = GetMinImageCount(renderer, surfaceCaps, mSwapchainPresentMode);

    if (isSwapchainValid)
    {
        // This device generates neither VK_ERROR_OUT_OF_DATE_KHR nor VK_SUBOPTIMAL_KHR.  Check for
        // whether the size and/or rotation have changed since the swapchain was created.
        uint32_t curSurfaceWidth  = mWidth;
        uint32_t curSurfaceHeight = mHeight;

        // On Android, rotation can cause the minImageCount to change
        if (surfaceCaps.currentTransform == mPreTransform &&
            surfaceCaps.currentExtent.width == curSurfaceWidth &&
            surfaceCaps.currentExtent.height == curSurfaceHeight && minImageCount == mMinImageCount)
        {
            return angle::Result::Continue;
        }

        if (renderer->getFeatures().avoidInvisibleWindowSwapchainRecreate.enabled)
        {
            bool isWindowVisible = false;
            ANGLE_TRY(getWindowVisibility(context, &isWindowVisible));
            if (!isWindowVisible)
            {
                return angle::Result::Continue;
            }
        }

        invalidateSwapchain(renderer);
    }
    ASSERT(mSwapchain == VK_NULL_HANDLE);

    {
        // Lock protects individual |mWidth| and |mHeight| writes from this thread and reads from
        // other threads.  The acquire memory order of the mutex lock will prevent |mSizeState|
        // relaxed atomic assignment to be moved before the lock.  When other thread reads that
        // |mSizeState| is resolved, the mutex will be already locked, preventing reading old
        // |mWidth| and |mHeight| values.  Similar goal may be archived by using atomics instead of
        // the mutex.  The mutex is used for code simplicity and to avoid non relaxed atomic stores
        // on each frame.
        std::lock_guard<angle::SimpleMutex> lock(mSizeMutex);
        mWidth  = surfaceCaps.currentExtent.width;
        mHeight = surfaceCaps.currentExtent.height;
    }

    mMinImageCount = minImageCount;

    // Use the surface's transform.  For many platforms, this will always be identity (ANGLE does
    // not need to do any pre-rotation).  However, when surfaceCaps.currentTransform is not
    // identity, the device has been rotated away from its natural orientation.  In such a case,
    // ANGLE must rotate all rendering in order to avoid the compositor (e.g. SurfaceFlinger on
    // Android) performing an additional rotation blit.  In addition, ANGLE must create the
    // swapchain with VkSwapchainCreateInfoKHR::preTransform set to the value of
    // surfaceCaps.currentTransform.
    mPreTransform = surfaceCaps.currentTransform;

    return recreateSwapchain(context);
}

void WindowSurfaceVk::releaseSwapchainImages(vk::Renderer *renderer)
{
    ASSERT(mAcquireOperation.state == ImageAcquireState::Unacquired);
    ASSERT(mSwapchain == VK_NULL_HANDLE);

    // This is the last chance when resource uses may be merged.
    mergeImageResourceUses();

    mColorRenderTarget.releaseSwapchainImage();
    mDepthStencilRenderTarget.releaseSwapchainImage();

    if (mDepthStencilImage.valid())
    {
        ASSERT(!mDepthStencilImage.hasAnyRenderPassUsageFlags());
        mDepthStencilImageViews.release(renderer, mDepthStencilImage.getResourceUse());
        mDepthStencilImage.releaseImage(renderer);
        mDepthStencilImage.releaseStagedUpdates(renderer);
    }

    if (mColorImageMS.valid())
    {
        ASSERT(!mColorImageMS.hasAnyRenderPassUsageFlags());
        renderer->collectGarbage(mColorImageMS.getResourceUse(), &mFramebufferMS);
        mColorImageMSViews.release(renderer, mColorImageMS.getResourceUse());
        mColorImageMS.releaseImage(renderer);
        mColorImageMS.releaseStagedUpdates(renderer);
    }

    mSwapchainImageBindings.clear();

    for (SwapchainImage &swapchainImage : mSwapchainImages)
    {
        ASSERT(swapchainImage.image);
        ASSERT(!swapchainImage.image->hasAnyRenderPassUsageFlags());

        renderer->collectGarbage(swapchainImage.image->getResourceUse(),
                                 &swapchainImage.framebuffer);
        renderer->collectGarbage(swapchainImage.image->getResourceUse(),
                                 &swapchainImage.fetchFramebuffer);

        swapchainImage.imageViews.release(renderer, swapchainImage.image->getResourceUse());
        // swapchain image must not have ANI semaphore assigned here, since acquired image must be
        // presented before swapchain recreation.
        swapchainImage.image->resetImageWeakReference();
        swapchainImage.image->destroy(renderer);
    }

    mSwapchainImages.clear();
}

void WindowSurfaceVk::mergeImageResourceUses()
{
    mUse.merge(mDepthStencilImage.getResourceUse());
    mUse.merge(mColorImageMS.getResourceUse());
    for (SwapchainImage &swapchainImage : mSwapchainImages)
    {
        mUse.merge(swapchainImage.image->getResourceUse());
    }
}

angle::Result WindowSurfaceVk::finish(vk::ErrorContext *context)
{
    vk::Renderer *renderer = context->getRenderer();

    // Image acquire semaphores are tracked by the ResourceUse of the corresponding swapchain images
    // (waiting for image will also wait for the semaphore).  Present semaphores are tracked
    // explicitly after pre-present submission.
    mergeImageResourceUses();

    return renderer->finishResourceUse(context, mUse);
}

void WindowSurfaceVk::destroySwapChainImages(DisplayVk *displayVk)
{
    vk::Renderer *renderer = displayVk->getRenderer();
    VkDevice device        = displayVk->getDevice();

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
    }

    mSwapchainImages.clear();
}

egl::Error WindowSurfaceVk::prepareSwap(const gl::Context *context)
{
    // Image is only required to be acquired here in case of a blocking present modes (FIFO).
    // However, we will acquire the image in any case, for simplicity and possibly for performance.
    if (mAcquireOperation.state != ImageAcquireState::Unacquired)
    {
        return egl::NoError();
    }

    ContextVk *contextVk = vk::GetImpl(context);

    angle::Result result = prepareSwapchainForAcquireNextImage(contextVk);
    if (result != angle::Result::Continue)
    {
        return angle::ToEGL(result, EGL_BAD_SURFACE);
    }

    // |mColorRenderTarget| may be invalid at this point (in case of swapchain recreate above),
    // however it will not be accessed until update in the |acquireNextSwapchainImage| call.

    // Must check present mode after the above prepare (in case of swapchain recreate).
    ASSERT(mSwapchain != VK_NULL_HANDLE);
    ASSERT(!skipAcquireNextSwapchainImageForSharedPresentMode());

    // Call vkAcquireNextImageKHR without holding the share group and global locks.
    // The following are accessed by this function:
    //
    // - mAcquireOperation.state
    // - Contents of mAcquireOperation.unlockedAcquireData and
    //   mAcquireOperation.unlockedAcquireResult
    // - contextVk->getDevice(), which doesn't need external synchronization
    // - mSwapchain
    // - mSizeState, which is atomic
    //
    // All these members MUST only be accessed from a thread where Surface is current.
    // The |AcquireNextImageUnlocked| itself is also possible only from this thread, therefore there
    // is no need in synchronization between locked and unlocked calls.
    //
    // The result of this call is processed in doDeferredAcquireNextImage() by whoever ends up
    // calling it (likely the eglSwapBuffers call that follows)

    egl::Display::GetCurrentThreadUnlockedTailCall()->add(
        [device = contextVk->getDevice(), swapchain = mSwapchain, acquire = &mAcquireOperation,
         surfaceSizeState = &mSizeState](void *resultOut) {
            ANGLE_TRACE_EVENT0("gpu.angle", "Acquire Swap Image Before Swap");
            ANGLE_UNUSED_VARIABLE(resultOut);
            AcquireNextImageUnlocked(device, swapchain, acquire, surfaceSizeState);
        });

    return egl::NoError();
}

egl::Error WindowSurfaceVk::swapWithDamage(const gl::Context *context,
                                           const EGLint *rects,
                                           EGLint n_rects,
                                           SurfaceSwapFeedback *feedback)
{
    ContextVk *contextVk = vk::GetImpl(context);
    angle::Result result = swapImpl(contextVk, rects, n_rects, nullptr, feedback);
    if (result == angle::Result::Continue)
    {
        result = contextVk->onFramebufferBoundary(context);
    }

    return angle::ToEGL(result, EGL_BAD_SURFACE);
}

egl::Error WindowSurfaceVk::swap(const gl::Context *context, SurfaceSwapFeedback *feedback)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // When in shared present mode, eglSwapBuffers is unnecessary except for mode change.  When mode
    // change is not expected, the eglSwapBuffers call is forwarded to the context as a glFlush.
    // This allows the context to skip it if there's nothing to flush.  Otherwise control is bounced
    // back swapImpl().
    //
    // Some apps issue eglSwapBuffers after glFlush unnecessary, causing the CPU throttling logic to
    // effectively wait for the just submitted commands.
    if (isSharedPresentMode() && mSwapchainPresentMode == getDesiredSwapchainPresentMode())
    {
        const angle::Result result = contextVk->flush(context);
        return angle::ToEGL(result, EGL_BAD_SURFACE);
    }

    angle::Result result = swapImpl(contextVk, nullptr, 0, nullptr, feedback);
    if (result == angle::Result::Continue)
    {
        result = contextVk->onFramebufferBoundary(context);
    }
    return angle::ToEGL(result, EGL_BAD_SURFACE);
}

angle::Result WindowSurfaceVk::checkSwapchainOutOfDate(vk::ErrorContext *context,
                                                       VkResult presentResult)
{
    ASSERT(mAcquireOperation.state == ImageAcquireState::Unacquired ||
           (mAcquireOperation.state == ImageAcquireState::Ready &&
            skipAcquireNextSwapchainImageForSharedPresentMode()));
    ASSERT(mSwapchain != VK_NULL_HANDLE);

    bool presentOutOfDate = false;
    bool isFailure        = false;

    // If OUT_OF_DATE is returned, it's ok, we just need to recreate the swapchain before
    // continuing.  We do the same when VK_SUBOPTIMAL_KHR is returned to avoid visual degradation
    // (except when in shared present mode).
    switch (presentResult)
    {
        case VK_SUCCESS:
            break;
        case VK_SUBOPTIMAL_KHR:
            presentOutOfDate = !isSharedPresentMode();
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            presentOutOfDate = true;
            break;
        case VK_ERROR_SURFACE_LOST_KHR:
            // Handle SURFACE_LOST_KHR the same way as OUT_OF_DATE when in shared present mode,
            // because on some platforms (observed on Android) swapchain recreate still succeeds
            // making this error behave the same as OUT_OF_DATE.  In case of a real surface lost,
            // following swapchain recreate will also fail, effectively deferring the failure.
            if (isSharedPresentMode())
            {
                presentOutOfDate = true;
            }
            else
            {
                isFailure = true;
            }
            break;
        default:
            isFailure = true;
            break;
    }

    const vk::PresentMode desiredSwapchainPresentMode = getDesiredSwapchainPresentMode();

    // Invalidate the swapchain on failure to avoid repeated swapchain use and to be able to recover
    // from the error.
    if (presentOutOfDate || isFailure ||
        !IsCompatiblePresentMode(desiredSwapchainPresentMode, mCompatiblePresentModes.data(),
                                 mCompatiblePresentModes.size()))
    {
        invalidateSwapchain(context->getRenderer());
        mSwapchainPresentMode = desiredSwapchainPresentMode;
        if (isFailure)
        {
            ANGLE_VK_TRY(context, presentResult);
            UNREACHABLE();
        }
    }

    ASSERT(!isFailure);
    return angle::Result::Continue;
}

vk::Framebuffer &WindowSurfaceVk::chooseFramebuffer()
{
    if (isMultiSampled())
    {
        return mFramebufferMS;
    }

    // Choose which framebuffer to use based on fetch, so it will have a matching renderpass
    return mFramebufferFetchMode == vk::FramebufferFetchMode::Color
               ? mSwapchainImages[mCurrentSwapchainImageIndex].fetchFramebuffer
               : mSwapchainImages[mCurrentSwapchainImageIndex].framebuffer;
}

angle::Result WindowSurfaceVk::prePresentSubmit(ContextVk *contextVk,
                                                const vk::Semaphore &presentSemaphore)
{
    vk::Renderer *renderer = contextVk->getRenderer();

    SwapchainImage &image = mSwapchainImages[mCurrentSwapchainImageIndex];

    bool imageResolved = false;
    // Make sure deferred clears are applied, if any.
    if (mColorImageMS.valid())
    {
        ASSERT(mColorImageMS.areStagedUpdatesClearOnly());
        // http://anglebug.com/382006939
        // If app calls:
        //     glClear(GL_COLOR_BUFFER_BIT);
        //     eglSwapBuffers();
        // As an optimization, deferred clear could skip msaa buffer and applied to back buffer
        // directly instead of clearing msaa buffer and then resolve.
        // The exception is that when we back buffer data has to be preserved under
        // certain situations, we must also ensure msaa buffer contains the right content.
        // Under that situation, this optimization will not apply.

        if (!isSharedPresentMode() &&
            (mState.swapBehavior == EGL_BUFFER_DESTROYED && mBufferAgeQueryFrameNumber == 0))
        {
            vk::ClearValuesArray deferredClearValues;
            ANGLE_TRY(mColorImageMS.flushSingleSubresourceStagedUpdates(
                contextVk, gl::LevelIndex(0), 0, 1, &deferredClearValues, 0));
            if (deferredClearValues.any())
            {
                // Apply clear color directly to the single sampled image if the EGL surface is
                // double buffered and when EGL_SWAP_BEHAVIOR is EGL_BUFFER_DESTROYED
                gl::ImageIndex imageIndex = gl::ImageIndex::Make2D(gl::LevelIndex(0).get());
                image.image->stageClear(imageIndex, VK_IMAGE_ASPECT_COLOR_BIT,
                                        deferredClearValues[0]);
                ANGLE_TRY(image.image->flushStagedUpdates(contextVk, gl::LevelIndex(0),
                                                          gl::LevelIndex(1), 0, 1, {}));
                imageResolved = true;
            }
        }
        else
        {
            // Apply clear value to multisampled mColorImageMS and then resolve to single sampled
            // image later if EGL surface is single buffered or when EGL_SWAP_BEHAVIOR is
            // EGL_BUFFER_PRESERVED
            ANGLE_TRY(mColorImageMS.flushStagedUpdates(contextVk, gl::LevelIndex(0),
                                                       gl::LevelIndex(1), 0, 1, {}));
        }
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
    if (contextVk->hasStartedRenderPassWithDefaultFramebuffer())
    {
        // If image is resolved above, render pass is necessary closed.
        ASSERT(!imageResolved);

        ANGLE_TRY(contextVk->optimizeRenderPassForPresent(&image.imageViews, image.image.get(),
                                                          &mColorImageMS, isSharedPresentMode(),
                                                          &imageResolved));
    }

    if (mColorImageMS.valid() && !imageResolved)
    {
        // Transition the multisampled image to TRANSFER_SRC for resolve.
        vk::CommandBufferAccess access;
        access.onImageTransferRead(VK_IMAGE_ASPECT_COLOR_BIT, &mColorImageMS);
        access.onImageTransferWrite(gl::LevelIndex(0), 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                    image.image.get());

        vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
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

    // The overlay is drawn after this.  This ensures that drawing the overlay does not interfere
    // with other functionality, especially counters used to validate said functionality.
    const bool shouldDrawOverlay = overlayHasEnabledWidget(contextVk);

    if (!shouldDrawOverlay)
    {
        ANGLE_TRY(recordPresentLayoutBarrierIfNecessary(contextVk));
    }

    ANGLE_TRY(contextVk->flushAndSubmitCommands(shouldDrawOverlay ? nullptr : &presentSemaphore,
                                                nullptr, RenderPassClosureReason::EGLSwapBuffers));

    if (shouldDrawOverlay)
    {
        updateOverlay(contextVk);
        ANGLE_TRY(drawOverlay(contextVk, &image));

        ANGLE_TRY(recordPresentLayoutBarrierIfNecessary(contextVk));

        ANGLE_TRY(contextVk->flushAndSubmitCommands(
            &presentSemaphore, nullptr, RenderPassClosureReason::AlreadySpecifiedElsewhere));
    }

    ASSERT(image.image->getCurrentImageLayout() ==
           (isSharedPresentMode() ? vk::ImageLayout::SharedPresent : vk::ImageLayout::Present));

    // This is to track |presentSemaphore| submission.
    mUse.setQueueSerial(contextVk->getLastSubmittedQueueSerial());

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::recordPresentLayoutBarrierIfNecessary(ContextVk *contextVk)
{
    if (!contextVk->getFeatures().supportsPresentation.enabled || isSharedPresentMode())
    {
        return angle::Result::Continue;
    }
    vk::ImageHelper *image = mSwapchainImages[mCurrentSwapchainImageIndex].image.get();

    // Note that renderpass will be automatically closed in case of outside renderpass resolve.
    if (contextVk->hasStartedRenderPassWithDefaultFramebuffer())
    {
        // When we have a renderpass with default framebuffer it must be optimized for present.
        ASSERT(contextVk->getStartedRenderPassCommands().isImageOptimizedForPresent(image));
        return angle::Result::Continue;
    }

    // Image may be already in Present layout if swap without any draw.
    if (image->getCurrentImageLayout() != vk::ImageLayout::Present)
    {
        vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
        ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper({}, &commandBufferHelper));

        image->recordReadBarrier(contextVk, VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::Present,
                                 commandBufferHelper);
        commandBufferHelper->retainImage(image);
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::present(ContextVk *contextVk,
                                       const EGLint *rects,
                                       EGLint n_rects,
                                       const void *pNextChain,
                                       SurfaceSwapFeedback *feedback)
{
    ASSERT(mAcquireOperation.state == ImageAcquireState::Ready);
    ASSERT(mSizeState == SurfaceSizeState::Resolved);
    ASSERT(mSwapchain != VK_NULL_HANDLE);

    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::present");
    vk::Renderer *renderer = contextVk->getRenderer();

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
        EGLint width  = mWidth;
        EGLint height = mHeight;

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
        vk::PresentMode desiredSwapchainPresentMode = getDesiredSwapchainPresentMode();
        if (mSwapchainPresentMode != desiredSwapchainPresentMode &&
            IsCompatiblePresentMode(desiredSwapchainPresentMode, mCompatiblePresentModes.data(),
                                    mCompatiblePresentModes.size()))
        {
            presentMode = vk::ConvertPresentModeToVkPresentMode(desiredSwapchainPresentMode);

            presentModeInfo.sType          = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT;
            presentModeInfo.swapchainCount = 1;
            presentModeInfo.pPresentModes  = &presentMode;

            vk::AddToPNextChain(&presentInfo, &presentModeInfo);

            mSwapchainPresentMode = desiredSwapchainPresentMode;
        }
    }

    // The ANI semaphore must have been submitted and waited.
    ASSERT(!mSwapchainImages[mCurrentSwapchainImageIndex]
                .image->getAcquireNextImageSemaphore()
                .valid());

    // EGL_ANDROID_presentation_time: set the desired presentation time for the frame.
    VkPresentTimesInfoGOOGLE presentTimesInfo = {};
    VkPresentTimeGOOGLE presentTime           = {};
    if (mDesiredPresentTime.has_value())
    {
        ASSERT(contextVk->getFeatures().supportsTimestampSurfaceAttribute.enabled);
        presentTime.presentID          = mPresentID++;
        presentTime.desiredPresentTime = mDesiredPresentTime.value();
        mDesiredPresentTime.reset();

        presentTimesInfo.sType          = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE;
        presentTimesInfo.swapchainCount = 1;
        presentTimesInfo.pTimes         = &presentTime;

        vk::AddToPNextChain(&presentInfo, &presentTimesInfo);
    }

    VkResult presentResult =
        renderer->queuePresent(contextVk, contextVk->getPriority(), presentInfo);

    // EGL_EXT_buffer_age
    // 4) What is the buffer age of a single buffered surface?
    //     RESOLVED: 0.  This falls out implicitly from the buffer age
    //     calculations, which dictate that a buffer's age starts at 0,
    //     and is only incremented by frame boundaries.  Since frame
    //     boundary functions do not affect single buffered surfaces,
    //     their age will always be 0.
    if (!isSharedPresentMode())
    {
        // Set FrameNumber for the presented image.
        mSwapchainImages[mCurrentSwapchainImageIndex].frameNumber = mFrameCount++;
        // Always defer acquiring the next swapchain image, except when in shared present mode.
        // Note, if desired present mode is not compatible with the current mode or present is
        // out-of-date, swapchain will be invalidated in |checkSwapchainOutOfDate| call below.
        deferAcquireNextImage();
        // Tell front end that swapChain image changed so that it could dirty default framebuffer.
        ASSERT(feedback != nullptr);
        feedback->swapChainImageChanged = true;
    }

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

    // Check for out of date swapchain.  Note, possible swapchain invalidate will also defer ANI.
    ANGLE_TRY(checkSwapchainOutOfDate(contextVk, presentResult));

    // Now apply CPU throttle if needed
    ANGLE_TRY(throttleCPU(contextVk, swapSerial));

    contextVk->resetPerFramePerfCounters();

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::throttleCPU(vk::ErrorContext *context,
                                           const QueueSerial &currentSubmitSerial)
{
    // Wait on the oldest serial and replace it with the newest as the circular buffer moves
    // forward.
    QueueSerial swapSerial = mSwapHistory.front();
    mSwapHistory.front()   = currentSubmitSerial;
    mSwapHistory.next();

    if (swapSerial.valid() && !context->getRenderer()->hasQueueSerialFinished(swapSerial))
    {
        // Make this call after unlocking the EGL lock.  Renderer::finishQueueSerial is necessarily
        // thread-safe because it can get called from any number of GL commands, which don't
        // necessarily hold the EGL lock.
        //
        // As this is an unlocked tail call, it must not access anything else in Renderer.  The
        // display passed to |finishQueueSerial| is a |vk::ErrorContext|, and the only possible
        // modification to it is through |handleError()|.
        egl::Display::GetCurrentThreadUnlockedTailCall()->add(
            [context, swapSerial](void *resultOut) {
                ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::throttleCPU");
                ANGLE_UNUSED_VARIABLE(resultOut);
                (void)context->getRenderer()->finishQueueSerial(context, swapSerial);
            });
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::cleanUpPresentHistory(vk::ErrorContext *context)
{
    const VkDevice device = context->getDevice();

    while (!mPresentHistory.empty())
    {
        ImagePresentOperation &presentOperation = mPresentHistory.front();

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
        ImagePresentOperation presentOperation = std::move(mPresentHistory.front());
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

angle::Result WindowSurfaceVk::cleanUpOldSwapchains(vk::ErrorContext *context)
{
    const VkDevice device = context->getDevice();

    ASSERT(context->getFeatures().supportsSwapchainMaintenance1.enabled);

    while (!mOldSwapchains.empty())
    {
        SwapchainCleanupData &oldSwapchain = mOldSwapchains.front();
        VkResult result                    = oldSwapchain.getFencesStatus(device);
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

angle::Result WindowSurfaceVk::swapImpl(ContextVk *contextVk,
                                        const EGLint *rects,
                                        EGLint n_rects,
                                        const void *pNextChain,
                                        SurfaceSwapFeedback *feedback)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::swapImpl");

    // prepareSwap() has already called vkAcquireNextImageKHR if necessary, but its results need to
    // be processed now if not already.  doDeferredAcquireNextImage() will
    // automatically skip the prepareSwapchainForAcquireNextImage() and vkAcquireNextImageKHR calls
    // in that case.  The swapchain recreation path in doDeferredAcquireNextImage() is acceptable
    // because it only happens if previous vkAcquireNextImageKHR failed.
    // Note: this method may be called from |onSharedPresentContextFlush|, therefore can't assume
    // that image is always acquired at this point.
    if (mAcquireOperation.state != ImageAcquireState::Ready)
    {
        ANGLE_TRY(doDeferredAcquireNextImage(contextVk));
    }

    ANGLE_TRY(present(contextVk, rects, n_rects, pNextChain, feedback));

    // |mColorRenderTarget| may be invalid at this point (in case of swapchain recreate above),
    // however it will not be accessed until update in the |acquireNextSwapchainImage| call.
    ASSERT(mAcquireOperation.state == ImageAcquireState::Unacquired ||
           (mAcquireOperation.state == ImageAcquireState::Ready &&
            skipAcquireNextSwapchainImageForSharedPresentMode()));

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::onSharedPresentContextFlush(ContextVk *contextVk)
{
    return swapImpl(contextVk, nullptr, 0, nullptr, nullptr);
}

bool WindowSurfaceVk::hasStagedUpdates() const
{
    return mAcquireOperation.state == ImageAcquireState::Ready &&
           mColorRenderTarget.getImageForRenderPass().hasStagedUpdatesInAllocatedLevels();
}

void WindowSurfaceVk::setTimestampsEnabled(bool enabled)
{
    // The frontend has already cached the state, nothing to do.
    ASSERT(IsAndroid());
}

egl::Error WindowSurfaceVk::setPresentationTime(EGLnsecsANDROID time)
{
    mDesiredPresentTime = time;
    return egl::NoError();
}

void WindowSurfaceVk::deferAcquireNextImage()
{
    ASSERT(mAcquireOperation.state == ImageAcquireState::Ready);
    ASSERT(mSizeState == SurfaceSizeState::Resolved);
    ASSERT(mSwapchain != VK_NULL_HANDLE);
    ASSERT(!mSwapchainImages[mCurrentSwapchainImageIndex]
                .image->getAcquireNextImageSemaphore()
                .valid());
    ASSERT(!isSharedPresentMode());

    mAcquireOperation.state = ImageAcquireState::Unacquired;

    // Swapchain may be recreated in prepareSwapchainForAcquireNextImage() call.
    setSizeState(SurfaceSizeState::Unresolved);
}

angle::Result WindowSurfaceVk::doDeferredAcquireNextImage(vk::ErrorContext *context)
{
    ASSERT(mAcquireOperation.state != ImageAcquireState::Ready);
    // prepareSwapchainForAcquireNextImage() may recreate Swapchain even if there is an image
    // acquired. Avoid this, by skipping the prepare call.
    if (mAcquireOperation.state == ImageAcquireState::Unacquired)
    {
        ANGLE_TRY(prepareSwapchainForAcquireNextImage(context));
    }
    ASSERT(mSwapchain != VK_NULL_HANDLE);

    VkResult result = VK_ERROR_UNKNOWN;

    constexpr uint32_t kMaxAttempts = 2;
    for (uint32_t attempt = 1; attempt <= kMaxAttempts; ++attempt)
    {
        // Get the next available swapchain image.
        result = acquireNextSwapchainImage(context);
        if (result == VK_SUCCESS)
        {
            break;
        }

        // Always invalidate the swapchain in case of the failure.
        invalidateSwapchain(context->getRenderer());

        ASSERT(result != VK_SUBOPTIMAL_KHR);
        // If OUT_OF_DATE is returned, it's ok, we just need to recreate the swapchain before
        // continuing.
        if (ANGLE_UNLIKELY(result != VK_ERROR_OUT_OF_DATE_KHR))
        {
            break;
        }

        // Do not recreate the swapchain if it's the last attempt.
        if (attempt < kMaxAttempts)
        {
            ANGLE_TRY(prepareSwapchainForAcquireNextImage(context));
        }
    }

    ANGLE_VK_TRY(context, result);

    return angle::Result::Continue;
}

bool WindowSurfaceVk::skipAcquireNextSwapchainImageForSharedPresentMode() const
{
    if (isSharedPresentMode())
    {
        ASSERT(mSwapchainImages.size());
        const SwapchainImage &image = mSwapchainImages[0];
        ASSERT(image.image->valid());
        if (image.image->getCurrentImageLayout() == vk::ImageLayout::SharedPresent)
        {
            return true;
        }
    }

    return false;
}

// This method will either return VK_SUCCESS or VK_ERROR_*.  Thus, it is appropriate to ASSERT that
// the return value won't be VK_SUBOPTIMAL_KHR.
VkResult WindowSurfaceVk::acquireNextSwapchainImage(vk::ErrorContext *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "acquireNextSwapchainImage");
    ASSERT(mAcquireOperation.state != ImageAcquireState::Ready);
    ASSERT(mSwapchain != VK_NULL_HANDLE);
    ASSERT(!skipAcquireNextSwapchainImageForSharedPresentMode());

    vk::Renderer *renderer = context->getRenderer();
    VkDevice device        = renderer->getDevice();

    // If calling vkAcquireNextImageKHR is necessary, do so first.
    if (mAcquireOperation.state == ImageAcquireState::Unacquired)
    {
        AcquireNextImageUnlocked(device, mSwapchain, &mAcquireOperation, &mSizeState);
    }

    // After the above call result is always ready for processing.
    ASSERT(mAcquireOperation.state == ImageAcquireState::NeedToProcessResult);

    const VkResult result = mAcquireOperation.unlockedAcquireResult.result;

    if (IsImageAcquireFailed(result))
    {
        ASSERT(mSizeState == SurfaceSizeState::Unresolved);
        return result;
    }
    ASSERT(mSizeState == SurfaceSizeState::Resolved);

    mCurrentSwapchainImageIndex = mAcquireOperation.unlockedAcquireResult.imageIndex;
    ASSERT(!isSharedPresentMode() || mCurrentSwapchainImageIndex == 0);

    SwapchainImage &image = mSwapchainImages[mCurrentSwapchainImageIndex];

    const VkSemaphore acquireImageSemaphore =
        mAcquireOperation.unlockedAcquireResult.acquireSemaphore;

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
        vk::ScopedPrimaryCommandBuffer scopedCommandBuffer(device);
        auto protectionType = vk::ConvertProtectionBoolToType(mState.hasProtectedContent());
        if (renderer->getCommandBufferOneOff(context, protectionType, &scopedCommandBuffer) ==
            angle::Result::Continue)
        {
            vk::PrimaryCommandBuffer &primaryCommandBuffer = scopedCommandBuffer.get();
            VkSemaphore semaphore;
            // Note return errors is early exit may leave new Image and Swapchain in unknown state.
            image.image->recordWriteBarrierOneOff(renderer, vk::ImageLayout::SharedPresent,
                                                  &primaryCommandBuffer, &semaphore);
            ASSERT(semaphore == acquireImageSemaphore);
            if (primaryCommandBuffer.end() != VK_SUCCESS)
            {
                setDesiredSwapInterval(mState.swapInterval);
                return VK_ERROR_OUT_OF_DATE_KHR;
            }
            QueueSerial queueSerial;
            if (renderer->queueSubmitOneOff(context, std::move(scopedCommandBuffer), protectionType,
                                            egl::ContextPriority::Medium, semaphore,
                                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            &queueSerial) != angle::Result::Continue)
            {
                setDesiredSwapInterval(mState.swapInterval);
                return VK_ERROR_OUT_OF_DATE_KHR;
            }
            image.image->setQueueSerial(queueSerial);
        }
    }

    // Note, please add new code that may fail before this comment.

    // The semaphore will be waited on in the next flush.
    mAcquireOperation.unlockedAcquireData.acquireImageSemaphores.next();

    // Update RenderTarget pointers to this swapchain image if not multisampling.  Note: a possible
    // optimization is to defer the |vkAcquireNextImageKHR| call itself to |present()| if
    // multisampling, as the swapchain image is essentially unused until then.
    if (!mColorImageMS.valid())
    {
        mColorRenderTarget.updateSwapchainImage(image.image.get(), &image.imageViews, nullptr,
                                                nullptr);
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
            image.image->invalidateEntireLevelContent(context, gl::LevelIndex(0));
            if (mColorImageMS.valid())
            {
                mColorImageMS.invalidateEntireLevelContent(context, gl::LevelIndex(0));
            }
        }
        if (mDepthStencilImage.valid())
        {
            mDepthStencilImage.invalidateEntireLevelContent(context, gl::LevelIndex(0));
            mDepthStencilImage.invalidateEntireLevelStencilContent(context, gl::LevelIndex(0));
        }
    }

    // Note that an acquire and result processing is no longer needed.
    mAcquireOperation.state = ImageAcquireState::Ready;

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
    return egl::Error(EGL_BAD_CURRENT_SURFACE);
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
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error WindowSurfaceVk::getMscRate(EGLint * /*numerator*/, EGLint * /*denominator*/)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

vk::PresentMode WindowSurfaceVk::getDesiredSwapchainPresentMode() const
{
    return mDesiredSwapchainPresentMode.load(std::memory_order_relaxed);
}

void WindowSurfaceVk::setDesiredSwapchainPresentMode(vk::PresentMode presentMode)
{
    mDesiredSwapchainPresentMode.store(presentMode, std::memory_order_relaxed);
}

void WindowSurfaceVk::setDesiredSwapInterval(EGLint interval)
{
    const EGLint minSwapInterval = mState.config->minSwapInterval;
    const EGLint maxSwapInterval = mState.config->maxSwapInterval;
    ASSERT(minSwapInterval == 0 || minSwapInterval == 1);
    ASSERT(maxSwapInterval == 0 || maxSwapInterval == 1);

    interval = gl::clamp(interval, minSwapInterval, maxSwapInterval);

    setDesiredSwapchainPresentMode(GetDesiredPresentMode(mPresentModes, interval));

    // On the next swap, if the desired present mode is different from the current one, the
    // swapchain will be recreated.
}

void WindowSurfaceVk::setSwapInterval(const egl::Display *display, EGLint interval)
{
    // Don't let setSwapInterval change presentation mode if using SHARED present.
    if (!isSharedPresentModeDesired())
    {
        setDesiredSwapInterval(interval);
    }
}

angle::Result WindowSurfaceVk::getWindowVisibility(vk::ErrorContext *context,
                                                   bool *isVisibleOut) const
{
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

SurfaceSizeState WindowSurfaceVk::getSizeState() const
{
    return GetSizeState(mSizeState);
}

void WindowSurfaceVk::setSizeState(SurfaceSizeState sizeState)
{
    SetSizeState(&mSizeState, sizeState);
}

angle::Result WindowSurfaceVk::ensureSizeResolved(const gl::Context *context)
{
    if (getSizeState() == SurfaceSizeState::Resolved)
    {
        return angle::Result::Continue;
    }
    ASSERT(mAcquireOperation.state == ImageAcquireState::Unacquired);

    ANGLE_TRY(doDeferredAcquireNextImage(vk::GetImpl(context)));

    ASSERT(mSizeState == SurfaceSizeState::Resolved);
    return angle::Result::Continue;
}

gl::Extents WindowSurfaceVk::getSize() const
{
    ASSERT(mSizeState == SurfaceSizeState::Resolved);
    return gl::Extents(mWidth, mHeight, 1);
}

egl::Error WindowSurfaceVk::getUserSize(const egl::Display *display,
                                        EGLint *width,
                                        EGLint *height) const
{
    if (getSizeState() == SurfaceSizeState::Resolved)
    {
        std::lock_guard<angle::SimpleMutex> lock(mSizeMutex);
        // Surface size is resolved; use current size.
        if (width != nullptr)
        {
            *width = mWidth;
        }
        if (height != nullptr)
        {
            *height = mHeight;
        }
        return egl::NoError();
    }

    VkExtent2D extent;
    angle::Result result = getUserExtentsImpl(vk::GetImpl(display), &extent);
    if (result == angle::Result::Continue)
    {
        // The EGL spec states that value is not written if there is an error
        if (width != nullptr)
        {
            *width = static_cast<EGLint>(extent.width);
        }
        if (height != nullptr)
        {
            *height = static_cast<EGLint>(extent.height);
        }
        return egl::NoError();
    }

    return angle::ToEGL(result, EGL_BAD_SURFACE);
}

angle::Result WindowSurfaceVk::getUserExtentsImpl(vk::ErrorContext *context,
                                                  VkExtent2D *extentOut) const
{
    if (mIsSurfaceSizedBySwapchain)
    {
        gl::Extents windowExtents;
        ANGLE_TRY(getCurrentWindowSize(context, &windowExtents));
        extentOut->width  = windowExtents.width;
        extentOut->height = windowExtents.height;
    }
    else
    {
        VkSurfaceCapabilitiesKHR surfaceCaps;
        ANGLE_VK_TRY(context,
                     vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                         context->getRenderer()->getPhysicalDevice(), mSurface, &surfaceCaps));
        *extentOut = surfaceCaps.currentExtent;
    }

    adjustSurfaceExtent(extentOut);

    // Must return current surface size if swapchain recreate will be skipped in the future
    // |prepareSwapchainForAcquireNextImage| call.  Can't skip recreate if swapchain is already
    // invalid.  Avoid unnecessary |getWindowVisibility| call if window and surface sizes match.
    if (context->getFeatures().avoidInvisibleWindowSwapchainRecreate.enabled &&
        getSizeState() == SurfaceSizeState::Unresolved)
    {
        std::lock_guard<angle::SimpleMutex> lock(mSizeMutex);
        if (extentOut->width != static_cast<uint32_t>(mWidth) ||
            extentOut->height != static_cast<uint32_t>(mHeight))
        {
            bool isWindowVisible = false;
            ANGLE_TRY(getWindowVisibility(context, &isWindowVisible));
            if (!isWindowVisible)
            {
                extentOut->width  = mWidth;
                extentOut->height = mHeight;
            }
        }
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
                                                     vk::FramebufferFetchMode fetchMode,
                                                     const vk::RenderPass &compatibleRenderPass,
                                                     vk::Framebuffer *framebufferOut)
{
    ASSERT(!contextVk->getFeatures().preferDynamicRendering.enabled);

    // FramebufferVk dirty-bit processing should ensure that a new image was acquired.
    ASSERT(mAcquireOperation.state == ImageAcquireState::Ready);
    ASSERT(mSizeState == SurfaceSizeState::Resolved);
    ASSERT(mSwapchain != VK_NULL_HANDLE);

    // Track the new fetch mode
    mFramebufferFetchMode = fetchMode;

    SwapchainImage &swapchainImage = mSwapchainImages[mCurrentSwapchainImageIndex];

    vk::Framebuffer *currentFramebuffer = &chooseFramebuffer();
    if (currentFramebuffer->valid())
    {
        // Validation layers should detect if the render pass is really compatible.
        framebufferOut->setHandle(currentFramebuffer->getHandle());
        return angle::Result::Continue;
    }

    const gl::Extents rotatedExtents = mColorRenderTarget.getRotatedExtents();
    const uint32_t attachmentCount   = 1 + (mDepthStencilImage.valid() ? 1 : 0);

    std::array<VkImageView, 3> imageViews = {};
    if (mDepthStencilImage.valid())
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(mDepthStencilRenderTarget.getImageView(contextVk, &imageView));
        imageViews[1] = imageView->getHandle();
    }

    if (isMultiSampled())
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(mColorRenderTarget.getImageView(contextVk, &imageView));
        imageViews[0] = imageView->getHandle();
    }
    else
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(swapchainImage.imageViews.getLevelLayerDrawImageView(
            contextVk, *swapchainImage.image, vk::LevelIndex(0), 0, &imageView));
        imageViews[0] = imageView->getHandle();
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.flags                   = 0;
    framebufferInfo.renderPass              = compatibleRenderPass.getHandle();
    framebufferInfo.attachmentCount         = attachmentCount;
    framebufferInfo.pAttachments            = imageViews.data();
    framebufferInfo.width                   = static_cast<uint32_t>(rotatedExtents.width);
    framebufferInfo.height                  = static_cast<uint32_t>(rotatedExtents.height);
    framebufferInfo.layers                  = 1;

    ANGLE_VK_TRY(contextVk, currentFramebuffer->init(contextVk->getDevice(), framebufferInfo));

    framebufferOut->setHandle(currentFramebuffer->getHandle());
    return angle::Result::Continue;
}

angle::Result WindowSurfaceVk::initializeContents(const gl::Context *context,
                                                  GLenum binding,
                                                  const gl::ImageIndex &imageIndex)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (mAcquireOperation.state != ImageAcquireState::Ready)
    {
        // Acquire the next image (previously deferred).  Some tests (e.g.
        // GenerateMipmapWithRedefineBenchmark.Run/vulkan_webgl) cause this path to be taken,
        // because of dirty-object processing.
        ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, "Initialize Swap Image");
        ANGLE_TRY(doDeferredAcquireNextImage(contextVk));
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

    vk::Renderer *renderer = contextVk->getRenderer();

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
    ANGLE_TRY(image->imageViews.getLevelLayerDrawImageView(contextVk, *image->image,
                                                           vk::LevelIndex(0), 0, &imageView));
    if (overlayVk)
    {
        ANGLE_TRY(overlayVk->onPresent(contextVk, image->image.get(), imageView,
                                       Is90DegreeRotation(getPreTransform())));
    }

    return angle::Result::Continue;
}

egl::Error WindowSurfaceVk::setAutoRefreshEnabled(bool enabled)
{
    // Auto refresh is only applicable in shared present mode
    if (!isSharedPresentModeDesired())
    {
        return egl::NoError();
    }

    vk::PresentMode newDesiredSwapchainPresentMode =
        enabled ? vk::PresentMode::SharedContinuousRefreshKHR
                : vk::PresentMode::SharedDemandRefreshKHR;

    // We only expose EGL_ANDROID_front_buffer_auto_refresh extension on Android with supported
    // VK_EXT_swapchain_maintenance1 extension, where current and new present modes expected to be
    // compatible.  Can't use |mCompatiblePresentModes| here to check if this is true because it is
    // not thread safe.  Instead of the check, ASSERT is added to the |queryAndAdjustSurfaceCaps|
    // method where |mCompatiblePresentModes| are queried.

    // Simply change mDesiredSwapchainPresentMode regardless if we are already in single buffer mode
    // or not, since compatible present modes does not require swapchain recreation.
    setDesiredSwapchainPresentMode(newDesiredSwapchainPresentMode);

    return egl::NoError();
}

egl::Error WindowSurfaceVk::getBufferAge(const gl::Context *context, EGLint *age)
{
    ContextVk *contextVk = vk::GetImpl(context);

    ANGLE_TRACE_EVENT0("gpu.angle", "getBufferAge");

    // ANI may be skipped in case of multi sampled surface.
    if (isMultiSampled())
    {
        *age = 0;
        return egl::NoError();
    }

    // Image must be already acquired in the |prepareSwap| call.
    ASSERT(mAcquireOperation.state != ImageAcquireState::Unacquired);

    // If the result of vkAcquireNextImageKHR is not yet processed, do so now.
    if (mAcquireOperation.state == ImageAcquireState::NeedToProcessResult)
    {
        egl::Error result = angle::ToEGL(doDeferredAcquireNextImage(contextVk), EGL_BAD_SURFACE);
        if (result.isError())
        {
            return result;
        }
    }

    if (mBufferAgeQueryFrameNumber == 0)
    {
        ANGLE_VK_PERF_WARNING(contextVk, GL_DEBUG_SEVERITY_LOW,
                              "Querying age of a surface will make it retain its content");

        mBufferAgeQueryFrameNumber = mFrameCount;
    }

    if (age != nullptr)
    {
        if (mState.swapBehavior == EGL_BUFFER_PRESERVED)
        {
            // EGL_EXT_buffer_age
            //
            // 1) What are the semantics if EGL_BUFFER_PRESERVED is in use
            //
            //     RESOLVED: The age will always be 1 in this case.

            // Note: if the query is made before the 1st swap then age needs to be 0
            *age = (mFrameCount == 1) ? 0 : 1;

            return egl::NoError();
        }

        uint64_t frameNumber = mSwapchainImages[mCurrentSwapchainImageIndex].frameNumber;
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
            return egl::Error(EGL_BAD_MATCH);
        }
        setDesiredSwapchainPresentMode(presentMode);
    }
    else  // EGL_BACK_BUFFER
    {
        setDesiredSwapInterval(mState.swapInterval);
    }
    return egl::NoError();
}

bool WindowSurfaceVk::supportsSingleRenderBuffer() const
{
    return supportsPresentMode(vk::PresentMode::SharedDemandRefreshKHR);
}

egl::Error WindowSurfaceVk::lockSurface(const egl::Display *display,
                                        EGLint usageHint,
                                        bool preservePixels,
                                        uint8_t **bufferPtrOut,
                                        EGLint *bufferPitchOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "WindowSurfaceVk::lockSurface");

    DisplayVk *displayVk = vk::GetImpl(display);

    if (mAcquireOperation.state != ImageAcquireState::Ready)
    {
        angle::Result result = doDeferredAcquireNextImage(displayVk);
        if (result != angle::Result::Continue)
        {
            return angle::ToEGL(result, EGL_BAD_ACCESS);
        }
    }

    vk::ImageHelper *image = mSwapchainImages[mCurrentSwapchainImageIndex].image.get();
    ASSERT(image->valid());

    angle::Result result = LockSurfaceImpl(displayVk, image, mLockBufferHelper, mWidth, mHeight,
                                           usageHint, preservePixels, bufferPtrOut, bufferPitchOut);
    return angle::ToEGL(result, EGL_BAD_ACCESS);
}

egl::Error WindowSurfaceVk::unlockSurface(const egl::Display *display, bool preservePixels)
{
    ASSERT(mAcquireOperation.state == ImageAcquireState::Ready);

    vk::ImageHelper *image = mSwapchainImages[mCurrentSwapchainImageIndex].image.get();
    ASSERT(image->valid());
    ASSERT(mLockBufferHelper.valid());

    return angle::ToEGL(UnlockSurfaceImpl(vk::GetImpl(display), image, mLockBufferHelper, mWidth,
                                          mHeight, preservePixels),
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

egl::Error WindowSurfaceVk::getCompressionRate(const egl::Display *display,
                                               const gl::Context *context,
                                               EGLint *rate)
{
    ASSERT(mSwapchain != VK_NULL_HANDLE);
    ASSERT(!mSwapchainImages.empty());

    DisplayVk *displayVk   = vk::GetImpl(display);
    ContextVk *contextVk   = vk::GetImpl(context);
    vk::Renderer *renderer = displayVk->getRenderer();

    ANGLE_TRACE_EVENT0("gpu.angle", "getCompressionRate");

    ASSERT(renderer->getFeatures().supportsImageCompressionControl.enabled);
    ASSERT(renderer->getFeatures().supportsImageCompressionControlSwapchain.enabled);

    // Image must be already acquired in the |prepareSwap| call.
    ASSERT(mAcquireOperation.state != ImageAcquireState::Unacquired);

    // If the result of vkAcquireNextImageKHR is not yet processed, do so now.
    if (mAcquireOperation.state == ImageAcquireState::NeedToProcessResult)
    {
        egl::Error result = angle::ToEGL(doDeferredAcquireNextImage(contextVk), EGL_BAD_SURFACE);
        if (result.isError())
        {
            return result;
        }
    }

    VkImageSubresource2EXT imageSubresource2      = {};
    imageSubresource2.sType                       = VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_EXT;
    imageSubresource2.imageSubresource.aspectMask = mSwapchainImages[0].image->getAspectFlags();
    VkImageCompressionPropertiesEXT compressionProperties = {};
    compressionProperties.sType = VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT;

    VkSubresourceLayout2EXT subresourceLayout = {};
    subresourceLayout.sType                   = VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2_EXT;
    subresourceLayout.pNext                   = &compressionProperties;

    vkGetImageSubresourceLayout2EXT(displayVk->getDevice(),
                                    mSwapchainImages[0].image->getImage().getHandle(),
                                    &imageSubresource2, &subresourceLayout);

    std::vector<EGLint> eglFixedRates = vk_gl::ConvertCompressionFlagsToEGLFixedRate(
        compressionProperties.imageCompressionFixedRateFlags, 1);
    *rate =
        (eglFixedRates.empty() ? EGL_SURFACE_COMPRESSION_FIXED_RATE_NONE_EXT : eglFixedRates[0]);

    return egl::NoError();
}

}  // namespace rx
