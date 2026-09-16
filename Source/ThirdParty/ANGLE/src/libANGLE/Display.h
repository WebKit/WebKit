//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.h: Defines the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#ifndef LIBANGLE_DISPLAY_H_
#define LIBANGLE_DISPLAY_H_

#include <atomic>
#include <mutex>
#include <vector>

#include "common/SimpleMutex.h"
#include "common/WorkerThread.h"
#include "common/platform.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/HandleAllocator.h"
#include "libANGLE/LoggingAnnotator.h"
#include "libANGLE/MemoryProgramCache.h"
#include "libANGLE/MemoryShaderCache.h"
#include "libANGLE/ObjectMap.h"
#include "libANGLE/Observer.h"
#include "libANGLE/ShareGroup.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Version.h"
#include "platform/Feature.h"
#include "platform/autogen/FrontendFeatures_autogen.h"

namespace angle
{
class FrameCaptureShared;
}  // namespace angle

namespace gl
{
class Context;
class TextureManager;
class SemaphoreManager;
}  // namespace gl

namespace rx
{
class DisplayImpl;
class EGLImplFactory;
}  // namespace rx

namespace egl
{
class Device;
class Image;
class Stream;
class Surface;
class Sync;
class ScopedSyncRef;
class Thread;

template <typename DisplayT>
class ScopedDisplayRefT;
using ScopedDisplayRef      = ScopedDisplayRefT<Display>;
using ScopedConstDisplayRef = ScopedDisplayRefT<const Display>;

class ScopedDisplayMutexLock;

template <typename DisplayT>
class ScopedDisplayLockAndRefT;
using ScopedDisplayLockAndRef      = ScopedDisplayLockAndRefT<Display>;
using ScopedConstDisplayLockAndRef = ScopedDisplayLockAndRefT<const Display>;
using ScopedDisplayRefAndLock      = ScopedDisplayLockAndRef;
using ScopedConstDisplayRefAndLock = ScopedConstDisplayLockAndRef;

using SurfaceMap = priv::ObjectMap<Surface, angle::SimpleMutex>;
using ThreadSet  = angle::HashSet<Thread *>;

struct DisplayState final : private angle::NonCopyable
{
    DisplayState(EGLNativeDisplayType nativeDisplayId);
    ~DisplayState();

    void notifyDeviceLost() const;

    EGLLabelKHR label;
    ContextMap contextMap;
    SurfaceMap surfaceMap;
    angle::FeatureOverrides featureOverrides;
    EGLNativeDisplayType displayId;

    // Single-threaded and multithread pools for use by various parts of ANGLE, such as shader
    // compilation.  These pools are internally synchronized.
    std::shared_ptr<angle::WorkerThreadPool> singleThreadPool;
    std::shared_ptr<angle::WorkerThreadPool> multiThreadPool;

    mutable bool deviceLost;
};

// Constant coded here as a reasonable limit.
constexpr EGLAttrib kProgramCacheSizeAbsoluteMax = 0x4000000;

using ImageMap  = angle::HashMap<GLuint, Image *>;
using StreamSet = angle::HashSet<Stream *>;
using SyncMap   = angle::HashMap<GLuint, Sync *>;

class [[nodiscard]] ScopedSyncMap final : angle::NonCopyable
{
  public:
    ScopedSyncMap(angle::SimpleMutex &mutex, const SyncMap &syncMap)
        : mLock(mutex), mSyncMap(syncMap)
    {}

    ScopedSyncMap(ScopedSyncMap &&other) noexcept
        : mLock(std::move(other.mLock)), mSyncMap(other.mSyncMap)
    {}

    SyncMap::const_iterator begin() const { return mSyncMap.begin(); }
    SyncMap::const_iterator end() const { return mSyncMap.end(); }
    bool empty() const { return mSyncMap.empty(); }
    size_t size() const { return mSyncMap.size(); }

  private:
    std::unique_lock<angle::SimpleMutex> mLock;
    const SyncMap &mSyncMap;
};

class SyncSet final : angle::NonCopyable
{
  public:
    SyncSet();
    ~SyncSet();

    Error createSync(Display *display,
                     const gl::Context *currentContext,
                     EGLenum type,
                     const AttributeMap &attribs,
                     Sync **outSync);

    ScopedSyncRef getSync(Display *display, SyncID syncID) const;

    void destroySync(Display *display, SyncID syncID);
    void releaseSync(Display *display, Sync *sync);

