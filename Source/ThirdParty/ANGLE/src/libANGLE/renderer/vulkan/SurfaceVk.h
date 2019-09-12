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
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class RendererVk;

class OffscreenSurfaceVk : public SurfaceImpl
{
  public:
    OffscreenSurfaceVk(const egl::SurfaceState &surfaceState, EGLint width, EGLint height);
    ~OffscreenSurfaceVk() override;

    egl::Error initialize(const egl::Display *display) override;
    void destroy(const egl::Display *display) override;

    FramebufferImpl *createDefaultFramebuffer(const gl::Context *context,
                                              const gl::FramebufferState &state) override;
    egl::Error swap(const gl::Context *context) override;
    egl::Error postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(const gl::Context *context,
                            gl::Texture *texture,
                            EGLint buffer) override;
    egl::Error releaseTexImage(const gl::Context *context, EGLint buffer) override;
    egl::Error getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc) override;
    void setSwapInterval(EGLint interval) override;

    // width and height can change with client window resizing
    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

    angle::Result initializeContents(const gl::Context *context,
                                     const gl::ImageIndex &imageIndex) override;

    vk::ImageHelper *getColorAttachmentImage();

  private:
    struct AttachmentImage final : angle::NonCopyable
    {
        AttachmentImage();
        ~AttachmentImage();

        angle::Result initialize(DisplayVk *displayVk,
                                 EGLint width,
                                 EGLint height,
                                 const vk::Format &vkFormat);
        void destroy(const egl::Display *display);

        vk::ImageHelper image;
        vk::ImageView imageView;
        RenderTargetVk renderTarget;
    };

    angle::Result initializeImpl(DisplayVk *displayVk);

    EGLint mWidth;
    EGLint mHeight;

    AttachmentImage mColorAttachment;
    AttachmentImage mDepthStencilAttachment;
};

class WindowSurfaceVk : public SurfaceImpl
{
  public:
    WindowSurfaceVk(const egl::SurfaceState &surfaceState,
                    EGLNativeWindowType window,
                    EGLint width,
                    EGLint height);
    ~WindowSurfaceVk() override;

    void destroy(const egl::Display *display) override;

    egl::Error initialize(const egl::Display *display) override;
    FramebufferImpl *createDefaultFramebuffer(const gl::Context *context,
                                              const gl::FramebufferState &state) override;
    egl::Error swap(const gl::Context *context) override;
    egl::Error swapWithDamage(const gl::Context *context, EGLint *rects, EGLint n_rects) override;
    egl::Error postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(const gl::Context *context,
                            gl::Texture *texture,
                            EGLint buffer) override;
    egl::Error releaseTexImage(const gl::Context *context, EGLint buffer) override;
    egl::Error getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc) override;
    void setSwapInterval(EGLint interval) override;

    // width and height can change with client window resizing
    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

    angle::Result initializeContents(const gl::Context *context,
                                     const gl::ImageIndex &imageIndex) override;

    angle::Result getCurrentFramebuffer(vk::Context *context,
                                        const vk::RenderPass &compatibleRenderPass,
                                        vk::Framebuffer **framebufferOut);

    angle::Result generateSemaphoresForFlush(vk::Context *context,
                                             const vk::Semaphore **outWaitSemaphore,
                                             const vk::Semaphore **outSignalSempahore);

  protected:
    EGLNativeWindowType mNativeWindowType;
    VkSurfaceKHR mSurface;
    VkInstance mInstance;

  private:
    virtual angle::Result createSurfaceVk(vk::Context *context, gl::Extents *extentsOut) = 0;
    virtual angle::Result getCurrentWindowSize(vk::Context *context, gl::Extents *extentsOut) = 0;

    angle::Result initializeImpl(DisplayVk *displayVk);
    angle::Result recreateSwapchain(DisplayVk *displayVk,
                                    const gl::Extents &extents,
                                    uint32_t swapHistoryIndex);
    angle::Result checkForOutOfDateSwapchain(DisplayVk *displayVk,
                                             uint32_t swapHistoryIndex,
                                             bool presentOutOfDate);
    void releaseSwapchainImages(RendererVk *renderer);
    angle::Result nextSwapchainImage(DisplayVk *displayVk);
    angle::Result present(DisplayVk *displayVk,
                          EGLint *rects,
                          EGLint n_rects,
                          bool &swapchainOutOfDate);
    angle::Result swapImpl(DisplayVk *displayVk, EGLint *rects, EGLint n_rects);

    VkSurfaceCapabilitiesKHR mSurfaceCaps;
    std::vector<VkPresentModeKHR> mPresentModes;

    VkSwapchainKHR mSwapchain;
    // Cached information used to recreate swapchains.
    VkPresentModeKHR mSwapchainPresentMode;         // Current swapchain mode
    VkPresentModeKHR mDesiredSwapchainPresentMode;  // Desired mode set through setSwapInterval()
    uint32_t mMinImageCount;
    VkSurfaceTransformFlagBitsKHR mPreTransform;
    VkCompositeAlphaFlagBitsKHR mCompositeAlpha;

    RenderTargetVk mColorRenderTarget;
    RenderTargetVk mDepthStencilRenderTarget;

    uint32_t mCurrentSwapchainImageIndex;

    struct SwapchainImage : angle::NonCopyable
    {
        SwapchainImage();
        SwapchainImage(SwapchainImage &&other);
        ~SwapchainImage();

        vk::ImageHelper image;
        vk::ImageView imageView;
        vk::Framebuffer framebuffer;
    };

    std::vector<SwapchainImage> mSwapchainImages;

    // Each time vkPresent is called, a wait semaphore is needed to know when the work to render the
    // frame is done. For ANGLE to know when that is, it needs to add a signal semaphore to each
    // flush. Conversely, before being able to use a swap chain image, ANGLE needs to wait on the
    // semaphore returned by vkAcquireNextImage.
    //
    // We build a chain of semaphores starting with the semaphore returned by vkAcquireNextImageKHR
    // and ending with the semaphore provided to vkPresent. Each time generateSemaphoresForFlush is
    // called, a new semaphore is created and appended to mFlushSemaphoreChain. The second last
    // semaphore is used as a wait semaphore and the last one is used as a signal semaphore for the
    // flush.
    //
    // The semaphore chain is cleared after every call to present and a new one is started once
    // vkAquireImage is called.
    //
    // We don't need a semaphore chain for offscreen surfaces or surfaceless rendering because the
    // results cannot affect the images in a swap chain.
    std::vector<vk::Semaphore> mFlushSemaphoreChain;

    // A circular buffer that stores the serial of the renderer on every swap.  The CPU is
    // throttled by waiting for the 2nd previous serial to finish.  Old swapchains are scheduled to
    // be destroyed at the same time.
    struct SwapHistory : angle::NonCopyable
    {
        SwapHistory();
        SwapHistory(SwapHistory &&other);
        SwapHistory &operator=(SwapHistory &&other);
        ~SwapHistory();

        void destroy(VkDevice device);

        Serial serial;
        std::vector<vk::Semaphore> semaphores;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    };
    static constexpr size_t kSwapHistorySize = 2;
    std::array<SwapHistory, kSwapHistorySize> mSwapHistory;
    size_t mCurrentSwapHistoryIndex;

    vk::ImageHelper mDepthStencilImage;
    vk::ImageView mDepthStencilImageView;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SURFACEVK_H_
