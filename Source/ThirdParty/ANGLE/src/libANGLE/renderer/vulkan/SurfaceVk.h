//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceVk.h:
//    Defines the class interface for SurfaceVk, implementing SurfaceImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_SURFACEVK_H_
#define LIBANGLE_RENDERER_VULKAN_SURFACEVK_H_

#include <vulkan/vulkan.h>

#include "libANGLE/renderer/SurfaceImpl.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/renderervk_utils.h"

namespace rx
{
class RendererVk;

class OffscreenSurfaceVk : public SurfaceImpl
{
  public:
    OffscreenSurfaceVk(const egl::SurfaceState &surfaceState, EGLint width, EGLint height);
    ~OffscreenSurfaceVk() override;

    egl::Error initialize(const DisplayImpl *displayImpl) override;
    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) override;
    egl::Error swap(const DisplayImpl *displayImpl) override;
    egl::Error postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(gl::Texture *texture, EGLint buffer) override;
    egl::Error releaseTexImage(EGLint buffer) override;
    egl::Error getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc) override;
    void setSwapInterval(EGLint interval) override;

    // width and height can change with client window resizing
    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    gl::Error getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                        FramebufferAttachmentRenderTarget **rtOut) override;

  private:
    EGLint mWidth;
    EGLint mHeight;
};

class WindowSurfaceVk : public SurfaceImpl, public ResourceVk
{
  public:
    WindowSurfaceVk(const egl::SurfaceState &surfaceState,
                    EGLNativeWindowType window,
                    EGLint width,
                    EGLint height);
    ~WindowSurfaceVk() override;

    void destroy(const DisplayImpl *contextImpl) override;

    egl::Error initialize(const DisplayImpl *displayImpl) override;
    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) override;
    egl::Error swap(const DisplayImpl *displayImpl) override;
    egl::Error postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(gl::Texture *texture, EGLint buffer) override;
    egl::Error releaseTexImage(EGLint buffer) override;
    egl::Error getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc) override;
    void setSwapInterval(EGLint interval) override;

    // width and height can change with client window resizing
    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    gl::Error getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                        FramebufferAttachmentRenderTarget **rtOut) override;

    gl::ErrorOrResult<vk::Framebuffer *> getCurrentFramebuffer(
        VkDevice device,
        const vk::RenderPass &compatibleRenderPass);

  private:
    vk::Error initializeImpl(RendererVk *renderer);
    vk::Error nextSwapchainImage(RendererVk *renderer);
    vk::Error swapImpl(RendererVk *renderer);

    EGLNativeWindowType mNativeWindowType;
    VkSurfaceKHR mSurface;
    VkSwapchainKHR mSwapchain;

    RenderTargetVk mRenderTarget;
    vk::Semaphore mImageAvailableSemaphore;
    vk::Semaphore mRenderingCompleteSemaphore;

    uint32_t mCurrentSwapchainImageIndex;
    std::vector<vk::Image> mSwapchainImages;
    std::vector<vk::ImageView> mSwapchainImageViews;
    std::vector<vk::Framebuffer> mSwapchainFramebuffers;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SURFACEVK_H_
