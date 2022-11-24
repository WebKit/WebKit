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

#include "common/CircularBuffer.h"
#include "common/vulkan/vk_headers.h"
#include "libANGLE/renderer/SurfaceImpl.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class RendererVk;

class SurfaceVk : public SurfaceImpl, public angle::ObserverInterface
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

    void destroy(const egl::Display *display) override;
    // We monitor the staging buffer for changes. This handles staged data from outside this class.
    void onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message) override;

    RenderTargetVk mColorRenderTarget;
    RenderTargetVk mDepthStencilRenderTarget;
};

class OffscreenSurfaceVk : public SurfaceVk
{
  public:
    OffscreenSurfaceVk(const egl::SurfaceState &surfaceState, RendererVk *renderer);
    ~OffscreenSurfaceVk() override;

    egl::Error initialize(const egl::Display *display) override;
    void destroy(const egl::Display *display) override;

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
    egl::Error getMscRate(EGLint *numerator, EGLint *denominator) override;
    void setSwapInterval(EGLint interval) override;

    // width and height can change with client window resizing
    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    angle::Result initializeContents(const gl::Context *context,
                                     GLenum binding,
                                     const gl::ImageIndex &imageIndex) override;

    vk::ImageHelper *getColorAttachmentImage();

    egl::Error lockSurface(const egl::Display *display,
                           EGLint usageHint,
                           bool preservePixels,
                           uint8_t **bufferPtrOut,
                           EGLint *bufferPitchOut) override;
    egl::Error unlockSurface(const egl::Display *display, bool preservePixels) override;
    EGLint origin() const override;

    egl::Error attachToFramebuffer(const gl::Context *context,
                                   gl::Framebuffer *framebuffer) override;
    egl::Error detachFromFramebuffer(const gl::Context *context,
                                     gl::Framebuffer *framebuffer) override;

  protected:
    struct AttachmentImage final : angle::NonCopyable
    {
        AttachmentImage(SurfaceVk *surfaceVk);
        ~AttachmentImage();

        angle::Result initialize(DisplayVk *displayVk,
                                 EGLint width,
                                 EGLint height,
                                 const vk::Format &vkFormat,
                                 GLint samples,
                                 bool isRobustResourceInitEnabled,
                                 bool hasProtectedContent);

        void destroy(const egl::Display *display);

        vk::ImageHelper image;
        vk::ImageViewHelper imageViews;
        angle::ObserverBinding imageObserverBinding;
    };

    virtual angle::Result initializeImpl(DisplayVk *displayVk);

    EGLint mWidth;
    EGLint mHeight;

    AttachmentImage mColorAttachment;
    AttachmentImage mDepthStencilAttachment;

    // EGL_KHR_lock_surface3
    vk::BufferHelper mLockBufferHelper;
};

// Data structures used in WindowSurfaceVk
namespace impl
{
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
    ImagePresentHistory &operator=(ImagePresentHistory &&other);
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
    vk::Framebuffer fetchFramebuffer;
    vk::Framebuffer framebufferResolveMS;

    // A circular array of semaphores used for presenting this image.
    static constexpr size_t kPresentHistorySize = kSwapHistorySize + 1;
    angle::CircularBuffer<ImagePresentHistory, kPresentHistorySize> presentHistory;
    uint64_t mFrameNumber = 0;
};
}  // namespace impl

enum class FramebufferFetchMode
{
    Disabled,
    Enabled,
};

enum class SwapchainResolveMode
{
    Disabled,
    Enabled,
};

class WindowSurfaceVk : public SurfaceVk
{
  public:
    WindowSurfaceVk(const egl::SurfaceState &surfaceState, EGLNativeWindowType window);
    ~WindowSurfaceVk() override;

    void destroy(const egl::Display *display) override;

    egl::Error initialize(const egl::Display *display) override;
    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;
    egl::Error prepareSwap(const gl::Context *context) override;
    egl::Error swap(const gl::Context *context) override;
    egl::Error swapWithDamage(const gl::Context *context,
                              const EGLint *rects,
                              EGLint n_rects) override;
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
    egl::Error getMscRate(EGLint *numerator, EGLint *denominator) override;
    void setSwapInterval(EGLint interval) override;

    // width and height can change with client window resizing
    EGLint getWidth() const override;
    EGLint getHeight() const override;
    EGLint getRotatedWidth() const;
    EGLint getRotatedHeight() const;
    // Note: windows cannot be resized on Android.  The approach requires
    // calling vkGetPhysicalDeviceSurfaceCapabilitiesKHR.  However, that is
    // expensive; and there are troublesome timing issues for other parts of
    // ANGLE (which cause test failures and crashes).  Therefore, a
    // special-Android-only path is created just for the querying of EGL_WIDTH
    // and EGL_HEIGHT.
    // https://issuetracker.google.com/issues/153329980
    egl::Error getUserWidth(const egl::Display *display, EGLint *value) const override;
    egl::Error getUserHeight(const egl::Display *display, EGLint *value) const override;
    angle::Result getUserExtentsImpl(DisplayVk *displayVk,
                                     VkSurfaceCapabilitiesKHR *surfaceCaps) const;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    angle::Result initializeContents(const gl::Context *context,
                                     GLenum binding,
                                     const gl::ImageIndex &imageIndex) override;

