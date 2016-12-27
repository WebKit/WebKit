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

namespace rx
{

SurfaceNULL::SurfaceNULL(const egl::SurfaceState &surfaceState) : SurfaceImpl(surfaceState)
{
}

SurfaceNULL::~SurfaceNULL()
{
}

egl::Error SurfaceNULL::initialize()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

FramebufferImpl *SurfaceNULL::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    UNIMPLEMENTED();
    return static_cast<FramebufferImpl *>(0);
}

egl::Error SurfaceNULL::swap()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceNULL::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceNULL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceNULL::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceNULL::releaseTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

void SurfaceNULL::setSwapInterval(EGLint interval)
{
    UNIMPLEMENTED();
}

EGLint SurfaceNULL::getWidth() const
{
    UNIMPLEMENTED();
    return EGLint();
}

EGLint SurfaceNULL::getHeight() const
{
    UNIMPLEMENTED();
    return EGLint();
}

EGLint SurfaceNULL::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGLint();
}

EGLint SurfaceNULL::getSwapBehavior() const
{
    UNIMPLEMENTED();
    return EGLint();
}

gl::Error SurfaceNULL::getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                                 FramebufferAttachmentRenderTarget **rtOut)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
