//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceGbm.cpp: Gbm implementation of egl::SurfaceGL

#include "libANGLE/renderer/gl/egl/gbm/SurfaceGbm.h"

#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/egl/gbm/DisplayGbm.h"

namespace rx
{

SurfaceGbm::SurfaceGbm(const egl::SurfaceState &state, DisplayGbm::Buffer *buffer)
    : SurfaceGL(state), mBuffer(buffer)
{}

SurfaceGbm::~SurfaceGbm()
{
    delete mBuffer;
}

egl::Error SurfaceGbm::initialize(const egl::Display *display)
{
    return egl::NoError();
}

FramebufferImpl *SurfaceGbm::createDefaultFramebuffer(const gl::Context *context,
                                                      const gl::FramebufferState &state)
{
    return mBuffer->framebufferGL(context, state);
}

egl::Error SurfaceGbm::makeCurrent(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error SurfaceGbm::swap(const gl::Context *context)
{
    mBuffer->present(context);
    return egl::NoError();
}

egl::Error SurfaceGbm::postSubBuffer(const gl::Context *context,
                                     EGLint x,
                                     EGLint y,
                                     EGLint width,
                                     EGLint height)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error SurfaceGbm::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error SurfaceGbm::bindTexImage(const gl::Context *context, gl::Texture *texture, EGLint buffer)
{
    mBuffer->bindTexImage();
    return egl::NoError();
}

egl::Error SurfaceGbm::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    return egl::NoError();
}

void SurfaceGbm::setSwapInterval(EGLint interval)
{
    mSwapControl.targetSwapInterval = interval;
}

EGLint SurfaceGbm::getWidth() const
{
    return mBuffer->getWidth();
}

EGLint SurfaceGbm::getHeight() const
{
    return mBuffer->getHeight();
}

EGLint SurfaceGbm::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGL_FALSE;
}

EGLint SurfaceGbm::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}
}  // namespace rx
