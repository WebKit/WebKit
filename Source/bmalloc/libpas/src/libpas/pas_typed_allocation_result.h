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

#ifndef PAS_TYPED_ALLOCATION_RESULT_H
#define PAS_TYPED_ALLOCATION_RESULT_H

#include "pas_intrinsic_allocation_result.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_typed_allocation_result;
typedef struct pas_typed_allocation_result pas_typed_allocation_result;

struct pas_typed_allocation_result {
    pas_heap_type* type;
    void* ptr;
    size_t size;
    bool did_succeed;
    pas_zero_mode zero_mode;
};

static PAS_ALWAYS_INLINE pas_typed_allocation_result pas_typed_allocation_result_create_empty(void)
{
    pas_typed_allocation_result result;
    result.type = NULL;
    result.ptr = NULL;
    result.size = 0;
    result.did_succeed = false;
    result.zero_mode = pas_zero_mode_may_have_non_zero;
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_typed_allocation_result_create(pas_heap_type* type,
                                   pas_allocation_result allocation_result,
                                   size_t size)
{
    pas_typed_allocation_result result;
    
    if (!allocation_result.did_succeed)
        return pas_typed_allocation_result_create_empty();
    
    result.type = type;
    result.ptr = (void*)allocation_result.begin;
    result.size = size;
    result.did_succeed = allocation_result.did_succeed;
    result.zero_mode = allocation_result.zero_mode;
    return result;
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_typed_allocation_result_as_intrinsic_allocation_result(
    pas_typed_allocation_result internal_result)
{
    pas_intrinsic_allocation_result result;
    result.ptr = internal_result.ptr;
    result.did_succeed = internal_result.did_succeed;
    result.zero_mode = internal_result.zero_mode;
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_typed_allocation_result_create_with_intrinsic_allocation_result(
    pas_intrinsic_allocation_result intrinsic_result,
    pas_heap_type* type,
    size_t size)
{
    pas_typed_allocation_result result;
    result.type = type;
    result.ptr = intrinsic_result.ptr;
    result.size = size;
    result.did_succeed = intrinsic_result.did_succeed;
    result.zero_mode = intrinsic_result.zero_mode;
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_typed_allocation_result_zero(pas_typed_allocation_result result)
{
    pas_intrinsic_allocation_result_zero(
        pas_typed_allocation_result_as_intrinsic_allocation_result(result),
        result.size);
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_typed_allocation_result_set_errno(pas_typed_allocation_result result)
{
    pas_intrinsic_allocation_result_set_errno(
        pas_typed_allocation_result_as_intrinsic_allocation_result(result));
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_typed_allocation_result_crash_on_error(pas_typed_allocation_result result)
{
    pas_intrinsic_allocation_result_crash_on_error(
        pas_typed_allocation_result_as_intrinsic_allocation_result(result));
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_TYPED_ALLOCATION_RESULT_H */

