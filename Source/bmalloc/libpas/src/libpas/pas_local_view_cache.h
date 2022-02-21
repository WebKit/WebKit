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

#ifndef PAS_LOCAL_VIEW_CACHE_H
#define PAS_LOCAL_VIEW_CACHE_H

#include "pas_compact_segregated_exclusive_view_ptr.h"
#include "pas_local_allocator_scavenger_data.h"
#include "pas_lock.h"
#include "pas_log.h"

PAS_BEGIN_EXTERN_C;

struct pas_local_view_cache;
typedef struct pas_local_view_cache pas_local_view_cache;

enum pas_local_view_cache_state {
    pas_local_view_cache_stopped_state,
    pas_local_view_cache_not_full_state,
    pas_local_view_cache_full_state
};

typedef enum pas_local_view_cache_state pas_local_view_cache_state;

struct pas_local_view_cache {
    pas_local_allocator_scavenger_data scavenger_data;
    uint8_t capacity;
    /* bottom_index and top_index must be < capacity. */
    uint8_t bottom_index;
    uint8_t top_index;
    uint8_t state;
    pas_compact_segregated_exclusive_view_ptr views[1];
};

#define PAS_LOCAL_VIEW_CACHE_SIZE(capacity) \
    PAS_ROUND_UP_TO_POWER_OF_2((PAS_OFFSETOF(pas_local_view_cache, views) + \
                               sizeof(pas_compact_segregated_exclusive_view_ptr) * (capacity)), \
                               sizeof(uint64_t))

PAS_API void pas_local_view_cache_construct(pas_local_view_cache* cache,
                                            uint8_t capacity);

static inline pas_local_view_cache_state pas_local_view_cache_get_state(pas_local_view_cache* cache)
{
    return (pas_local_view_cache_state)cache->state;
}

static inline void pas_local_view_cache_set_state(pas_local_view_cache* cache,
                                                  pas_local_view_cache_state state)
{
    cache->state = (uint8_t)state;
}

static inline bool pas_local_view_cache_is_full(pas_local_view_cache* cache)
{
    return pas_local_view_cache_get_state(cache) == pas_local_view_cache_full_state;
}

static inline bool pas_local_view_cache_can_push(pas_local_view_cache* cache)
{
    return pas_local_view_cache_get_state(cache) == pas_local_view_cache_not_full_state;
}

static inline bool pas_local_view_cache_is_empty(pas_local_view_cache* cache)
{
    PAS_TESTING_ASSERT(cache->bottom_index < cache->capacity);
    PAS_TESTING_ASSERT(cache->top_index < cache->capacity);
    PAS_TESTING_ASSERT(!pas_local_view_cache_is_full(cache) || cache->bottom_index == cache->top_index);

    return cache->top_index == cache->bottom_index && !pas_local_view_cache_is_full(cache);
}

static inline bool pas_local_view_cache_prepare_to_pop(pas_local_view_cache* cache)
{
    pas_local_view_cache_state state;
    
    PAS_TESTING_ASSERT(cache->bottom_index < cache->capacity
                       || pas_local_view_cache_get_state(cache) == pas_local_view_cache_stopped_state);
    PAS_TESTING_ASSERT(cache->top_index < cache->capacity
                       || pas_local_view_cache_get_state(cache) == pas_local_view_cache_stopped_state);
    PAS_TESTING_ASSERT(!pas_local_view_cache_is_full(cache) || cache->bottom_index == cache->top_index);

    if (cache->top_index != cache->bottom_index)
        return true;

    state = pas_local_view_cache_get_state(cache);
    switch (state) {
    case pas_local_view_cache_stopped_state:
        pas_local_allocator_scavenger_data_commit_if_necessary_slow(
            &cache->scavenger_data,
            pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_in_use_with_no_locks_held_mode,
            pas_local_allocator_view_cache_kind);
        return false;
    case pas_local_view_cache_full_state:
        return true;
    case pas_local_view_cache_not_full_state:
        return false;
    }
    return false;
}

