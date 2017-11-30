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
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/formatutilsvk.h"

namespace rx
{

namespace
{

const vk::Format &GetVkFormatFromConfig(RendererVk *renderer, const egl::Config &config)
{
    // TODO(jmadill): Properly handle format interpretation.
    return renderer->getFormat(GL_BGRA8_EXT);
}

VkPresentModeKHR GetDesiredPresentMode(const std::vector<VkPresentModeKHR> &presentModes,
                                       EGLint minSwapInterval,
                                       EGLint maxSwapInterval)
{
    ASSERT(!presentModes.empty());

    // Use FIFO mode for v-sync, since it throttles you to the display rate. Mailbox is more
    // similar to triple-buffering. For now we hard-code Mailbox for perf testing.
    // TODO(jmadill): Properly select present mode and re-create display if changed.
    VkPresentModeKHR bestChoice = VK_PRESENT_MODE_MAILBOX_KHR;

    for (auto presentMode : presentModes)
    {
        if (presentMode == bestChoice)
        {
            return bestChoice;
        }
    }

    WARN() << "Present mode " << bestChoice << " not available. Falling back to "
           << presentModes[0];
    return presentModes[0];
}

}  // namespace

OffscreenSurfaceVk::OffscreenSurfaceVk(const egl::SurfaceState &surfaceState,
                                       EGLint width,
                                       EGLint height)
    : SurfaceImpl(surfaceState), mWidth(width), mHeight(height)
{
}

OffscreenSurfaceVk::~OffscreenSurfaceVk()
{
}

egl::Error OffscreenSurfaceVk::initialize(const egl::Display *display)
{
    return egl::NoError();
}

FramebufferImpl *OffscreenSurfaceVk::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    // Use a user FBO for an offscreen RT.
    return FramebufferVk::CreateUserFBO(state);
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

egl::Error OffscreenSurfaceVk::bindTexImage(gl::Texture * /*texture*/, EGLint /*buffer*/)
{
    return egl::NoError();
}

egl::Error OffscreenSurfaceVk::releaseTexImage(EGLint /*buffer*/)
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

void OffscreenSurfaceVk::setSwapInterval(EGLint /*interval*/)
{
}

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
    return EGL_BUFFER_PRESERVED;
}

gl::Error OffscreenSurfaceVk::getAttachmentRenderTarget(
    const gl::Context * /*context*/,
    GLenum /*binding*/,
    const gl::ImageIndex & /*imageIndex*/,
    FramebufferAttachmentRenderTarget ** /*rtOut*/)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error OffscreenSurfaceVk::initializeContents(const gl::Context *context,
                                                 const gl::ImageIndex &imageIndex)
{
    UNIMPLEMENTED();
    return gl::NoError();
}

WindowSurfaceVk::SwapchainImage::SwapchainImage()                                        = default;
WindowSurfaceVk::SwapchainImage::SwapchainImage(WindowSurfaceVk::SwapchainImage &&other) = default;
WindowSurfaceVk::SwapchainImage::~SwapchainImage()                                       = default;

WindowSurfaceVk::WindowSurfaceVk(const egl::SurfaceState &surfaceState,
                                 EGLNativeWindowType window,
                                 EGLint width,
                                 EGLint height)
    : SurfaceImpl(surfaceState),
      mNativeWindowType(window),
      mSurface(VK_NULL_HANDLE),
      mInstance(VK_NULL_HANDLE),
      mSwapchain(VK_NULL_HANDLE),
      mRenderTarget(),
      mCurrentSwapchainImageIndex(0)
{
    mRenderTarget.extents.width  = static_cast<GLint>(width);
    mRenderTarget.extents.height = static_cast<GLint>(height);
    mRenderTarget.extents.depth  = 1;
    mRenderTarget.resource       = this;
}

WindowSurfaceVk::~WindowSurfaceVk()
{
    ASSERT(mSurface == VK_NULL_HANDLE);
    ASSERT(mSwapchain == VK_NULL_HANDLE);
}

