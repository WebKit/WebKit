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

#ifndef PAS_BASELINE_ALLOCATOR_RESULT_H
#define PAS_BASELINE_ALLOCATOR_RESULT_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_baseline_allocator_result;
struct pas_local_allocator;
struct pas_lock;
typedef struct pas_baseline_allocator_result pas_baseline_allocator_result;
typedef struct pas_local_allocator pas_local_allocator;
typedef struct pas_lock pas_lock;

/* This exists to allow jump-threading of success. */
struct pas_baseline_allocator_result {
    bool did_succeed;
    pas_local_allocator* allocator;
    pas_lock* lock;
};

static inline pas_baseline_allocator_result pas_baseline_allocator_result_create_failure(void)
{
    pas_baseline_allocator_result result;
    result.did_succeed = false;
    result.allocator = NULL;
    result.lock = NULL;
    return result;
}

static inline pas_baseline_allocator_result pas_baseline_allocator_result_create_success(
    pas_local_allocator* allocator, pas_lock* lock)
{
    pas_baseline_allocator_result result;
    result.did_succeed = true;
    result.allocator = allocator;
    result.lock = lock;
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_BASELINE_ALLOCATOR_RESULT_H */
