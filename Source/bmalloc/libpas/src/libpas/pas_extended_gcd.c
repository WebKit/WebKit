/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

#include "pas_extended_gcd.h"

pas_extended_gcd_result pas_extended_gcd(int64_t left, int64_t right)
{
    /* Source: https://en.wikipedia.org/wiki/Extended_Euclidean_algorithm */
    
    int64_t s, t, r;
    int64_t old_s, old_t, old_r;
    pas_extended_gcd_result result;
    
    /* We will see these common cases. */
    if (left == 1) {
        result.left_bezout_coefficient = 1;
        result.right_bezout_coefficient = 0;
        result.result = 1;
        return result;
    }
    
    if (right == 1) {
        result.left_bezout_coefficient = 0;
        result.right_bezout_coefficient = 1;
        result.result = 1;
        return result;
    }
    
    s = 0;
    old_s = 1;
    t = 1;
    old_t = 0;
    r = right;
    old_r = left;
    
    while (r) {
        int64_t quotient;
        int64_t prev_s, prev_t, prev_r;

        quotient = old_r / r;
        
        prev_r = r;
        r = old_r - quotient * r;
        old_r = prev_r;
        
        prev_s = s;
        s = old_s - quotient * s;
        old_s = prev_s;
        
        prev_t = t;
        t = old_t - quotient * t;
        old_t = prev_t;
    }
    
    result.left_bezout_coefficient = old_s;
    result.right_bezout_coefficient = old_t;
    result.result = old_r;
    return result;
}

#endif /* LIBPAS_ENABLED */
