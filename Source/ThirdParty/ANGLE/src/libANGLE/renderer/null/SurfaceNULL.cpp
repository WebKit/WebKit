//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceNULL.cpp:
//    Implements the class methods for SurfaceNULL.
//

#include "libANGLE/renderer/null/SurfaceNULL.h"

#include "common/debug.h"

#include "libANGLE/renderer/null/FramebufferNULL.h"

namespace rx
{

SurfaceNULL::SurfaceNULL(const egl::SurfaceState &surfaceState) : SurfaceImpl(surfaceState) {}

SurfaceNULL::~SurfaceNULL() {}

egl::Error SurfaceNULL::initialize(const egl::Display *display)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::swap(const gl::Context *context, SurfaceSwapFeedback *feedback)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::postSubBuffer(const gl::Context *context,
                                      EGLint x,
                                      EGLint y,
                                      EGLint width,
                                      EGLint height)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNREACHABLE();
    return egl::NoError();
}

egl::Error SurfaceNULL::bindTexImage(const gl::Context *context,
                                     gl::Texture *texture,
                                     EGLint buffer)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceNULL::getMscRate(EGLint *numerator, EGLint *denominator)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

void SurfaceNULL::setSwapInterval(const egl::Display *display, EGLint interval) {}

gl::Extents SurfaceNULL::getSize() const
{
    // TODO(geofflang): Read from an actual window?
    return gl::Extents(100, 100, 1);
}

EGLint SurfaceNULL::isPostSubBufferSupported() const
{
    return EGL_TRUE;
}

EGLint SurfaceNULL::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

angle::Result SurfaceNULL::initializeContents(const gl::Context *context,
                                              GLenum binding,
                                              const gl::ImageIndex &imageIndex)
{
    return angle::Result::Continue;
}

egl::Error SurfaceNULL::attachToFramebuffer(const gl::Context *context,
                                            gl::Framebuffer *framebuffer)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::detachFromFramebuffer(const gl::Context *context,
                                              gl::Framebuffer *framebuffer)
{
    return egl::NoError();
}

}  // namespace rx
