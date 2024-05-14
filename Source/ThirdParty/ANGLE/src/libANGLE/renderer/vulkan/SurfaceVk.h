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
#include "common/SimpleMutex.h"
#include "common/vulkan/vk_headers.h"
#include "libANGLE/renderer/SurfaceImpl.h"
#include "libANGLE/renderer/vulkan/CommandProcessor.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class SurfaceVk : public SurfaceImpl, public angle::ObserverInterface, public vk::Resource
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
    OffscreenSurfaceVk(const egl::SurfaceState &surfaceState, vk::Renderer *renderer);
    ~OffscreenSurfaceVk() override;

    egl::Error initialize(const egl::Display *display) override;
    void destroy(const egl::Display *display) override;

    egl::Error unMakeCurrent(const gl::Context *context) override;
    const vk::ImageHelper *getColorImage() const { return &mColorAttachment.image; }

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

// Old swapchain and associated present fences/semaphores that need to be scheduled for
// recycling/destruction when appropriate.
struct SwapchainCleanupData : angle::NonCopyable
{
    SwapchainCleanupData();
    SwapchainCleanupData(SwapchainCleanupData &&other);
    ~SwapchainCleanupData();

    // Fences must not be empty (VK_EXT_swapchain_maintenance1 is supported).
    VkResult getFencesStatus(VkDevice device) const;
    // Waits fences if any. Use before force destroying the swapchain.
    void waitFences(VkDevice device, uint64_t timeout) const;
    void destroy(VkDevice device,
                 vk::Recycler<vk::Fence> *fenceRecycler,
                 vk::Recycler<vk::Semaphore> *semaphoreRecycler);

    // The swapchain to be destroyed.
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    // Any present fences/semaphores that were pending recycle at the time the swapchain was
    // recreated will be scheduled for recycling at the same time as the swapchain's destruction.
    // fences must be in the present operation order.
    std::vector<vk::Fence> fences;
    std::vector<vk::Semaphore> semaphores;
};

// Each present operation is associated with a wait semaphore.  To know when that semaphore can be
// recycled, a swapSerial is used.  When that swapSerial is finished, the semaphore used in the
// previous present operation involving imageIndex can be recycled.  See doc/PresentSemaphores.md
// for details.
// When VK_EXT_swapchain_maintenance1 is supported, present fence is used instead of the swapSerial.
//
// Old swapchains are scheduled to be destroyed at the same time as the last wait semaphore used to
// present an image to the old swapchains can be recycled (only relevant when
// VK_EXT_swapchain_maintenance1 is not supported).
struct ImagePresentOperation : angle::NonCopyable
{
    ImagePresentOperation();
    ImagePresentOperation(ImagePresentOperation &&other);
    ImagePresentOperation &operator=(ImagePresentOperation &&other);
    ~ImagePresentOperation();

    void destroy(VkDevice device,
                 vk::Recycler<vk::Fence> *fenceRecycler,
                 vk::Recycler<vk::Semaphore> *semaphoreRecycler);

    // fence is only used when VK_EXT_swapchain_maintenance1 is supported.
    vk::Fence fence;
    vk::Semaphore semaphore;

    // Below members only relevant when VK_EXT_swapchain_maintenance1 is not supported.
    // Used to associate a swapSerial with the previous present operation of the image.
    uint32_t imageIndex;
    QueueSerial queueSerial;
    std::deque<SwapchainCleanupData> oldSwapchains;
};

// Swapchain images and their associated objects.
struct SwapchainImage : angle::NonCopyable
{
    SwapchainImage();
    SwapchainImage(SwapchainImage &&other);
    ~SwapchainImage();

    std::unique_ptr<vk::ImageHelper> image;
    vk::ImageViewHelper imageViews;
    vk::Framebuffer framebuffer;
    vk::Framebuffer fetchFramebuffer;

    uint64_t frameNumber = 0;
};

// Associated data for a call to vkAcquireNextImageKHR without necessarily holding the share group
// lock.
struct UnlockedTryAcquireData : angle::NonCopyable
{
    // A mutex to protect against concurrent attempts to call vkAcquireNextImageKHR.
    angle::SimpleMutex mutex;

