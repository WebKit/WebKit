//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceVk.cpp:
//    Implements the class methods for SurfaceVk.
//

#include "libANGLE/renderer/vulkan/SurfaceVk.h"

#include "common/debug.h"

namespace rx
{

SurfaceVk::SurfaceVk(const egl::SurfaceState &surfaceState) : SurfaceImpl(surfaceState)
{
}

SurfaceVk::~SurfaceVk()
{
}

egl::Error SurfaceVk::initialize()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

FramebufferImpl *SurfaceVk::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    UNIMPLEMENTED();
    return static_cast<FramebufferImpl *>(0);
}

egl::Error SurfaceVk::swap()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceVk::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceVk::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceVk::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceVk::releaseTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

egl::Error SurfaceVk::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

void SurfaceVk::setSwapInterval(EGLint interval)
{
    UNIMPLEMENTED();
}

EGLint SurfaceVk::getWidth() const
{
    UNIMPLEMENTED();
    return EGLint();
}

EGLint SurfaceVk::getHeight() const
{
    UNIMPLEMENTED();
    return EGLint();
}

EGLint SurfaceVk::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGLint();
}

EGLint SurfaceVk::getSwapBehavior() const
{
    UNIMPLEMENTED();
    return EGLint();
}

gl::Error SurfaceVk::getAttachmentRenderTarget(const gl::FramebufferAttachment::Target &target,
                                               FramebufferAttachmentRenderTarget **rtOut)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
