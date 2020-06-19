//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayEAGL.h: EAGL implementation of egl::Display

#ifndef LIBANGLE_RENDERER_GL_EAGL_DISPLAYEAGL_H_
#define LIBANGLE_RENDERER_GL_EAGL_DISPLAYEAGL_H_

#import "common/platform.h"

#if defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)

#include "libANGLE/renderer/gl/DisplayGL.h"

#ifdef __OBJC__
@class EAGLContext;
typedef EAGLContext *EAGLContextObj;
#else
typedef void *EAGLContextObj;
#endif

namespace rx
{

class WorkerContext;

class DisplayEAGL : public DisplayGL
{
  public:
    DisplayEAGL(const egl::DisplayState &state);
    ~DisplayEAGL() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

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

    ContextImpl *createContext(const gl::State &state,
                               gl::ErrorSet *errorSet,
                               const egl::Config *configuration,
                               const gl::Context *shareContext,
                               const egl::AttributeMap &attribs) override;

    egl::ConfigSet generateConfigs() override;

    bool testDeviceLost() override;
    egl::Error restoreLostDevice(const egl::Display *display) override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;
    egl::Error validateClientBuffer(const egl::Config *configuration,
                                    EGLenum buftype,
                                    EGLClientBuffer clientBuffer,
                                    const egl::AttributeMap &attribs) const override;

    DeviceImpl *createDevice() override;

    std::string getVendorString() const override;

    egl::Error waitClient(const gl::Context *context) override;
    egl::Error waitNative(const gl::Context *context, EGLint engine) override;

    gl::Version getMaxSupportedESVersion() const override;

    EAGLContextObj getEAGLContext() const;

    WorkerContext *createWorkerContext(std::string *infoLog);

    void initializeFrontendFeatures(angle::FrontendFeatures *features) const override;

    void populateFeatureList(angle::FeatureList *features) override;

  private:
    egl::Error makeCurrentSurfaceless(gl::Context *context) override;

    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;
    void generateCaps(egl::Caps *outCaps) const override;

    std::shared_ptr<RendererGL> mRenderer;

    egl::Display *mEGLDisplay;
    EAGLContextObj mContext;
};

}  // namespace rx

#endif // defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)

#endif  // LIBANGLE_RENDERER_GL_EAGL_DISPLAYEAGL_H_
