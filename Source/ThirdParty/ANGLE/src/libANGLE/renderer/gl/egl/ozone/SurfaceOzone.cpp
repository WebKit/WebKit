//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceOzone.cpp: Ozone implementation of egl::SurfaceGL

#include "libANGLE/renderer/gl/egl/ozone/SurfaceOzone.h"

#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/egl/ozone/DisplayOzone.h"

namespace rx
{

SurfaceOzone::SurfaceOzone(const egl::SurfaceState &state,
                           RendererGL *renderer,
                           DisplayOzone::Buffer *buffer)
    : SurfaceGL(state, renderer), mBuffer(buffer)
{
}

SurfaceOzone::~SurfaceOzone()
{
    delete mBuffer;
}

egl::Error SurfaceOzone::initialize()
{
    return egl::Error(EGL_SUCCESS);
}

FramebufferImpl *SurfaceOzone::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    return mBuffer->framebufferGL(state);
}

egl::Error SurfaceOzone::makeCurrent()
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceOzone::swap()
{
    mBuffer->present();
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceOzone::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceOzone::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceOzone::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    mBuffer->bindTexImage();
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceOzone::releaseTexImage(EGLint buffer)
{
    return egl::Error(EGL_SUCCESS);
}

void SurfaceOzone::setSwapInterval(EGLint interval)
{
    mSwapControl.targetSwapInterval = interval;
}

EGLint SurfaceOzone::getWidth() const
{
    return mBuffer->getWidth();
}

EGLint SurfaceOzone::getHeight() const
{
    return mBuffer->getHeight();
}

EGLint SurfaceOzone::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGL_FALSE;
}

EGLint SurfaceOzone::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}
}  // namespace rx
