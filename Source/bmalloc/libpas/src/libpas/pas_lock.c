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

#include "pas_lock.h"

#if !PAS_OS(DARWIN)

#include "pas_futex.h"

/* lock->futex is 0 when unlocked, 1 when locked, and 2 when locked with possible sleepers. Only an
   unlock that sees 2 pays for a wake. */
void pas_lock_lock_slow(pas_lock* lock, unsigned expected)
{
    do {
        if (expected == 1) {
            expected = __atomic_exchange_n(&lock->futex, 2, __ATOMIC_ACQUIRE);
            if (expected == 0)
                return;
        }

        if (expected != 0)
            pas_futex_wait(&lock->futex, 2);

        /* Acquire as 2, not 1: other threads may still be asleep, and the next unlock must wake them. */
        expected = 0;
    } while (!__atomic_compare_exchange_n(&lock->futex, &expected, 2, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));
}

void pas_lock_unlock_slow(pas_lock* lock)
{
    __atomic_store_n(&lock->futex, 0, __ATOMIC_RELEASE);
    pas_futex_wake_one(&lock->futex);
}

#endif /* !PAS_OS(DARWIN) */
