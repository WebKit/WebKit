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

#if PAS_OS(WINDOWS)
#include "pas_thread.h"
#include "pas_log.h"
#include "pas_utils.h"

#include <windows.h>

static const bool verbose = false;

int pthread_create(pthread_t* tid, const pthread_attr_t* attr, unsigned (*start)(void *), void* arg)
{
    PAS_UNUSED_PARAM(attr);

    /* Create thread handle */
    HANDLE hThread;

    /* Thread ID (optional) */
    unsigned threadIdentifier = 0;

    /* Create the thread */
    hThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        start,
        arg,
        0,
        &threadIdentifier
    );

    if (!hThread) {
        if (verbose)
            pas_log("Failed to create thread.\n");
        return 1;
    }

    *tid = (pthread_t)hThread;
    return 0;
}

int pthread_detach(pthread_t thread)
{
    /* _beginthreadex returns a HANDLE that must be closed; closing it while the
       thread is still running tells the kernel to release thread resources when
       the thread exits, which matches POSIX pthread_detach semantics. */
    if (!CloseHandle((HANDLE)thread))
        return 1;
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
    BOOL result;
    result = InitOnceExecuteOnce(once_control, once_init_runner, init_routine, NULL);
    if (!result) {
        if (verbose)
            pas_log("Failed to run pthread_once.\n");
        return 1;
    }
    return 0;
}

#endif
