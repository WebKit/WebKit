//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PBufferSurfaceEAGL.cpp: an implementation of egl::Surface for PBuffers for the EAGL backend,
//                      currently implemented using renderbuffers

#import "common/platform.h"

#if defined(ANGLE_ENABLE_EAGL)

#    include "libANGLE/renderer/gl/eagl/PbufferSurfaceEAGL.h"

#    include "common/debug.h"
#    include "libANGLE/renderer/gl/FramebufferGL.h"
#    include "libANGLE/renderer/gl/FunctionsGL.h"
#    include "libANGLE/renderer/gl/RendererGL.h"
#    include "libANGLE/renderer/gl/StateManagerGL.h"

namespace rx
{

PbufferSurfaceEAGL::PbufferSurfaceEAGL(const egl::SurfaceState &state,
                                       RendererGL *renderer,
                                       EGLint width,
                                       EGLint height)
    : SurfaceGL(state),
      mWidth(width),
      mHeight(height),
      mFunctions(renderer->getFunctions()),
      mStateManager(renderer->getStateManager()),
      mColorRenderbuffer(0),
      mDSRenderbuffer(0)
{}

PbufferSurfaceEAGL::~PbufferSurfaceEAGL()
{
    if (mColorRenderbuffer != 0)
    {
        mFunctions->deleteRenderbuffers(1, &mColorRenderbuffer);
        mColorRenderbuffer = 0;
    }
    if (mDSRenderbuffer != 0)
    {
        mFunctions->deleteRenderbuffers(1, &mDSRenderbuffer);
        mDSRenderbuffer = 0;
    }
}

egl::Error PbufferSurfaceEAGL::initialize(const egl::Display *display)
{
    mFunctions->genRenderbuffers(1, &mColorRenderbuffer);
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mColorRenderbuffer);
    mFunctions->renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, mWidth, mHeight);

    mFunctions->genRenderbuffers(1, &mDSRenderbuffer);
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mDSRenderbuffer);
    mFunctions->renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWidth, mHeight);

    return egl::NoError();
}

egl::Error PbufferSurfaceEAGL::makeCurrent(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error PbufferSurfaceEAGL::swap(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error PbufferSurfaceEAGL::postSubBuffer(const gl::Context *context,
                                             EGLint x,
                                             EGLint y,
                                             EGLint width,
                                             EGLint height)
{
    return egl::NoError();
}

egl::Error PbufferSurfaceEAGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error PbufferSurfaceEAGL::bindTexImage(const gl::Context *context,
                                            gl::Texture *texture,
                                            EGLint buffer)
{
    ERR() << "PbufferSurfaceEAGL::bindTexImage";
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error PbufferSurfaceEAGL::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

void PbufferSurfaceEAGL::setSwapInterval(EGLint interval) {}

EGLint PbufferSurfaceEAGL::getWidth() const
{
    return mWidth;
}

EGLint PbufferSurfaceEAGL::getHeight() const
{
    return mHeight;
}

EGLint PbufferSurfaceEAGL::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGL_FALSE;
}

EGLint PbufferSurfaceEAGL::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

FramebufferImpl *PbufferSurfaceEAGL::createDefaultFramebuffer(const gl::Context *context,
                                                              const gl::FramebufferState &state)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    GLuint framebuffer = 0;
    functions->genFramebuffers(1, &framebuffer);
    stateManager->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    functions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                       mColorRenderbuffer);
    functions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                       mDSRenderbuffer);

    return new FramebufferGL(state, framebuffer, true, false);
}

}  // namespace rx

#endif  // defined(ANGLE_ENABLE_EAGL)
