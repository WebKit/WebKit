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

#include "volk.h"

#include "libANGLE/renderer/SurfaceImpl.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class RendererVk;

class SurfaceVk : public SurfaceImpl
{
  public:
    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

  protected:
    SurfaceVk(const egl::SurfaceState &surfaceState);
    ~SurfaceVk() override;

    RenderTargetVk mColorRenderTarget;
    RenderTargetVk mDepthStencilRenderTarget;
};

class OffscreenSurfaceVk : public SurfaceVk
{
  public:
    OffscreenSurfaceVk(const egl::SurfaceState &surfaceState);
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
                                 const vk::Format &vkFormat,
                                 GLint samples);
        void destroy(const egl::Display *display);

        vk::ImageHelper image;
        vk::ImageViewHelper imageViews;
    };

    angle::Result initializeImpl(DisplayVk *displayVk);

    EGLint mWidth;
    EGLint mHeight;

    AttachmentImage mColorAttachment;
    AttachmentImage mDepthStencilAttachment;
};

// Data structures used in WindowSurfaceVk
namespace impl
{
// The submission fence of the context used to throttle the CPU.
struct SwapHistory : angle::NonCopyable
{
    SwapHistory();
    SwapHistory(SwapHistory &&other) = delete;
    SwapHistory &operator=(SwapHistory &&other) = delete;
    ~SwapHistory();

    void destroy(RendererVk *renderer);

    angle::Result waitFence(ContextVk *contextVk);

    // Fence associated with the last submitted work to render to this swapchain image.
    vk::Shared<vk::Fence> sharedFence;
};
static constexpr size_t kSwapHistorySize = 2;

// Old swapchain and associated present semaphores that need to be scheduled for destruction when
// appropriate.
struct SwapchainCleanupData : angle::NonCopyable
{
    SwapchainCleanupData();
    SwapchainCleanupData(SwapchainCleanupData &&other);
    ~SwapchainCleanupData();

    void destroy(VkDevice device, vk::Recycler<vk::Semaphore> *semaphoreRecycler);

    // The swapchain to be destroyed.
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    // Any present semaphores that were pending destruction at the time the swapchain was
    // recreated will be scheduled for destruction at the same time as the swapchain.
    std::vector<vk::Semaphore> semaphores;
};

// A circular buffer per image stores the semaphores used for presenting that image.  Taking the
// swap history into account, only the oldest semaphore is guaranteed to be no longer in use by the
// presentation engine.  See doc/PresentSemaphores.md for details.
//
// Old swapchains are scheduled to be destroyed at the same time as the first semaphore used to
// present an image of the new swapchain.  This is to ensure that the presentation engine is no
// longer presenting an image from the old swapchain.
struct ImagePresentHistory : angle::NonCopyable
{
    ImagePresentHistory();
    ImagePresentHistory(ImagePresentHistory &&other);
    ~ImagePresentHistory();

    vk::Semaphore semaphore;
    std::vector<SwapchainCleanupData> oldSwapchains;
};

// Swapchain images and their associated objects.
struct SwapchainImage : angle::NonCopyable
{
    SwapchainImage();
    SwapchainImage(SwapchainImage &&other);
    ~SwapchainImage();

    vk::ImageHelper image;
    vk::ImageViewHelper imageViews;
    vk::Framebuffer framebuffer;

    // A circular array of semaphores used for presenting this image.
    static constexpr size_t kPresentHistorySize = kSwapHistorySize + 1;
    std::array<ImagePresentHistory, kPresentHistorySize> presentHistory;
    size_t currentPresentHistoryIndex = 0;
};
}  // namespace impl

class WindowSurfaceVk : public SurfaceVk
{
  public:
    WindowSurfaceVk(const egl::SurfaceState &surfaceState, EGLNativeWindowType window);
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

    angle::Result initializeContents(const gl::Context *context,
                                     const gl::ImageIndex &imageIndex) override;

