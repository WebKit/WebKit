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

#include "pas_local_view_cache.h"

#include "pas_segregated_exclusive_view_inlines.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory.h"

void pas_local_view_cache_construct(pas_local_view_cache* cache,
                                    uint8_t capacity)
{
    pas_local_allocator_scavenger_data_construct(
        &cache->scavenger_data, pas_local_allocator_view_cache_kind);
    
    cache->capacity = capacity;
    cache->bottom_index = 0;
    cache->top_index = 0;
    cache->state = pas_local_view_cache_not_full_state;

    if (PAS_ENABLE_TESTING) {
        uint8_t index;

        for (index = capacity; index--;)
            pas_compact_segregated_exclusive_view_ptr_store(cache->views + index, NULL);
    }
}

void pas_local_view_cache_move(pas_local_view_cache* destination,
                               pas_local_view_cache* source)
{
    PAS_ASSERT(!destination->scavenger_data.is_in_use);
    PAS_ASSERT(!source->scavenger_data.is_in_use);
    memcpy(destination, source, PAS_LOCAL_VIEW_CACHE_SIZE(source->capacity));
}

static bool stop_impl(pas_local_view_cache* cache,
                      pas_lock_lock_mode page_lock_mode)
{
    pas_lock* held_lock;
    pas_segregated_size_directory* directory;
    pas_segregated_page_config page_config;
    
    directory = NULL;
    page_config = (pas_segregated_page_config){ };
    held_lock = NULL;
    
    while (!pas_local_view_cache_is_empty(cache)) {
        pas_segregated_exclusive_view* cached_view;
        pas_segregated_page* page;
        bool should_notify_eligibility;
        
        cached_view = pas_local_view_cache_pop(cache);
        
        if (directory) {
            PAS_ASSERT(
                pas_compact_segregated_size_directory_ptr_load_non_null(&cached_view->directory)
                == directory);
        } else {
            directory = pas_compact_segregated_size_directory_ptr_load_non_null(&cached_view->directory);
            page_config = *pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);
        }
        
        page = pas_segregated_page_for_boundary(cached_view->page_boundary, page_config);
        
        if (!pas_segregated_page_switch_lock_with_mode(page, &held_lock, page_lock_mode, page_config)) {
            PAS_ASSERT(!pas_local_view_cache_is_full(cache));
            pas_local_view_cache_push(cache, cached_view);
            return false;
        }
        
        should_notify_eligibility = true;
        pas_segregated_exclusive_view_did_stop_allocating(
            cached_view, directory, page, page_config, should_notify_eligibility);
    }

    pas_lock_switch(&held_lock, NULL);
    return true;
}

bool pas_local_view_cache_stop(pas_local_view_cache* cache,
                               pas_lock_lock_mode page_lock_mode)
{
    bool result;

    PAS_ASSERT(!cache->scavenger_data.is_in_use);

    /* Doing this check before setting is_in_use guards against situations where calling stop would
       recommit a decommitted view cache. */
    if (pas_local_allocator_scavenger_data_is_stopped(&cache->scavenger_data))
        return true;

    cache->scavenger_data.is_in_use = true;
    pas_compiler_fence();

    /* The scavenger thread may race with the client thread on calling pas_local_view_cache_stop.
       The scavenger will first suspend the client thread before checking the is_in_use flag.
       If is_in_use is set, the scavenger will not call pas_local_view_cache_stop, and move on.
       If is_in_use is not set, the scavenger will call pas_local_allocator_scavenger_data_stop,
       which in turn calls pas_local_view_cache_stop to stop the local_view_cache.

       Normally, if the scavenger has already completed its call to pas_local_view_cache_stop
       before the client calls it, pas_local_view_cache_stop will just return early for the client.
       This is because pas_local_view_cache_stop first check pas_local_allocator_scavenger_data_is_stopped
       before doing the work to stop the local_view_cache.

       However, if the client thread is already in the process of executing pas_local_view_cache_stop,
       gets past the pas_local_allocator_scavenger_data_is_stopped check, and then, gets suspended by
       the scavenger before setting the is_in_use flag, the scavenger can stop the local_view_cache
       after the client already checked and thinks it is not stopped yet. When the client thread
       resumes from suspension, it will be unhappy to find that the local_view_cache is already
       stopped, and fails an assertion.

       The fix is to re-check pas_local_allocator_scavenger_data_is_stopped after setting the
       is_in_use flag.

       If the re-check shows that the local_view_cache is already stopped, then pas_local_view_cache_stop
       can return early like the first pas_local_allocator_scavenger_data_is_stopped check. This is
       fine to do because the caller expects this as a possible outcome.

       Before returning early due to the re-check, we also need to clear the is_in_use flag.
       The is_in_use flag is meant to be set like with an RAII scope. Hence, it is correct and
       required that we also clear it here before we return.
    */
    if (pas_local_allocator_scavenger_data_is_stopped(&cache->scavenger_data)) {
        cache->scavenger_data.is_in_use = false;
        return true;
    }

    result = stop_impl(cache, page_lock_mode);

    if (result) {
        PAS_ASSERT(pas_local_view_cache_get_state(cache) == pas_local_view_cache_not_full_state, pas_local_view_cache_get_state(cache));
        PAS_ASSERT(cache->top_index == cache->bottom_index);

        pas_local_view_cache_set_state(cache, pas_local_view_cache_stopped_state);
        cache->scavenger_data.should_stop_count = 0;
        cache->scavenger_data.kind = pas_local_allocator_stopped_view_cache_kind;
    }

    pas_compiler_fence();
    cache->scavenger_data.is_in_use = false;

    return result;
}

void pas_local_view_cache_did_restart(pas_local_view_cache* cache)
{
    pas_local_view_cache_set_state(cache, pas_local_view_cache_not_full_state);
}

#endif /* LIBPAS_ENABLED */

