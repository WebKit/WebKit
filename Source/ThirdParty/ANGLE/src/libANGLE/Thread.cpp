//
// Copyright(c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Thread.cpp : Defines the Thread class which represents a global EGL thread.

#include "libANGLE/Thread.h"

#include "libANGLE/Context.h"
#include "libANGLE/Error.h"

namespace egl
{
Thread::Thread()
    : mError(EGL_SUCCESS),
      mAPI(EGL_OPENGL_ES_API),
      mDisplay(static_cast<egl::Display *>(EGL_NO_DISPLAY)),
      mDrawSurface(static_cast<egl::Surface *>(EGL_NO_SURFACE)),
      mReadSurface(static_cast<egl::Surface *>(EGL_NO_SURFACE)),
      mContext(static_cast<gl::Context *>(EGL_NO_CONTEXT))
{
}

void Thread::setError(const Error &error)
{
    mError = error.getCode();
}

EGLint Thread::getError() const
{
    return mError;
}

void Thread::setAPI(EGLenum api)
{
    mAPI = api;
}

EGLenum Thread::getAPI() const
{
    return mAPI;
}

void Thread::setCurrent(Display *display,
                        Surface *drawSurface,
                        Surface *readSurface,
                        gl::Context *context)
{
    mDisplay     = display;
    mDrawSurface = drawSurface;
    mReadSurface = readSurface;
    mContext     = context;
}

Display *Thread::getDisplay() const
{
    return mDisplay;
}

Surface *Thread::getDrawSurface() const
{
    return mDrawSurface;
}

Surface *Thread::getReadSurface() const
{
    return mReadSurface;
}

gl::Context *Thread::getContext() const
{
    return mContext;
}

gl::Context *Thread::getValidContext() const
{
    if (mContext && mContext->isContextLost())
    {
        mContext->handleError(gl::Error(GL_OUT_OF_MEMORY, "Context has been lost."));
        return nullptr;
    }

    return mContext;
}

}  // namespace egl
