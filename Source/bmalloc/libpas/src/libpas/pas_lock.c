/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#include "pas_lock.h"
#if PAS_OS(DARWIN)
#include <mach/mach_traps.h>
#include <mach/thread_switch.h>
#endif

#if !PAS_PLATFORM(PLAYSTATION) && (PAS_OS(LINUX) || PAS_OS(WINDOWS) || PAS_OS(FREEBSD))

#if PAS_OS(LINUX)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif PAS_OS(WINDOWS)
#include <synchapi.h>
#elif PAS_OS(FREEBSD)
#include <sys/umtx.h>
#endif

#if PAS_OS(LINUX)
static inline long pas_lock_futex_wait(unsigned* addr, unsigned val)
{
    return syscall(SYS_futex, addr, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, val, NULL, NULL, 0);
}

static inline long pas_lock_futex_wake(unsigned* addr)
{
    return syscall(SYS_futex, addr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, NULL, NULL, 0);
}
#elif PAS_OS(WINDOWS)
static inline void pas_lock_futex_wait(unsigned* addr, unsigned val)
{
    WaitOnAddress(addr, &val, sizeof(unsigned), INFINITE);
}

static inline void pas_lock_futex_wake(unsigned* addr)
{
    WakeByAddressSingle(addr);
}
#elif PAS_OS(FREEBSD)
static inline long pas_lock_futex_wait(unsigned* addr, unsigned val)
{
    return _umtx_op(addr, UMTX_OP_WAIT_UINT_PRIVATE, val, NULL, NULL);
}

static inline long pas_lock_futex_wake(unsigned* addr)
{
    return _umtx_op(addr, UMTX_OP_WAKE_PRIVATE, 1, NULL, NULL);
}
#endif

void pas_lock_lock_slow(pas_lock* lock, unsigned expected)
{
    /* Slow path: lock is contended */
    do {
        /* If lock is currently 1 (locked, no waiters), mark it as 2 (locked with waiters) */
        if (expected == 1) {
            expected = __atomic_exchange_n(&lock->futex, 2, __ATOMIC_ACQUIRE);
            /* If we swapped 0 -> 2, we actually acquired the lock. */
            if (expected == 0)
                return;
        }

        /* If still not free, wait for wakeup */
        if (expected != 0)
            pas_lock_futex_wait(&lock->futex, 2);

        /* Try to acquire the lock: 0 -> 2 (since we know there's contention) */
        expected = 0;
    } while (!__atomic_compare_exchange_n(&lock->futex, &expected, 2, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));
}

void pas_lock_unlock_slow(pas_lock* lock)
{
    /* We had waiters (state was 2), so we need to wake them up */
    __atomic_store_n(&lock->futex, 0, __ATOMIC_RELEASE);
    pas_lock_futex_wake(&lock->futex);
}

#endif /* !PAS_PLATFORM(PLAYSTATION) && (PAS_OS(LINUX) || PAS_OS(WINDOWS) || PAS_OS(FREEBSD)) */

#endif /* LIBPAS_ENABLED */
