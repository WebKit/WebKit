//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceNULL.h:
//    Defines the class interface for SurfaceNULL, implementing SurfaceImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_SURFACENULL_H_
#define LIBANGLE_RENDERER_NULL_SURFACENULL_H_

#include "libANGLE/renderer/SurfaceImpl.h"

namespace rx
{

class SurfaceNULL : public SurfaceImpl
{
  public:
    SurfaceNULL(const egl::SurfaceState &surfaceState);
    ~SurfaceNULL() override;

    egl::Error initialize() override;
    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) override;
    egl::Error swap() override;
    egl::Error postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(gl::Texture *texture, EGLint buffer) override;
    egl::Error releaseTexImage(EGLint buffer) override;
    void setSwapInterval(EGLint interval) override;

    // width and height can change with client window resizing
    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    gl::Error getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                        FramebufferAttachmentRenderTarget **rtOut) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_SURFACENULL_H_
