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

#include <mutex>
#include <set>
#include <vector>

#include "common/WorkerThread.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/LoggingAnnotator.h"
#include "libANGLE/MemoryProgramCache.h"
#include "libANGLE/MemoryShaderCache.h"
#include "libANGLE/Observer.h"
#include "libANGLE/Version.h"
#include "platform/Feature.h"
#include "platform/FrontendFeatures_autogen.h"

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
class ShareGroupImpl;
}  // namespace rx

namespace egl
{
class Device;
class Image;
class Stream;
class Surface;
class Sync;
class Thread;

using ContextSet = angle::HashSet<gl::Context *>;
using SurfaceSet = angle::HashSet<Surface *>;
using ThreadSet  = angle::HashSet<Thread *>;

struct DisplayState final : private angle::NonCopyable
{
    DisplayState(EGLNativeDisplayType nativeDisplayId);
    ~DisplayState();

    EGLLabelKHR label;
    ContextSet contextSet;
    SurfaceSet surfaceSet;
    std::vector<std::string> featureOverridesEnabled;
    std::vector<std::string> featureOverridesDisabled;
    bool featuresAllDisabled;
    EGLNativeDisplayType displayId;
};

class ShareGroup final : angle::NonCopyable
{
  public:
    ShareGroup(rx::EGLImplFactory *factory);

    void addRef();

    void release(const egl::Display *display);

    rx::ShareGroupImpl *getImplementation() const { return mImplementation; }

    rx::Serial generateFramebufferSerial() { return mFramebufferSerialFactory.generate(); }

    angle::FrameCaptureShared *getFrameCaptureShared() { return mFrameCaptureShared.get(); }

    void finishAllContexts();

    const ContextSet &getContexts() const { return mContexts; }
    void addSharedContext(gl::Context *context);
    void removeSharedContext(gl::Context *context);

    size_t getShareGroupContextCount() const { return mContexts.size(); }

  protected:
    ~ShareGroup();

  private:
    size_t mRefCount;
    rx::ShareGroupImpl *mImplementation;
    rx::SerialFactory mFramebufferSerialFactory;

    // Note: we use a raw pointer here so we can exclude frame capture sources from the build.
    std::unique_ptr<angle::FrameCaptureShared> mFrameCaptureShared;

    // The list of contexts within the share group
    ContextSet mContexts;
};

// Constant coded here as a reasonable limit.
constexpr EGLAttrib kProgramCacheSizeAbsoluteMax = 0x4000000;

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
        NoActiveThreads,

