/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pas_thread_suspend.h"

#include "pas_log.h"
#include "pas_machine_registers.h"
#include "pas_platform.h"
#include "pas_thread_suspend_lock.h"
#include "pas_thread_suspend_signal_handler.h"
#include "pas_utils.h"

#if PAS_OS(DARWIN)
#include <mach/mach.h>
#include <mach/thread_act.h>
#elif PAS_OS(WINDOWS)
#include <windows.h>
#else
#include <pthread.h>
#endif

PAS_BEGIN_EXTERN_C;

static bool pas_thread_suspend_initialized = false;

void pas_thread_suspend_initialize(void)
{
#if PAS_ENABLE_ASSERT
    pas_thread_suspend_initialized = true;
#endif
#if !PAS_OS(DARWIN) && !PAS_OS(WINDOWS)
    pas_thread_suspend_signal_handler_install();
#endif
}

#if PAS_OS(DARWIN)
bool pas_thread_suspend_suspend(pthread_t thread)
{
    PAS_ASSERT(pas_thread_suspend_initialized);
    pas_thread_suspend_lock_assert_held();

    thread_act_t mach_thread = pthread_mach_thread_np(thread);
    kern_return_t result = thread_suspend(mach_thread);

    if (result != KERN_SUCCESS) {
        pas_log("[%d] Failed to suspend pthread %p (mach thread %d): %d\n", getpid(), thread, mach_thread, result);
        return false;
    }
    return true;
}

void pas_thread_suspend_resume(pthread_t thread)
{
    PAS_ASSERT(pas_thread_suspend_initialized);
    pas_thread_suspend_lock_assert_held();

    thread_act_t mach_thread = pthread_mach_thread_np(thread);
    kern_return_t result = thread_resume(mach_thread);

    if (result != KERN_SUCCESS) {
        pas_log("[%d] Failed to resume pthread %p (mach thread %d): %d\n", getpid(), thread, mach_thread, result);
        PAS_ASSERT(result == KERN_SUCCESS);
    }
}
#else
bool pas_thread_suspend_suspend(pas_thread_suspend_data* data)
{
    PAS_ASSERT(pas_thread_suspend_initialized);
    pas_thread_suspend_lock_assert_held();
    if (data->did_suspend)
        return true;
#if PAS_OS(WINDOWS)
    HANDLE handle = (HANDLE)data->native_thread;
    DWORD result = SuspendThread(handle);
    if (result == (DWORD)-1) {
        pas_log("Failed to suspend Windows thread handle %p: %lu\n",
            handle, GetLastError());
        return false;
    }
    data->registers = NULL;
    data->did_suspend = true;
    return true;
#else
    return pas_thread_suspend_signal_handler_suspend(data);
#endif
}

void pas_thread_suspend_resume(pas_thread_suspend_data* data)
{
    PAS_ASSERT(pas_thread_suspend_initialized);
    pas_thread_suspend_lock_assert_held();
#if PAS_OS(WINDOWS)
    PAS_ASSERT(data->did_suspend);
    HANDLE handle = (HANDLE)data->native_thread;
    if (ResumeThread(handle) == (DWORD)-1) {
        pas_log("Failed to resume Windows thread handle %p: %lu\n",
            handle, GetLastError());
        PAS_ASSERT(false);
    }
    data->registers = NULL;
    data->did_suspend = false;
#else
    pas_thread_suspend_signal_handler_resume(data);
#endif
}

pas_machine_registers* pas_thread_suspend_get_registers(
    pas_thread_suspend_data* data, pas_machine_registers* scratch)
{
    PAS_ASSERT(data->did_suspend);
#if PAS_OS(WINDOWS)
    HANDLE handle = (HANDLE)data->native_thread;
    scratch->platformRegisters.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    if (!GetThreadContext(handle, &scratch->platformRegisters)) {
        pas_log("Failed to get Windows thread context for handle %p: %lu\n",
            handle, GetLastError());
        PAS_ASSERT(false);
    }
    return scratch;
#else
    PAS_ASSERT(data->registers);
    PAS_UNUSED_PARAM(scratch);
    return data->registers;
#endif
}
#endif // PAS_OS(DARWIN)

PAS_END_EXTERN_C;
