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

#include "pas_thread_suspend_signal_handler.h"

#if !PAS_OS(DARWIN) && !PAS_OS(WINDOWS)

#include "pas_log.h"
#include "pas_machine_current_stack_pointer.h"
#include "pas_machine_registers.h"
#include "pas_runtime_config.h"
#include "pas_thread_suspend_lock.h"
#include "pas_thread_yield.h"
#include "pas_utils.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>

PAS_BEGIN_EXTERN_C;

static bool pas_thread_suspend_signal_handler_installed = false;

static sem_t global_suspend_semaphore;
static pas_thread_suspend_data* _Atomic target_thread = NULL;

#if PAS_ENABLE_TESTING
/* Number of times a suspension had to be retried because the target was running on an
   alternative signal stack. */
static unsigned retry_count = 0;

unsigned pas_thread_suspend_signal_handler_retry_count(void)
{
    return retry_count;
}
#endif /* PAS_ENABLE_TESTING */

int pas_thread_suspend_signal_number(void)
{
    int sig = PAS_RUNTIME_CONFIG_PTR->thread_suspend_signal;
    return sig ? sig : SIGUSR1;
}

void pas_thread_suspend_set_signal(int signal)
{
    PAS_ASSERT(signal);
    if (pas_thread_suspend_signal_handler_installed && pas_thread_suspend_signal_number() != signal) {
        pas_panic("pas_thread_suspend_set_signal(%d) called after the suspend handler was installed for signal %d; "
            "configure the signal before the first allocation or thread suspension\n",
            signal, pas_thread_suspend_signal_number());
    }
    PAS_RUNTIME_CONFIG_PTR->thread_suspend_signal = signal;
}

static void pas_thread_suspend_signal_handler_impl(int signal, siginfo_t* siginfo, void* ucontext)
{
    PAS_UNUSED_PARAM(signal);
    PAS_UNUSED_PARAM(siginfo);
    PAS_ASSERT(pas_thread_suspend_signal_handler_installed);
    PAS_ASSERT(signal == pas_thread_suspend_signal_number());

    pas_thread_suspend_data* data = atomic_load(&target_thread);
    PAS_ASSERT(data);
    if (!data)
        return;

    if (data->did_suspend) {
        // This is signal handler invocation that is intended to be used to resume sigsuspend.
        // So this handler invocation itself should not process.
        //
        // When a signal comes, first, the system calls signal handler. And later, sigsuspend will be resumed. Signal handler invocation always precedes.
        // So, the problem never happens that store(target_thread_suspended, true) will be executed before the handler is called.
        // http://pubs.opengroup.org/onlinepubs/009695399/functions/sigsuspend.html
        return;
    }

#if !PAS_HAVE(MACHINE_CONTEXT)
    pas_machine_registers fallback_registers;
#endif

    if (data->stack_bounds.origin) {
        void* approximate_stack_pointer = pas_machine_current_stack_pointer();
        if (!pas_machine_stack_bounds_contains(data->stack_bounds, approximate_stack_pointer)) {
            // This happens if we use an alternative signal stack.
            // 1. A user-defined signal handler is invoked with an alternative signal stack.
            // 2. In the middle of the execution of the handler, we attempt to suspend the target thread.
            // 3. A nested signal handler is executed.
            // 4. The stack pointer saved in the machine context will be pointing to the alternative signal stack.
            // In this case, we back off the suspension and retry a bit later.
            sem_post(&global_suspend_semaphore);
            return;
        }

    // This thread will be suspended, so this pointer onto the stack will be valid.
#if PAS_HAVE(MACHINE_CONTEXT)
        ucontext_t* userContext = (ucontext_t*)ucontext;
        data->registers = pas_registers_from_ucontext(userContext);
#else
        PAS_UNUSED_PARAM(ucontext);
        fallback_registers.stackPointer = approximate_stack_pointer;
        data->registers = &fallback_registers;
#endif
    } else {
        // Caller did not provide stack bounds: it does not need register state.
        PAS_UNUSED_PARAM(ucontext);
        data->registers = NULL;
    }

    data->did_suspend = true;

    // Allow suspend caller to see that this thread is suspended.
    // sem_post is async-signal-safe function. It means that we can call this from a signal handler.
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03
    sem_post(&global_suspend_semaphore);

    // Reaching here, the suspend/resume signal is blocked in this handler (this is configured by sigaction's sa_mask).
    // So before calling sigsuspend, the suspend/resume signal to this thread is deferred. This ensures that the handler is not executed recursively.
    sigset_t blockedSignalSet;
    sigfillset(&blockedSignalSet);
    sigdelset(&blockedSignalSet, pas_thread_suspend_signal_number());
    sigsuspend(&blockedSignalSet);

    data->did_suspend = false;
    data->registers = NULL;

    // Allow resume caller to see that this thread is resumed.
    sem_post(&global_suspend_semaphore);
}

