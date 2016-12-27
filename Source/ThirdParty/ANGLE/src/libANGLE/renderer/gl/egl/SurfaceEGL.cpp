//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceEGL.cpp: EGL implementation of egl::Surface

#include "libANGLE/renderer/gl/egl/SurfaceEGL.h"

#include "common/debug.h"

namespace rx
{

SurfaceEGL::SurfaceEGL(const egl::SurfaceState &state,
                       const FunctionsEGL *egl,
                       EGLConfig config,
                       const std::vector<EGLint> &attribList,
                       EGLContext context,
                       RendererGL *renderer)
    : SurfaceGL(state, renderer),
      mEGL(egl),
      mConfig(config),
      mAttribList(attribList),
      mSurface(EGL_NO_SURFACE),
      mContext(context)
{
}

SurfaceEGL::~SurfaceEGL()
{
    if (mSurface != EGL_NO_SURFACE)
    {
        EGLBoolean success = mEGL->destroySurface(mSurface);
        UNUSED_ASSERTION_VARIABLE(success);
        ASSERT(success == EGL_TRUE);
    }
}

egl::Error SurfaceEGL::makeCurrent()
{
    EGLBoolean success = mEGL->makeCurrent(mSurface, mContext);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglMakeCurrent failed");
    }
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceEGL::swap()
{
    EGLBoolean success = mEGL->swapBuffers(mSurface);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglSwapBuffers failed");
    }
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceEGL::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_SURFACE);
}

egl::Error SurfaceEGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_SURFACE);
}

egl::Error SurfaceEGL::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    EGLBoolean success = mEGL->bindTexImage(mSurface, buffer);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglBindTexImage failed");
    }
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceEGL::releaseTexImage(EGLint buffer)
{
    EGLBoolean success = mEGL->releaseTexImage(mSurface, buffer);
    if (success == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglReleaseTexImage failed");
    }
    return egl::Error(EGL_SUCCESS);
}

void SurfaceEGL::setSwapInterval(EGLint interval)
{
    EGLBoolean success = mEGL->swapInterval(interval);
    if (success == EGL_FALSE)
    {
        ERR("eglSwapInterval error 0x%04x", mEGL->getError());
        ASSERT(false);
    }
}

EGLint SurfaceEGL::getWidth() const
{
    EGLint value;
    EGLBoolean success = mEGL->querySurface(mSurface, EGL_WIDTH, &value);
    UNUSED_ASSERTION_VARIABLE(success);
    ASSERT(success == EGL_TRUE);
    return value;
}

EGLint SurfaceEGL::getHeight() const
{
    EGLint value;
    EGLBoolean success = mEGL->querySurface(mSurface, EGL_HEIGHT, &value);
    UNUSED_ASSERTION_VARIABLE(success);
    ASSERT(success == EGL_TRUE);
    return value;
}

EGLint SurfaceEGL::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return 0;
}

EGLint SurfaceEGL::getSwapBehavior() const
{
    EGLint value;
    EGLBoolean success = mEGL->querySurface(mSurface, EGL_SWAP_BEHAVIOR, &value);
    UNUSED_ASSERTION_VARIABLE(success);
    ASSERT(success == EGL_TRUE);
    return value;
}

}  // namespace rx
