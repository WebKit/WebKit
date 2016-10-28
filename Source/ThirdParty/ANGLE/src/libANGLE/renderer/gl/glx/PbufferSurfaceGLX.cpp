//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PbufferSurfaceGLX.cpp: GLX implementation of egl::Surface for PBuffers

#include "libANGLE/renderer/gl/glx/PbufferSurfaceGLX.h"

#include "common/debug.h"
#include "libANGLE/renderer/gl/glx/DisplayGLX.h"
#include "libANGLE/renderer/gl/glx/FunctionsGLX.h"

namespace rx
{

PbufferSurfaceGLX::PbufferSurfaceGLX(const egl::SurfaceState &state,
                                     RendererGL *renderer,
                                     EGLint width,
                                     EGLint height,
                                     bool largest,
                                     const FunctionsGLX &glx,
                                     glx::Context context,
                                     glx::FBConfig fbConfig)
    : SurfaceGLX(state, renderer),
      mWidth(width),
      mHeight(height),
      mLargest(largest),
      mGLX(glx),
      mContext(context),
      mFBConfig(fbConfig),
      mPbuffer(0)
{
}

PbufferSurfaceGLX::~PbufferSurfaceGLX()
{
    if (mPbuffer)
    {
        mGLX.destroyPbuffer(mPbuffer);
    }
}

egl::Error PbufferSurfaceGLX::initialize()
{
    // Avoid creating 0-sized PBuffers as it fails on the Intel Mesa driver
    // as commented on https://bugs.freedesktop.org/show_bug.cgi?id=38869 so we
    // use (w, 1) or (1, h) instead.
    int width = std::max(1, static_cast<int>(mWidth));
    int height = std::max(1, static_cast<int>(mHeight));

    const int attribs[] =
    {
        GLX_PBUFFER_WIDTH, width,
        GLX_PBUFFER_HEIGHT, height,
        GLX_LARGEST_PBUFFER, mLargest,
        None
    };

    mPbuffer = mGLX.createPbuffer(mFBConfig, attribs);
    if (!mPbuffer)
    {
        return egl::Error(EGL_BAD_ALLOC, "Failed to create a native GLX pbuffer.");
    }

    if (mLargest)
    {
        mGLX.queryDrawable(mPbuffer, GLX_WIDTH, &mWidth);
        mGLX.queryDrawable(mPbuffer, GLX_HEIGHT, &mHeight);
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error PbufferSurfaceGLX::makeCurrent()
{
    if (mGLX.makeCurrent(mPbuffer, mContext) != True)
    {
        return egl::Error(EGL_BAD_DISPLAY);
    }
    return egl::Error(EGL_SUCCESS);
}

egl::Error PbufferSurfaceGLX::swap()
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error PbufferSurfaceGLX::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error PbufferSurfaceGLX::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error PbufferSurfaceGLX::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error PbufferSurfaceGLX::releaseTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

void PbufferSurfaceGLX::setSwapInterval(EGLint interval)
{
}

EGLint PbufferSurfaceGLX::getWidth() const
{
    return mWidth;
}

EGLint PbufferSurfaceGLX::getHeight() const
{
    return mHeight;
}

EGLint PbufferSurfaceGLX::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGL_FALSE;
}

EGLint PbufferSurfaceGLX::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

egl::Error PbufferSurfaceGLX::checkForResize()
{
    // The size of pbuffers never change
    return egl::Error(EGL_SUCCESS);
}
}
