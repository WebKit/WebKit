/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_local_allocator_scavenger_data.h"

#include "pas_allocator_index.h"
#include "pas_baseline_allocator_table.h"
#include "pas_local_allocator.h"
#include "pas_local_view_cache.h"
#include "pas_heap_lock.h"
#include "pas_thread_local_cache.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_thread_local_cache_layout_node.h"
#include "pas_thread_local_cache_node.h"

uint8_t pas_local_allocator_should_stop_count_for_suspend = 5;

bool pas_local_allocator_scavenger_data_is_baseline_allocator(pas_local_allocator_scavenger_data* data)
{
    uintptr_t data_ptr;
    uintptr_t baseline_begin;
    uintptr_t baseline_end;

    data_ptr = (uintptr_t)data;
    baseline_begin = (uintptr_t)pas_baseline_allocator_table;
    baseline_end = (uintptr_t)(pas_baseline_allocator_table + pas_baseline_allocator_table_bound);

    return data_ptr >= baseline_begin && data_ptr < baseline_end;
}

bool pas_local_allocator_scavenger_data_is_stopped(pas_local_allocator_scavenger_data* data)
{
    return pas_local_allocator_kind_is_stopped(data->kind);
}

void pas_local_allocator_scavenger_data_commit_if_necessary_slow(
    pas_local_allocator_scavenger_data* data,
    pas_local_allocator_scavenger_data_commit_if_necessary_slow_mode mode,
    pas_local_allocator_kind expected_kind)
{
    pas_thread_local_cache* cache;
    pas_allocator_index index;
    pas_thread_local_cache_layout_node layout_node;
    pas_lock_hold_mode scavenger_lock_hold_mode;
    bool is_in_use;

    PAS_ASSERT(expected_kind == pas_local_allocator_allocator_kind
               || expected_kind == pas_local_allocator_view_cache_kind);

    switch (mode) {
    case pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_in_use_with_no_locks_held_mode:
        scavenger_lock_hold_mode = pas_lock_is_not_held;
        is_in_use = true;
        break;
    case pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_not_in_use_with_scavenger_lock_held_mode:
        scavenger_lock_hold_mode = pas_lock_is_held;
        is_in_use = false;
        break;
    }

    /* NOTE: this can only be called when is_in_use, but it's possible that this allocator will get
       decommitted at any time, which may result in is_in_use being cleared. */

    if (pas_local_allocator_scavenger_data_is_baseline_allocator(data)) {
        PAS_ASSERT(data->kind == pas_local_allocator_stopped_allocator_kind);
        PAS_ASSERT(expected_kind == pas_local_allocator_allocator_kind);
        PAS_ASSERT(data->is_in_use == is_in_use);
        data->kind = pas_local_allocator_allocator_kind;
        return;
    }

    cache = pas_thread_local_cache_try_get();
    PAS_ASSERT(cache);
    
    if (data->kind == pas_local_allocator_stopped_allocator_kind
        || data->kind == pas_local_allocator_stopped_view_cache_kind) {
        bool done;
        
        done = false;
        
        pas_lock_lock_conditionally(&cache->node->scavenger_lock, scavenger_lock_hold_mode);
        pas_lock_testing_assert_held(&cache->node->scavenger_lock);
        switch (data->kind) {
        case pas_local_allocator_decommitted_kind:
            break;
        case pas_local_allocator_stopped_allocator_kind:
            PAS_ASSERT(expected_kind == pas_local_allocator_allocator_kind);
            data->kind = pas_local_allocator_allocator_kind;
            done = true;
            break;
        case pas_local_allocator_stopped_view_cache_kind:
            PAS_ASSERT(expected_kind == pas_local_allocator_view_cache_kind);
            data->kind = pas_local_allocator_view_cache_kind;
            pas_local_view_cache_did_restart((pas_local_view_cache*)data);
            done = true;
            break;
        default:
            PAS_ASSERT(!"Should not be reached");
            break;
        }
        pas_lock_unlock_conditionally(&cache->node->scavenger_lock, scavenger_lock_hold_mode);
        if (done) {
            PAS_ASSERT(data->is_in_use == is_in_use);
            PAS_ASSERT(data->kind == expected_kind);
            return;
        }
    }

    PAS_ASSERT(data->kind == pas_local_allocator_decommitted_kind);

    pas_lock_lock_conditionally(&cache->node->scavenger_lock, scavenger_lock_hold_mode);
    pas_lock_testing_assert_held(&cache->node->scavenger_lock);

    PAS_ASSERT(data->kind == pas_local_allocator_decommitted_kind);

    index = pas_thread_local_cache_allocator_index_for_allocator(cache, data);
    layout_node = pas_thread_local_cache_layout_get_node_for_index(index);

    pas_thread_local_cache_layout_node_commit_and_construct(layout_node, cache);

    PAS_ASSERT(data->kind == expected_kind);
    PAS_ASSERT(data->kind == pas_local_allocator_allocator_kind
               || data->kind == pas_local_allocator_view_cache_kind);

    data->is_in_use = is_in_use;
    
    pas_lock_unlock_conditionally(&cache->node->scavenger_lock, scavenger_lock_hold_mode);
}

bool pas_local_allocator_scavenger_data_stop(
    pas_local_allocator_scavenger_data* data,
    pas_lock_lock_mode page_lock_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    switch (data->kind) {
    case pas_local_allocator_decommitted_kind:
    case pas_local_allocator_stopped_allocator_kind:
    case pas_local_allocator_stopped_view_cache_kind:
        return true;
    case pas_local_allocator_allocator_kind:
        return pas_local_allocator_stop((pas_local_allocator*)data, page_lock_mode, heap_lock_hold_mode);
    case pas_local_allocator_view_cache_kind:
        return pas_local_view_cache_stop((pas_local_view_cache*)data, page_lock_mode);
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

void pas_local_allocator_scavenger_data_prepare_to_decommit(pas_local_allocator_scavenger_data* data)
{
    /* Fun fact: the allocator might have is_in_use = true when we're in there, but in that case, that
       allocator can't do anything without grabbing the scavenger_lock (because the allocator says it's
       stopped). */
    
    PAS_ASSERT(pas_local_allocator_scavenger_data_is_stopped(data));

    pas_heap_lock_assert_held();

    data->kind = pas_local_allocator_decommitted_kind;
}

#endif /* LIBPAS_ENABLED */