static inline bool pas_local_view_cache_prepare_to_push(pas_local_view_cache* cache)
{
    pas_local_view_cache_state state;
    
    PAS_TESTING_ASSERT(cache->bottom_index < cache->capacity
                       || pas_local_view_cache_get_state(cache) == pas_local_view_cache_stopped_state);
    PAS_TESTING_ASSERT(cache->top_index < cache->capacity
                       || pas_local_view_cache_get_state(cache) == pas_local_view_cache_stopped_state);
    PAS_TESTING_ASSERT(!pas_local_view_cache_is_full(cache) || cache->bottom_index == cache->top_index);

    state = pas_local_view_cache_get_state(cache);
    if (state == pas_local_view_cache_not_full_state)
        return true;

    pas_compiler_fence();
    if (state == pas_local_view_cache_stopped_state) {
        pas_local_allocator_scavenger_data_commit_if_necessary_slow(
            &cache->scavenger_data,
            pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_not_in_use_with_scavenger_lock_held_mode,
            pas_local_allocator_view_cache_kind);
        PAS_TESTING_ASSERT(pas_local_view_cache_can_push(cache));
        return true;
    }

    PAS_TESTING_ASSERT(state == pas_local_view_cache_full_state);
    return false;
}

static inline pas_segregated_exclusive_view* pas_local_view_cache_pop(pas_local_view_cache* cache)
{
    static const bool verbose = false;
    static const bool take_first = true; /* If setting this to false was profitable, then this would
                                            be written as a stack. I keep this option here just so
                                            I can occasionally test if it matters. */
    
    pas_segregated_exclusive_view* result;
    uint8_t pop_index;

    PAS_TESTING_ASSERT(cache->bottom_index < cache->capacity);
    PAS_TESTING_ASSERT(cache->top_index < cache->capacity);
    PAS_TESTING_ASSERT(!pas_local_view_cache_is_empty(cache));

    if (take_first) {
        uint8_t bottom_index;
        uint8_t next_bottom_index;

        bottom_index = cache->bottom_index;
        next_bottom_index = bottom_index + 1;
        if (next_bottom_index >= cache->capacity) {
            PAS_TESTING_ASSERT(next_bottom_index == cache->capacity);
            next_bottom_index = 0;
        }
        
        pop_index = bottom_index;

        cache->bottom_index = next_bottom_index;
    } else {
        uint8_t top_index;
        
        top_index = cache->top_index;
        
        if (!top_index)
            top_index = cache->capacity - 1;
        else
            top_index--;

        pop_index = top_index;
        
        cache->top_index = top_index;
    }
    
    result = pas_compact_segregated_exclusive_view_ptr_load_non_null(cache->views + pop_index);
    
    if (PAS_ENABLE_TESTING)
        pas_compact_segregated_exclusive_view_ptr_store(cache->views + pop_index, NULL);

    pas_local_view_cache_set_state(cache, pas_local_view_cache_not_full_state);
    
    if (verbose)
        pas_log("%p: popped %p.\n", cache, result);

    return result;
}

static inline void pas_local_view_cache_push(
    pas_local_view_cache* cache,
    pas_segregated_exclusive_view* view)
{
    static const bool verbose = false;
    
    uint8_t top_index;
    uint8_t next_top_index;

    PAS_TESTING_ASSERT(cache->bottom_index < cache->capacity);
    PAS_TESTING_ASSERT(cache->top_index < cache->capacity);
    PAS_TESTING_ASSERT(pas_local_view_cache_can_push(cache));
    PAS_TESTING_ASSERT(view);

    if (verbose)
        pas_log("%p: pushing %p.\n", cache, view);

    top_index = cache->top_index;
    next_top_index = top_index + 1;
    if (next_top_index >= cache->capacity) {
        PAS_TESTING_ASSERT(next_top_index == cache->capacity);
        next_top_index = 0;
    }

    if (next_top_index == cache->bottom_index)
        pas_local_view_cache_set_state(cache, pas_local_view_cache_full_state);
    
    PAS_TESTING_ASSERT(pas_compact_segregated_exclusive_view_ptr_is_null(cache->views + top_index));

    pas_compact_segregated_exclusive_view_ptr_store(cache->views + top_index, view);
    cache->top_index = next_top_index;
}

PAS_API void pas_local_view_cache_move(pas_local_view_cache* destination,
                                       pas_local_view_cache* source);

PAS_API bool pas_local_view_cache_stop(pas_local_view_cache* cache, pas_lock_lock_mode page_lock_mode);

PAS_API void pas_local_view_cache_did_restart(pas_local_view_cache* cache);

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_VIEW_CACHE_H */

