//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// tls.h: Simple cross-platform interface for thread local storage.

#ifndef COMMON_TLS_H_
#define COMMON_TLS_H_

#include "common/angleutils.h"
#include "common/platform.h"

namespace gl
{
class Context;
}

#ifdef ANGLE_PLATFORM_WINDOWS

// TLS does not exist for Windows Store and needs to be emulated
#    ifdef ANGLE_ENABLE_WINDOWS_UWP
#        ifndef TLS_OUT_OF_INDEXES
#            define TLS_OUT_OF_INDEXES static_cast<DWORD>(0xFFFFFFFF)
#        endif
#        ifndef CREATE_SUSPENDED
#            define CREATE_SUSPENDED 0x00000004
#        endif
#    endif
typedef DWORD TLSIndex;
#    define TLS_INVALID_INDEX (TLS_OUT_OF_INDEXES)
#elif defined(ANGLE_PLATFORM_POSIX)
#    include <errno.h>
#    include <pthread.h>
#    include <semaphore.h>
typedef pthread_key_t TLSIndex;
#    define TLS_INVALID_INDEX (static_cast<TLSIndex>(-1))
#else
#    error Unsupported platform.
#endif

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

//  - TLS_SLOT_OPENGL and TLS_SLOT_OPENGL_API: These two aren't used by bionic
//    itself, but allow the graphics code to access TLS directly rather than
//    using the pthread API.
//
// Choose the TLS_SLOT_OPENGL TLS slot with the value that matches value in the header file in
// bionic(tls_defines.h)
constexpr TLSIndex kAndroidOpenGLTlsSlot = 3;

extern bool gUseAndroidOpenGLTlsSlot;
ANGLE_INLINE bool SetContextToAndroidOpenGLTLSSlot(gl::Context *value)
{
#if defined(ANGLE_PLATFORM_ANDROID)
    if (gUseAndroidOpenGLTlsSlot)
    {
        ANGLE_ANDROID_GET_GL_TLS()[kAndroidOpenGLTlsSlot] = static_cast<void *>(value);
        return true;
    }
#endif
    return false;
}

void SetUseAndroidOpenGLTlsSlot(bool platformTypeVulkan);

// TODO(kbr): for POSIX platforms this will have to be changed to take
// in a destructor function pointer, to allow the thread-local storage
// to be properly deallocated upon thread exit.
TLSIndex CreateTLSIndex();
bool DestroyTLSIndex(TLSIndex index);

bool SetTLSValue(TLSIndex index, void *value);
void *GetTLSValue(TLSIndex index);

#endif  // COMMON_TLS_H_
