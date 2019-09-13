//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayEGL.h: Common across EGL parts of platform specific egl::Display implementations

#ifndef LIBANGLE_RENDERER_GL_EGL_DISPLAYEGL_H_
#define LIBANGLE_RENDERER_GL_EGL_DISPLAYEGL_H_

#include "libANGLE/renderer/gl/DisplayGL.h"
#include "libANGLE/renderer/gl/egl/FunctionsEGL.h"
#include "libANGLE/renderer/gl/egl/egl_utils.h"

namespace rx
{

class WorkerContext;

class DisplayEGL : public DisplayGL
{
  public:
    DisplayEGL(const egl::DisplayState &state);
    ~DisplayEGL() override;

    ImageImpl *createImage(const egl::ImageState &state,
                           const gl::Context *context,
                           EGLenum target,
                           const egl::AttributeMap &attribs) override;

    EGLSyncImpl *createSync(const egl::AttributeMap &attribs) override;

    std::string getVendorString() const override;

    void setBlobCacheFuncs(EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get) override;

    virtual void destroyNativeContext(EGLContext context) = 0;

    virtual WorkerContext *createWorkerContext(std::string *infoLog,
                                               EGLContext sharedContext,
                                               const native_egl::AttributeVector workerAttribs) = 0;

  protected:
    egl::Error initializeContext(EGLContext shareContext,
                                 const egl::AttributeMap &eglAttributes,
                                 EGLContext *outContext,
                                 native_egl::AttributeVector *outAttribs) const;

    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;

    FunctionsEGL *mEGL;
    EGLConfig mConfig;

  private:
    void generateCaps(egl::Caps *outCaps) const override;
};

}  // namespace rx

#endif /* LIBANGLE_RENDERER_GL_EGL_DISPLAYEGL_H_ */
