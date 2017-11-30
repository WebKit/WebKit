//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceImpl.h: Implementation methods of egl::Surface

#ifndef LIBANGLE_RENDERER_SURFACEIMPL_H_
#define LIBANGLE_RENDERER_SURFACEIMPL_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/renderer/FramebufferAttachmentObjectImpl.h"

namespace gl
{
class FramebufferState;
}

namespace egl
{
class Display;
struct Config;
struct SurfaceState;
class Thread;
}

namespace rx
{
class FramebufferImpl;

class SurfaceImpl : public FramebufferAttachmentObjectImpl
{
  public:
    SurfaceImpl(const egl::SurfaceState &surfaceState);
    ~SurfaceImpl() override;
    virtual void destroy(const egl::Display *display) {}

    virtual egl::Error initialize(const egl::Display *display)                           = 0;
    virtual FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) = 0;
    virtual egl::Error swap(const gl::Context *context)                                  = 0;
    virtual egl::Error swapWithDamage(const gl::Context *context, EGLint *rects, EGLint n_rects);
    virtual egl::Error postSubBuffer(const gl::Context *context,
                                     EGLint x,
                                     EGLint y,
                                     EGLint width,
                                     EGLint height) = 0;
    virtual egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) = 0;
    virtual egl::Error bindTexImage(gl::Texture *texture, EGLint buffer) = 0;
    virtual egl::Error releaseTexImage(EGLint buffer) = 0;
    virtual egl::Error getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc) = 0;
    virtual void setSwapInterval(EGLint interval) = 0;

    // width and height can change with client window resizing
    virtual EGLint getWidth() const = 0;
    virtual EGLint getHeight() const = 0;

    virtual EGLint isPostSubBufferSupported() const = 0;
    virtual EGLint getSwapBehavior() const = 0;

  protected:
    const egl::SurfaceState &mState;
};

}

#endif // LIBANGLE_RENDERER_SURFACEIMPL_H_

