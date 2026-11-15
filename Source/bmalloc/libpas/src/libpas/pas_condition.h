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

#ifndef PAS_CONDITION_H
#define PAS_CONDITION_H

#include "pas_futex.h"
#include "pas_lock.h"

PAS_BEGIN_EXTERN_C;

struct pas_condition;
typedef struct pas_condition pas_condition;

struct pas_condition {
    unsigned futex;
    unsigned num_waiters;
};

#define PAS_CONDITION_INITIALIZER ((pas_condition) { .futex = 0, .num_waiters = 0 })

static inline void pas_condition_construct(pas_condition* condition)
{
    *condition = PAS_CONDITION_INITIALIZER;
}

PAS_BEXPORT PAS_NEVER_INLINE void pas_condition_signal_slow(pas_condition* condition);
PAS_BEXPORT PAS_NEVER_INLINE void pas_condition_broadcast_slow(pas_condition* condition);

static inline void pas_condition_signal(pas_condition* condition)
{
    if (PAS_UNLIKELY(__atomic_load_n(&condition->num_waiters, __ATOMIC_SEQ_CST)))
        pas_condition_signal_slow(condition);
}

static inline void pas_condition_broadcast(pas_condition* condition)
{
    if (PAS_UNLIKELY(__atomic_load_n(&condition->num_waiters, __ATOMIC_SEQ_CST)))
        pas_condition_broadcast_slow(condition);
}

/* The clock pas_condition_timed_wait_until deadlines are measured against. A timed wait reports a
   timeout only once this clock has reached the deadline. */
static inline uint64_t pas_condition_get_time_in_nanoseconds(void)
{
    return pas_futex_get_time_in_nanoseconds();
}

PAS_BEXPORT void pas_condition_wait(pas_condition* condition, pas_lock* lock);
PAS_BEXPORT bool pas_condition_timed_wait_for(pas_condition* condition,
                                              pas_lock* lock,
                                              uint64_t relative_timeout_in_nanoseconds);
PAS_BEXPORT bool pas_condition_timed_wait_until(pas_condition* condition,
                                                pas_lock* lock,
                                                uint64_t absolute_timeout_in_nanoseconds);

PAS_END_EXTERN_C;

#endif /* PAS_CONDITION_H */
