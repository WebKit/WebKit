//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayWgpu.h:
//    Defines the class interface for DisplayWgpu, implementing DisplayImpl.
//

#ifndef LIBANGLE_RENDERER_WGPU_DISPLAYWGPU_H_
#define LIBANGLE_RENDERER_WGPU_DISPLAYWGPU_H_

#include <webgpu/webgpu.h>

#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/ShareGroupImpl.h"
#include "libANGLE/renderer/wgpu/wgpu_format_utils.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"
#include "platform/autogen/FeaturesWgpu_autogen.h"

namespace rx
{
class ShareGroupWgpu : public ShareGroupImpl
{
  public:
    ShareGroupWgpu(const egl::ShareGroupState &state) : ShareGroupImpl(state) {}
};

class AllocationTrackerWgpu;

class DisplayWgpu : public DisplayImpl
{
  public:
    DisplayWgpu(const egl::DisplayState &state);
    ~DisplayWgpu() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    egl::Error makeCurrent(egl::Display *display,
                           egl::Surface *drawSurface,
                           egl::Surface *readSurface,
                           gl::Context *context) override;

    egl::ConfigSet generateConfigs() override;

    bool testDeviceLost() override;
    egl::Error restoreLostDevice(const egl::Display *display) override;

    egl::Error validateClientBuffer(const egl::Config *configuration,
                                    EGLenum buftype,
                                    EGLClientBuffer clientBuffer,
                                    const egl::AttributeMap &attribs) const override;
    egl::Error validateImageClientBuffer(const gl::Context *context,
                                         EGLenum target,
                                         EGLClientBuffer clientBuffer,
                                         const egl::AttributeMap &attribs) const override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;

    std::string getRendererDescription() override;
    std::string getVendorString() override;
    std::string getVersionString(bool includeFullVersion) override;

    DeviceImpl *createDevice() override;

    egl::Error waitClient(const gl::Context *context) override;
    egl::Error waitNative(const gl::Context *context, EGLint engine) override;
    gl::Version getMaxSupportedESVersion() const override;
    gl::Version getMaxConformantESVersion() const override;

    SurfaceImpl *createWindowSurface(const egl::SurfaceState &state,
                                     EGLNativeWindowType window,
                                     const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferSurface(const egl::SurfaceState &state,
                                      const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                               EGLenum buftype,
                                               EGLClientBuffer buffer,
                                               const egl::AttributeMap &attribs) override;
    SurfaceImpl *createPixmapSurface(const egl::SurfaceState &state,
                                     NativePixmapType nativePixmap,
                                     const egl::AttributeMap &attribs) override;

    ImageImpl *createImage(const egl::ImageState &state,
                           const gl::Context *context,
                           EGLenum target,
                           const egl::AttributeMap &attribs) override;
    ExternalImageSiblingImpl *createExternalImageSibling(const gl::Context *context,
                                                         EGLenum target,
                                                         EGLClientBuffer buffer,
                                                         const egl::AttributeMap &attribs) override;

    ContextImpl *createContext(const gl::State &state,
                               gl::ErrorSet *errorSet,
                               const egl::Config *configuration,
                               const gl::Context *shareContext,
                               const egl::AttributeMap &attribs) override;

    StreamProducerImpl *createStreamProducerD3DTexture(egl::Stream::ConsumerType consumerType,
                                                       const egl::AttributeMap &attribs) override;

    ShareGroupImpl *createShareGroup(const egl::ShareGroupState &state) override;

    void populateFeatureList(angle::FeatureList *features) override;

    angle::NativeWindowSystem getWindowSystem() const override;

    const DawnProcTable *getProcs() const { return &mProcTable; }
    const angle::FeaturesWgpu &getFeatures() const { return mFeatures; }
    webgpu::AdapterHandle getAdapter() { return mAdapter; }
    webgpu::DeviceHandle getDevice() { return mDevice; }
    webgpu::QueueHandle getQueue() { return mQueue; }
    webgpu::InstanceHandle getInstance() { return mInstance; }

    const WGPULimits &getLimitsWgpu() const { return mLimitsWgpu; }

    const gl::Caps &getGLCaps() const { return mGLCaps; }
    const gl::TextureCapsMap &getGLTextureCaps() const { return mGLTextureCaps; }
    const gl::Extensions &getGLExtensions() const { return mGLExtensions; }
    const gl::Limitations &getGLLimitations() const { return mGLLimitations; }
    const ShPixelLocalStorageOptions &getPLSOptions() const { return mPLSOptions; }

    const webgpu::Format &getFormat(GLenum internalFormat) const
    {
        return mFormatTable[internalFormat];
    }

    const webgpu::Format *getFormatForImportedTexture(const egl::AttributeMap &attribs,
                                                      WGPUTextureFormat wgpuFormat) const;

  private:
    egl::Error validateExternalWebGPUTexture(EGLClientBuffer buffer,
                                             const egl::AttributeMap &attribs) const;

    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;
    void generateCaps(egl::Caps *outCaps) const override;

    void initializeFeatures();

    egl::Error createWgpuDevice();

    DawnProcTable mProcTable;

    webgpu::AdapterHandle mAdapter;
    webgpu::InstanceHandle mInstance;
    webgpu::DeviceHandle mDevice;
    webgpu::QueueHandle mQueue;

    WGPULimits mLimitsWgpu;

    gl::Caps mGLCaps;
    gl::TextureCapsMap mGLTextureCaps;
    gl::Extensions mGLExtensions;
    gl::Limitations mGLLimitations;
    egl::Caps mEGLCaps;
    egl::DisplayExtensions mEGLExtensions;
    gl::Version mMaxSupportedClientVersion;
    ShPixelLocalStorageOptions mPLSOptions;

    webgpu::FormatTable mFormatTable;

    angle::FeaturesWgpu mFeatures;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_DISPLAYWGPU_H_
