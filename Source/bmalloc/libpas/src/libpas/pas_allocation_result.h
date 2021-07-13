/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_ALLOCATION_RESULT_H
#define PAS_ALLOCATION_RESULT_H

#include "pas_utils.h"
#include "pas_zero_mode.h"

PAS_BEGIN_EXTERN_C;

struct pas_allocation_result;
typedef struct pas_allocation_result pas_allocation_result;

/* Normally, we just return NULL to mean that allocation failed. But in some cases it's better
   to separate out the allocation result status from the pointer to make compiler transformations
   on the allocation fast path work. */
struct pas_allocation_result {
    bool did_succeed;
    pas_zero_mode zero_mode;
    uintptr_t begin;
};

static inline pas_allocation_result pas_allocation_result_create_failure(void)
{
    pas_allocation_result result;
    result.did_succeed = false;
    result.zero_mode = pas_zero_mode_may_have_non_zero;
    result.begin = 0;
    return result;
}

static inline pas_allocation_result
pas_allocation_result_create_success_with_zero_mode(uintptr_t begin,
                                                    pas_zero_mode zero_mode)
{
    pas_allocation_result result;
    result.did_succeed = true;
    result.zero_mode = zero_mode;
    result.begin = begin;
    return result;
}

static inline pas_allocation_result pas_allocation_result_create_success(uintptr_t begin)
{
    return pas_allocation_result_create_success_with_zero_mode(
        begin, pas_zero_mode_may_have_non_zero);
}

PAS_API void pas_allocation_result_zero(pas_allocation_result* result,
                                        size_t size);

PAS_END_EXTERN_C;

#endif /* PAS_ALLOCATION_RESULT_H */