    void invalidateAllSyncs();
    void destroyAllInvalidSyncs(Display *display);
    void clearPools();

    ScopedSyncMap getSyncsForCapture() const { return ScopedSyncMap(mMutex, mSyncMap); }

  private:
    static constexpr size_t kMaxSyncPoolSizePerType = 32;
    using SyncPool = angle::FixedVector<std::unique_ptr<Sync>, kMaxSyncPoolSizePerType>;

    void releaseSyncImpl(Display *display, Sync *sync);

    mutable angle::SimpleMutex mMutex;
    SyncMap mSyncMap;
    SyncMap mInvalidSyncMap;
    std::map<EGLenum, SyncPool> mSyncPools;
    gl::HandleAllocator mHandleAllocator;
};

class Display final : public LabeledObject,
                      public angle::ObserverInterface,
                      public angle::NonCopyable
{
  public:
    ~Display() override;

    void setLabel(EGLLabelKHR label) override;
    EGLLabelKHR getLabel() const override;

    // Observer implementation.
    void onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message) override;

    Error initialize();

    enum class TerminateReason
    {
        Api,
        InternalCleanup,

        InvalidEnum,
        EnumCount = InvalidEnum,
    };
    Error terminate(Thread *thread, TerminateReason terminateReason);

    // Called on eglReleaseThread. Backends can tear down thread-specific backend state through
    // this function.
    Error releaseThread();

    static Display *GetDisplayFromDevice(Device *device, const AttributeMap &attribMap);
    static Display *GetDisplayFromNativeDisplay(EGLenum platform,
                                                EGLNativeDisplayType nativeDisplay,
                                                const AttributeMap &attribMap);
    static Display *GetExistingDisplayFromNativeDisplay(EGLNativeDisplayType nativeDisplay);

    using EglDisplaySet = angle::HashSet<Display *>;

    static const ClientExtensions &GetClientExtensions();
    static const std::string &GetClientExtensionString();

    std::vector<const Config *> getConfigs(const AttributeMap &attribs) const;
    std::vector<const Config *> chooseConfig(const AttributeMap &attribs) const;

    Error createWindowSurface(const Config *configuration,
                              EGLNativeWindowType window,
                              const AttributeMap &attribs,
                              Surface **outSurface);
    Error createPbufferSurface(const Config *configuration,
                               const AttributeMap &attribs,
                               Surface **outSurface);
    Error createPbufferFromClientBuffer(const Config *configuration,
                                        EGLenum buftype,
                                        EGLClientBuffer clientBuffer,
                                        const AttributeMap &attribs,
                                        Surface **outSurface);
    Error createPixmapSurface(const Config *configuration,
                              NativePixmapType nativePixmap,
                              const AttributeMap &attribs,
                              Surface **outSurface);

    Error createImage(const gl::Context *context,
                      EGLenum target,
                      EGLClientBuffer buffer,
                      const AttributeMap &attribs,
                      Image **outImage);

    Error createStream(const AttributeMap &attribs, Stream **outStream);

    Error createContext(const Config *configuration,
                        gl::Context *shareContext,
                        const AttributeMap &attribs,
                        gl::Context **outContext);

    Error createSync(const gl::Context *currentContext,
                     EGLenum type,
                     const AttributeMap &attribs,
                     Sync **outSync);

    Error makeCurrent(Thread *thread,
                      gl::Context *previousContext,
                      Surface *drawSurface,
                      Surface *readSurface,
                      gl::Context *context);

    Error destroySurface(Surface *surface);
    void destroyImage(Image *image);
    void destroyStream(Stream *stream);
    Error destroyContext(Thread *thread, gl::Context *context);

    void destroySync(Sync *sync);

    bool isInitialized() const;
    bool isValidConfig(const Config *config) const;
    bool isValidContext(gl::ContextID contextID) const;
    bool isValidSurface(SurfaceID surfaceID) const;
    bool isValidImage(ImageID imageID) const;
    bool isValidStream(const Stream *stream) const;
    bool isValidSync(SyncID sync) const;
    bool isValidNativeWindow(EGLNativeWindowType window) const;

    Error validateClientBuffer(const Config *configuration,
                               EGLenum buftype,
                               EGLClientBuffer clientBuffer,
                               const AttributeMap &attribs) const;
    Error validateImageClientBuffer(const gl::Context *context,
                                    EGLenum target,
                                    EGLClientBuffer clientBuffer,
                                    const egl::AttributeMap &attribs) const;
    Error validatePixmap(const Config *config,
                         EGLNativePixmapType pixmap,
                         const AttributeMap &attributes) const;

