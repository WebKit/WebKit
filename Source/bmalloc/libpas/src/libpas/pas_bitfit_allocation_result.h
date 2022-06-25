/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_BITFIT_ALLOCATION_RESULT_H
#define PAS_BITFIT_ALLOCATION_RESULT_H

#include "pas_range.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_allocation_result;
typedef struct pas_bitfit_allocation_result pas_bitfit_allocation_result;

struct pas_bitfit_allocation_result {
    bool did_succeed;
    unsigned pages_to_commit_on_reloop;
    union {
        uintptr_t result;
        uintptr_t largest_available;
    } u;
};

static PAS_ALWAYS_INLINE pas_bitfit_allocation_result
pas_bitfit_allocation_result_create_success(uintptr_t result_ptr)
{
    pas_bitfit_allocation_result result;
    result.did_succeed = true;
    result.pages_to_commit_on_reloop = 0;
    result.u.result = result_ptr;
    return result;
}

static PAS_ALWAYS_INLINE pas_bitfit_allocation_result
pas_bitfit_allocation_result_create_failure(uintptr_t largest_available)
{
    pas_bitfit_allocation_result result;
    result.did_succeed = false;
    result.pages_to_commit_on_reloop = 0;
    result.u.largest_available = largest_available;
    return result;
}

static PAS_ALWAYS_INLINE pas_bitfit_allocation_result
pas_bitfit_allocation_result_create_empty(void)
{
    return pas_bitfit_allocation_result_create_failure(0);
}

static PAS_ALWAYS_INLINE pas_bitfit_allocation_result
pas_bitfit_allocation_result_create_need_to_lock_commit_lock(unsigned pages_to_commit_on_reloop)
{
    pas_bitfit_allocation_result result;
    result.did_succeed = false;
    result.pages_to_commit_on_reloop = pages_to_commit_on_reloop;
    result.u.result = 0;
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_ALLOCATION_RESULT_H */

