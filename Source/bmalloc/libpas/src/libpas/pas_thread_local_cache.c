/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "pas_thread_local_cache.h"

#include <mach/thread_act.h>
#include "pas_all_heap_configs.h"
#include "pas_compact_large_utility_free_heap.h"
#include "pas_debug_heap.h"
#include "pas_heap_lock.h"
#include "pas_log.h"
#include "pas_monotonic_time.h"
#include "pas_scavenger.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_segregated_page.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_thread_local_cache_node.h"
#include <unistd.h>

pas_fast_tls pas_thread_local_cache_fast_tls = PAS_FAST_TLS_INITIALIZER;

size_t pas_thread_local_cache_size_for_allocator_index_capacity(unsigned allocator_index_capacity)
{
    size_t result;
    
    result = PAS_OFFSETOF(pas_thread_local_cache, local_allocators)
        + 8 * allocator_index_capacity;
    
    return result;
}

static void deallocate(pas_thread_local_cache* thread_local_cache)
{
    char* begin;

    pas_compact_large_utility_free_heap_deallocate(
        thread_local_cache->should_stop_bitvector,
        PAS_BITVECTOR_NUM_BYTES(thread_local_cache->allocator_index_capacity));
    
    begin = (char*)thread_local_cache;
    pas_compact_large_utility_free_heap_deallocate(
        begin,
        pas_thread_local_cache_size_for_allocator_index_capacity(
            thread_local_cache->allocator_index_capacity));
}

static void destroy(pas_thread_local_cache* thread_local_cache, pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    if (verbose)
        pas_log("[%d] Destroying TLC %p\n", getpid(), thread_local_cache);
    
    pas_thread_local_cache_shrink(thread_local_cache, pas_lock_is_held);
    pas_thread_local_cache_node_deallocate(thread_local_cache->node);
    deallocate(thread_local_cache);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

static void destructor(void* arg)
{
    pas_thread_local_cache* thread_local_cache;
    
    thread_local_cache = arg;

    destroy(thread_local_cache, pas_lock_is_not_held);
}

static pas_thread_local_cache* allocate_cache(unsigned allocator_index_capacity)
{
    static const bool verbose = false;
    
    size_t size;
    pas_thread_local_cache* result;

    size = pas_thread_local_cache_size_for_allocator_index_capacity(allocator_index_capacity);
    
    if (verbose)
        printf("Cache size: %zu\n", size);
    
    result = pas_compact_large_utility_free_heap_allocate(size, "pas_thread_local_cache");

    pas_zero_memory(result, size);

    result->should_stop_bitvector = pas_compact_large_utility_free_heap_allocate(
        PAS_BITVECTOR_NUM_BYTES(allocator_index_capacity),
        "pas_thread_local_cache/should_stop_bitvector");

    pas_zero_memory(result->should_stop_bitvector, PAS_BITVECTOR_NUM_BYTES(allocator_index_capacity));
    
    result->allocator_index_capacity = allocator_index_capacity;

    return result;
}

static void dump_thread_diagnostics(pthread_t thread)
{
    uint64_t thread_id;
    char thread_name[256];
    if (!pthread_threadid_np(thread, &thread_id))
        pas_log("[%d] thread %p has id %llu\n", getpid(), thread, thread_id);
    else
        pas_log("[%d] thread %p does not have id\n", getpid(), thread);
    if (!pthread_getname_np(thread, thread_name, sizeof(thread_name)))
        pas_log("[%d] thread %p has name %s\n", getpid(), thread, thread_name);
    else
        pas_log("[%d] thread %p does not have name\n", getpid(), thread);
}

pas_thread_local_cache* pas_thread_local_cache_create(void)
{
    static const bool verbose = false;
    
    pas_thread_local_cache* thread_local_cache;
    unsigned allocator_index_upper_bound;
    pas_thread_local_cache_layout_node layout_node;

    allocator_index_upper_bound = pas_thread_local_cache_layout_next_allocator_index;
    
    thread_local_cache = allocate_cache(allocator_index_upper_bound);
    
    thread_local_cache->node = pas_thread_local_cache_node_allocate();
    thread_local_cache->node->cache = thread_local_cache;

    if (verbose) {
        pas_log("[%d] TLC %p created with thread %p\n", getpid(), thread_local_cache, pthread_self());
        dump_thread_diagnostics(pthread_self());
    }
    thread_local_cache->thread = pthread_self();
    
    thread_local_cache->allocator_index_upper_bound = allocator_index_upper_bound;

    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node))
        pas_thread_local_cache_layout_node_construct(layout_node, thread_local_cache);

    pas_thread_local_cache_set_impl(thread_local_cache);
    
    return thread_local_cache;
}

