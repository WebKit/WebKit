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

SurfaceNULL::SurfaceNULL(const egl::SurfaceState &surfaceState) : SurfaceImpl(surfaceState)
{
}

SurfaceNULL::~SurfaceNULL()
{
}

egl::Error SurfaceNULL::initialize(const DisplayImpl *displayImpl)
{
    return egl::NoError();
}

FramebufferImpl *SurfaceNULL::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    return new FramebufferNULL(state);
}

egl::Error SurfaceNULL::swap(const DisplayImpl *displayImpl)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNREACHABLE();
    return egl::NoError();
}

egl::Error SurfaceNULL::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::releaseTexImage(EGLint buffer)
{
    return egl::NoError();
}

egl::Error SurfaceNULL::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_ACCESS);
}

void SurfaceNULL::setSwapInterval(EGLint interval)
{
}

EGLint SurfaceNULL::getWidth() const
{
    // TODO(geofflang): Read from an actual window?
    return 100;
}

EGLint SurfaceNULL::getHeight() const
{
    // TODO(geofflang): Read from an actual window?
    return 100;
}

EGLint SurfaceNULL::isPostSubBufferSupported() const
{
    return EGL_TRUE;
}

EGLint SurfaceNULL::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

}  // namespace rx
