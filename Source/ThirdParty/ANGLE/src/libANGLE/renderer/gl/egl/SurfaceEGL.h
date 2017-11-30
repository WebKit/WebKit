//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceEGL.h: common interface for EGL surfaces

#ifndef LIBANGLE_RENDERER_GL_EGL_SURFACEEGL_H_
#define LIBANGLE_RENDERER_GL_EGL_SURFACEEGL_H_

#include <EGL/egl.h>

#include "libANGLE/renderer/gl/SurfaceGL.h"
#include "libANGLE/renderer/gl/egl/FunctionsEGL.h"

namespace rx
{

class SurfaceEGL : public SurfaceGL
{
  public:
    SurfaceEGL(const egl::SurfaceState &state,
               const FunctionsEGL *egl,
               EGLConfig config,
               RendererGL *renderer);
    ~SurfaceEGL() override;

    egl::Error makeCurrent() override;
    egl::Error swap(const gl::Context *context) override;
    egl::Error postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(gl::Texture *texture, EGLint buffer) override;
    egl::Error releaseTexImage(EGLint buffer) override;
    void setSwapInterval(EGLint interval) override;
    EGLint getWidth() const override;
    EGLint getHeight() const override;
    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    EGLSurface getSurface() const;

  protected:
    const FunctionsEGL *mEGL;
    EGLConfig mConfig;
    EGLSurface mSurface;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_SURFACEEGL_H_
