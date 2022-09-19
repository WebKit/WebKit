//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PBufferSurfaceEAGL.h: an implementation of egl::Surface for PBuffers for the EAGL backend,
//                       currently implemented using renderbuffers

#ifndef LIBANGLE_RENDERER_GL_EAGL_PBUFFERSURFACEEAGL_H_
#define LIBANGLE_RENDERER_GL_EAGL_PBUFFERSURFACEEAGL_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"

namespace rx
{

class FunctionsGL;
class RendererGL;
class StateManagerGL;

class PbufferSurfaceEAGL : public SurfaceGL
{
  public:
    PbufferSurfaceEAGL(const egl::SurfaceState &state,
                       RendererGL *renderer,
                       EGLint width,
                       EGLint height);
    ~PbufferSurfaceEAGL() override;

    egl::Error initialize(const egl::Display *display) override;
    egl::Error makeCurrent(const gl::Context *context) override;

    egl::Error swap(const gl::Context *context) override;
    egl::Error postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(const gl::Context *context,
                            gl::Texture *texture,
                            EGLint buffer) override;
    egl::Error releaseTexImage(const gl::Context *context, EGLint buffer) override;
    void setSwapInterval(EGLint interval) override;

    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    egl::Error attachToFramebuffer(const gl::Context *context,
                                   gl::Framebuffer *framebuffer) override;
    egl::Error detachFromFramebuffer(const gl::Context *context,
                                     gl::Framebuffer *framebuffer) override;

  private:
    unsigned mWidth;
    unsigned mHeight;

    // TODO(geofflang): Don't store these, they are potentially specific to a single GL context.
    // http://anglebug.com/2464
    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;

    GLuint mColorRenderbuffer;
    GLuint mDSRenderbuffer;
    GLuint mFramebufferID;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EAGL_PBUFFERSURFACEEAGL_H_
