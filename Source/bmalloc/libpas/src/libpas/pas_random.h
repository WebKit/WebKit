/*
 * Copyright (c) 2019-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_RANDOM_H
#define PAS_RANDOM_H

#include "pas_utils.h"
#include <stdlib.h>

PAS_BEGIN_EXTERN_C;

extern PAS_API unsigned pas_fast_random_state;

/* This is useful for testing. */
extern PAS_API unsigned (*pas_mock_fast_random)(void);

/* This is a PRNG optimized for speed and nothing else. It's used whenever we need a random number only as a
   performance optimization. Returns a random number in [0, upper_bound). If the upper_bound is set to zero, than
   the range shall be [0, UINT32_MAX). */
static inline unsigned pas_get_fast_random(unsigned upper_bound)
{
    unsigned rand_value;

    if (!upper_bound)
        upper_bound = UINT32_MAX;

    if (PAS_LIKELY(!pas_mock_fast_random)) {
        pas_fast_random_state = pas_xorshift32(pas_fast_random_state);
        rand_value = pas_fast_random_state % upper_bound;
    } else {
        /* This is testing code. It will not be called during regular code flow. */
        rand_value = pas_mock_fast_random() % upper_bound;
    }

    return rand_value;
}

/* This is a PRNG optimized for security. It's used whenever we need unpredictable data. This will incur significant
  performance penalties over pas_fast_random. Returns a random number in [0, upper_bound). If the upper_bound is set
  to zero, than the range shall be [0, UINT32_MAX). */
static inline unsigned pas_get_secure_random(unsigned upper_bound)
{
    unsigned rand_value;

    if (!upper_bound)
        upper_bound = UINT32_MAX;

    /* Secure random is only supported on Darwin and FreeBSD at the moment due to arc4random being built into the
      stdlib. Fall back to fast behavior on other operating systems. */
#if PAS_OS(DARWIN) || PAS_OS(FREEBSD)
    rand_value = arc4random_uniform(upper_bound);
#else
    pas_fast_random_state = pas_xorshift32(pas_fast_random_state);
    rand_value = pas_fast_random_state % upper_bound;
#endif

    return rand_value;
}

PAS_END_EXTERN_C;

#endif /* PAS_RANDOM_H */
