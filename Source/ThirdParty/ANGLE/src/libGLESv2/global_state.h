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
#include "libANGLE/Thread.h"
#include "libANGLE/features.h"

#if defined(ANGLE_PLATFORM_APPLE) || (ANGLE_PLATFORM_ANDROID)
#    include "common/tls.h"
#endif

#include <mutex>

namespace angle
{
using GlobalMutex = std::recursive_mutex;

//  - TLS_SLOT_OPENGL and TLS_SLOT_OPENGL_API: These two aren't used by bionic
//    itself, but allow the graphics code to access TLS directly rather than
//    using the pthread API.
//
// Choose the TLS_SLOT_OPENGL TLS slot with the value that matches value in the header file in
// bionic(tls_defines.h)
constexpr size_t kAndroidOpenGLTlsSlot = 3;

#if defined(ANGLE_PLATFORM_ANDROID)

// The following ASM variant provides a much more performant store/retrieve interface
// compared to those provided by the pthread library. These have been derived from code
// in the bionic module of Android ->
// https://cs.android.com/android/platform/superproject/+/master:bionic/libc/platform/bionic/tls.h;l=30

#    if defined(__aarch64__)
#        define ANGLE_ANDROID_GET_GL_TLS()                  \
            ({                                              \
                void **__val;                               \
                __asm__("mrs %0, tpidr_el0" : "=r"(__val)); \
                __val;                                      \
            })
#    elif defined(__arm__)
#        define ANGLE_ANDROID_GET_GL_TLS()                           \
            ({                                                       \
                void **__val;                                        \
                __asm__("mrc p15, 0, %0, c13, c0, 3" : "=r"(__val)); \
                __val;                                               \
            })
#    elif defined(__mips__)
// On mips32r1, this goes via a kernel illegal instruction trap that's
// optimized for v1
#        define ANGLE_ANDROID_GET_GL_TLS()       \
            ({                                   \
                register void **__val asm("v1"); \
                __asm__(                         \
                    ".set    push\n"             \
                    ".set    mips32r2\n"         \
                    "rdhwr   %0,$29\n"           \
                    ".set    pop\n"              \
                    : "=r"(__val));              \
                __val;                           \
            })
#    elif defined(__i386__)
#        define ANGLE_ANDROID_GET_GL_TLS()                \
            ({                                            \
                void **__val;                             \
                __asm__("movl %%gs:0, %0" : "=r"(__val)); \
                __val;                                    \
            })
#    elif defined(__x86_64__)
#        define ANGLE_ANDROID_GET_GL_TLS()               \
            ({                                           \
                void **__val;                            \
                __asm__("mov %%fs:0, %0" : "=r"(__val)); \
                __val;                                   \
            })
#    else
#        error unsupported architecture
#    endif

#endif  // ANGLE_PLATFORM_ANDROID
}  // namespace angle

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

angle::GlobalMutex &GetGlobalMutex();
angle::GlobalMutex &GetGlobalSurfaceMutex();
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

#define ANGLE_GLOBAL_SURFACE_LOCK_VAR_NAME globalSurfaceMutexLock
#define ANGLE_SCOPED_GLOBAL_SURFACE_LOCK()                                  \
    std::lock_guard<angle::GlobalMutex> ANGLE_GLOBAL_SURFACE_LOCK_VAR_NAME( \
        egl::GetGlobalSurfaceMutex())

#define ANGLE_GLOBAL_LOCK_VAR_NAME globalMutexLock
#define ANGLE_SCOPED_GLOBAL_LOCK() \
    std::lock_guard<angle::GlobalMutex> ANGLE_GLOBAL_LOCK_VAR_NAME(egl::GetGlobalMutex())

namespace gl
{
ANGLE_INLINE Context *GetGlobalContext()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    // TODO: Replace this branch with a compile time flag (http://anglebug.com/4764)
    if (angle::gUseAndroidOpenGLTlsSlot)
    {
        return static_cast<gl::Context *>(ANGLE_ANDROID_GET_GL_TLS()[angle::kAndroidOpenGLTlsSlot]);
    }
#endif

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
#if defined(ANGLE_PLATFORM_ANDROID)
    // TODO: Replace this branch with a compile time flag (http://anglebug.com/4764)
    if (angle::gUseAndroidOpenGLTlsSlot)
    {
        Context *context =
            static_cast<gl::Context *>(ANGLE_ANDROID_GET_GL_TLS()[angle::kAndroidOpenGLTlsSlot]);
        if (context && !context->isContextLost())
        {
            return context;
        }
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

ANGLE_INLINE std::unique_lock<angle::GlobalMutex> GetContextLock(Context *context)
{
#if defined(ANGLE_FORCE_CONTEXT_CHECK_EVERY_CALL)
    auto lock = std::unique_lock<angle::GlobalMutex>(egl::GetGlobalMutex());

    DirtyContextIfNeeded(context);
    return lock;
#else
    return context->isShared() ? std::unique_lock<angle::GlobalMutex>(egl::GetGlobalMutex())
                               : std::unique_lock<angle::GlobalMutex>();
#endif
}

}  // namespace gl

#endif  // LIBGLESV2_GLOBALSTATE_H_
