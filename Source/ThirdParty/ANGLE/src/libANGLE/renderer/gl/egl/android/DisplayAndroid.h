//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayAndroid.h: Android implementation of egl::Display

#ifndef LIBANGLE_RENDERER_GL_EGL_ANDROID_DISPLAYANDROID_H_
#define LIBANGLE_RENDERER_GL_EGL_ANDROID_DISPLAYANDROID_H_

#include <map>
#include <string>
#include <vector>

#include "libANGLE/renderer/gl/egl/DisplayEGL.h"

namespace rx
{

class RendererEGL;

class DisplayAndroid : public DisplayEGL
{
  public:
    DisplayAndroid(const egl::DisplayState &state);
    ~DisplayAndroid() override;

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    ContextImpl *createContext(const gl::State &state,
                               gl::ErrorSet *errorSet,
                               const egl::Config *configuration,
                               const gl::Context *shareContext,
                               const egl::AttributeMap &attribs) override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;
    egl::Error validateImageClientBuffer(const gl::Context *context,
                                         EGLenum target,
                                         EGLClientBuffer clientBuffer,
                                         const egl::AttributeMap &attribs) const override;

    ExternalImageSiblingImpl *createExternalImageSibling(const gl::Context *context,
                                                         EGLenum target,
                                                         EGLClientBuffer buffer,
                                                         const egl::AttributeMap &attribs) override;

    egl::Error makeCurrent(egl::Surface *drawSurface,
                           egl::Surface *readSurface,
                           gl::Context *context) override;

    void destroyNativeContext(EGLContext context) override;

    WorkerContext *createWorkerContext(std::string *infoLog,
                                       EGLContext sharedContext,
                                       const native_egl::AttributeVector workerAttribs) override;

  private:
    void generateExtensions(egl::DisplayExtensions *outExtensions) const override;

    egl::Error createRenderer(EGLContext shareContext,
                              bool makeNewContextCurrent,
                              std::shared_ptr<RendererEGL> *outRenderer);

    bool mVirtualizedContexts;

    bool mSupportsSurfaceless;

    EGLSurface mDummyPbuffer;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_ANDROID_DISPLAYANDROID_H_