    vk::Framebuffer &chooseFramebuffer(const SwapchainResolveMode swapchainResolveMode);

    angle::Result getCurrentFramebuffer(ContextVk *context,
                                        FramebufferFetchMode fetchMode,
                                        const vk::RenderPass &compatibleRenderPass,
                                        const SwapchainResolveMode swapchainResolveMode,
                                        vk::MaybeImagelessFramebuffer *framebufferOut);

    const vk::Semaphore *getAndResetAcquireImageSemaphore();

    VkSurfaceTransformFlagBitsKHR getPreTransform() const
    {
        if (mEmulatedPreTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        {
            return mEmulatedPreTransform;
        }
        return mPreTransform;
    }

    egl::Error setAutoRefreshEnabled(bool enabled) override;

    egl::Error getBufferAge(const gl::Context *context, EGLint *age) override;

    egl::Error setRenderBuffer(EGLint renderBuffer) override;

    bool isSharedPresentMode() const
    {
        return (mSwapchainPresentMode == vk::PresentMode::SharedDemandRefreshKHR ||
                mSwapchainPresentMode == vk::PresentMode::SharedContinuousRefreshKHR);
    }

    bool isSharedPresentModeDesired() const
    {
        return (mDesiredSwapchainPresentMode == vk::PresentMode::SharedDemandRefreshKHR ||
                mDesiredSwapchainPresentMode == vk::PresentMode::SharedContinuousRefreshKHR);
    }

    egl::Error lockSurface(const egl::Display *display,
                           EGLint usageHint,
                           bool preservePixels,
                           uint8_t **bufferPtrOut,
                           EGLint *bufferPitchOut) override;
    egl::Error unlockSurface(const egl::Display *display, bool preservePixels) override;
    EGLint origin() const override;

    egl::Error attachToFramebuffer(const gl::Context *context,
                                   gl::Framebuffer *framebuffer) override;
    egl::Error detachFromFramebuffer(const gl::Context *context,
                                     gl::Framebuffer *framebuffer) override;

    angle::Result onSharedPresentContextFlush(const gl::Context *context);

    bool hasStagedUpdates() const;

    void setTimestampsEnabled(bool enabled) override;

  protected:
    angle::Result prepareSwapImpl(const gl::Context *context);
    angle::Result swapImpl(const gl::Context *context,
                           const EGLint *rects,
                           EGLint n_rects,
                           const void *pNextChain);
    // Called when a swapchain image whose acquisition was deferred must be acquired.  This method
    // will recreate the swapchain (if needed) and call the acquireNextSwapchainImage() method.
    angle::Result doDeferredAcquireNextImage(const gl::Context *context, bool presentOutOfDate);

    EGLNativeWindowType mNativeWindowType;
    VkSurfaceKHR mSurface;
    VkSurfaceCapabilitiesKHR mSurfaceCaps;
    VkBool32 mSupportsProtectedSwapchain;

  private:
    virtual angle::Result createSurfaceVk(vk::Context *context, gl::Extents *extentsOut)      = 0;
    virtual angle::Result getCurrentWindowSize(vk::Context *context, gl::Extents *extentsOut) = 0;

    angle::Result initializeImpl(DisplayVk *displayVk);
    angle::Result recreateSwapchain(ContextVk *contextVk, const gl::Extents &extents);
    angle::Result createSwapChain(vk::Context *context,
                                  const gl::Extents &extents,
                                  VkSwapchainKHR oldSwapchain);
    angle::Result queryAndAdjustSurfaceCaps(ContextVk *contextVk,
                                            VkSurfaceCapabilitiesKHR *surfaceCaps);
    angle::Result checkForOutOfDateSwapchain(ContextVk *contextVk, bool presentOutOfDate);
    angle::Result resizeSwapchainImages(vk::Context *context, uint32_t imageCount);
    void releaseSwapchainImages(ContextVk *contextVk);
    void destroySwapChainImages(DisplayVk *displayVk);
    // This method calls vkAcquireNextImageKHR() to acquire the next swapchain image.  It is called
    // when the swapchain is initially created and when present() finds the swapchain out of date.
    // Otherwise, it is scheduled to be called later by deferAcquireNextImage().
    VkResult acquireNextSwapchainImage(vk::Context *context);
    // This method is called when a swapchain image is presented.  It schedules
    // acquireNextSwapchainImage() to be called later.
    void deferAcquireNextImage();

    angle::Result computePresentOutOfDate(vk::Context *context,
                                          VkResult result,
                                          bool *presentOutOfDate);
    angle::Result present(ContextVk *contextVk,
                          const EGLint *rects,
                          EGLint n_rects,
                          const void *pNextChain,
                          bool *presentOutOfDate);

    void updateOverlay(ContextVk *contextVk) const;
    bool overlayHasEnabledWidget(ContextVk *contextVk) const;
    angle::Result drawOverlay(ContextVk *contextVk, impl::SwapchainImage *image) const;

    angle::Result newPresentSemaphore(vk::Context *context, vk::Semaphore *semaphoreOut);

    bool isMultiSampled() const;

    bool supportsPresentMode(vk::PresentMode presentMode) const;

    std::vector<vk::PresentMode> mPresentModes;

    VkSwapchainKHR mSwapchain;
    // Cached information used to recreate swapchains.
    vk::PresentMode mSwapchainPresentMode;         // Current swapchain mode
    vk::PresentMode mDesiredSwapchainPresentMode;  // Desired mode set through setSwapInterval()
    uint32_t mMinImageCount;
    VkSurfaceTransformFlagBitsKHR mPreTransform;
    VkSurfaceTransformFlagBitsKHR mEmulatedPreTransform;
    VkCompositeAlphaFlagBitsKHR mCompositeAlpha;

    // A circular buffer that stores the serial of the submission fence of the context on every
    // swap. The CPU is throttled by waiting for the 2nd previous serial to finish.
    angle::CircularBuffer<Serial, impl::kSwapHistorySize> mSwapHistory;

    // The previous swapchain which needs to be scheduled for destruction when appropriate.  This
    // will be done when the first image of the current swapchain is presented.  If there were
    // older swapchains pending destruction when the swapchain is recreated, they will accumulate
    // and be destroyed with the previous swapchain.
    //
    // Note that if the user resizes the window such that the swapchain is recreated every frame,
    // this array can go grow indefinitely.
    std::vector<impl::SwapchainCleanupData> mOldSwapchains;

    std::vector<impl::SwapchainImage> mSwapchainImages;
    std::vector<angle::ObserverBinding> mSwapchainImageBindings;
    uint32_t mCurrentSwapchainImageIndex;

    // Given that the CPU is throttled after a number of swaps, there is an upper bound to the
    // number of semaphores that are used to acquire swapchain images, and that is
    // kSwapHistorySize+1:
    //
    //                    Unrelated submission in      Submission as part of
    //                      the middle of frame            buffer swap
    //                               |                          |
    //                               V                          V
    //     Frame i:     ... ANI ... QS (fence Fa) ... Wait(..) QS (Fence Fb) QP
    //     Frame i+1:   ... ANI ... QS (fence Fc) ... Wait(..) QS (Fence Fd) QP
    //     Frame i+2:   ... ANI ... QS (fence Fe) ... Wait(Fb) QS (Fence Ff) QP
    //                                                 ^
    //                                                 |
    //                                          CPU throttling
    //
    // In frame i+2 (2 is kSwapHistorySize), ANGLE waits on fence Fb which means that the semaphore
    // used for Frame i's ANI can be reused (because Fb-is-signalled implies Fa-is-signalled).
    // Before this wait, there were three acquire semaphores in use corresponding to frames i, i+1
    // and i+2.  Frame i+3 can reuse the semaphore of frame i.
    angle::CircularBuffer<vk::Semaphore, impl::kSwapHistorySize + 1> mAcquireImageSemaphores;
    // A pointer to mAcquireImageSemaphores.  This is set when an image is acquired and is waited on
    // by the next submission (which uses this image), at which point it is reset so future
    // submissions don't wait on it until the next acquire.
    const vk::Semaphore *mAcquireImageSemaphore;

    // There is no direct signal from Vulkan regarding when a Present semaphore can be be reused.
    // During window resizing when swapchains are recreated every frame, the number of in-flight
    // present semaphores can grow indefinitely.  See doc/PresentSemaphores.md.
    vk::Recycler<vk::Semaphore> mPresentSemaphoreRecycler;

    // Depth/stencil image.  Possibly multisampled.
    vk::ImageHelper mDepthStencilImage;
    vk::ImageViewHelper mDepthStencilImageViews;
    angle::ObserverBinding mDepthStencilImageBinding;

    // Multisample color image, view and framebuffer, if multisampling enabled.
    vk::ImageHelper mColorImageMS;
    vk::ImageViewHelper mColorImageMSViews;
    angle::ObserverBinding mColorImageMSBinding;
    vk::Framebuffer mFramebufferMS;

    // True when acquiring the next image is deferred.
    bool mNeedToAcquireNextSwapchainImage;

    // EGL_EXT_buffer_age: Track frame count.
    uint64_t mFrameCount;

    // EGL_KHR_lock_surface3
    vk::BufferHelper mLockBufferHelper;

    // EGL_KHR_partial_update
    uint64_t mBufferAgeQueryFrameNumber;

    // GL_EXT_shader_framebuffer_fetch
    FramebufferFetchMode mFramebufferFetchMode = FramebufferFetchMode::Disabled;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SURFACEVK_H_
