/*
 * Copyright (c) 2025 Ian Grunert <ian.grunert@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Implement the subset of pthreads that libpas requires to run on Windows
   It's not a complete implementation - we only care about implementing the
   subset required to get libpas working. */

#include "pas_config.h"

#if LIBPAS_ENABLED && PAS_OS(WINDOWS)
#include "pas_thread.h"
#include "pas_log.h"
#include "pas_utils.h"

#include <windows.h>

static const bool verbose = false;

int pthread_create(pthread_t* tid, const pthread_attr_t* attr, unsigned (*start)(void *), void* arg)
{
    PAS_UNUSED_PARAM(tid);
    PAS_UNUSED_PARAM(attr);
    PAS_UNUSED_PARAM(arg);

    /* Create thread handle */
    HANDLE hThread;

    /* Thread ID (optional) */
    unsigned threadIdentifier = 0;

    /* Create the thread */
    hThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        start,
        NULL,
        0,
        &threadIdentifier
    );

    if (!hThread) {
        if (verbose)
            pas_log("Failed to create thread.\n");
        return 1;
    }

    return 0;
}

int pthread_detach(pthread_t thread)
{
    PAS_UNUSED_PARAM(thread);
    /* Detach is a no-op on Windows */
    return 0;
}

int pthread_getname_np(pthread_t thread, const char* name, size_t len)
{
    /* This is only used for dump_thread_diagnostics */
    PAS_UNUSED_PARAM(thread);
    PAS_UNUSED_PARAM(name);
    PAS_UNUSED_PARAM(len);

    PAS_ASSERT(false);

    return 0;
}

pthread_t pthread_self(void)
{
    return GetCurrentThreadId();
}

int sched_yield()
{
    /* SwitchToThread only yields to threads on the same processor
       If that fails we'll Sleep, which will also yield for threads ready to run on other processors */
    if (!SwitchToThread())
        Sleep(0);
    return 0;
}

/* This is used to wrap the passed void(*)(void) function so it can be passed to InitOnceExecuteOnce */
BOOL once_init_runner(PINIT_ONCE once_control, PVOID init_routine, PVOID* context)
{
    PAS_UNUSED_PARAM(once_control);
    PAS_UNUSED_PARAM(context);
    ((void (*)(void))init_routine)();

    return TRUE;
}

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
    int result;
    result = InitOnceExecuteOnce(*once_control, once_init_runner, init_routine, NULL);
    if (verbose && !result)
        pas_log("Failed to run pthread_once.\n");

    return result;
}

/* Thread Local Storage
   The regular Windows Thread Local Storage APIs like TlsAlloc don't have a way an easy way
   to register a per-thread destructor to be able to clean up the memory.
   Blink gets around this with some crazy pragmas to manually insert a function to be called
   on each thread's exit:
   https://web.archive.org/web/20070921204540/https://www.codeproject.com/threads/tls.asp
   https://source.chromium.org/chromium/chromium/src/+/main:base/threading/thread_local_storage_win.cc;l=38-48
  
   However the fiber-local storage APIs do let you register a per-thread destructor. Even
   though we don't care about fibers, the API is strictly better and avoids the magic:
   https://devblogs.microsoft.com/oldnewthing/20191011-00/?p=102989
  
   TODO: FlsGetValue is around 2x slower than TlsGetValue on single threaded access, so we should
   switch to Thread Local Storage and deal with the extra complexity. */
int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    PAS_UNUSED_PARAM(key);
    DWORD result = FlsAlloc(destructor);
    PAS_ASSERT(result != FLS_OUT_OF_INDEXES);

    return 0;
}

void *pthread_getspecific(pthread_key_t key)
{
    return FlsGetValue(key);
}

int pthread_setspecific(pthread_key_t key, void* value)
{
    return FlsSetValue(key, value);
}

/* Mutexes, conditions */

int pthread_mutex_init(pthread_mutex_t* mutex, const void* unused_attr)
{
    PAS_UNUSED_PARAM(unused_attr);

    InitializeSRWLock(mutex);
    return 0;
}

int pthread_cond_init(pthread_cond_t* cond, const void* unused_attr)
{
    PAS_UNUSED_PARAM(unused_attr);

    InitializeConditionVariable(cond);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond)
{
    WakeAllConditionVariable(cond);
    return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    SleepConditionVariableSRW(cond, mutex, INFINITE, 0);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime)
{
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    uint64_t current_ms = (counter.QuadPart * 1000) / frequency.QuadPart;
    uint64_t wait_until_ms = abstime->tv_sec * 1000 + abstime->tv_nsec / 1000000;

    return SleepConditionVariableSRW(cond, mutex, wait_until_ms - current_ms, 0);
}

int pthread_mutex_lock(pthread_mutex_t* mutex)
{
    AcquireSRWLockExclusive(mutex);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    ReleaseSRWLockExclusive(mutex);
    return 0;
}

#endif
