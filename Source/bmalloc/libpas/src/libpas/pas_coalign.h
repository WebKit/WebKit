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

#ifndef PAS_COALIGN_H
#define PAS_COALIGN_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_coalign_result;
typedef struct pas_coalign_result pas_coalign_result;

struct pas_coalign_result {
    bool has_result;
    uintptr_t result;
};

static inline pas_coalign_result pas_coalign_empty_result(void)
{
    pas_coalign_result result;
    result.has_result = false;
    result.result = 0;
    return result;
}

static inline pas_coalign_result pas_coalign_result_create(uintptr_t value)
{
    pas_coalign_result result;
    result.has_result = true;
    result.result = value;
    return result;
}

PAS_API pas_coalign_result pas_coalign_one_sided(uintptr_t begin_left,
                                                 uintptr_t left_size,
                                                 uintptr_t right_size);

PAS_API pas_coalign_result pas_coalign(uintptr_t begin_left, uintptr_t left_size,
                                       uintptr_t begin_right, uintptr_t right_size);

PAS_END_EXTERN_C;

#endif /* PAS_COALIGN_H */

