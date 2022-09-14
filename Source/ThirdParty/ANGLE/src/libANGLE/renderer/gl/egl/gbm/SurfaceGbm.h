//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceGbm.h: Gbm implementation of egl::SurfaceGL

#ifndef LIBANGLE_RENDERER_GL_EGL_GBM_SURFACEGBM_H_
#define LIBANGLE_RENDERER_GL_EGL_GBM_SURFACEGBM_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"
#include "libANGLE/renderer/gl/egl/gbm/DisplayGbm.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace rx
{

class SurfaceGbm : public SurfaceGL
{
  public:
    SurfaceGbm(const egl::SurfaceState &state, DisplayGbm::Buffer *buffer);
    ~SurfaceGbm() override;

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
    DisplayGbm::Buffer *mBuffer;

    // TODO(fjhenigman) Implement swap control.  This will be used for that.
    SwapControlData mSwapControl;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_GBM_SURFACEGBM_H_
