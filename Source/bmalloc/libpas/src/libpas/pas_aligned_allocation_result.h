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

#ifndef PAS_ALIGNED_ALLOCATION_RESULT_H
#define PAS_ALIGNED_ALLOCATION_RESULT_H

#include "pas_allocation_result.h"
#include "pas_utils.h"
#include "pas_zero_mode.h"

PAS_BEGIN_EXTERN_C;

struct pas_aligned_allocation_result;
typedef struct pas_aligned_allocation_result pas_aligned_allocation_result;

struct pas_aligned_allocation_result {
    void* result;
    size_t result_size;
    void* left_padding;
    size_t left_padding_size;
    void* right_padding;
    size_t right_padding_size;
    pas_zero_mode zero_mode;
};

static inline pas_aligned_allocation_result pas_aligned_allocation_result_create_empty(void)
{
    pas_aligned_allocation_result result;
    result.result = NULL;
    result.result_size = 0;
    result.left_padding = NULL;
    result.left_padding_size = 0;
    result.right_padding = NULL;
    result.right_padding_size = 0;
    result.zero_mode = pas_zero_mode_may_have_non_zero;
    return result;
}

static inline pas_allocation_result
pas_aligned_allocation_result_as_allocation_result(pas_aligned_allocation_result result)
{
    if (!result.result)
        return pas_allocation_result_create_failure();
    
    return pas_allocation_result_create_success_with_zero_mode((uintptr_t)result.result,
                                                               result.zero_mode);
}


PAS_END_EXTERN_C;

#endif /* PAS_ALIGNED_ALLOCATION_RESULT_H */

