//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayCGL.h: CGL implementation of egl::Display

#ifndef LIBANGLE_RENDERER_GL_CGL_DISPLAYCGL_H_
#define LIBANGLE_RENDERER_GL_CGL_DISPLAYCGL_H_

#include "libANGLE/renderer/gl/DisplayGL.h"

struct _CGLContextObject;
typedef _CGLContextObject *CGLContextObj;

namespace rx
{

class DisplayCGL : public DisplayGL
{
  public:
    DisplayCGL(const egl::DisplayState &state);
    ~DisplayCGL() override;

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

    egl::ConfigSet generateConfigs() override;

    bool testDeviceLost() override;
    egl::Error restoreLostDevice(const egl::Display *display) override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;

    egl::Error getDevice(DeviceImpl **device) override;

    std::string getVendorString() const override;

    egl::Error waitClient(const gl::Context *context) const override;
    egl::Error waitNative(const gl::Context *context, EGLint engine) const override;

  private:
    const FunctionsGL *getFunctionsGL() const override;
    egl::Error makeCurrentSurfaceless(gl::Context *context) override;

    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;
    void generateCaps(egl::Caps *outCaps) const override;

    egl::Display *mEGLDisplay;
    FunctionsGL *mFunctions;
    CGLContextObj mContext;
};

}

#endif // LIBANGLE_RENDERER_GL_CGL_DISPLAYCGL_H_
