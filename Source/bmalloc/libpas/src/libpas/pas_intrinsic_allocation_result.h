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

#ifndef PAS_INTRINSIC_ALLOCATION_RESULT_H
#define PAS_INTRINSIC_ALLOCATION_RESULT_H

#include <errno.h>
#include "pas_allocation_result.h"
#include "pas_config.h"
#include "pas_utils.h"
#include "pas_zero_mode.h"

PAS_BEGIN_EXTERN_C;

/* We keep this separate from pas_allocation_result since this is supposed to be able to
   communicate any other meta-data that the intrinsic/primitive allocator has available. At
   one point that included the object_size_mode, but that's no longer the case.

   If we go a long time without adding additional fields to this to distinguish it from
   pas_allocation_result then we should probably get rid of this. */

struct pas_intrinsic_allocation_result;
typedef struct pas_intrinsic_allocation_result pas_intrinsic_allocation_result;

struct pas_intrinsic_allocation_result {
    void* ptr;
    bool did_succeed;
    pas_zero_mode zero_mode;
};

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_intrinsic_allocation_result_create_empty(void)
{
    pas_intrinsic_allocation_result result;
    result.did_succeed = false;
    result.zero_mode = pas_zero_mode_may_have_non_zero;
    result.ptr = NULL;
    return result;
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_intrinsic_allocation_result_create(pas_allocation_result allocation_result)
{
    pas_intrinsic_allocation_result result;
    result.ptr = (void*)allocation_result.begin;
    result.did_succeed = allocation_result.did_succeed;
    result.zero_mode = allocation_result.zero_mode;
    return result;
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_intrinsic_allocation_result_identity(pas_intrinsic_allocation_result result)
{
    return result;
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_intrinsic_allocation_result_zero(pas_intrinsic_allocation_result result,
                                     size_t size)
{
    if (result.did_succeed
        && result.zero_mode == pas_zero_mode_may_have_non_zero)
        pas_zero_memory(result.ptr, size);
    return result;
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_intrinsic_allocation_result_set_errno(
    pas_intrinsic_allocation_result result)
{
    if (!result.did_succeed)
        errno = ENOMEM;
    return result;
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_intrinsic_allocation_result_crash_on_error(
    pas_intrinsic_allocation_result result)
{
    PAS_ASSERT(result.did_succeed);
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_INTRINSIC_ALLOCATION_RESULT_H */