void pas_thread_local_cache_destroy(pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_thread_local_cache* thread_local_cache;

    thread_local_cache = pas_thread_local_cache_try_get();

    if (!thread_local_cache)
        return;

    if (verbose)
        pas_log("[%d] TLC %p getting destroyed\n", getpid(), thread_local_cache);
    destroy(thread_local_cache, heap_lock_hold_mode);

    pas_thread_local_cache_set_impl(NULL);
}

pas_thread_local_cache* pas_thread_local_cache_get_slow(pas_heap_config* config,
                                                        pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_thread_local_cache* thread_local_cache;
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    PAS_FAST_TLS_CONSTRUCT_IF_NECESSARY(
        PAS_THREAD_LOCAL_KEY, &pas_thread_local_cache_fast_tls, destructor);

    /* This is allowed to set up a particular TLC layout that every TLC has to obey. So, it has to
       happen before any TLCs are allocated. */
    pas_heap_config_activate(config);
    
    PAS_ASSERT(!pas_thread_local_cache_try_get());
    
    thread_local_cache = pas_thread_local_cache_create();
    
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    
    return thread_local_cache;
}

pas_local_allocator_result pas_thread_local_cache_get_local_allocator_slow(
    pas_thread_local_cache* thread_local_cache,
    unsigned desired_allocator_index,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_thread_local_cache* new_thread_local_cache;
    pas_thread_local_cache_layout_node layout_node;
    unsigned old_index_upper_bound;
    unsigned desired_index_upper_bound;

    old_index_upper_bound = thread_local_cache->allocator_index_upper_bound;
    
    PAS_ASSERT(desired_allocator_index >= old_index_upper_bound);
    PAS_ASSERT(desired_allocator_index < (pas_allocator_index)UINT_MAX);
    
    /* Rather than copy the deallocation log let's just flush it. */
    pas_thread_local_cache_flush_deallocation_log(thread_local_cache, heap_lock_hold_mode);
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    
    PAS_ASSERT(desired_allocator_index < pas_thread_local_cache_layout_next_allocator_index);

    desired_index_upper_bound = pas_thread_local_cache_layout_next_allocator_index;
    if (desired_index_upper_bound <= thread_local_cache->allocator_index_capacity)
        new_thread_local_cache = thread_local_cache;
    else {
        unsigned index_capacity;
        
        index_capacity = PAS_MAX(thread_local_cache->allocator_index_capacity << 1,
                                 desired_index_upper_bound);
        
        new_thread_local_cache = allocate_cache(index_capacity);
    
        if (verbose)
            printf("[%d] Reallocating TLC %p -> %p\n", getpid(), thread_local_cache, new_thread_local_cache);
    
        new_thread_local_cache->node = thread_local_cache->node;
    
        new_thread_local_cache->thread = thread_local_cache->thread;
    
        new_thread_local_cache->allocator_index_upper_bound =
            thread_local_cache->allocator_index_upper_bound;

        for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node)) {
            pas_allocator_index allocator_index;
            
            allocator_index = pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node);
            
            if (allocator_index >= old_index_upper_bound)
                break;

            pas_thread_local_cache_layout_node_move(
                layout_node, new_thread_local_cache, thread_local_cache);
        }

        memcpy(new_thread_local_cache->should_stop_bitvector,
               thread_local_cache->should_stop_bitvector,
               PAS_BITVECTOR_NUM_BYTES(thread_local_cache->allocator_index_upper_bound));

        pas_compiler_fence();
        new_thread_local_cache->node->cache = new_thread_local_cache;
    }
    
    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node)) {
        pas_allocator_index allocator_index;

        allocator_index = pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node);
        
        if (allocator_index < old_index_upper_bound)
            continue;
        if (allocator_index >= desired_index_upper_bound)
            break;

        pas_thread_local_cache_layout_node_construct(layout_node, new_thread_local_cache);
    }

    pas_compiler_fence();
    new_thread_local_cache->allocator_index_upper_bound = desired_index_upper_bound;

    if (thread_local_cache != new_thread_local_cache)
        deallocate(thread_local_cache);

    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    if (thread_local_cache != new_thread_local_cache)
        pas_thread_local_cache_set_impl(new_thread_local_cache);
    
    PAS_ASSERT(desired_allocator_index < new_thread_local_cache->allocator_index_upper_bound);
    return pas_local_allocator_result_create_success(
        pas_thread_local_cache_get_local_allocator_impl(new_thread_local_cache,
                                                        desired_allocator_index));
}

pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_if_can_set_cache_slow(unsigned allocator_index,
                                                                 pas_heap_config* heap_config)
{
    if (!pas_thread_local_cache_can_set() || pas_debug_heap_is_enabled(heap_config->kind))
        return pas_local_allocator_result_create_failure();

    return pas_thread_local_cache_get_local_allocator(
        pas_thread_local_cache_get(heap_config),
        allocator_index,
        pas_lock_is_not_held);
}

void pas_thread_local_cache_stop_local_allocators(pas_thread_local_cache* thread_local_cache,
                                                  pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_thread_local_cache_layout_node layout_node;

    /* We have to hold the heap lock to do things to our local allocators unless we also set the
       is_in_use bit. */
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    pas_lock_lock(&thread_local_cache->node->log_flush_lock);

    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node)) {
        pas_allocator_index allocator_index;

        allocator_index = pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node);
        
        if (allocator_index >= thread_local_cache->allocator_index_upper_bound)
            break;

        pas_thread_local_cache_layout_node_stop(
            layout_node, thread_local_cache, pas_lock_lock_mode_lock, pas_lock_is_held);
    }

    pas_lock_unlock(&thread_local_cache->node->log_flush_lock);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

typedef struct {
    pas_thread_local_cache* thread_local_cache;
    pas_local_allocator* requesting_allocator;
    pas_lock_hold_mode heap_lock_hold_mode;
    unsigned* should_stop_bitvector;
} stop_local_allocators_if_necessary_data;

static unsigned stop_local_allocators_if_necessary_set_bit_source(
    size_t word_index,
    void* arg)
{
    stop_local_allocators_if_necessary_data* data;

    data = arg;

    return data->should_stop_bitvector[word_index];
}

static bool stop_local_allocators_if_necessary_set_bit_callback(
    pas_found_bit_index index,
    void* arg)
{
    static const bool verbose = false;
    
    stop_local_allocators_if_necessary_data* data;
    pas_allocator_index allocator_index;
    pas_local_allocator_scavenger_data* scavenger_data;

    data = arg;

    allocator_index = index.index;

    PAS_TESTING_ASSERT(pas_bitvector_get(data->should_stop_bitvector, allocator_index));
    
    pas_bitvector_set(data->should_stop_bitvector, allocator_index, false);
    
    scavenger_data = pas_thread_local_cache_get_local_allocator_impl(
        data->thread_local_cache, allocator_index);

    if ((pas_local_allocator*)scavenger_data == data->requesting_allocator)
        return true;
    
    if (!scavenger_data->should_stop_count)
        return true;
    
    if (verbose)
        pas_log("Stopping allocator %p because pas_thread_local_cache_stop_local_allocators_if_necessary\n", scavenger_data);
    pas_local_allocator_scavenger_data_stop(
        scavenger_data, pas_lock_lock_mode_lock, data->heap_lock_hold_mode);

    return true;
}

