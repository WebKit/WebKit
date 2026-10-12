/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_thread_suspend_lock.h"

#if PAS_OS(LINUX)
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#endif

PAS_DEFINE_LOCK(pas_thread_suspend);

pas_thread_suspend_reason pas_thread_suspend_current_reason = pas_thread_suspend_reason_libpas;

#if PAS_OS(LINUX)

/* This follows WTF::Thread::suspend() and resume(), except that we do not need registers. */

static int suspend_signal;
static sem_t suspend_semaphore;
static bool is_suspended;

void pas_thread_suspend_set_signal(int signal)
{
    int result = sem_init(&suspend_semaphore, 0, 0);
    PAS_ASSERT(!result, errno);
    suspend_signal = signal;
}

void pas_thread_suspend_handle_signal(void)
{
    sigset_t mask;
    int saved_errno;

    /* This invocation only wakes up the sigsuspend() below. */
    if (is_suspended)
        return;

    saved_errno = errno;
    sem_post(&suspend_semaphore);
    sigfillset(&mask);
    sigdelset(&mask, suspend_signal);
    sigsuspend(&mask);
    sem_post(&suspend_semaphore);
    errno = saved_errno;
}

static void signal_and_wait(pthread_t thread)
{
    int result = pthread_kill(thread, suspend_signal);
    PAS_ASSERT(!result, result);
    while (sem_wait(&suspend_semaphore))
        PAS_ASSERT(errno == EINTR, errno);
}

bool pas_thread_suspend_suspend(pthread_t thread)
{
    pas_thread_suspend_lock_assert_held();
    if (!suspend_signal)
        return false;
    signal_and_wait(thread);
    is_suspended = true;
    return true;
}

void pas_thread_suspend_resume(pthread_t thread)
{
    pas_thread_suspend_lock_assert_held();
    signal_and_wait(thread);
    is_suspended = false;
}

#endif /* PAS_OS(LINUX) */

#endif /* LIBPAS_ENABLED */
