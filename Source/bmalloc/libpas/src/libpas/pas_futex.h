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

#ifndef PAS_FUTEX_H
#define PAS_FUTEX_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* The clock the operating system measures wait timeouts against, so deadlines computed from it
   neither expire early nor drift from what the kernel waits for. */
PAS_BEXPORT uint64_t pas_futex_get_time_in_nanoseconds(void);

/* Each wait may also return spuriously, so callers must recheck the value they wait on. */
PAS_API void pas_futex_wait(unsigned* address, unsigned expected_value);

/* Returns false only when pas_futex_get_time_in_nanoseconds() has reached the deadline. */
PAS_API bool pas_futex_wait_until(unsigned* address, unsigned expected_value,
                                  uint64_t absolute_deadline_in_nanoseconds);

PAS_API void pas_futex_wake_one(unsigned* address);
PAS_API void pas_futex_wake_all(unsigned* address);

PAS_END_EXTERN_C;

#endif /* PAS_FUTEX_H */