void pas_thread_local_cache_stop_local_allocators_if_necessary(
    pas_thread_local_cache* thread_local_cache,
    pas_local_allocator* requesting_allocator,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    stop_local_allocators_if_necessary_data data;

    if (!thread_local_cache->should_stop_some)
        return;

    data.thread_local_cache = thread_local_cache;
    data.requesting_allocator = requesting_allocator;
    data.heap_lock_hold_mode = heap_lock_hold_mode;
    data.should_stop_bitvector = thread_local_cache->should_stop_bitvector;

    pas_bitvector_for_each_set_bit(
        stop_local_allocators_if_necessary_set_bit_source,
        0,
        PAS_BITVECTOR_NUM_WORDS(thread_local_cache->allocator_index_upper_bound),
        stop_local_allocators_if_necessary_set_bit_callback,
        &data);

    thread_local_cache->should_stop_some = false;
}

static PAS_ALWAYS_INLINE void
process_deallocation_log_with_config(pas_thread_local_cache* cache,
                                     pas_segregated_deallocation_mode deallocation_mode,
                                     pas_segregated_page_config page_config,
                                     size_t* index,
                                     uintptr_t encoded_begin,
                                     pas_lock** held_lock)
{
    static const bool verbose = false;

    pas_lock* last_held_lock;

    last_held_lock = NULL;

    for (;;) {
        uintptr_t begin;
        begin = encoded_begin >> PAS_SEGREGATED_PAGE_CONFIG_KIND_NUM_BITS;

        switch (page_config.kind) {
        case pas_segregated_page_config_kind_null:
            PAS_ASSERT(!begin);
            break;

        case pas_segregated_page_config_kind_pas_utility_small:
            PAS_ASSERT(!"Should not be reached");
            break;

        default:
            last_held_lock = *held_lock;
            pas_segregated_page_deallocate(
                begin, held_lock, deallocation_mode, cache, page_config);
            if (verbose && *held_lock != last_held_lock && last_held_lock)
                pas_log("Switched lock from %p to %p.\n", last_held_lock, *held_lock);
            
            pas_compiler_fence();
            cache->deallocation_log[*index] = 0;
            break;
        }

        if (!*index)
            return;

        encoded_begin = cache->deallocation_log[--*index];
        if (PAS_UNLIKELY((encoded_begin & PAS_SEGREGATED_PAGE_CONFIG_KIND_MASK) != page_config.kind)) {
            ++*index;
            return;
        }
    }
}

static PAS_ALWAYS_INLINE void flush_deallocation_log(
    pas_thread_local_cache* thread_local_cache,
    pas_segregated_deallocation_mode deallocation_mode)
{
    size_t index;
    pas_lock* held_lock;

    pas_lock_testing_assert_held(&thread_local_cache->node->log_flush_lock);
    
    index = thread_local_cache->deallocation_log_index;
    held_lock = NULL;

    while (index) {
        uintptr_t encoded_begin;

        encoded_begin = thread_local_cache->deallocation_log[--index];

        switch ((pas_segregated_page_config_kind)
                (encoded_begin & PAS_SEGREGATED_PAGE_CONFIG_KIND_MASK)) {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
        case pas_segregated_page_config_kind_ ## name: \
            process_deallocation_log_with_config( \
                thread_local_cache, deallocation_mode, value, &index, encoded_begin, &held_lock); \
            break;
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
        }
    }
    
    pas_lock_switch(&held_lock, NULL);
}

static void flush_deallocation_log_for_scavenger(pas_thread_local_cache* thread_local_cache)
{
    if (!thread_local_cache->deallocation_log_index)
        return;
    
    flush_deallocation_log(thread_local_cache, pas_segregated_deallocation_direct_mode);
}

