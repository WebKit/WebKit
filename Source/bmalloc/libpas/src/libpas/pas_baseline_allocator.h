/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_BASELINE_ALLOCATOR_H
#define PAS_BASELINE_ALLOCATOR_H

#include "pas_internal_config.h"
#include "pas_local_allocator.h"
#include "pas_lock.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_baseline_allocator;
typedef struct pas_baseline_allocator pas_baseline_allocator;

#define PAS_BASELINE_LOCAL_ALLOCATOR_SIZE \
    PAS_LOCAL_ALLOCATOR_SIZE(PAS_MAX_OBJECTS_PER_PAGE)

struct pas_baseline_allocator {
    pas_lock lock; /* Can hold this before getting the heap lock. */
    union {
        pas_local_allocator allocator;
        char fake_field_to_force_size[PAS_BASELINE_LOCAL_ALLOCATOR_SIZE];
    } u;
};

#define PAS_BASELINE_ALLOCATOR_INITIALIZER ((pas_baseline_allocator){ \
        .lock = PAS_LOCK_INITIALIZER, \
        .u = { \
            .allocator = PAS_LOCAL_ALLOCATOR_NULL_INITIALIZER \
        } \
    })

PAS_API void pas_baseline_allocator_attach_directory(pas_baseline_allocator* allocator,
                                                     pas_segregated_size_directory* directory);

PAS_API void pas_baseline_allocator_detach_directory(pas_baseline_allocator* allocator);

PAS_END_EXTERN_C;

#endif /* PAS_BASELINE_ALLOCATOR_H */

