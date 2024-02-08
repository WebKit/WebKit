//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceWgpu.h:
//    Defines the class interface for SurfaceWgpu, implementing SurfaceImpl.
//

#ifndef LIBANGLE_RENDERER_WGPU_SURFACEWGPU_H_
#define LIBANGLE_RENDERER_WGPU_SURFACEWGPU_H_

#include "libANGLE/renderer/SurfaceImpl.h"

namespace rx
{

class SurfaceWgpu : public SurfaceImpl
{
  public:
    SurfaceWgpu(const egl::SurfaceState &surfaceState);
    ~SurfaceWgpu() override;

    egl::Error initialize(const egl::Display *display) override;
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
    egl::Error getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc) override;
    egl::Error getMscRate(EGLint *numerator, EGLint *denominator) override;
    void setSwapInterval(EGLint interval) override;

    // width and height can change with client window resizing
    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    angle::Result initializeContents(const gl::Context *context,
                                     GLenum binding,
                                     const gl::ImageIndex &imageIndex) override;

    egl::Error attachToFramebuffer(const gl::Context *context,
                                   gl::Framebuffer *framebuffer) override;
    egl::Error detachFromFramebuffer(const gl::Context *context,
                                     gl::Framebuffer *framebuffer) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_SURFACEWGPU_H_