void pas_thread_local_cache_flush_deallocation_log(pas_thread_local_cache* thread_local_cache,
                                                   pas_lock_hold_mode heap_lock_hold_mode)
{
    if (!thread_local_cache)
        return;

    pas_lock_lock(&thread_local_cache->node->log_flush_lock);

    flush_deallocation_log(thread_local_cache, pas_segregated_deallocation_to_view_cache_mode);
    
    thread_local_cache->deallocation_log_index = 0;
    thread_local_cache->num_logged_bytes = 0;

    /* Dirtying the deallocation log is a signal to the scavenger that we are actively operating on
       this log. */
    thread_local_cache->deallocation_log_dirty = true;

    pas_lock_unlock(&thread_local_cache->node->log_flush_lock);
    
    if (heap_lock_hold_mode == pas_lock_is_not_held)
        pas_scavenger_notify_eligibility_if_needed();
}

static void suspend(pas_thread_local_cache* cache)
{
    static const bool verbose = false;

    pthread_t thread;
    mach_port_t mach_thread;
    kern_return_t result;
    
    if (verbose)
        printf("Suspending TLC %p with thread %p.\n", cache, cache->thread);

    thread = cache->thread;
    if (thread == pthread_self())
        return;

    mach_thread = pthread_mach_thread_np(thread);
    result = thread_suspend(mach_thread);
    
    /* Fun fact: it's impossible for us to try to suspend a thread that has exited, since
       thread exit for any thread with a TLC needs to grab the heap lock and we hold the
       heap lock. */

    if (result != KERN_SUCCESS) {
        pas_log("[%d] Failed to suspend pthread %p (mach thread %d) associated with TLC %p: %d\n",
                getpid(), thread, mach_thread, cache, result);
        dump_thread_diagnostics(thread);
        PAS_ASSERT(result == KERN_SUCCESS);
    }
}

static void resume(pas_thread_local_cache* cache)
{
    kern_return_t result;
    
    if (cache->thread == pthread_self())
        return;

    result = thread_resume(pthread_mach_thread_np(cache->thread));
    
    PAS_ASSERT(result == KERN_SUCCESS);
}