void WindowSurfaceVk::destroy(const egl::Display *display)
{
    const DisplayVk *displayVk = vk::GetImpl(display);
    RendererVk *rendererVk     = displayVk->getRenderer();
    VkDevice device            = rendererVk->getDevice();
    VkInstance instance        = rendererVk->getInstance();

    rendererVk->finish();

    mAcquireNextImageSemaphore.destroy(device);

    for (auto &swapchainImage : mSwapchainImages)
    {
        // Although we don't own the swapchain image handles, we need to keep our shutdown clean.
        swapchainImage.image.reset();

        swapchainImage.imageView.destroy(device);
        swapchainImage.framebuffer.destroy(device);
        swapchainImage.imageAcquiredSemaphore.destroy(device);
        swapchainImage.commandsCompleteSemaphore.destroy(device);
    }

    if (mSwapchain)
    {
        vkDestroySwapchainKHR(device, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    if (mSurface)
    {
        vkDestroySurfaceKHR(instance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }
}

egl::Error WindowSurfaceVk::initialize(const egl::Display *display)
{
    const DisplayVk *displayVk = vk::GetImpl(display);
    return initializeImpl(displayVk->getRenderer()).toEGL(EGL_BAD_SURFACE);
}

vk::Error WindowSurfaceVk::initializeImpl(RendererVk *renderer)
{
    gl::Extents windowSize;
    ANGLE_TRY_RESULT(createSurfaceVk(renderer), windowSize);

    uint32_t presentQueue = 0;
    ANGLE_TRY_RESULT(renderer->selectPresentQueueForSurface(mSurface), presentQueue);

    const VkPhysicalDevice &physicalDevice = renderer->getPhysicalDevice();

    VkSurfaceCapabilitiesKHR surfaceCaps;
    ANGLE_VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, &surfaceCaps));

    // Adjust width and height to the swapchain if necessary.
    uint32_t width  = surfaceCaps.currentExtent.width;
    uint32_t height = surfaceCaps.currentExtent.height;

    // TODO(jmadill): Support devices which don't support copy. We use this for ReadPixels.
    ANGLE_VK_CHECK((surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0,
                   VK_ERROR_INITIALIZATION_FAILED);

    if (surfaceCaps.currentExtent.width == 0xFFFFFFFFu)
    {
        ASSERT(surfaceCaps.currentExtent.height == 0xFFFFFFFFu);

        if (mRenderTarget.extents.width == 0)
        {
            width = windowSize.width;
        }
        if (mRenderTarget.extents.height == 0)
        {
            height = windowSize.height;
        }
    }

    mRenderTarget.extents.width  = static_cast<int>(width);
    mRenderTarget.extents.height = static_cast<int>(height);

    uint32_t presentModeCount = 0;
    ANGLE_VK_TRY(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface,
                                                           &presentModeCount, nullptr));
    ASSERT(presentModeCount > 0);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    ANGLE_VK_TRY(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface,
                                                           &presentModeCount, presentModes.data()));

    // Select appropriate present mode based on vsync parameter.
    // TODO(jmadill): More complete implementation, which allows for changing and more values.
    const EGLint minSwapInterval = mState.config->minSwapInterval;
    const EGLint maxSwapInterval = mState.config->maxSwapInterval;
    ASSERT(minSwapInterval == 0 || minSwapInterval == 1);
    ASSERT(maxSwapInterval == 0 || maxSwapInterval == 1);

    VkPresentModeKHR swapchainPresentMode =
        GetDesiredPresentMode(presentModes, minSwapInterval, maxSwapInterval);

    // Determine number of swapchain images. Aim for one more than the minimum.
    uint32_t minImageCount = surfaceCaps.minImageCount + 1;
    if (surfaceCaps.maxImageCount > 0 && minImageCount > surfaceCaps.maxImageCount)
    {
        minImageCount = surfaceCaps.maxImageCount;
    }

    // Default to identity transform.
    VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    if ((surfaceCaps.supportedTransforms & preTransform) == 0)
    {
        preTransform = surfaceCaps.currentTransform;
    }

    uint32_t surfaceFormatCount = 0;
    ANGLE_VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &surfaceFormatCount,
                                                      nullptr));

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    ANGLE_VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &surfaceFormatCount,
                                                      surfaceFormats.data()));

    mRenderTarget.format  = &GetVkFormatFromConfig(renderer, *mState.config);
    VkFormat nativeFormat = mRenderTarget.format->vkTextureFormat;

    if (surfaceFormatCount == 1u && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        // This is fine.
    }
    else
    {
        bool foundFormat = false;
        for (const auto &surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == nativeFormat)
            {
                foundFormat = true;
                break;
            }
        }

        ANGLE_VK_CHECK(foundFormat, VK_ERROR_INITIALIZATION_FAILED);
    }

    VkSwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.sType              = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext              = nullptr;
    swapchainInfo.flags              = 0;
    swapchainInfo.surface            = mSurface;
    swapchainInfo.minImageCount      = minImageCount;
    swapchainInfo.imageFormat        = nativeFormat;
    swapchainInfo.imageColorSpace    = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    swapchainInfo.imageExtent.width  = width;
    swapchainInfo.imageExtent.height = height;
    swapchainInfo.imageArrayLayers   = 1;
    swapchainInfo.imageUsage         = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    swapchainInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices   = nullptr;
    swapchainInfo.preTransform          = preTransform;
    swapchainInfo.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode           = swapchainPresentMode;
    swapchainInfo.clipped               = VK_TRUE;
    swapchainInfo.oldSwapchain          = VK_NULL_HANDLE;

    VkDevice device = renderer->getDevice();
    ANGLE_VK_TRY(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &mSwapchain));

    // Intialize the swapchain image views.
    uint32_t imageCount = 0;
    ANGLE_VK_TRY(vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, nullptr));

    std::vector<VkImage> swapchainImages(imageCount);
    ANGLE_VK_TRY(vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, swapchainImages.data()));

    // CommandBuffer is a singleton in the Renderer.
    vk::CommandBufferAndState *commandBuffer = nullptr;
    ANGLE_TRY(renderer->getStartedCommandBuffer(&commandBuffer));

    VkClearColorValue transparentBlack;
    transparentBlack.float32[0] = 0.0f;
    transparentBlack.float32[1] = 0.0f;
    transparentBlack.float32[2] = 0.0f;
    transparentBlack.float32[3] = 0.0f;

    mSwapchainImages.resize(imageCount);

    ANGLE_TRY(mAcquireNextImageSemaphore.init(device));

    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex)
    {
        VkImage swapchainImage = swapchainImages[imageIndex];

        VkImageViewCreateInfo imageViewInfo;
        imageViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.pNext                           = nullptr;
        imageViewInfo.flags                           = 0;
        imageViewInfo.image                           = swapchainImage;
        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format                          = nativeFormat;
        imageViewInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
        imageViewInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
        imageViewInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
        imageViewInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
        imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel   = 0;
        imageViewInfo.subresourceRange.levelCount     = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount     = 1;

        auto &member = mSwapchainImages[imageIndex];

        member.image.setHandle(swapchainImage);
        ANGLE_TRY(member.imageView.init(device, imageViewInfo));

        // Set transfer dest layout, and clear the image to black.
        member.image.changeLayoutWithStages(
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, commandBuffer);
        commandBuffer->clearSingleColorImage(member.image, transparentBlack);

        ANGLE_TRY(member.imageAcquiredSemaphore.init(device));
        ANGLE_TRY(member.commandsCompleteSemaphore.init(device));
    }

    ANGLE_TRY(renderer->submitAndFinishCommandBuffer(commandBuffer));

    // Get the first available swapchain iamge.
    ANGLE_TRY(nextSwapchainImage(renderer));

    return vk::NoError();
}

