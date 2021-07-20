/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

PAS_BEGIN_EXTERN_C;

enum pas_random_kind {
    /* This is a PRNG optimized for speed and nothing else. It's used whenever we need a random
       number only as a performance optimization. */
    pas_cheesy_random,

    /* This is a PRNG optimized for security. It's used whenever we need unpredictable data.
       
       FIXME: Implement this. */
    pas_secure_random
};

typedef enum pas_random_kind pas_random_kind;

extern PAS_API unsigned pas_cheesy_random_state;

/* This is useful for testing. */
extern PAS_API unsigned (*pas_mock_cheesy_random)(void);

/* Returns a random number in [0, upper_bound). If upper_bound is zero, returns a number somewhere
   in the whole unsigned range. */
static inline unsigned pas_get_random(pas_random_kind kind, unsigned upper_bound)
{
    unsigned current_state;
    
    /* FIXME: Need to do something different if kind is pas_secure_random. */
    /* FIXME: Need to do something other than modulo for dealing with max_value. */

    if (kind == pas_cheesy_random && pas_mock_cheesy_random)
        current_state = pas_mock_cheesy_random();
    else {
        current_state = pas_xorshift32(pas_cheesy_random_state);
        pas_cheesy_random_state = current_state;
    }

    if (!upper_bound)
        return current_state;
    
    return current_state % upper_bound;
}

PAS_END_EXTERN_C;

#endif /* PAS_RANDOM_H */