    static bool isValidDisplay(const Display *display);
    static bool isValidNativeDisplay(EGLNativeDisplayType display);
    static bool hasExistingWindowSurface(EGLNativeWindowType window);

    bool isDeviceLost() const;
    bool testDeviceLost();
    void notifyDeviceLost();

    void setBlobCacheFuncs(EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get);
    bool areBlobCacheFuncsSet() const { return mBlobCache.areBlobCacheFuncsSet(); }
    BlobCache &getBlobCache() { return mBlobCache; }

    static EGLClientBuffer GetNativeClientBuffer(const struct AHardwareBuffer *buffer);
    static Error CreateNativeClientBuffer(const egl::AttributeMap &attribMap,
                                          EGLClientBuffer *eglClientBuffer);

    Error waitClient(const gl::Context *context);
    Error waitNative(const gl::Context *context, EGLint engine);

    const Caps &getCaps() const;

    const DisplayExtensions &getExtensions() const;
    const std::string &getExtensionString() const;
    const std::string &getVendorString() const;
    const std::string &getVersionString() const;
    const std::string &getClientAPIString() const;

    std::string getBackendRendererDescription() const;
    std::string getBackendVendorString() const;
    std::string getBackendVersionString(bool includeFullVersion) const;

    EGLint programCacheGetAttrib(EGLenum attrib) const;
    Error programCacheQuery(EGLint index,
                            void *key,
                            EGLint *keysize,
                            void *binary,
                            EGLint *binarysize);
    Error programCachePopulate(const void *key,
                               EGLint keysize,
                               const void *binary,
                               EGLint binarysize);
    EGLint programCacheResize(EGLint limit, EGLenum mode);

    const AttributeMap &getAttributeMap() const { return mAttributeMap; }
    EGLNativeDisplayType getNativeDisplayId() const { return mState.displayId; }

    rx::DisplayImpl *getImplementation() const { return mImplementation; }
    Device *getDevice() const;
    Surface *getWGLSurface() const;
    EGLenum getPlatform() const { return mPlatform; }

    gl::Version getMaxSupportedESVersion() const;

    const DisplayState &getState() const { return mState; }

    const angle::FrontendFeatures &getFrontendFeatures() { return mFrontendFeatures; }
    void overrideFrontendFeatures(const std::vector<std::string> &featureNames, bool enabled);

    const angle::FeatureList &getFeatures() const { return mFeatures; }

    const char *queryStringi(const EGLint name, const EGLint index);

    EGLAttrib queryAttrib(const EGLint attribute);

    angle::ScratchBuffer requestScratchBuffer();
    void returnScratchBuffer(angle::ScratchBuffer scratchBuffer);

    angle::ScratchBuffer requestZeroFilledBuffer();
    void returnZeroFilledBuffer(angle::ScratchBuffer zeroFilledBuffer);

    egl::Error handleGPUSwitch();
    egl::Error forceGPUSwitch(EGLint gpuIDHigh, EGLint gpuIDLow);

    egl::Error waitUntilWorkScheduled();

    void lockVulkanQueue();
    void unlockVulkanQueue();

    // Installs LoggingAnnotator as the global DebugAnnotator, for back-ends that do not implement
    // their own DebugAnnotator.
    void setGlobalDebugAnnotator() { gl::InitializeDebugAnnotations(&mAnnotator); }

    bool supportsDmaBufFormat(EGLint format) const;
    Error queryDmaBufFormats(EGLint max_formats, EGLint *formats, EGLint *num_formats);
    Error queryDmaBufModifiers(EGLint format,
                               EGLint max_modifiers,
                               EGLuint64KHR *modifiers,
                               EGLBoolean *external_only,
                               EGLint *num_modifiers);

    Error querySupportedCompressionRates(const Config *configuration,
                                         const AttributeMap &attributes,
                                         EGLint *rates,
                                         EGLint rate_size,
                                         EGLint *num_rates) const;

    std::shared_ptr<angle::WorkerThreadPool> getSingleThreadPool() const
    {
        return mState.singleThreadPool;
    }
    std::shared_ptr<angle::WorkerThreadPool> getMultiThreadPool() const
    {
        return mState.multiThreadPool;
    }