        InvalidEnum,
        EnumCount = InvalidEnum,
    };
    Error terminate(Thread *thread, TerminateReason terminateReason);
    // Called before all display state dependent EGL functions. Backends can set up, for example,
    // thread-specific backend state through this function. Not called for functions that do not
    // need the state.
    Error prepareForCall();
    // Called on eglReleaseThread. Backends can tear down thread-specific backend state through
    // this function.
    Error releaseThread();

    // Helpers to maintain active thread set to assist with freeing invalid EGL objects.
    void addActiveThread(Thread *thread);
    void threadCleanup(Thread *thread);

    static Display *GetDisplayFromDevice(Device *device, const AttributeMap &attribMap);
    static Display *GetDisplayFromNativeDisplay(EGLenum platform,
                                                EGLNativeDisplayType nativeDisplay,
                                                const AttributeMap &attribMap);
    static Display *GetExistingDisplayFromNativeDisplay(EGLNativeDisplayType nativeDisplay);

    using EglDisplaySet = angle::HashSet<Display *>;
    static EglDisplaySet GetEglDisplaySet();

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
                        const EGLenum clientType,
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
    bool isValidSync(const Sync *sync) const;
    bool isValidNativeWindow(EGLNativeWindowType window) const;

    Error validateClientBuffer(const Config *configuration,
                               EGLenum buftype,
                               EGLClientBuffer clientBuffer,
                               const AttributeMap &attribs) const;
    Error validateImageClientBuffer(const gl::Context *context,
                                    EGLenum target,
                                    EGLClientBuffer clientBuffer,
                                    const egl::AttributeMap &attribs) const;
    Error valdiatePixmap(const Config *config,
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

    std::mutex &getDisplayGlobalMutex() { return mDisplayGlobalMutex; }
    std::mutex &getProgramCacheMutex() { return mProgramCacheMutex; }

    gl::MemoryShaderCache *getMemoryShaderCache() { return &mMemoryShaderCache; }

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

    std::shared_ptr<angle::WorkerThreadPool> getSingleThreadPool() const
    {
        return mSingleThreadPool;
    }
    std::shared_ptr<angle::WorkerThreadPool> getMultiThreadPool() const { return mMultiThreadPool; }

    angle::ImageLoadContext getImageLoadContext() const;

    const gl::Context *getContext(gl::ContextID contextID) const;
    const egl::Surface *getSurface(egl::SurfaceID surfaceID) const;
    const egl::Image *getImage(egl::ImageID imageID) const;
    gl::Context *getContext(gl::ContextID contextID);
    egl::Surface *getSurface(egl::SurfaceID surfaceID);
    egl::Image *getImage(egl::ImageID imageID);

  private:
    Display(EGLenum platform, EGLNativeDisplayType displayId, Device *eglDevice);

    void setAttributes(const AttributeMap &attribMap) { mAttributeMap = attribMap; }
    void setupDisplayPlatform(rx::DisplayImpl *impl);

    Error restoreLostDevice();
    Error releaseContext(gl::Context *context, Thread *thread);
    Error releaseContextImpl(gl::Context *context, ContextSet *contexts);

    void initDisplayExtensions();
    void initVendorString();
    void initVersionString();
    void initClientAPIString();
    void initializeFrontendFeatures();

    angle::ScratchBuffer requestScratchBufferImpl(std::vector<angle::ScratchBuffer> *bufferVector);
    void returnScratchBufferImpl(angle::ScratchBuffer scratchBuffer,
                                 std::vector<angle::ScratchBuffer> *bufferVector);

    Error destroyInvalidEglObjects();

    DisplayState mState;
    rx::DisplayImpl *mImplementation;
    angle::ObserverBinding mGPUSwitchedBinding;

    AttributeMap mAttributeMap;

    ConfigSet mConfigSet;

    typedef angle::HashSet<Image *> ImageSet;
    ImageSet mImageSet;

    typedef angle::HashSet<Stream *> StreamSet;
    StreamSet mStreamSet;

    typedef angle::HashSet<Sync *> SyncSet;
    SyncSet mSyncSet;

    void destroyImageImpl(Image *image, ImageSet *images);
    void destroyStreamImpl(Stream *stream, StreamSet *streams);
    Error destroySurfaceImpl(Surface *surface, SurfaceSet *surfaces);
    void destroySyncImpl(Sync *sync, SyncSet *syncs);

    ContextSet mInvalidContextSet;
    ImageSet mInvalidImageSet;
    StreamSet mInvalidStreamSet;
    SurfaceSet mInvalidSurfaceSet;
    SyncSet mInvalidSyncSet;

    bool mInitialized;
    bool mDeviceLost;

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

    std::mutex mScratchBufferMutex;
    std::vector<angle::ScratchBuffer> mScratchBuffers;
    std::vector<angle::ScratchBuffer> mZeroFilledBuffers;

    std::mutex mDisplayGlobalMutex;
    std::mutex mProgramCacheMutex;

    bool mTerminatedByApi;
    ThreadSet mActiveThreads;

    // Single-threaded and multithread pools for use by various parts of ANGLE, such as shader
    // compilation.  These pools are internally synchronized.
    std::shared_ptr<angle::WorkerThreadPool> mSingleThreadPool;
    std::shared_ptr<angle::WorkerThreadPool> mMultiThreadPool;
};

}  // namespace egl

#endif  // LIBANGLE_DISPLAY_H_
