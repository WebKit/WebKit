/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Basic portability.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deInt32.h"

/* Assert base type sizes. */
DE_STATIC_ASSERT(sizeof(uint8_t) == 1);
DE_STATIC_ASSERT(sizeof(uint16_t) == 2);
DE_STATIC_ASSERT(sizeof(uint32_t) == 4);
DE_STATIC_ASSERT(sizeof(uint64_t) == 8);
DE_STATIC_ASSERT(sizeof(int8_t) == 1);
DE_STATIC_ASSERT(sizeof(int16_t) == 2);
DE_STATIC_ASSERT(sizeof(int32_t) == 4);
DE_STATIC_ASSERT(sizeof(int64_t) == 8);
DE_STATIC_ASSERT(sizeof(uintptr_t) == sizeof(void *));
DE_STATIC_ASSERT(sizeof(intptr_t) == sizeof(void *));
DE_STATIC_ASSERT(DE_PTR_SIZE == sizeof(void *));

/* Sanity checks for DE_PTR_SIZE & DE_CPU */
#if !((DE_CPU == DE_CPU_X86_64 || DE_CPU == DE_CPU_ARM_64 || DE_CPU == DE_CPU_MIPS_64 || DE_CPU == DE_CPU_RISCV_64) && \
      (DE_PTR_SIZE == 8)) &&                                                                                           \
    !((DE_CPU == DE_CPU_X86 || DE_CPU == DE_CPU_ARM || DE_CPU == DE_CPU_MIPS || DE_CPU == DE_CPU_RISCV_32) &&          \
      (DE_PTR_SIZE == 4))
#error "DE_CPU and DE_PTR_SIZE mismatch"
#endif

#if (DE_OS == DE_OS_UNIX || DE_OS == DE_OS_WIN32) && defined(NDEBUG)
/* We need __assert_fail declaration from assert.h */
#undef NDEBUG
#endif

#include <stdio.h>
#include <assert.h>
#include <string.h>

#if (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS) || defined(__FreeBSD__)
#include <signal.h>
#include <stdlib.h>
#endif

#if (DE_OS == DE_OS_ANDROID)
#include <android/log.h>
#endif

/*
#if (DE_OS == DE_OS_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif
*/

DE_BEGIN_EXTERN_C

#if defined(DE_ASSERT_FAILURE_CALLBACK)
static deAssertFailureCallbackFunc g_assertFailureCallback = NULL;
#endif

void deSetAssertFailureCallback(deAssertFailureCallbackFunc callback)
{
#if defined(DE_ASSERT_FAILURE_CALLBACK)
    g_assertFailureCallback = callback;
#else
    DE_UNREF(callback);
#endif
}

void deAssertFail(const char *reason, const char *file, int line)
{
#if defined(DE_ASSERT_FAILURE_CALLBACK)
    if (g_assertFailureCallback != NULL)
    {
        /* Remove callback in case of the callback causes further asserts. */
        deAssertFailureCallbackFunc callback = g_assertFailureCallback;
        deSetAssertFailureCallback(NULL);
        callback(reason, file, line);
    }
#endif

#if (((DE_OS == DE_OS_WIN32) || (DE_OS == DE_OS_WINCE)) && (DE_COMPILER == DE_COMPILER_MSC))
    {
        wchar_t wreason[1024];
        wchar_t wfile[128];
        int num;
        int i;

        /*    MessageBox(reason, "Assertion failed", MB_OK); */

        num = deMin32((int)strlen(reason), DE_LENGTH_OF_ARRAY(wreason) - 1);
        for (i = 0; i < num; i++)
            wreason[i] = reason[i];
        wreason[i] = 0;

        num = deMin32((int)strlen(file), DE_LENGTH_OF_ARRAY(wfile) - 1);
        for (i = 0; i < num; i++)
            wfile[i] = file[i];
        wfile[i] = 0;

#if (DE_OS == DE_OS_WIN32)
        _wassert(wreason, wfile, line);
#else /* WINCE */
        assert(wreason);
#endif
    }
#elif ((DE_OS == DE_OS_WIN32) && (DE_COMPILER == DE_COMPILER_CLANG || DE_COMPILER == DE_COMPILER_GCC))
    _assert(reason, file, line);
#elif (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS) || defined(__FreeBSD__)
    fprintf(stderr, "Assertion '%s' failed at %s:%d\n", reason, file, line);
    raise(SIGTRAP);
    abort();
#elif (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_FUCHSIA)
    __assert_fail(reason, file, (unsigned int)line, "Unknown function");
#elif (DE_OS == DE_OS_QNX)
    __assert(reason, file, (unsigned int)line, "Unknown function");
#elif (DE_OS == DE_OS_SYMBIAN)
    __assert("Unknown function", file, line, reason);
#elif (DE_OS == DE_OS_ANDROID)
    __android_log_print(ANDROID_LOG_ERROR, "delibs", "Assertion '%s' failed at %s:%d", reason, file, line);
    __assert(file, line, reason);
#else
#error Implement assertion function on your platform.
#endif
}

DE_END_EXTERN_C
