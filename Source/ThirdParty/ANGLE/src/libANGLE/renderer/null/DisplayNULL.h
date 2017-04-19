//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayNULL.h:
//    Defines the class interface for DisplayNULL, implementing DisplayImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_DISPLAYNULL_H_
#define LIBANGLE_RENDERER_NULL_DISPLAYNULL_H_

#include "libANGLE/renderer/DisplayImpl.h"

namespace rx
{

class AllocationTrackerNULL;

class DisplayNULL : public DisplayImpl
{
  public:
    DisplayNULL(const egl::DisplayState &state);
    ~DisplayNULL() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    egl::Error makeCurrent(egl::Surface *drawSurface,
                           egl::Surface *readSurface,
                           gl::Context *context) override;

    egl::ConfigSet generateConfigs() override;

    bool testDeviceLost() override;
    egl::Error restoreLostDevice() override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;

    std::string getVendorString() const override;

    egl::Error getDevice(DeviceImpl **device) override;

    egl::Error waitClient() const override;
    egl::Error waitNative(EGLint engine,
                          egl::Surface *drawSurface,
                          egl::Surface *readSurface) const override;
    gl::Version getMaxSupportedESVersion() const override;

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

    ImageImpl *createImage(EGLenum target,
                           egl::ImageSibling *buffer,
                           const egl::AttributeMap &attribs) override;

    ContextImpl *createContext(const gl::ContextState &state) override;

    StreamProducerImpl *createStreamProducerD3DTextureNV12(
        egl::Stream::ConsumerType consumerType,
        const egl::AttributeMap &attribs) override;

  private:
    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;
    void generateCaps(egl::Caps *outCaps) const override;

    DeviceImpl *mDevice;

    std::unique_ptr<AllocationTrackerNULL> mAllocationTracker;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_DISPLAYNULL_H_
