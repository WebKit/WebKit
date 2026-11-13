//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SyncEGL.cpp: Implements the rx::SyncEGL class.

#include "libANGLE/renderer/gl/egl/SyncEGL.h"

#include "libANGLE/AttributeMap.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/egl/FunctionsEGL.h"

namespace rx
{

SyncEGL::SyncEGL(const FunctionsEGL *egl) : mEGL(egl) {}

SyncEGL::~SyncEGL()
{
    ASSERT(mSync == EGL_NO_SYNC_KHR);
}

void SyncEGL::onDestroy(const egl::ThreadSafeDisplay *display)
{
    if (mSync != EGL_NO_SYNC_KHR)
    {
        EGLBoolean result = mEGL->destroySyncKHR(mSync);
        mSync             = EGL_NO_SYNC_KHR;
        if (result == EGL_FALSE)
        {
            ERR() << "eglDestroySyncKHR failed: " << mEGL->getError();
        }
    }
}

egl::Error SyncEGL::initialize(const egl::ThreadSafeDisplay *display,
                               const gl::Context *context,
                               EGLenum type,
                               const egl::AttributeMap &attribs)
{
    ASSERT(type == EGL_SYNC_FENCE_KHR || type == EGL_SYNC_NATIVE_FENCE_ANDROID);
    ASSERT(mSync == EGL_NO_SYNC_KHR);

    constexpr size_t kAttribVectorSize = 3;
    angle::FixedVector<EGLint, kAttribVectorSize> nativeAttribs;
    if (type == EGL_SYNC_NATIVE_FENCE_ANDROID)
    {
        EGLint fenceFd =
            attribs.getAsInt(EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID);
        nativeAttribs.push_back(EGL_SYNC_NATIVE_FENCE_FD_ANDROID);
        nativeAttribs.push_back(fenceFd);
    }
    nativeAttribs.push_back(EGL_NONE);

    // Creating a fence adds a command to flush; importing a native fence does not.
    if (attribs.getAsInt(EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID) ==
        EGL_NO_NATIVE_FENCE_FD_ANDROID)
    {
        GetImplAs<ContextGL>(context)->markWorkSubmitted();
    }

    mSync = mEGL->createSyncKHR(type, nativeAttribs.data());
    if (mSync == EGL_NO_SYNC_KHR)
    {
        return egl::Error(mEGL->getError(), "eglCreateSyncKHR failed to create sync object");
    }

    return egl::NoError();
}

egl::Error SyncEGL::clientWait(const egl::ThreadSafeDisplay *display,
                               const gl::Context *context,
                               EGLint flags,
                               EGLTime timeout,
                               EGLint *outResult)
{
    ASSERT(mSync != EGL_NO_SYNC_KHR);

    // If we need to perform a CPU wait don't set the resultOut parameter passed into the
    // method, instead set the parameter passed into the unlocked tail call.
    //
    // The tail call runs after the entry point has released its reference to the sync.  The native
    // sync still outlives the wait: when a wait is deferred, ClientWaitSync() and
    // ClientWaitSyncKHR() add another tail call that holds a ScopedSyncRef, which is only released
    // after all tail calls have run.
    egl::Display::GetCurrentThreadUnlockedTailCall()->add(
        [egl = mEGL, sync = mSync, flags, timeout](void *resultOut) {
            *static_cast<EGLint *>(resultOut) = egl->clientWaitSyncKHR(sync, flags, timeout);
        });

    return egl::NoError();
}

egl::Error SyncEGL::serverWait(const egl::ThreadSafeDisplay *display,
                               const gl::Context *context,
                               EGLint flags)
{
    ASSERT(mSync != EGL_NO_SYNC_KHR);

    // A server wait doesn't block the CPU, so it is done right away, while the entry point keeps
    // the sync alive.
    EGLint result = mEGL->waitSyncKHR(mSync, flags);
    if (result == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglWaitSyncKHR failed");
    }

    return egl::NoError();
}

egl::Error SyncEGL::getStatus(const egl::ThreadSafeDisplay *display, EGLint *outStatus)
{
    ASSERT(mSync != EGL_NO_SYNC_KHR);
    EGLBoolean result = mEGL->getSyncAttribKHR(mSync, EGL_SYNC_STATUS_KHR, outStatus);

    if (result == EGL_FALSE)
    {
        return egl::Error(mEGL->getError(), "eglGetSyncAttribKHR with EGL_SYNC_STATUS_KHR failed");
    }

    return egl::NoError();
}

egl::Error SyncEGL::dupNativeFenceFD(const egl::ThreadSafeDisplay *display, EGLint *result) const
{
    ASSERT(mSync != EGL_NO_SYNC_KHR);
    *result = mEGL->dupNativeFenceFDANDROID(mSync);
    if (*result == EGL_NO_NATIVE_FENCE_FD_ANDROID)
    {
        return egl::Error(mEGL->getError(), "eglDupNativeFenceFDANDROID failed");
    }

    return egl::NoError();
}

}  // namespace rx
