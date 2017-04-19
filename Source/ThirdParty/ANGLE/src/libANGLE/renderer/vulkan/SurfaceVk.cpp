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
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/formatutilsvk.h"

namespace rx
{

namespace
{

const vk::Format &GetVkFormatFromConfig(const egl::Config &config)
{
    // TODO(jmadill): Properly handle format interpretation.
    return vk::Format::Get(GL_BGRA8_EXT);
}

VkPresentModeKHR GetDesiredPresentMode(const std::vector<VkPresentModeKHR> &presentModes,
                                       EGLint minSwapInterval,
                                       EGLint maxSwapInterval)
{
    ASSERT(!presentModes.empty());

    // Use FIFO mode for v-sync, since it throttles you to the display rate. Mailbox is more
    // similar to triple-buffering. For now we hard-code Mailbox for perf tseting.
    // TODO(jmadill): Properly select present mode and re-create display if changed.
    VkPresentModeKHR bestChoice = VK_PRESENT_MODE_MAILBOX_KHR;

    for (auto presentMode : presentModes)
    {
        if (presentMode == bestChoice)
        {
            return bestChoice;
        }
    }

    ERR() << "Desired present mode not available. Falling back to " << presentModes[0];
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

egl::Error OffscreenSurfaceVk::initialize(const DisplayImpl *displayImpl)
{
    return egl::Error(EGL_SUCCESS);
}

FramebufferImpl *OffscreenSurfaceVk::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    // Use a user FBO for an offscreen RT.
    return FramebufferVk::CreateUserFBO(state);
}

egl::Error OffscreenSurfaceVk::swap(const DisplayImpl *displayImpl)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error OffscreenSurfaceVk::postSubBuffer(EGLint /*x*/,
                                             EGLint /*y*/,
                                             EGLint /*width*/,
                                             EGLint /*height*/)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error OffscreenSurfaceVk::querySurfacePointerANGLE(EGLint /*attribute*/, void ** /*value*/)
{
    UNREACHABLE();
    return egl::Error(EGL_BAD_CURRENT_SURFACE);
}

egl::Error OffscreenSurfaceVk::bindTexImage(gl::Texture * /*texture*/, EGLint /*buffer*/)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error OffscreenSurfaceVk::releaseTexImage(EGLint /*buffer*/)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error OffscreenSurfaceVk::getSyncValues(EGLuint64KHR * /*ust*/,
                                             EGLuint64KHR * /*msc*/,
                                             EGLuint64KHR * /*sbc*/)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
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
    const gl::FramebufferAttachment::Target & /*target*/,
    FramebufferAttachmentRenderTarget ** /*rtOut*/)
{
    UNREACHABLE();
    return gl::Error(GL_INVALID_OPERATION);
}

WindowSurfaceVk::WindowSurfaceVk(const egl::SurfaceState &surfaceState,
                                 EGLNativeWindowType window,
                                 EGLint width,
                                 EGLint height)
    : SurfaceImpl(surfaceState),
      mNativeWindowType(window),
      mSurface(VK_NULL_HANDLE),
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

void WindowSurfaceVk::destroy(const DisplayImpl *displayImpl)
{
    const DisplayVk *displayVk = GetAs<DisplayVk>(displayImpl);
    RendererVk *rendererVk     = displayVk->getRenderer();
    VkDevice device            = rendererVk->getDevice();
    VkInstance instance        = rendererVk->getInstance();

    rendererVk->finish();

    mImageAvailableSemaphore.destroy(device);
    mRenderingCompleteSemaphore.destroy(device);

    for (auto &imageView : mSwapchainImageViews)
    {
        imageView.destroy(device);
    }

    // Although we don't own the swapchain image handles, we need to keep our shutdown clean.
    for (auto &image : mSwapchainImages)
    {
        image.reset();
    }

    for (auto &framebuffer : mSwapchainFramebuffers)
    {
        framebuffer.destroy(device);
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

egl::Error WindowSurfaceVk::initialize(const DisplayImpl *displayImpl)
{
    const DisplayVk *displayVk = GetAs<DisplayVk>(displayImpl);
    return initializeImpl(displayVk->getRenderer()).toEGL(EGL_BAD_SURFACE);
}

vk::Error WindowSurfaceVk::initializeImpl(RendererVk *renderer)
{
    // TODO(jmadill): Make this platform-specific.
    VkWin32SurfaceCreateInfoKHR createInfo;

    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext     = nullptr;
    createInfo.flags     = 0;
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd      = mNativeWindowType;
    ANGLE_VK_TRY(vkCreateWin32SurfaceKHR(renderer->getInstance(), &createInfo, nullptr, &mSurface));

    uint32_t presentQueue = 0;
    ANGLE_TRY_RESULT(renderer->selectPresentQueueForSurface(mSurface), presentQueue);

    const auto &physicalDevice = renderer->getPhysicalDevice();

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

        RECT rect;
        ANGLE_VK_CHECK(GetClientRect(mNativeWindowType, &rect) == TRUE,
                       VK_ERROR_INITIALIZATION_FAILED);
        if (mRenderTarget.extents.width == 0)
        {
            width = static_cast<uint32_t>(rect.right - rect.left);
        }
        if (mRenderTarget.extents.height == 0)
        {
            height = static_cast<uint32_t>(rect.bottom - rect.top);
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

    mRenderTarget.format = &GetVkFormatFromConfig(*mState.config);
    auto nativeFormat    = mRenderTarget.format->native;

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

    const auto &device = renderer->getDevice();
    ANGLE_VK_TRY(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &mSwapchain));

    // Intialize the swapchain image views.
    uint32_t imageCount = 0;
    ANGLE_VK_TRY(vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, nullptr));

    std::vector<VkImage> swapchainImages(imageCount);
    ANGLE_VK_TRY(vkGetSwapchainImagesKHR(device, mSwapchain, &imageCount, swapchainImages.data()));

    // CommandBuffer is a singleton in the Renderer.
    vk::CommandBuffer *commandBuffer = renderer->getCommandBuffer();
    ANGLE_TRY(commandBuffer->begin(device));

    VkClearColorValue transparentBlack;
    transparentBlack.float32[0] = 0.0f;
    transparentBlack.float32[1] = 0.0f;
    transparentBlack.float32[2] = 0.0f;
    transparentBlack.float32[3] = 0.0f;

    mSwapchainImages.resize(imageCount);
    mSwapchainImageViews.resize(imageCount);

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

        vk::Image image(swapchainImage);
        vk::ImageView imageView;
        ANGLE_TRY(imageView.init(device, imageViewInfo));

        // Set transfer dest layout, and clear the image to black.
        image.changeLayoutTop(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              commandBuffer);
        commandBuffer->clearSingleColorImage(image, transparentBlack);

        mSwapchainImages[imageIndex].retain(device, std::move(image));
        mSwapchainImageViews[imageIndex].retain(device, std::move(imageView));
    }

    ANGLE_TRY(commandBuffer->end());
    ANGLE_TRY(renderer->submitAndFinishCommandBuffer(*commandBuffer));

    ANGLE_TRY(mImageAvailableSemaphore.init(device));
    ANGLE_TRY(mRenderingCompleteSemaphore.init(device));

    // Get the first available swapchain iamge.
    ANGLE_TRY(nextSwapchainImage(renderer));

    return vk::NoError();
}

FramebufferImpl *WindowSurfaceVk::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    return FramebufferVk::CreateDefaultFBO(state, this);
}

