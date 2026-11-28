/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#include "pas_condition.h"

/* A deadline of UINT64_MAX waits without a timeout. */
static bool pas_condition_wait_until_deadline(pas_condition* condition, pas_lock* lock, uint64_t absolute_deadline_in_nanoseconds)
{
    unsigned sequence;
    bool did_not_time_out;

    __atomic_fetch_add(&condition->num_waiters, 1, __ATOMIC_SEQ_CST);
    sequence = __atomic_load_n(&condition->futex, __ATOMIC_SEQ_CST);

    pas_lock_unlock(lock);
    if (absolute_deadline_in_nanoseconds == UINT64_MAX) {
        pas_futex_wait(&condition->futex, sequence);
        did_not_time_out = true;
    } else
        did_not_time_out = pas_futex_wait_until(&condition->futex, sequence, absolute_deadline_in_nanoseconds);
    __atomic_fetch_sub(&condition->num_waiters, 1, __ATOMIC_SEQ_CST);
    pas_lock_lock(lock);

    return did_not_time_out;
}

void pas_condition_wait(pas_condition* condition, pas_lock* lock)
{
    pas_lock_testing_assert_held(lock);
    pas_condition_wait_until_deadline(condition, lock, UINT64_MAX);
}

bool pas_condition_timed_wait_for(pas_condition* condition,
                                  pas_lock* lock,
                                  uint64_t relative_timeout_in_nanoseconds)
{
    uint64_t now;
    uint64_t absolute_deadline_in_nanoseconds;

    pas_lock_testing_assert_held(lock);

    if (!relative_timeout_in_nanoseconds)
        return false;

    now = pas_condition_get_time_in_nanoseconds();
    if (__builtin_add_overflow(now, relative_timeout_in_nanoseconds, &absolute_deadline_in_nanoseconds))
        absolute_deadline_in_nanoseconds = UINT64_MAX;
    return pas_condition_wait_until_deadline(condition, lock, absolute_deadline_in_nanoseconds);
}

bool pas_condition_timed_wait_until(pas_condition* condition,
                                    pas_lock* lock,
                                    uint64_t absolute_timeout_in_nanoseconds)
{
    pas_lock_testing_assert_held(lock);

    if (absolute_timeout_in_nanoseconds <= pas_condition_get_time_in_nanoseconds())
        return false;

    return pas_condition_wait_until_deadline(condition, lock, absolute_timeout_in_nanoseconds);
}

void pas_condition_signal_slow(pas_condition* condition)
{
    __atomic_fetch_add(&condition->futex, 1, __ATOMIC_SEQ_CST);
    pas_futex_wake_one(&condition->futex);
}

void pas_condition_broadcast_slow(pas_condition* condition)
{
    __atomic_fetch_add(&condition->futex, 1, __ATOMIC_SEQ_CST);
    pas_futex_wake_all(&condition->futex);
}