    // Given that the CPU is throttled after a number of swaps, there is an upper bound to the
    // number of semaphores that are used to acquire swapchain images, and that is
    // kSwapHistorySize+1:
    //
    //             Unrelated submission in     Submission as part of
    //               the middle of frame          buffer swap
    //                              |                 |
    //                              V                 V
    //     Frame i:     ... ANI ... QS (fence Fa) ... QS (Fence Fb) QP Wait(..)
    //     Frame i+1:   ... ANI ... QS (fence Fc) ... QS (Fence Fd) QP Wait(..) <--\
    //     Frame i+2:   ... ANI ... QS (fence Fe) ... QS (Fence Ff) QP Wait(Fb)    |
    //                                                                  ^          |
    //                                                                  |          |
    //                                                           CPU throttling    |
    //                                                                             |
    //                               Note: app should throttle itself here (equivalent of Wait(Fb))
    //
    // In frame i+2 (2 is kSwapHistorySize), ANGLE waits on fence Fb which means that the semaphore
    // used for Frame i's ANI can be reused (because Fb-is-signalled implies Fa-is-signalled).
    // Before this wait, there were three acquire semaphores in use corresponding to frames i, i+1
    // and i+2.  Frame i+3 can reuse the semaphore of frame i.
    angle::CircularBuffer<vk::Semaphore, impl::kSwapHistorySize + 1> acquireImageSemaphores;
};

struct UnlockedTryAcquireResult : angle::NonCopyable
{
    // The result of the call to vkAcquireNextImageKHR.  This result is processed later under the
    // share group lock.
    VkResult result = VK_SUCCESS;

    // Semaphore to signal.
    VkSemaphore acquireSemaphore = VK_NULL_HANDLE;

    // Image index that was acquired
    uint32_t imageIndex = std::numeric_limits<uint32_t>::max();
};

struct ImageAcquireOperation : angle::NonCopyable
{
    ImageAcquireOperation();

    // True when acquiring the next image is deferred.
    std::atomic<bool> needToAcquireNextSwapchainImage;

    // Data used to call vkAcquireNextImageKHR without necessarily holding the share group lock.
    // The result of this operation can be found in mAcquireOperation.unlockedTryAcquireResult,
    // which is processed once the share group lock is taken in the future.
    //
    // |unlockedTryAcquireData::mutex| is necessary to hold when making the vkAcquireNextImageKHR
    // call as multiple contexts in the share group may end up provoking it (only one may be calling
    // it without the share group lock though, the one calling eglPrepareSwapBuffersANGLE).  During
    // processing of the results however (for example in the following eglSwapBuffers call, or if
    // called during a GL call, immediately afterwards), the contents of |unlockedTryAcquireResult|
    // can be accessed without |unlockedTryAcquireData::mutex| because the share group lock is
    // already taken, and no thread can be attempting an unlocked vkAcquireNextImageKHR.
    UnlockedTryAcquireData unlockedTryAcquireData;
    UnlockedTryAcquireResult unlockedTryAcquireResult;
};
}  // namespace impl