egl::Error WindowSurfaceVk::swap(const DisplayImpl *displayImpl)
{
    const DisplayVk *displayVk = GetAs<DisplayVk>(displayImpl);
    return swapImpl(displayVk->getRenderer()).toEGL(EGL_BAD_ALLOC);
}

vk::Error WindowSurfaceVk::swapImpl(RendererVk *renderer)
{
    vk::CommandBuffer *currentCB = renderer->getCommandBuffer();

    auto *image = &mSwapchainImages[mCurrentSwapchainImageIndex];

    currentCB->begin(renderer->getDevice());
    image->changeLayoutWithStages(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, currentCB);
    currentCB->end();

    ANGLE_TRY(renderer->submitCommandsWithSync(*currentCB, mImageAvailableSemaphore,
                                               mRenderingCompleteSemaphore));

    VkPresentInfoKHR presentInfo;
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext              = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = mRenderingCompleteSemaphore.ptr();
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

    ANGLE_VK_TRY(vkAcquireNextImageKHR(device, mSwapchain, std::numeric_limits<uint64_t>::max(),
                                       mImageAvailableSemaphore.getHandle(), VK_NULL_HANDLE,
                                       &mCurrentSwapchainImageIndex));

    // Update RenderTarget pointers.
    mRenderTarget.image     = &mSwapchainImages[mCurrentSwapchainImageIndex];
    mRenderTarget.imageView = &mSwapchainImageViews[mCurrentSwapchainImageIndex];

    return vk::NoError();
}

egl::Error WindowSurfaceVk::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    // TODO(jmadill)
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceVk::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNREACHABLE();
    return egl::Error(EGL_BAD_CURRENT_SURFACE);
}

egl::Error WindowSurfaceVk::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceVk::releaseTexImage(EGLint buffer)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceVk::getSyncValues(EGLuint64KHR * /*ust*/,
                                          EGLuint64KHR * /*msc*/,
                                          EGLuint64KHR * /*sbc*/)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
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

gl::Error WindowSurfaceVk::getAttachmentRenderTarget(
    const gl::FramebufferAttachment::Target & /*target*/,
    FramebufferAttachmentRenderTarget **rtOut)
{
    *rtOut = &mRenderTarget;
    return gl::NoError();
}

gl::ErrorOrResult<vk::Framebuffer *> WindowSurfaceVk::getCurrentFramebuffer(
    VkDevice device,
    const vk::RenderPass &compatibleRenderPass)
{
    if (!mSwapchainFramebuffers.empty())
    {
        // Validation layers should detect if the render pass is really compatible.
        return &mSwapchainFramebuffers[mCurrentSwapchainImageIndex];
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

    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());
    for (size_t imageIndex = 0; imageIndex < mSwapchainFramebuffers.size(); ++imageIndex)
    {
        const auto &imageView        = mSwapchainImageViews[imageIndex];
        VkImageView imageViewHandle  = imageView.getHandle();
        framebufferInfo.pAttachments = &imageViewHandle;

        vk::Framebuffer framebuffer;
        ANGLE_TRY(framebuffer.init(device, framebufferInfo));

        mSwapchainFramebuffers[imageIndex].retain(device, std::move(framebuffer));
    }

    // We should only initialize framebuffers on the first swap.
    ASSERT(mCurrentSwapchainImageIndex == 0u);
    return &mSwapchainFramebuffers[mCurrentSwapchainImageIndex];
}

}  // namespace rx