    angle::Result getCurrentFramebuffer(ContextVk *context,
                                        const vk::RenderPass &compatibleRenderPass,
                                        vk::Framebuffer **framebufferOut);

    vk::Semaphore getAcquireImageSemaphore();

  protected:
    angle::Result swapImpl(const gl::Context *context,
                           EGLint *rects,
                           EGLint n_rects,
                           const void *pNextChain);

    EGLNativeWindowType mNativeWindowType;
    VkSurfaceKHR mSurface;
    VkSurfaceCapabilitiesKHR mSurfaceCaps;

  private:
    virtual angle::Result createSurfaceVk(vk::Context *context, gl::Extents *extentsOut)      = 0;
    virtual angle::Result getCurrentWindowSize(vk::Context *context, gl::Extents *extentsOut) = 0;

    angle::Result initializeImpl(DisplayVk *displayVk);
    angle::Result recreateSwapchain(ContextVk *contextVk,
                                    const gl::Extents &extents,
                                    uint32_t swapHistoryIndex);
    angle::Result createSwapChain(vk::Context *context,
                                  const gl::Extents &extents,
                                  VkSwapchainKHR oldSwapchain);
    angle::Result checkForOutOfDateSwapchain(ContextVk *contextVk,
                                             uint32_t swapHistoryIndex,
                                             bool presentOutOfDate);
    angle::Result resizeSwapchainImages(vk::Context *context, uint32_t imageCount);
    void releaseSwapchainImages(ContextVk *contextVk);
    void destroySwapChainImages(DisplayVk *displayVk);
    VkResult nextSwapchainImage(vk::Context *context);
    angle::Result present(ContextVk *contextVk,
                          EGLint *rects,
                          EGLint n_rects,
                          const void *pNextChain,
                          bool *presentOutOfDate);

    angle::Result updateAndDrawOverlay(ContextVk *contextVk, impl::SwapchainImage *image) const;

    angle::Result newPresentSemaphore(vk::Context *context, vk::Semaphore *semaphoreOut);

    bool isMultiSampled() const;

    std::vector<VkPresentModeKHR> mPresentModes;

    VkSwapchainKHR mSwapchain;
    // Cached information used to recreate swapchains.
    VkPresentModeKHR mSwapchainPresentMode;         // Current swapchain mode
    VkPresentModeKHR mDesiredSwapchainPresentMode;  // Desired mode set through setSwapInterval()
    uint32_t mMinImageCount;
    VkSurfaceTransformFlagBitsKHR mPreTransform;
    VkCompositeAlphaFlagBitsKHR mCompositeAlpha;

    // A circular buffer that stores the submission fence of the context on every swap.  The CPU is
    // throttled by waiting for the 2nd previous serial to finish.
    std::array<impl::SwapHistory, impl::kSwapHistorySize> mSwapHistory;
    size_t mCurrentSwapHistoryIndex;

    // The previous swapchain which needs to be scheduled for destruction when appropriate.  This
    // will be done when the first image of the current swapchain is presented.  If there were
    // older swapchains pending destruction when the swapchain is recreated, they will accumulate
    // and be destroyed with the previous swapchain.
    //
    // Note that if the user resizes the window such that the swapchain is recreated every frame,
    // this array can go grow indefinitely.
    std::vector<impl::SwapchainCleanupData> mOldSwapchains;

    std::vector<impl::SwapchainImage> mSwapchainImages;
    vk::Semaphore mAcquireImageSemaphore;
    uint32_t mCurrentSwapchainImageIndex;

    vk::Recycler<vk::Semaphore> mPresentSemaphoreRecycler;

    // Depth/stencil image.  Possibly multisampled.
    vk::ImageHelper mDepthStencilImage;
    vk::ImageViewHelper mDepthStencilImageViews;

    // Multisample color image, view and framebuffer, if multisampling enabled.
    vk::ImageHelper mColorImageMS;
    vk::ImageViewHelper mColorImageMSViews;
    vk::Framebuffer mFramebufferMS;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SURFACEVK_H_