enum class FramebufferFetchMode
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

    egl::Error unMakeCurrent(const gl::Context *context) override;

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

    vk::Framebuffer &chooseFramebuffer();

    angle::Result getCurrentFramebuffer(ContextVk *context,
                                        FramebufferFetchMode fetchMode,
                                        const vk::RenderPass &compatibleRenderPass,
                                        vk::Framebuffer *framebufferOut);

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
    angle::Result swapImpl(const gl::Context *context,
                           const EGLint *rects,
                           EGLint n_rects,
                           const void *pNextChain);
    // Called when a swapchain image whose acquisition was deferred must be acquired.  This method
    // will recreate the swapchain (if needed due to present returning OUT_OF_DATE, swap interval
    // changing, surface size changing etc, by calling prepareForAcquireNextSwapchainImage()) and
    // call the doDeferredAcquireNextImageWithUsableSwapchain() method.
    angle::Result doDeferredAcquireNextImage(const gl::Context *context, bool presentOutOfDate);
    // Calls acquireNextSwapchainImage() and sets up the acquired image.  On some platforms,
    // vkAcquireNextImageKHR returns OUT_OF_DATE instead of present, so this function may still
    // recreate the swapchain.  The main difference with doDeferredAcquireNextImage is that it does
    // not check for surface property changes for the purposes of swapchain recreation (because
    // that's already done by prepareForAcquireNextSwapchainImage.
    angle::Result doDeferredAcquireNextImageWithUsableSwapchain(const gl::Context *context);

    EGLNativeWindowType mNativeWindowType;
    VkSurfaceKHR mSurface;
    VkSurfaceCapabilitiesKHR mSurfaceCaps;
    VkBool32 mSupportsProtectedSwapchain;

  private:
    virtual angle::Result createSurfaceVk(vk::Context *context, gl::Extents *extentsOut)      = 0;
    virtual angle::Result getCurrentWindowSize(vk::Context *context, gl::Extents *extentsOut) = 0;

    angle::Result initializeImpl(DisplayVk *displayVk, bool *anyMatchesOut);
    angle::Result recreateSwapchain(ContextVk *contextVk, const gl::Extents &extents);
    angle::Result createSwapChain(vk::Context *context,
                                  const gl::Extents &extents,
                                  VkSwapchainKHR oldSwapchain);
    angle::Result queryAndAdjustSurfaceCaps(ContextVk *contextVk,
                                            VkSurfaceCapabilitiesKHR *surfaceCaps);
    angle::Result checkForOutOfDateSwapchain(ContextVk *contextVk,
                                             bool presentOutOfDate,
                                             bool *swapchainRecreatedOut);
    angle::Result resizeSwapchainImages(vk::Context *context, uint32_t imageCount);
    void releaseSwapchainImages(ContextVk *contextVk);
    void destroySwapChainImages(DisplayVk *displayVk);
    angle::Result prepareForAcquireNextSwapchainImage(const gl::Context *context,
                                                      bool presentOutOfDate,
                                                      bool *swapchainRecreatedOut);
    // This method calls vkAcquireNextImageKHR() to acquire the next swapchain image.  It is called
    // when the swapchain is initially created and when present() finds the swapchain out of date.
    // Otherwise, it is scheduled to be called later by deferAcquireNextImage().
    VkResult acquireNextSwapchainImage(vk::Context *context);
    // Process the result of vkAcquireNextImageKHR, which may have been done previously without
    // holding a lock.
    VkResult postProcessUnlockedTryAcquire(vk::Context *context);
    // Whether vkAcquireNextImageKHR needs to be called or its results processed
    bool needsAcquireImageOrProcessResult() const;
    // This method is called when a swapchain image is presented.  It schedules
    // acquireNextSwapchainImage() to be called later.
    void deferAcquireNextImage();
    bool skipAcquireNextSwapchainImageForSharedPresentMode() const;

    angle::Result computePresentOutOfDate(vk::Context *context,
                                          VkResult result,
                                          bool *presentOutOfDate);
    angle::Result prePresentSubmit(ContextVk *contextVk, const vk::Semaphore &presentSemaphore);
    angle::Result present(ContextVk *contextVk,
                          const EGLint *rects,
                          EGLint n_rects,
                          const void *pNextChain,
                          bool *presentOutOfDate);

    angle::Result cleanUpPresentHistory(vk::Context *context);
    angle::Result cleanUpOldSwapchains(vk::Context *context);

    // Throttle the CPU such that application's logic and command buffer recording doesn't get more
    // than two frame ahead of the frame being rendered (and three frames ahead of the one being
    // presented).  This is a failsafe, as the application should ensure command buffer recording is
    // not ahead of the frame being rendered by *one* frame.
    angle::Result throttleCPU(vk::Context *context, const QueueSerial &currentSubmitSerial);

    // Finish all GPU operations on the surface
    angle::Result finish(vk::Context *context);

    void updateOverlay(ContextVk *contextVk) const;
    bool overlayHasEnabledWidget(ContextVk *contextVk) const;
    angle::Result drawOverlay(ContextVk *contextVk, impl::SwapchainImage *image) const;

    bool isMultiSampled() const;

    bool supportsPresentMode(vk::PresentMode presentMode) const;

    bool updateColorSpace(DisplayVk *displayVk);

    angle::FormatID getIntendedFormatID(vk::Renderer *renderer);
    angle::FormatID getActualFormatID(vk::Renderer *renderer);

    std::vector<vk::PresentMode> mPresentModes;

    VkSwapchainKHR mSwapchain;
    vk::SwapchainStatus mSwapchainStatus;
    // Cached information used to recreate swapchains.
    vk::PresentMode mSwapchainPresentMode;         // Current swapchain mode
    vk::PresentMode mDesiredSwapchainPresentMode;  // Desired mode set through setSwapInterval()
    uint32_t mMinImageCount;
    VkSurfaceTransformFlagBitsKHR mPreTransform;
    VkSurfaceTransformFlagBitsKHR mEmulatedPreTransform;
    VkCompositeAlphaFlagBitsKHR mCompositeAlpha;
    VkColorSpaceKHR mSurfaceColorSpace;

    // Present modes that are compatible with the current mode.  If mDesiredSwapchainPresentMode is
    // in this list, mode switch can happen without the need to recreate the swapchain.  Fast
    // vector's size is 6, as there are currently only 6 possible present modes.
    static constexpr uint32_t kMaxCompatiblePresentModes = 6;
    angle::FixedVector<VkPresentModeKHR, kMaxCompatiblePresentModes> mCompatiblePresentModes;

    // A circular buffer that stores the serial of the submission fence of the context on every
    // swap. The CPU is throttled by waiting for the 2nd previous serial to finish.  This should
    // normally be a no-op, as the application should pace itself to avoid input lag, and is
    // implemented in ANGLE as a fail safe.  Removing this throttling requires untangling it from
    // acquire semaphore recycling (see mAcquireImageSemaphores above)
    angle::CircularBuffer<QueueSerial, impl::kSwapHistorySize> mSwapHistory;

    // The previous swapchain which needs to be scheduled for destruction when appropriate.  This
    // will be done when the first image of the current swapchain is presented or when fences are
    // signaled (when VK_EXT_swapchain_maintenance1 is supported).  If there were older swapchains
    // pending destruction when the swapchain is recreated, they will accumulate and be destroyed
    // with the previous swapchain.
    //
    // Note that if the user resizes the window such that the swapchain is recreated every frame,
    // this array can go grow indefinitely.
    std::deque<impl::SwapchainCleanupData> mOldSwapchains;

    std::vector<impl::SwapchainImage> mSwapchainImages;
    std::vector<angle::ObserverBinding> mSwapchainImageBindings;
    uint32_t mCurrentSwapchainImageIndex;

    // There is no direct signal from Vulkan regarding when a Present semaphore can be be reused.
    // During window resizing when swapchains are recreated every frame, the number of in-flight
    // present semaphores can grow indefinitely.  See doc/PresentSemaphores.md.
    vk::Recycler<vk::Semaphore> mPresentSemaphoreRecycler;
    // Fences are associated with present semaphores to know when they can be recycled.
    vk::Recycler<vk::Fence> mPresentFenceRecycler;

    // The presentation history, used to recycle semaphores and destroy old swapchains.
    std::deque<impl::ImagePresentOperation> mPresentHistory;

    // Depth/stencil image.  Possibly multisampled.
    vk::ImageHelper mDepthStencilImage;
    vk::ImageViewHelper mDepthStencilImageViews;
    angle::ObserverBinding mDepthStencilImageBinding;

    // Multisample color image, view and framebuffer, if multisampling enabled.
    vk::ImageHelper mColorImageMS;
    vk::ImageViewHelper mColorImageMSViews;
    angle::ObserverBinding mColorImageMSBinding;
    vk::Framebuffer mFramebufferMS;

    impl::ImageAcquireOperation mAcquireOperation;

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