    angle::ImageLoadContext getImageLoadContext() const;

    const gl::Context *getContext(gl::ContextID contextID) const;
    const egl::Surface *getSurface(egl::SurfaceID surfaceID) const;
    const egl::Image *getImage(egl::ImageID imageID) const;
    ScopedSyncRef getSync(egl::SyncID syncID) const;
    gl::Context *getContext(gl::ContextID contextID);
    egl::Surface *getSurface(egl::SurfaceID surfaceID);
    egl::Image *getImage(egl::ImageID imageID);
    ScopedSyncRef getSync(egl::SyncID syncID);
    void releaseSync(Sync *sync);

    ScopedSyncMap getSyncsForCapture() const { return mSyncSet.getSyncsForCapture(); }
    const ImageMap &getImagesForCapture() const { return mImageMap; }

    // Initialize thread-local variables used by the Display and its backing implementations.  This
    // includes:
    //
    // - The unlocked tail call to be run at the end of the entry point.
    // - Scratch space for an egl::Error used by the backends (this is not used by all backends, and
    //   access *must* be restricted to backends that use it).
    //
    static void InitTLS();
    static angle::UnlockedTailCall *GetCurrentThreadUnlockedTailCall();
    static Error *GetCurrentThreadErrorScratchSpace();

  private:
    Display(EGLenum platform, EGLNativeDisplayType displayId, Device *eglDevice);

    void setAttributes(const AttributeMap &attribMap) { mAttributeMap = attribMap; }
    void setupDisplayPlatform(rx::DisplayImpl *impl);

    Error restoreLostDevice();
    Error releaseContext(gl::Context *context, Thread *thread);
    Error releaseContextImpl(std::unique_ptr<gl::Context> &&context);
    std::unique_ptr<gl::Context> eraseContextImpl(gl::Context *context, ContextMap *contexts);

    void initDisplayExtensions();
    void initVendorString();
    void initVersionString();
    void initClientAPIString();
    void initializeFrontendFeatures();

    angle::ScratchBuffer requestScratchBufferImpl(std::vector<angle::ScratchBuffer> *bufferVector);
    void returnScratchBufferImpl(angle::ScratchBuffer scratchBuffer,
                                 std::vector<angle::ScratchBuffer> *bufferVector);

    Error destroyInvalidEglObjects();

    void destroyImageImpl(Image *image, ImageMap *images);
    void destroyStreamImpl(Stream *stream, StreamSet *streams);
    Error destroySurfaceImpl(Surface *surface, SurfaceMap *surfaces);

    void initFromDevice(Device *device, const AttributeMap &attribMap);
    bool initFromNativeDisplay(const AttributeMap &attribMap, const EGLAttrib nativePlatformType);

    [[nodiscard]] bool addRefIfNotTerminating() const
    {
        // std::memory_order_relaxed might be sufficient here, but std::memory_order_acquire is used
        // for safety since there is no performance difference.
        uint32_t count = mRefCount.load(std::memory_order_acquire);
        while (!(count & kTerminatingBit))
        {
            if (mRefCount.compare_exchange_weak(count, count + 1, std::memory_order_acquire,
                                                std::memory_order_acquire))
            {
                return true;
            }
        }
        return false;
    }
    void releaseRef() const
    {
        uint32_t prev = mRefCount.fetch_sub(1, std::memory_order_release);
        ASSERT((prev & kRefCountMask) > 0);
    }
    void waitUntilUnreferenced(uint32_t expectedCount);
    bool isTerminating() const;

    static constexpr uint32_t kTerminatingBit = 1u << 31;
    static constexpr uint32_t kRefCountMask   = ~kTerminatingBit;

    DisplayState mState;
    rx::DisplayImpl *mImplementation;
    angle::ObserverBinding mGPUSwitchedBinding;

    AttributeMap mAttributeMap;

    ConfigSet mConfigSet;

    ImageMap mImageMap;
    StreamSet mStreamSet;

    SyncSet mSyncSet;

    ContextMap mInvalidContextMap;
    ImageMap mInvalidImageMap;
    StreamSet mInvalidStreamSet;
    SurfaceMap mInvalidSurfaceMap;

    std::atomic<bool> mInitialized;

    Caps mCaps;

    DisplayExtensions mDisplayExtensions;
    std::string mDisplayExtensionString;

    std::string mVendorString;
    std::string mVersionString;
    std::string mClientAPIString;