static void pas_thread_suspend_signal_handler_install_impl(void)
{
    pas_thread_suspend_lock_lock();
    PAS_ASSERT(!pas_thread_suspend_signal_handler_installed);

    int sharedBetweenProcesses = 0;
    int initialValue = 0;
    sem_init(&global_suspend_semaphore, sharedBetweenProcesses, initialValue);

    int signal_number = pas_thread_suspend_signal_number();

    // Signal handlers are process global configuration.
    // Intentionally block signal_number in the handler.
    // signal_number will be allowed in the handler by sigsuspend.
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, signal_number);

    action.sa_sigaction = &pas_thread_suspend_signal_handler_impl;
    action.sa_flags = SA_RESTART | SA_SIGINFO;

    // Theoretically, this can have race conditions but currently, there is no way to deal with it,
    // plus, we do not expect that this initialization is executed concurrently with the other
    // initialization which also installs specific signals. If this is the problem, applications should
    // change how to initialize things.
    struct sigaction oldAction;
    bool gotOldAction = !sigaction(signal_number, NULL, &oldAction);
    PAS_ASSERT(gotOldAction);
    if (oldAction.sa_handler != SIG_DFL || (void*)oldAction.sa_sigaction != (void*)SIG_DFL)
        pas_log("Overriding existing handler for signal %d. Publish a different signal via pas_thread_suspend_set_signal() before init to avoid this.\n", signal_number);

    bool installed = !sigaction(signal_number, &action, NULL);
    PAS_ASSERT(installed);

    pas_thread_suspend_signal_handler_installed = true;
    pas_thread_suspend_lock_unlock();
}

static pthread_once_t pas_thread_suspend_signal_handler_install_once = PTHREAD_ONCE_INIT;

PAS_API void pas_thread_suspend_signal_handler_install(void)
{
    pthread_once(&pas_thread_suspend_signal_handler_install_once, pas_thread_suspend_signal_handler_install_impl);
}

PAS_API bool pas_thread_suspend_signal_handler_suspend(pas_thread_suspend_data* data)
{
    const bool verbose = false;
    PAS_ASSERT(pas_thread_suspend_signal_handler_installed);
    pas_thread_suspend_lock_assert_held();
    if (!pas_thread_suspend_signal_handler_installed)
        return false;

    atomic_store(&target_thread, data);
    while (true) {
        // We must use pthread_kill to avoid queue-overflow problem with real-time signals.
        if (verbose)
            fprintf(stderr, "Sending signal to thread for suspension\n");
        int result = pthread_kill(data->native_thread, pas_thread_suspend_signal_number());
        if (result) {
            pas_log("[%d] Failed to send suspend signal %d to thread: %d\n",
                getpid(), pas_thread_suspend_signal_number(), result);
            atomic_store(&target_thread, NULL);
            return false;
        }
        sem_wait(&global_suspend_semaphore);
        if (data->did_suspend)
            break;
        // We aborted because the target was on an alternative signal stack. Try again after yielding.
#if PAS_ENABLE_TESTING
        ++retry_count;
#endif
        pas_thread_yield();
    }

    atomic_store(&target_thread, NULL);
    return true;
}

PAS_API void pas_thread_suspend_signal_handler_resume(pas_thread_suspend_data* data)
{
    const bool verbose = false;
    pas_thread_suspend_lock_assert_held();
    PAS_ASSERT(data->did_suspend);
    atomic_store(&target_thread, data);

    if (verbose)
        fprintf(stderr, "Resuming thread using signal handler\n");
    if (pthread_kill(data->native_thread, pas_thread_suspend_signal_number()) == ESRCH)
        return;
    sem_wait(&global_suspend_semaphore);
    PAS_ASSERT(!data->did_suspend);
    atomic_store(&target_thread, NULL);
}

PAS_END_EXTERN_C;

#endif /* !PAS_OS(DARWIN) && !PAS_OS(WINDOWS) */
