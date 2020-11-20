//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVk.h:
//    Defines the class interface for DisplayVk, implementing DisplayImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_DISPLAYVK_H_
#define LIBANGLE_RENDERER_VULKAN_DISPLAYVK_H_

#include "common/MemoryBuffer.h"
#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{
class RendererVk;

using ShareContextSet = std::set<ContextVk *>;

class ShareGroupVk : public ShareGroupImpl
{
  public:
    ShareGroupVk() {}
    void onDestroy(const egl::Display *display) override;

    // PipelineLayoutCache and DescriptorSetLayoutCache can be shared between multiple threads
    // accessing them via shared contexts. The ShareGroup locks around gl entrypoints ensuring
    // synchronous update to the caches.
    PipelineLayoutCache &getPipelineLayoutCache() { return mPipelineLayoutCache; }
    DescriptorSetLayoutCache &getDescriptorSetLayoutCache() { return mDescriptorSetLayoutCache; }
    ShareContextSet *getShareContextSet() { return &mShareContextSet; }

  private:
    // ANGLE uses a PipelineLayout cache to store compatible pipeline layouts.
    PipelineLayoutCache mPipelineLayoutCache;

    // DescriptorSetLayouts are also managed in a cache.
    DescriptorSetLayoutCache mDescriptorSetLayoutCache;

    // The list of contexts within the share group
    ShareContextSet mShareContextSet;
};

class DisplayVk : public DisplayImpl, public vk::Context
{
  public:
    DisplayVk(const egl::DisplayState &state);
    ~DisplayVk() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    egl::Error makeCurrent(egl::Display *display,
                           egl::Surface *drawSurface,
                           egl::Surface *readSurface,
                           gl::Context *context) override;

    bool testDeviceLost() override;
    egl::Error restoreLostDevice(const egl::Display *display) override;

    std::string getVendorString() const override;

    DeviceImpl *createDevice() override;

    egl::Error waitClient(const gl::Context *context) override;
    egl::Error waitNative(const gl::Context *context, EGLint engine) override;

    SurfaceImpl *createWindowSurface(const egl::SurfaceState &state,
                                     EGLNativeWindowType window,
                                     const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferSurface(const egl::SurfaceState &state,
                                      const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                               EGLenum buftype,
                                               EGLClientBuffer clientBuffer,
                                               const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPixmapSurface(const egl::SurfaceState &state,
                                     NativePixmapType nativePixmap,
                                     const egl::AttributeMap &attribs) override;

    ImageImpl *createImage(const egl::ImageState &state,
                           const gl::Context *context,
                           EGLenum target,
                           const egl::AttributeMap &attribs) override;

    ContextImpl *createContext(const gl::State &state,
                               gl::ErrorSet *errorSet,
                               const egl::Config *configuration,
                               const gl::Context *shareContext,
                               const egl::AttributeMap &attribs) override;

    StreamProducerImpl *createStreamProducerD3DTexture(egl::Stream::ConsumerType consumerType,
                                                       const egl::AttributeMap &attribs) override;

    EGLSyncImpl *createSync(const egl::AttributeMap &attribs) override;

    gl::Version getMaxSupportedESVersion() const override;
    gl::Version getMaxConformantESVersion() const override;

    virtual const char *getWSIExtension() const = 0;
    virtual const char *getWSILayer() const;

    // Determine if a config with given formats and sample counts is supported.  This callback may
    // modify the config to add or remove platform specific attributes such as nativeVisualID.  If
    // the config is not supported by the window system, it removes the EGL_WINDOW_BIT from
    // surfaceType, which would still allow the config to be used for pbuffers.
    virtual void checkConfigSupport(egl::Config *config) = 0;

    ANGLE_NO_DISCARD bool getScratchBuffer(size_t requestedSizeBytes,
                                           angle::MemoryBuffer **scratchBufferOut) const;
    angle::ScratchBuffer *getScratchBuffer() const { return &mScratchBuffer; }

    void handleError(VkResult result,
                     const char *file,
                     const char *function,
                     unsigned int line) override;

    // TODO(jmadill): Remove this once refactor is done. http://anglebug.com/3041
    egl::Error getEGLError(EGLint errorCode);

    void populateFeatureList(angle::FeatureList *features) override;

    ShareGroupImpl *createShareGroup() override;

  protected:
    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;

  private:
    virtual SurfaceImpl *createWindowSurfaceVk(const egl::SurfaceState &state,
                                               EGLNativeWindowType window) = 0;
    void generateCaps(egl::Caps *outCaps) const override;

    virtual angle::Result waitNativeImpl();

    mutable angle::ScratchBuffer mScratchBuffer;

    vk::Error mSavedError;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_DISPLAYVK_H_
