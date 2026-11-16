/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#include "pas_coalign.h"

#include "pas_extended_gcd.h"

static intptr_t formal_mod(intptr_t a, intptr_t b)
{
    intptr_t result;
    
    PAS_ASSERT(b > 0);
    
    result = a % b;
    if (result < 0)
        result += b;
    
    PAS_ASSERT(result >= 0);
    PAS_ASSERT(result < b);
    
    return result;
}

static uintptr_t ceiling_div(uintptr_t a, uintptr_t b)
{
    return (a + b - 1) / b;
}

pas_coalign_result pas_coalign_one_sided(uintptr_t begin_left, uintptr_t left_size,
                                         uintptr_t right_size)
{
    uintptr_t offset;
    uintptr_t multiple;
    pas_extended_gcd_result gcd_result;
    pas_coalign_result result;
    uintptr_t step;
    uintptr_t boundary;
    
    PAS_ASSERT((intptr_t)begin_left >= 0);
    PAS_ASSERT((intptr_t)left_size >= 0);
    PAS_ASSERT((intptr_t)right_size >= 0);
    
    offset = begin_left % left_size;
    gcd_result = pas_extended_gcd((int64_t)left_size, (int64_t)right_size);
    
    PAS_ASSERT(gcd_result.result >= 0);
    
    multiple = left_size * right_size / (uintptr_t)gcd_result.result;
    
    if (offset % (uintptr_t)gcd_result.result)
        return pas_coalign_empty_result();
    
    step = offset / (uintptr_t)gcd_result.result;
    boundary = right_size * (uintptr_t)formal_mod(
        gcd_result.right_bezout_coefficient * (intptr_t)step,
        (intptr_t)(multiple / right_size));
    PAS_ASSERT((intptr_t)boundary >= 0);
    
    result.has_result = true;
    result.result = ceiling_div(begin_left - boundary, multiple) * multiple + boundary;
    
    /* It's important that we check our results. If we're wrong, it could mean a security bug. */
    PAS_ASSERT(!(result.result % right_size));
    PAS_ASSERT(!((result.result - begin_left) % left_size));
    
    return result;
}

pas_coalign_result pas_coalign(uintptr_t begin_left, uintptr_t left_size,
                               uintptr_t begin_right, uintptr_t right_size)
{
    pas_coalign_result result;

    if (begin_right > begin_left) {
        PAS_SWAP(begin_left, begin_right);
        PAS_SWAP(left_size, right_size);
    }

    result = pas_coalign_one_sided(begin_left - begin_right, left_size,
                                   right_size);
    if (!result.has_result)
        return pas_coalign_empty_result();
    
    if (__builtin_add_overflow(result.result, begin_right, &result.result))
        return pas_coalign_empty_result();
    
    PAS_ASSERT(!((result.result - begin_left) % left_size));
    PAS_ASSERT(!((result.result - begin_right) % right_size));
    
    return result;
}

#endif /* LIBPAS_ENABLED */
