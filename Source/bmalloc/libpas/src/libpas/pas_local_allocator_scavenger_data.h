/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_H
#define PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_H

#include "pas_local_allocator_kind.h"
#include "pas_lock.h"

PAS_BEGIN_EXTERN_C;

struct pas_local_allocator_scavenger_data;
typedef struct pas_local_allocator_scavenger_data pas_local_allocator_scavenger_data;

/* This should just be 32-bit. */
struct pas_local_allocator_scavenger_data {
    bool is_in_use;
    uint8_t should_stop_count;
    bool dirty;
    pas_local_allocator_kind kind : 8;
};

#define PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_INITIALIZER(passed_kind) \
    ((pas_local_allocator_scavenger_data){ \
         .is_in_use = false, \
         .should_stop_count = 0, \
         .dirty = false, \
         .kind = (passed_kind) \
     })

PAS_API extern uint8_t pas_local_allocator_should_stop_count_for_suspend;

static inline void pas_local_allocator_scavenger_data_construct(pas_local_allocator_scavenger_data* data,
                                                                pas_local_allocator_kind kind)
{
    *data = PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_INITIALIZER(kind);
}

static inline void pas_local_allocator_scavenger_data_did_use_for_allocation(
    pas_local_allocator_scavenger_data* data)
{
    data->dirty = true;
    data->should_stop_count = 0;
}

PAS_API bool pas_local_allocator_scavenger_data_is_baseline_allocator(pas_local_allocator_scavenger_data* data);

PAS_API bool pas_local_allocator_scavenger_data_is_stopped(pas_local_allocator_scavenger_data* data);

enum pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode {
    /* This causes the commit_if_necessary code to always succeed and always return true. */
    pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_in_use_with_no_locks_held_mode,

    /* This causes the commit_if_necessary code to sometimes fail to acquire the heap lock, and then it will
       return false. */
    pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_not_in_use_with_scavenger_lock_held_mode
};

typedef enum pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode;

PAS_API void pas_local_allocator_scavenger_data_commit_if_necessary_slow(
    pas_local_allocator_scavenger_data* data,
    pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode mode,
    pas_local_allocator_kind expected_kind);

PAS_API bool pas_local_allocator_scavenger_data_stop(
    pas_local_allocator_scavenger_data* data,
    pas_lock_lock_mode page_lock_mode,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_local_allocator_scavenger_data_prepare_to_decommit(pas_local_allocator_scavenger_data* data);

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_H */

