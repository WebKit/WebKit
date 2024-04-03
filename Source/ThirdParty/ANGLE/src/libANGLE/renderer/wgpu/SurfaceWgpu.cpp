//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceWgpu.cpp:
//    Implements the class methods for SurfaceWgpu.
//

#include "libANGLE/renderer/wgpu/SurfaceWgpu.h"

#include "common/debug.h"

#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"

namespace rx
{

SurfaceWgpu::SurfaceWgpu(const egl::SurfaceState &surfaceState) : SurfaceImpl(surfaceState) {}

SurfaceWgpu::~SurfaceWgpu() {}

egl::Error SurfaceWgpu::initialize(const egl::Display *display)
{
    return egl::NoError();
}

egl::Error SurfaceWgpu::swap(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error SurfaceWgpu::postSubBuffer(const gl::Context *context,
                                      EGLint x,
                                      EGLint y,
                                      EGLint width,
                                      EGLint height)
{
    return egl::NoError();
}

egl::Error SurfaceWgpu::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNREACHABLE();
    return egl::NoError();
}

egl::Error SurfaceWgpu::bindTexImage(const gl::Context *context,
                                     gl::Texture *texture,
                                     EGLint buffer)
{
    return egl::NoError();
}

egl::Error SurfaceWgpu::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    return egl::NoError();
}

egl::Error SurfaceWgpu::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

egl::Error SurfaceWgpu::getMscRate(EGLint *numerator, EGLint *denominator)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

void SurfaceWgpu::setSwapInterval(EGLint interval) {}

EGLint SurfaceWgpu::getWidth() const
{
    // TODO(geofflang): Read from an actual window?
    return 100;
}

EGLint SurfaceWgpu::getHeight() const
{
    // TODO(geofflang): Read from an actual window?
    return 100;
}

EGLint SurfaceWgpu::isPostSubBufferSupported() const
{
    return EGL_TRUE;
}

EGLint SurfaceWgpu::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

angle::Result SurfaceWgpu::initializeContents(const gl::Context *context,
                                              GLenum binding,
                                              const gl::ImageIndex &imageIndex)
{
    return angle::Result::Continue;
}

egl::Error SurfaceWgpu::attachToFramebuffer(const gl::Context *context,
                                            gl::Framebuffer *framebuffer)
{
    return egl::NoError();
}

egl::Error SurfaceWgpu::detachFromFramebuffer(const gl::Context *context,
                                              gl::Framebuffer *framebuffer)
{
    return egl::NoError();
}

}  // namespace rx