FramebufferImpl *WindowSurfaceVk::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    return FramebufferVk::CreateDefaultFBO(state, this);
}

egl::Error WindowSurfaceVk::swap(const gl::Context *context)
{
    const DisplayVk *displayVk = vk::GetImpl(context->getCurrentDisplay());
    RendererVk *renderer       = displayVk->getRenderer();

    vk::CommandBufferAndState *currentCB = nullptr;
    ANGLE_TRY(renderer->getStartedCommandBuffer(&currentCB));

    // End render pass
    renderer->endRenderPass();

    auto &image = mSwapchainImages[mCurrentSwapchainImageIndex];

    image.image.changeLayoutWithStages(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, currentCB);

    ANGLE_TRY(renderer->submitCommandsWithSync(currentCB, image.imageAcquiredSemaphore,
                                               image.commandsCompleteSemaphore));

    VkPresentInfoKHR presentInfo;
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext              = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = image.commandsCompleteSemaphore.ptr();
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &mSwapchain;
    presentInfo.pImageIndices      = &mCurrentSwapchainImageIndex;
    presentInfo.pResults           = nullptr;

    ANGLE_VK_TRY(vkQueuePresentKHR(renderer->getQueue(), &presentInfo));

    // Get the next available swapchain image.
    ANGLE_TRY(nextSwapchainImage(renderer));

    return vk::NoError();
}

