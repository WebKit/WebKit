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

    egl::Error initialize(const egl::Display *display) override;
    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) override;
    egl::Error swap(const gl::Context *context) override;
    egl::Error postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height) override;
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

    gl::Error getAttachmentRenderTarget(const gl::Context *context,
                                        GLenum binding,
                                        const gl::ImageIndex &imageIndex,
                                        FramebufferAttachmentRenderTarget **rtOut) override;

    gl::Error initializeContents(const gl::Context *context,
                                 const gl::ImageIndex &imageIndex) override;

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

    void destroy(const egl::Display *display) override;

    egl::Error initialize(const egl::Display *display) override;
    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) override;
    egl::Error swap(const gl::Context *context) override;
    egl::Error postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height) override;
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

    gl::Error getAttachmentRenderTarget(const gl::Context *context,
                                        GLenum binding,
                                        const gl::ImageIndex &imageIndex,
                                        FramebufferAttachmentRenderTarget **rtOut) override;

    gl::Error initializeContents(const gl::Context *context,
                                 const gl::ImageIndex &imageIndex) override;

    gl::ErrorOrResult<vk::Framebuffer *> getCurrentFramebuffer(
        VkDevice device,
        const vk::RenderPass &compatibleRenderPass);

  protected:
    EGLNativeWindowType mNativeWindowType;
    VkSurfaceKHR mSurface;
    VkInstance mInstance;

  private:
    virtual vk::ErrorOrResult<gl::Extents> createSurfaceVk(RendererVk *renderer) = 0;
    vk::Error initializeImpl(RendererVk *renderer);
    vk::Error nextSwapchainImage(RendererVk *renderer);

    VkSwapchainKHR mSwapchain;

    RenderTargetVk mRenderTarget;

    uint32_t mCurrentSwapchainImageIndex;

    // When acquiring a new image for rendering, we keep a 'spare' semaphore. We pass this extra
    // semaphore to VkAcquireNextImage, then hand it to the next available SwapchainImage when
    // the command completes. We then make the old semaphore in the new SwapchainImage the spare
    // semaphore, since we know the image is no longer using it. This avoids the chicken and egg
    // problem with needing to know the next available image index before we acquire it.
    vk::Semaphore mAcquireNextImageSemaphore;

    struct SwapchainImage
    {
        SwapchainImage();
        SwapchainImage(SwapchainImage &&other);
        ~SwapchainImage();

        vk::Image image;
        vk::ImageView imageView;
        vk::Framebuffer framebuffer;
        vk::Semaphore imageAcquiredSemaphore;
        vk::Semaphore commandsCompleteSemaphore;
    };

    std::vector<SwapchainImage> mSwapchainImages;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SURFACEVK_H_