    Device *mDevice;
    Surface *mSurface;
    EGLenum mPlatform;
    angle::LoggingAnnotator mAnnotator;

    // mManagersMutex protects mTextureManager and mSemaphoreManager
    ContextMutex *mManagersMutex;
    gl::TextureManager *mTextureManager;
    gl::SemaphoreManager *mSemaphoreManager;

    BlobCache mBlobCache;
    gl::MemoryProgramCache mMemoryProgramCache;
    gl::MemoryShaderCache mMemoryShaderCache;
    size_t mGlobalTextureShareGroupUsers;
    size_t mGlobalSemaphoreShareGroupUsers;

    gl::HandleAllocator mImageHandleAllocator;
    gl::HandleAllocator mSurfaceHandleAllocator;

    angle::FrontendFeatures mFrontendFeatures;

    angle::FeatureList mFeatures;

    angle::SimpleMutex mScratchBufferMutex;
    std::vector<angle::ScratchBuffer> mScratchBuffers;
    std::vector<angle::ScratchBuffer> mZeroFilledBuffers;

    bool mTerminatedByApi;

    // Only these ScopedDisplay classes could directly access the lock and RefCount.
    friend class ScopedDisplayMutexLock;
    template <typename DisplayT>
    friend class ScopedDisplayLockAndRefT;
    template <typename DisplayT>
    friend class ScopedDisplayRefT;
    mutable std::atomic<uint32_t> mRefCount;
    mutable angle::SimpleMutex mDisplayMutex;
};

template <typename DisplayT>
class [[nodiscard]] ScopedDisplayRefT final
{
  public:
    ScopedDisplayRefT() : mDisplay(nullptr), mHoldingRef(false) {}
    explicit ScopedDisplayRefT(DisplayT &display)
        : mDisplay(&display), mHoldingRef(display.addRefIfNotTerminating())
    {}
    ~ScopedDisplayRefT()
    {
        if (mHoldingRef)
        {
            mDisplay->releaseRef();
        }
    }

    ScopedDisplayRefT(const ScopedDisplayRefT &other)
        : mDisplay(other.mDisplay),
          mHoldingRef(other.mHoldingRef && other.mDisplay->addRefIfNotTerminating())
    {}

    template <typename OtherDisplayT,
              std::enable_if_t<std::is_convertible_v<OtherDisplayT *, DisplayT *>, bool> = true>
    ScopedDisplayRefT(const ScopedDisplayRefT<OtherDisplayT> &other)
        : mDisplay(other.mDisplay),
          mHoldingRef(other.mHoldingRef && other.mDisplay->addRefIfNotTerminating())
    {}

    ScopedDisplayRefT &operator=(const ScopedDisplayRefT &other)
    {
        if (this != &other)
        {
            if (mHoldingRef)
            {
                mDisplay->releaseRef();
            }
            mDisplay    = other.mDisplay;
            mHoldingRef = other.mHoldingRef && other.mDisplay->addRefIfNotTerminating();
        }
        return *this;
    }

    template <typename OtherDisplayT,
              std::enable_if_t<std::is_convertible_v<OtherDisplayT *, DisplayT *>, bool> = true>
    ScopedDisplayRefT &operator=(const ScopedDisplayRefT<OtherDisplayT> &other)
    {
        if (static_cast<const void *>(this) != static_cast<const void *>(&other))
        {
            if (mHoldingRef)
            {
                mDisplay->releaseRef();
            }
            mDisplay    = other.mDisplay;
            mHoldingRef = other.mHoldingRef && other.mDisplay->addRefIfNotTerminating();
        }
        return *this;
    }

    ScopedDisplayRefT(ScopedDisplayRefT &&other) noexcept
        : mDisplay(other.mDisplay), mHoldingRef(other.mHoldingRef)
    {
        other.mDisplay    = nullptr;
        other.mHoldingRef = false;
    }

    template <typename OtherDisplayT,
              std::enable_if_t<std::is_convertible_v<OtherDisplayT *, DisplayT *>, bool> = true>
    ScopedDisplayRefT(ScopedDisplayRefT<OtherDisplayT> &&other) noexcept
        : mDisplay(other.mDisplay), mHoldingRef(other.mHoldingRef)
    {
        other.mDisplay    = nullptr;
        other.mHoldingRef = false;
    }