vk::Error WindowSurfaceVk::nextSwapchainImage(RendererVk *renderer)
{
    VkDevice device = renderer->getDevice();

    // Use a timeout of zero for AcquireNextImage so we don't actually block.
    // TODO(jmadill): We should handle VK_NOT_READY and block until we can acquire the image.
    ANGLE_VK_TRY(vkAcquireNextImageKHR(device, mSwapchain, 0,
                                       mAcquireNextImageSemaphore.getHandle(), VK_NULL_HANDLE,
                                       &mCurrentSwapchainImageIndex));

    auto &image = mSwapchainImages[mCurrentSwapchainImageIndex];

    // Swap the unused swapchain semaphore and the now active spare semaphore.
    std::swap(image.imageAcquiredSemaphore, mAcquireNextImageSemaphore);

    // Update RenderTarget pointers.
    mRenderTarget.image     = &image.image;
    mRenderTarget.imageView = &image.imageView;

    return vk::NoError();
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

egl::Error WindowSurfaceVk::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    return egl::NoError();
}

egl::Error WindowSurfaceVk::releaseTexImage(EGLint buffer)
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

void WindowSurfaceVk::setSwapInterval(EGLint interval)
{
}

EGLint WindowSurfaceVk::getWidth() const
{
    return static_cast<EGLint>(mRenderTarget.extents.width);
}

EGLint WindowSurfaceVk::getHeight() const
{
    return static_cast<EGLint>(mRenderTarget.extents.height);
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

gl::Error WindowSurfaceVk::getAttachmentRenderTarget(const gl::Context * /*context*/,
                                                     GLenum /*binding*/,
                                                     const gl::ImageIndex & /*target*/,
                                                     FramebufferAttachmentRenderTarget **rtOut)
{
    *rtOut = &mRenderTarget;
    return gl::NoError();
}

gl::ErrorOrResult<vk::Framebuffer *> WindowSurfaceVk::getCurrentFramebuffer(
    VkDevice device,
    const vk::RenderPass &compatibleRenderPass)
{
    auto &currentFramebuffer = mSwapchainImages[mCurrentSwapchainImageIndex].framebuffer;

    if (currentFramebuffer.valid())
    {
        // Validation layers should detect if the render pass is really compatible.
        return &currentFramebuffer;
    }

    VkFramebufferCreateInfo framebufferInfo;

    // TODO(jmadill): Depth/Stencil attachments.
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext           = nullptr;
    framebufferInfo.flags           = 0;
    framebufferInfo.renderPass      = compatibleRenderPass.getHandle();
    framebufferInfo.attachmentCount = 1u;
    framebufferInfo.pAttachments    = nullptr;
    framebufferInfo.width           = static_cast<uint32_t>(mRenderTarget.extents.width);
    framebufferInfo.height          = static_cast<uint32_t>(mRenderTarget.extents.height);
    framebufferInfo.layers          = 1;

    for (auto &swapchainImage : mSwapchainImages)
    {
        framebufferInfo.pAttachments = swapchainImage.imageView.ptr();
        ANGLE_TRY(swapchainImage.framebuffer.init(device, framebufferInfo));
    }

    ASSERT(currentFramebuffer.valid());
    return &currentFramebuffer;
}

gl::Error WindowSurfaceVk::initializeContents(const gl::Context *context,
                                              const gl::ImageIndex &imageIndex)
{
    UNIMPLEMENTED();
    return gl::NoError();
}

}  // namespace rx
