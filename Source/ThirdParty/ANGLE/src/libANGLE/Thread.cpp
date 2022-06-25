//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Thread.cpp : Defines the Thread class which represents a global EGL thread.

#include "libANGLE/Thread.h"

#include "libANGLE/Context.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Display.h"
#include "libANGLE/Error.h"

namespace angle
{
bool gUseAndroidOpenGLTlsSlot;
std::atomic_int gProcessCleanupRefCount(0);

void ProcessCleanupCallback(void *ptr)
{
    egl::Thread *thread = static_cast<egl::Thread *>(ptr);
    ASSERT(thread);

    ASSERT(gProcessCleanupRefCount > 0);
    if (--gProcessCleanupRefCount == 0)
    {
        egl::Display::EglDisplaySet displays = egl::Display::GetEglDisplaySet();
        for (egl::Display *display : displays)
        {
            ASSERT(display);
            (void)display->terminate(thread, egl::Display::TerminateReason::ProcessExit);
        }
    }
}
}  // namespace angle

namespace egl
{
namespace
{
Debug *sDebug = nullptr;
}  // namespace

Thread::Thread()
    : mLabel(nullptr),
      mError(EGL_SUCCESS),
      mAPI(EGL_OPENGL_ES_API),
      mContext(static_cast<gl::Context *>(EGL_NO_CONTEXT))
{}

void Thread::setLabel(EGLLabelKHR label)
{
    mLabel = label;
}

EGLLabelKHR Thread::getLabel() const
{
    return mLabel;
}

void Thread::setSuccess()
{
    mError = EGL_SUCCESS;
}

void Thread::setError(EGLint error,
                      const char *command,
                      const LabeledObject *object,
                      const char *message)
{
    mError = error;
    if (error != EGL_SUCCESS && message)
    {
        EnsureDebugAllocated();
        sDebug->insertMessage(error, command, ErrorCodeToMessageType(error), getLabel(),
                              object ? object->getLabel() : nullptr, message);
    }
}

void Thread::setError(const Error &error, const char *command, const LabeledObject *object)
{
    mError = error.getCode();
    if (error.isError() && !error.getMessage().empty())
    {
        EnsureDebugAllocated();
        sDebug->insertMessage(error.getCode(), command, ErrorCodeToMessageType(error.getCode()),
                              getLabel(), object ? object->getLabel() : nullptr,
                              error.getMessage());
    }
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

Display *Thread::getDisplay() const
{
    if (mContext)
    {
        return mContext->getDisplay();
    }
    return nullptr;
}

void EnsureDebugAllocated()
{
    // All EGL calls use a global lock, this is thread safe
    if (sDebug == nullptr)
    {
        sDebug = new Debug();
    }
}

void DeallocateDebug()
{
    SafeDelete(sDebug);
}

Debug *GetDebug()
{
    EnsureDebugAllocated();
    return sDebug;
}
}  // namespace egl