    ScopedDisplayRefT &operator=(ScopedDisplayRefT &&other) noexcept
    {
        if (this != &other)
        {
            if (mHoldingRef)
            {
                mDisplay->releaseRef();
            }
            mDisplay          = other.mDisplay;
            mHoldingRef       = other.mHoldingRef;
            other.mDisplay    = nullptr;
            other.mHoldingRef = false;
        }
        return *this;
    }

    template <typename OtherDisplayT,
              std::enable_if_t<std::is_convertible_v<OtherDisplayT *, DisplayT *>, bool> = true>
    ScopedDisplayRefT &operator=(ScopedDisplayRefT<OtherDisplayT> &&other) noexcept
    {
        if (static_cast<const void *>(this) != static_cast<const void *>(&other))
        {
            if (mHoldingRef)
            {
                mDisplay->releaseRef();
            }
            mDisplay          = other.mDisplay;
            mHoldingRef       = other.mHoldingRef;
            other.mDisplay    = nullptr;
            other.mHoldingRef = false;
        }
        return *this;
    }

    DisplayT *get() const { return mDisplay; }

  private:
    template <typename OtherDisplayT>
    friend class ScopedDisplayRefT;

    DisplayT *mDisplay;
    bool mHoldingRef;
};

class [[nodiscard]] ScopedDisplayMutexLock final
{
  public:
    ScopedDisplayMutexLock() = default;
    explicit ScopedDisplayMutexLock(const Display &display) : mLock(display.mDisplayMutex) {}

    ScopedDisplayMutexLock(const ScopedDisplayMutexLock &)            = delete;
    ScopedDisplayMutexLock &operator=(const ScopedDisplayMutexLock &) = delete;

    ScopedDisplayMutexLock(ScopedDisplayMutexLock &&) noexcept            = default;
    ScopedDisplayMutexLock &operator=(ScopedDisplayMutexLock &&) noexcept = default;

  private:
    std::unique_lock<angle::SimpleMutex> mLock;
};

// ScopedDisplayLockAndRefT locks mDisplayMutex before incrementing mRefCount on construction,
// and decrements mRefCount before unlocking mDisplayMutex on destruction (via C++ reverse member
// declaration destruction order).
// The lock must be taken before the reference count is increased to prevent deadlock with
// Display::waitUntilUnreferenced(), which holds mDisplayMutex while waiting for outstanding
// references from other threads to be released.
template <typename DisplayT>
class [[nodiscard]] ScopedDisplayLockAndRefT final
{
  public:
    ScopedDisplayLockAndRefT() = default;
    explicit ScopedDisplayLockAndRefT(DisplayT &display) : mLock(display), mDisplay(display) {}

    ScopedDisplayLockAndRefT(const ScopedDisplayLockAndRefT &)            = delete;
    ScopedDisplayLockAndRefT &operator=(const ScopedDisplayLockAndRefT &) = delete;

    ScopedDisplayLockAndRefT(ScopedDisplayLockAndRefT &&other) noexcept            = default;
    ScopedDisplayLockAndRefT &operator=(ScopedDisplayLockAndRefT &&other) noexcept = default;

    template <typename OtherDisplayT,
              std::enable_if_t<std::is_convertible_v<OtherDisplayT *, DisplayT *>, bool> = true>
    ScopedDisplayLockAndRefT(ScopedDisplayLockAndRefT<OtherDisplayT> &&other) noexcept
        : mLock(std::move(other.mLock)), mDisplay(std::move(other.mDisplay))
    {}

    template <typename OtherDisplayT,
              std::enable_if_t<std::is_convertible_v<OtherDisplayT *, DisplayT *>, bool> = true>
    ScopedDisplayLockAndRefT &operator=(ScopedDisplayLockAndRefT<OtherDisplayT> &&other) noexcept
    {
        if (static_cast<const void *>(this) != static_cast<const void *>(&other))
        {
            mLock    = std::move(other.mLock);
            mDisplay = std::move(other.mDisplay);
        }
        return *this;
    }

    DisplayT *get() const { return mDisplay.get(); }

  private:
    template <typename OtherDisplayT>
    friend class ScopedDisplayLockAndRefT;

    // mLock must be declared before mDisplay to ensure mDisplayMutex is locked before mRefCount
    // is incremented on construction, and mRefCount is decremented before mDisplayMutex is
    // unlocked on destruction.
    ScopedDisplayMutexLock mLock;
    ScopedDisplayRefT<DisplayT> mDisplay;
};

}  // namespace egl

#endif  // LIBANGLE_DISPLAY_H_