bool pas_thread_local_cache_for_all(pas_allocator_scavenge_action allocator_action,
                                    pas_deallocator_scavenge_action deallocator_action,
                                    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_thread_local_cache_node* node;
    bool result;

    /* Set this to true if we encounter a situation where we think that more memory could
       be returned if this was called again. */
    result = false;

    /* The heap lock protects two things:
       
       - The iteration over thread local caches. Otherwise we wouldn't be able to ask if a cache
         node has an actual cache.
       
       - The local allocators themselves. You're allowed to do things to the local allocator only
         if you:
         
         A) are the thread that owns the cache and you have set the is_in_use bit.
         
         B) are the thread that owns the cache and you hold the heap lock.
         
         C) you hold the heap lock and you have checked that the is_in_use bit is not set. */
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    
    for (node = pas_thread_local_cache_node_first; node; node = node->next) {
        pas_thread_local_cache* cache;
        
        cache = node->cache;
        if (!cache)
            continue;

        pas_lock_lock(&node->log_flush_lock);

        switch (deallocator_action) {
        case pas_deallocator_scavenge_no_action:
            break;
            
        case pas_deallocator_scavenge_flush_log_if_clean_action:
            if (cache->deallocation_log_dirty) {
                cache->deallocation_log_dirty = false;
                result = !!cache->deallocation_log_index;
            } else
                flush_deallocation_log_for_scavenger(cache);
            break;
            
        case pas_deallocator_scavenge_flush_log_action:
            flush_deallocation_log_for_scavenger(cache);
            break;
        }

        if (allocator_action != pas_allocator_scavenge_no_action) {
            bool did_suspend;
            pas_thread_local_cache_layout_node layout_node;
            
            did_suspend = false;

            for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node)) {
                pas_allocator_index allocator_index;
                pas_local_allocator_scavenger_data* scavenger_data;

                allocator_index =
                    pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node);

                if (allocator_index >= cache->allocator_index_upper_bound)
                    break;

                scavenger_data = pas_thread_local_cache_get_local_allocator_impl(cache, allocator_index);

                if (allocator_action == pas_allocator_scavenge_request_stop_action) {
                    uint8_t should_stop_count;
                    
                    if (verbose)
                        pas_log("Doing the request thingy.\n");
                    
                    if (scavenger_data->dirty) {
                        if (verbose)
                            pas_log("It was dirty.\n");
                        scavenger_data->dirty = false;
                        result = true;
                        continue;
                    }

                    /* Bail out if it's no longer active. */
                    if (!pas_local_allocator_scavenger_data_is_active(scavenger_data)) {
                        if (verbose)
                            pas_log("It was empty.\n");
                        continue;
                    }

                    should_stop_count = scavenger_data->should_stop_count;
                    if (should_stop_count < pas_local_allocator_should_stop_count_for_suspend) {
                        scavenger_data->should_stop_count = ++should_stop_count;
                        
                        PAS_ASSERT(should_stop_count
                                   <= pas_local_allocator_should_stop_count_for_suspend);
                        
                        pas_bitvector_set(cache->should_stop_bitvector, allocator_index, true);
                        cache->should_stop_some = true;
                        
                        result = true;
                        
                        if (verbose)
                            pas_log("Told it to stop.\n");
                        
                        continue;
                    }
                    
                    PAS_ASSERT(should_stop_count
                               == pas_local_allocator_should_stop_count_for_suspend);
                } else {
                    if (verbose)
                        pas_log("Doing actual stop.\n");
                    
                    PAS_ASSERT(allocator_action == pas_allocator_scavenge_force_stop_action);
                }

                if (!pas_local_allocator_scavenger_data_is_active(scavenger_data)) {
                    if (verbose)
                        pas_log("It was empty.\n");
                    continue;
                }

                if (!pas_thread_local_cache_is_guaranteed_to_destruct()) {
                    /* We're on a platform that can't guarantee that thread local caches are destructed.
                       Therefore, we might have a TLC that has a dangling thread pointer. So, we don't
                       attempt to do the suspend thing. */
                    continue;
                }

                if (verbose)
                    pas_log("Need to suspend for allocator %p\n", scavenger_data);
                
                if (!did_suspend) {
                    suspend(cache);
                    did_suspend = true;
                }
                
                if (scavenger_data->is_in_use) {
                    result = true;
                    continue;
                }

                if (verbose)
                    pas_log("Stopping allocator %p while suspended\n", scavenger_data);
                if (!pas_local_allocator_scavenger_data_stop(
                        scavenger_data, pas_lock_lock_mode_try_lock, pas_lock_is_held))
                    result = true;
            }
            
            if (did_suspend)
                resume(cache);
        }
        
        pas_lock_unlock(&node->log_flush_lock);
    }
    
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    return result;
}

void pas_thread_local_cache_append_deallocation_slow(pas_thread_local_cache* thread_local_cache,
                                                     uintptr_t begin,
                                                     pas_segregated_page_config_kind kind)
{
    unsigned index;

    index = thread_local_cache->deallocation_log_index;
    PAS_ASSERT(index < PAS_DEALLOCATION_LOG_SIZE);
    thread_local_cache->deallocation_log[index++] = pas_thread_local_cache_encode_object(begin, kind);
    thread_local_cache->deallocation_log_index = index;

    pas_thread_local_cache_flush_deallocation_log(thread_local_cache, pas_lock_is_not_held);
}

void pas_thread_local_cache_shrink(pas_thread_local_cache* thread_local_cache,
                                   pas_lock_hold_mode heap_lock_hold_mode)
{
    if (!thread_local_cache)
        return;
    pas_thread_local_cache_flush_deallocation_log(thread_local_cache, heap_lock_hold_mode);
    pas_thread_local_cache_stop_local_allocators(thread_local_cache, heap_lock_hold_mode);
    if (heap_lock_hold_mode == pas_lock_is_not_held)
        pas_scavenger_notify_eligibility_if_needed();
}

#endif /* LIBPAS_ENABLED */
