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

void Thread::setCurrent(gl::Context *context)
{
    mContext = context;
}

Surface *Thread::getCurrentDrawSurface() const
{
    if (mContext)
    {
        return mContext->getCurrentDrawSurface();
    }
    return nullptr;
}

Surface *Thread::getCurrentReadSurface() const
{
    if (mContext)
    {
        return mContext->getCurrentReadSurface();
    }
    return nullptr;
}

gl::Context *Thread::getContext() const
{
    return mContext;
}

gl::Context *Thread::getValidContext() const
{
    if (mContext && mContext->isContextLost())
    {
        mContext->handleError(gl::OutOfMemory() << "Context has been lost.");
        return nullptr;
    }

    return mContext;
}

Display *Thread::getCurrentDisplay() const
{
    if (mContext)
    {
        return mContext->getCurrentDisplay();
    }
    return nullptr;
}

}  // namespace egl
