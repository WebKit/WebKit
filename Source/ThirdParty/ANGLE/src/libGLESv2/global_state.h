//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// global_state.h : Defines functions for querying the thread-local GL and EGL state.

#ifndef LIBGLESV2_GLOBALSTATE_H_
#define LIBGLESV2_GLOBALSTATE_H_

#include "libANGLE/Context.h"
#include "libANGLE/Debug.h"
#include "libANGLE/GlobalMutex.h"
#include "libANGLE/Thread.h"
#include "libANGLE/features.h"

#if defined(ANGLE_PLATFORM_APPLE) || (ANGLE_PLATFORM_ANDROID)
#    include "common/tls.h"
#endif

#include <mutex>

namespace egl
{
class Debug;
class Thread;

#if defined(ANGLE_PLATFORM_APPLE)
extern Thread *GetCurrentThreadTLS();
extern void SetCurrentThreadTLS(Thread *thread);
#else
extern thread_local Thread *gCurrentThread;
#endif

gl::Context *GetGlobalLastContext();
void SetGlobalLastContext(gl::Context *context);
Thread *GetCurrentThread();
Debug *GetDebug();

// Sync the current context from Thread to global state.
class [[nodiscard]] ScopedSyncCurrentContextFromThread
{
  public:
    ScopedSyncCurrentContextFromThread(egl::Thread *thread);
    ~ScopedSyncCurrentContextFromThread();

  private:
    egl::Thread *const mThread;
};

}  // namespace egl

#define ANGLE_SCOPED_GLOBAL_LOCK() egl::ScopedGlobalMutexLock globalMutexLock

namespace gl
{
ANGLE_INLINE Context *GetGlobalContext()
{
#if defined(ANGLE_PLATFORM_APPLE)
    egl::Thread *currentThread = egl::GetCurrentThreadTLS();
#else
    egl::Thread *currentThread = egl::gCurrentThread;
#endif
    ASSERT(currentThread);
    return currentThread->getContext();
}

ANGLE_INLINE Context *GetValidGlobalContext()
{
#if defined(ANGLE_USE_ANDROID_TLS_SLOT)
    // TODO: Replace this branch with a compile time flag (http://anglebug.com/4764)
    if (angle::gUseAndroidOpenGLTlsSlot)
    {
        return static_cast<gl::Context *>(ANGLE_ANDROID_GET_GL_TLS()[angle::kAndroidOpenGLTlsSlot]);
    }
#endif

#if defined(ANGLE_PLATFORM_APPLE)
    return GetCurrentValidContextTLS();
#else
    return gCurrentValidContext;
#endif
}

// Generate a context lost error on the context if it is non-null and lost.
void GenerateContextLostErrorOnContext(Context *context);
void GenerateContextLostErrorOnCurrentGlobalContext();

#if defined(ANGLE_FORCE_CONTEXT_CHECK_EVERY_CALL)
// TODO(b/177574181): This should be handled in a backend-specific way.
// if previous context different from current context, dirty all state
static ANGLE_INLINE void DirtyContextIfNeeded(Context *context)
{
    if (context && context != egl::GetGlobalLastContext())
    {
        context->dirtyAllState();
        SetGlobalLastContext(context);
    }
}

#endif

#if !defined(ANGLE_ENABLE_SHARE_CONTEXT_LOCK)
#    define SCOPED_SHARE_CONTEXT_LOCK(context)
#    define SCOPED_GLOBAL_AND_SHARE_CONTEXT_LOCK(context) ANGLE_SCOPED_GLOBAL_LOCK()
#else
#    if defined(ANGLE_FORCE_CONTEXT_CHECK_EVERY_CALL)
#        define SCOPED_SHARE_CONTEXT_LOCK(context)       \
            egl::ScopedGlobalMutexLock shareContextLock; \
            DirtyContextIfNeeded(context)
#        define SCOPED_GLOBAL_AND_SHARE_CONTEXT_LOCK(context) SCOPED_SHARE_CONTEXT_LOCK(context)
#    else
#        define SCOPED_SHARE_CONTEXT_LOCK(context) \
            egl::ScopedOptionalGlobalMutexLock shareContextLock(context->isShared())
#        define SCOPED_GLOBAL_AND_SHARE_CONTEXT_LOCK(context) ANGLE_SCOPED_GLOBAL_LOCK()
#    endif
#endif

}  // namespace gl

#endif  // LIBGLESV2_GLOBALSTATE_H_
