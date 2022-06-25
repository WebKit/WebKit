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

#ifndef PAS_FAST_PATH_ALLOCATION_RESULT_H
#define PAS_FAST_PATH_ALLOCATION_RESULT_H

#include "pas_allocation_result.h"
#include "pas_fast_path_allocation_result_kind.h"

PAS_BEGIN_EXTERN_C;

struct pas_fast_path_allocation_result;
typedef struct pas_fast_path_allocation_result pas_fast_path_allocation_result;

struct pas_fast_path_allocation_result {
    pas_fast_path_allocation_result_kind kind;
    uintptr_t begin;
};

static inline pas_fast_path_allocation_result
pas_fast_path_allocation_result_create(pas_fast_path_allocation_result_kind kind)
{
    pas_fast_path_allocation_result result;
    PAS_ASSERT(kind == pas_fast_path_allocation_result_need_slow
               || kind == pas_fast_path_allocation_result_out_of_memory);
    result.kind = kind;
    result.begin = 0;
    return result;
}

static inline pas_fast_path_allocation_result
pas_fast_path_allocation_result_create_need_slow(void)
{
    return pas_fast_path_allocation_result_create(pas_fast_path_allocation_result_need_slow);
}

static inline pas_fast_path_allocation_result
pas_fast_path_allocation_result_create_out_of_memory(void)
{
    return pas_fast_path_allocation_result_create(pas_fast_path_allocation_result_out_of_memory);
}

static inline pas_fast_path_allocation_result
pas_fast_path_allocation_result_create_success(uintptr_t begin)
{
    pas_fast_path_allocation_result result;
    result.kind = pas_fast_path_allocation_result_success;
    result.begin = begin;
    return result;
}

static inline pas_fast_path_allocation_result
pas_fast_path_allocation_result_from_allocation_result(
    pas_allocation_result allocation_result,
    pas_fast_path_allocation_result_kind failure_kind)
{
    pas_fast_path_allocation_result result;
    PAS_ASSERT(failure_kind != pas_fast_path_allocation_result_success);
    result.kind = allocation_result.did_succeed
        ? pas_fast_path_allocation_result_success
        : failure_kind;
    result.begin = allocation_result.begin;
    return result;
}

static inline pas_allocation_result
pas_fast_path_allocation_result_to_allocation_result(pas_fast_path_allocation_result fast_result)
{
    if (fast_result.kind == pas_fast_path_allocation_result_success)
        return pas_allocation_result_create_success(fast_result.begin);
    return pas_allocation_result_create_failure();
}

PAS_END_EXTERN_C;

#endif /* PAS_FAST_PATH_ALLOCATION_RESULT_H */

