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

#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>

#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/renderer/ShareGroupImpl.h"

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

    bool isValidNativeWindow(EGLNativeWindowType window) const override;

    std::string getRendererDescription() override;
    std::string getVendorString() override;
    std::string getVersionString(bool includeFullVersion) override;

    DeviceImpl *createDevice() override;

    egl::Error waitClient(const gl::Context *context) override;
    egl::Error waitNative(const gl::Context *context, EGLint engine) override;
    gl::Version getMaxSupportedESVersion() const override;
    gl::Version getMaxConformantESVersion() const override;
    Optional<gl::Version> getMaxSupportedDesktopVersion() const override;

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

    ContextImpl *createContext(const gl::State &state,
                               gl::ErrorSet *errorSet,
                               const egl::Config *configuration,
                               const gl::Context *shareContext,
                               const egl::AttributeMap &attribs) override;

    StreamProducerImpl *createStreamProducerD3DTexture(egl::Stream::ConsumerType consumerType,
                                                       const egl::AttributeMap &attribs) override;

    ShareGroupImpl *createShareGroup(const egl::ShareGroupState &state) override;

    void populateFeatureList(angle::FeatureList *features) override {}

    wgpu::Device &getDevice() { return mDevice; }
    wgpu::Queue &getQueue() { return mQueue; }

  private:
    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;
    void generateCaps(egl::Caps *outCaps) const override;

    egl::Error createWgpuDevice();

    std::unique_ptr<dawn::native::Instance> mInstance;
    wgpu::Device mDevice;
    wgpu::Queue mQueue;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_DISPLAYWGPU_H_
