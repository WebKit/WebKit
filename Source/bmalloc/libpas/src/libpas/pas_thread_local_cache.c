/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#include "pas_all_heap_configs.h"
#include "pas_bitvector.h"
#include "pas_committed_pages_vector.h"
#include "pas_debug_heap.h"
#include "pas_heap_lock.h"
#include "pas_large_utility_free_heap.h"
#include "pas_log.h"
#include "pas_monotonic_time.h"
#include "pas_scavenger.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_segregated_page.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_thread_local_cache_node.h"
#include "pas_thread_suspend_lock.h"
#include <unistd.h>
#if PAS_OS(DARWIN)
#include <mach/thread_act.h>
#endif
#if PAS_PLATFORM(PLAYSTATION)
#include <pthread_np.h>
#endif

PAS_BEGIN_EXTERN_C;

#if PAS_HAVE_THREAD_KEYWORD
__thread void* pas_thread_local_cache_pointer = NULL;
#endif

pas_fast_tls pas_thread_local_cache_fast_tls = PAS_FAST_TLS_INITIALIZER;

size_t pas_thread_local_cache_size_for_allocator_index_capacity(unsigned allocator_index_capacity)
{
    size_t result;
    
    result = PAS_OFFSETOF(pas_thread_local_cache, local_allocators)
        + 8 * allocator_index_capacity;
    
    return pas_round_up_to_power_of_2(result, pas_page_malloc_alignment());
}

static size_t num_pages(unsigned allocator_index_capacity)
{
    return pas_thread_local_cache_size_for_allocator_index_capacity(allocator_index_capacity)
        >> pas_page_malloc_alignment_shift();
}

static void compute_page_indices(pas_allocator_index begin_allocator_index,
                                 pas_allocator_index end_allocator_index,
                                 uintptr_t* begin_page_index,
                                 uintptr_t* end_page_index)
{
    size_t page_size_shift;
    page_size_shift = pas_page_malloc_alignment_shift();
    *begin_page_index = pas_thread_local_cache_offset_of_allocator(begin_allocator_index) >> page_size_shift;
    *end_page_index =
        ((pas_thread_local_cache_offset_of_allocator(end_allocator_index) - 1) >> page_size_shift) + 1;
}

static size_t pages_committed_size(unsigned allocator_index_capacity)
{
    return PAS_BITVECTOR_NUM_BYTES(num_pages(allocator_index_capacity));
}

static void deallocate(pas_thread_local_cache* thread_local_cache)
{
    char* begin;
    size_t size;

    pas_large_utility_free_heap_deallocate(
        thread_local_cache->should_stop_bitvector,
        PAS_BITVECTOR_NUM_BYTES(thread_local_cache->allocator_index_capacity));

    pas_large_utility_free_heap_deallocate(
        thread_local_cache->pages_committed,
        pages_committed_size(thread_local_cache->allocator_index_capacity));

    begin = (char*)thread_local_cache;
    size = pas_thread_local_cache_size_for_allocator_index_capacity(
        thread_local_cache->allocator_index_capacity);

    /* If we're doing symmetric decommit, then we need to commit the memory for the TLC now. */
    pas_page_malloc_commit_without_mprotect(begin, size, pas_may_mmap);
    
    pas_large_utility_free_heap_deallocate(begin, size);
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
    static const bool verbose = false;

    pas_thread_local_cache* thread_local_cache;

    thread_local_cache = (pas_thread_local_cache*)arg;

    if (verbose)
        pas_log("[%d] Destructor call for TLS %p\n", getpid(), thread_local_cache);

#if !PAS_OS(DARWIN)
    /* If pthread_self_is_exiting_np does not exist, we set PAS_THREAD_LOCAL_CACHE_DESTROYED in the TLS so that
       subsequent calls of pas_thread_local_cache_try_get() can detect whether TLS is destroyed. Since
       PAS_THREAD_LOCAL_CACHE_DESTROYED is a non-null value, pthread will call this destructor again (up to
       PTHREAD_DESTRUCTOR_ITERATIONS times). Each time it does, it will clear the TLS entry. Hence, we need to re-set
       PAS_THREAD_LOCAL_CACHE_DESTROYED in the TLS each time to continue to indicate that destroy() has already been called once. */
    pas_thread_local_cache_set_impl((pas_thread_local_cache*)PAS_THREAD_LOCAL_CACHE_DESTROYED);
    PAS_ASSERT(!pas_thread_local_cache_can_set());
#endif

    if (((uintptr_t)thread_local_cache) != PAS_THREAD_LOCAL_CACHE_DESTROYED)
        destroy(thread_local_cache, pas_lock_is_not_held);
    else {
        if (verbose)
            pas_log("[%d] Repeated destructor call for TLS %p\n", getpid(), thread_local_cache);
    }
}

static pas_thread_local_cache* allocate_cache(unsigned allocator_index_capacity)
{
    static const bool verbose = false;
    
    size_t size;
    pas_thread_local_cache* result;
    size_t index;

    PAS_ASSERT(allocator_index_capacity >= PAS_LOCAL_ALLOCATOR_UNSELECTED_NUM_INDICES);

    size = pas_thread_local_cache_size_for_allocator_index_capacity(allocator_index_capacity);
    
    if (verbose)
        printf("Cache size: %zu\n", size);
    
    result = (pas_thread_local_cache*)pas_large_utility_free_heap_allocate_with_alignment(
        size, pas_alignment_create_traditional(pas_page_malloc_alignment()), "pas_thread_local_cache");

    pas_zero_memory(result, size);

    result->should_stop_bitvector = (unsigned int*)pas_large_utility_free_heap_allocate(
        PAS_BITVECTOR_NUM_BYTES(allocator_index_capacity),
        "pas_thread_local_cache/should_stop_bitvector");

    pas_zero_memory(result->should_stop_bitvector, PAS_BITVECTOR_NUM_BYTES(allocator_index_capacity));

    PAS_ASSERT(pages_committed_size(allocator_index_capacity));
    
    result->pages_committed = (unsigned*)pas_large_utility_free_heap_allocate(
        pages_committed_size(allocator_index_capacity),
        "pas_thread_local_cache/pages_committed");

    pas_zero_memory(result->pages_committed, pages_committed_size(allocator_index_capacity));
    for (index = num_pages(allocator_index_capacity); index--;)
        pas_bitvector_set(result->pages_committed, index, true);
    
    result->allocator_index_capacity = allocator_index_capacity;

    return result;
}

static void dump_thread_diagnostics(pthread_t thread)
{
    uint64_t thread_id;
    char thread_name[256];
    int getname_result;
    PAS_UNUSED_PARAM(thread_id);
#if PAS_OS(DARWIN)
    if (!pthread_threadid_np(thread, &thread_id))
        pas_log("[%d] thread %p has id %llu\n", getpid(), thread, thread_id);
    else
        pas_log("[%d] thread %p does not have id\n", getpid(), thread);
#endif
#if PAS_PLATFORM(PLAYSTATION)
    getname_result = pthread_get_name_np(thread, thread_name);
#else
    getname_result = pthread_getname_np(thread, thread_name, sizeof(thread_name));
#endif
    if (!getname_result)
        pas_log("[%d] thread %p has name %s\n", getpid(), (void*)thread, thread_name);
    else
        pas_log("[%d] thread %p does not have name\n", getpid(), (void*)thread);
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

    if (verbose) {
        pas_log("[%d] TLC %p created with thread %p\n", getpid(), thread_local_cache, (void*)pthread_self());
        dump_thread_diagnostics(pthread_self());
    }
    thread_local_cache->thread = pthread_self();

    pas_compiler_fence();
    thread_local_cache->allocator_index_upper_bound = allocator_index_upper_bound;
    
    pas_local_allocator_construct_unselected(
        (pas_local_allocator*)pas_thread_local_cache_get_local_allocator_direct_unchecked(
            thread_local_cache, PAS_LOCAL_ALLOCATOR_UNSELECTED_INDEX));

    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node))
        pas_thread_local_cache_layout_node_commit_and_construct(layout_node, thread_local_cache);

    pas_compiler_fence();
    thread_local_cache->node->cache = thread_local_cache;

    pas_thread_local_cache_set(thread_local_cache);
    
    return thread_local_cache;
}

void pas_thread_local_cache_destroy(pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_thread_local_cache* thread_local_cache;

    thread_local_cache = pas_thread_local_cache_try_get();

    if (!thread_local_cache)
        return;

    pas_thread_local_cache_set(NULL);

    if (verbose)
        pas_log("[%d] TLC %p getting destroyed\n", getpid(), thread_local_cache);
    destroy(thread_local_cache, heap_lock_hold_mode);
}

pas_thread_local_cache* pas_thread_local_cache_get_slow(const pas_heap_config* config,
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

    PAS_ASSERT(desired_allocator_index);
    PAS_ASSERT(desired_allocator_index >= old_index_upper_bound);
    PAS_ASSERT(desired_allocator_index < (pas_allocator_index)UINT_MAX);
    
    /* Rather than copy the deallocation log let's just flush it. */
    pas_thread_local_cache_flush_deallocation_log(thread_local_cache, heap_lock_hold_mode);
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    pas_lock_lock(&thread_local_cache->node->scavenger_lock);
    
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

        pas_local_allocator_construct_unselected(
            (pas_local_allocator*)pas_thread_local_cache_get_local_allocator_direct_unchecked(
                new_thread_local_cache, PAS_LOCAL_ALLOCATOR_UNSELECTED_INDEX));
        
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

        pas_thread_local_cache_layout_node_commit_and_construct(layout_node, new_thread_local_cache);
    }

    pas_compiler_fence();
    new_thread_local_cache->allocator_index_upper_bound = desired_index_upper_bound;

    if (thread_local_cache != new_thread_local_cache)
        deallocate(thread_local_cache);

    pas_lock_unlock(&thread_local_cache->node->scavenger_lock);
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    if (thread_local_cache != new_thread_local_cache)
        pas_thread_local_cache_set(new_thread_local_cache);
    
    PAS_ASSERT(desired_allocator_index < new_thread_local_cache->allocator_index_upper_bound);
    return pas_local_allocator_result_create_success(
        pas_thread_local_cache_get_local_allocator_direct(new_thread_local_cache,
                                                          desired_allocator_index));
}

bool pas_thread_local_cache_is_committed(pas_thread_local_cache* thread_local_cache,
                                         pas_allocator_index begin_allocator_index,
                                         pas_allocator_index end_allocator_index)
{
    uintptr_t begin_page_index;
    uintptr_t end_page_index;
    uintptr_t page_index;

    compute_page_indices(begin_allocator_index, end_allocator_index, &begin_page_index, &end_page_index);

    for (page_index = begin_page_index; page_index < end_page_index; ++page_index) {
        if (!pas_bitvector_get(thread_local_cache->pages_committed, page_index))
            return false;
    }

    return true;
}

pas_decommit_exclusion_range pas_thread_local_cache_compute_decommit_exclusion_range(
    pas_thread_local_cache* thread_local_cache,
    pas_allocator_index begin_allocator_index,
    pas_allocator_index end_allocator_index)
{
    uintptr_t begin_page_index;
    uintptr_t end_page_index;
    uintptr_t page_index;
    pas_range allocator_range;
    pas_decommit_exclusion_range result;
    size_t page_size_shift;

    allocator_range = pas_range_create(pas_thread_local_cache_offset_of_allocator(begin_allocator_index),
                                       pas_thread_local_cache_offset_of_allocator(end_allocator_index));
    result.start_of_possible_decommit = allocator_range.begin;
    result.end_of_possible_decommit = allocator_range.end;

    compute_page_indices(begin_allocator_index, end_allocator_index, &begin_page_index, &end_page_index);

    page_size_shift = pas_page_malloc_alignment_shift();

    for (page_index = begin_page_index; page_index < end_page_index; ++page_index) {
        if (!pas_bitvector_get(thread_local_cache->pages_committed, page_index)) {
            result.start_of_possible_decommit = PAS_MAX(
                result.start_of_possible_decommit,
                PAS_MIN(
                    allocator_range.end,
                    (page_index + 1) << page_size_shift));
            result.end_of_possible_decommit = PAS_MIN(
                result.end_of_possible_decommit,
                PAS_MAX(
                    allocator_range.begin,
                    page_index << page_size_shift));
        }
    }

    PAS_ASSERT(!pas_decommit_exclusion_range_is_empty(result));

    return result;
}

void pas_thread_local_cache_ensure_committed(pas_thread_local_cache* thread_local_cache,
                                             pas_allocator_index begin_allocator_index,
                                             pas_allocator_index end_allocator_index)
{
    uintptr_t begin_page_index;
    uintptr_t end_page_index;
    uintptr_t page_index;

    compute_page_indices(begin_allocator_index, end_allocator_index, &begin_page_index, &end_page_index);

    for (page_index = begin_page_index; page_index < end_page_index; ++page_index) {
        if (pas_bitvector_get(thread_local_cache->pages_committed, page_index))
            continue;
        
        pas_lock_assert_held(&thread_local_cache->node->scavenger_lock);
        
        /* Don't attempt to do fancy things with spans for commit, since we're no longer really
           optimizing for symmetric commit anyway. */
        pas_page_malloc_commit_without_mprotect(
            (char*)thread_local_cache + (page_index << pas_page_malloc_alignment_shift()),
            pas_page_malloc_alignment(),
            pas_may_mmap);

        pas_bitvector_set(thread_local_cache->pages_committed, page_index, true);
    }
}

pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_if_can_set_cache_for_possibly_uninitialized_index_slow(
    unsigned allocator_index,
    const pas_heap_config* heap_config)
{
    if (!pas_thread_local_cache_can_set() || pas_debug_heap_is_enabled(heap_config->kind))
        return pas_local_allocator_result_create_failure();

    return pas_thread_local_cache_get_local_allocator_for_possibly_uninitialized_index(
        pas_thread_local_cache_get(heap_config),
        allocator_index,
        pas_lock_is_not_held);
}

pas_allocator_index pas_thread_local_cache_allocator_index_for_allocator(pas_thread_local_cache* cache,
                                                                         void* allocator)
{
    uintptr_t cache_address;
    uintptr_t allocator_address;
    uintptr_t begin_allocators;
    uintptr_t end_allocators;
    uintptr_t offset;
    uintptr_t index;

    cache_address = (uintptr_t)cache;
    allocator_address = (uintptr_t)allocator;

    begin_allocators = cache_address + PAS_OFFSETOF(pas_thread_local_cache, local_allocators);
    end_allocators = begin_allocators + sizeof(uint64_t) * cache->allocator_index_upper_bound;

    PAS_ASSERT(allocator_address >= begin_allocators);
    PAS_ASSERT(allocator_address < end_allocators);

    offset = allocator_address - begin_allocators;

    PAS_ASSERT(pas_is_aligned(offset, sizeof(uint64_t)));
    
    index = offset / sizeof(uint64_t);

    PAS_ASSERT(index < cache->allocator_index_upper_bound);

    PAS_ASSERT((pas_allocator_index)index == index);
    return (pas_allocator_index)index;
}

void pas_thread_local_cache_stop_local_allocators(pas_thread_local_cache* thread_local_cache,
                                                  pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_thread_local_cache_layout_node layout_node;

    /* We have to hold the heap lock to do things to our local allocators unless we also set the
       is_in_use bit. */
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    pas_lock_lock(&thread_local_cache->node->scavenger_lock);

    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node)) {
        pas_allocator_index allocator_index;

        allocator_index = pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node);
        
        if (allocator_index >= thread_local_cache->allocator_index_upper_bound)
            break;

        if (!pas_thread_local_cache_layout_node_is_committed(layout_node, thread_local_cache))
            continue;

        pas_thread_local_cache_layout_node_stop(
            layout_node, thread_local_cache, pas_lock_lock_mode_lock, pas_lock_is_held);
    }

    pas_lock_unlock(&thread_local_cache->node->scavenger_lock);
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

    data = (stop_local_allocators_if_necessary_data*)arg;

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

    data = (stop_local_allocators_if_necessary_data*)arg;

    allocator_index = (pas_allocator_index)index.index;
    PAS_ASSERT(allocator_index == index.index);

    PAS_TESTING_ASSERT(pas_bitvector_get(data->should_stop_bitvector, allocator_index));
    
    pas_bitvector_set(data->should_stop_bitvector, allocator_index, false);
    
    scavenger_data = (pas_local_allocator_scavenger_data*)pas_thread_local_cache_get_local_allocator_direct(
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
                                     pas_segregated_page_role role,
                                     size_t* index,
                                     uintptr_t encoded_begin,
                                     pas_lock** held_lock)
{
    static const bool verbose = false;

    pas_lock* last_held_lock;

    if (!pas_segregated_deallocation_logging_mode_does_logging(
            pas_segregated_page_config_logging_mode_for_role(page_config, role))) {
        pas_panic("Deallocation logging is disabled for %s/%s, but here we are.\n",
                  pas_segregated_page_config_kind_get_string(page_config.kind),
                  pas_segregated_page_role_get_string(role));
    }

    last_held_lock = NULL;

    for (;;) {
        uintptr_t begin;
        begin = encoded_begin >> PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_NUM_BITS;

        switch (page_config.kind) {
        case pas_segregated_page_config_kind_null:
            PAS_ASSERT(!begin);
            break;

        case pas_segregated_page_config_kind_pas_utility_small:
            PAS_ASSERT(!"Should not be reached");
            break;

        default:
            last_held_lock = *held_lock;
            pas_segregated_page_deallocate(begin, held_lock, deallocation_mode, cache, page_config, role);
            if (verbose && *held_lock != last_held_lock && last_held_lock)
                pas_log("Switched lock from %p to %p.\n", last_held_lock, *held_lock);
            
            pas_compiler_fence();
            cache->deallocation_log[*index] = 0;
            break;
        }

        if (!*index)
            return;

        encoded_begin = cache->deallocation_log[--*index];
        if (PAS_UNLIKELY((encoded_begin & PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_MASK)
            != pas_segregated_page_config_kind_and_role_create(page_config.kind, role))) {
            ++*index;
            return;
        }
    }
}

static PAS_ALWAYS_INLINE void flush_deallocation_log_without_resetting(
    pas_thread_local_cache* thread_local_cache,
    pas_segregated_deallocation_mode deallocation_mode)
{
    size_t index;
    pas_lock* held_lock;

    pas_lock_testing_assert_held(&thread_local_cache->node->scavenger_lock);
    
    index = thread_local_cache->deallocation_log_index;
    held_lock = NULL;

    while (index) {
        uintptr_t encoded_begin;

        encoded_begin = thread_local_cache->deallocation_log[--index];

        switch ((pas_segregated_page_config_kind_and_role)
                (encoded_begin & PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_MASK)) {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
        case pas_segregated_page_config_kind_ ## name ## _and_shared_role: \
            process_deallocation_log_with_config( \
                thread_local_cache, deallocation_mode, value, pas_segregated_page_shared_role, &index, \
                encoded_begin, &held_lock); \
            break; \
        case pas_segregated_page_config_kind_ ## name ## _and_exclusive_role: \
            process_deallocation_log_with_config( \
                thread_local_cache, deallocation_mode, value, pas_segregated_page_exclusive_role, &index, \
                encoded_begin, &held_lock); \
            break;
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
        }
    }
    
    pas_lock_switch(&held_lock, NULL);
}

static void flush_deallocation_log_for_scavenger(pas_thread_local_cache* thread_local_cache)
{
    flush_deallocation_log_without_resetting(thread_local_cache, pas_segregated_deallocation_direct_mode);
}

static PAS_ALWAYS_INLINE void flush_deallocation_log_with_mode(
    pas_thread_local_cache* thread_local_cache,
    pas_lock_hold_mode heap_lock_hold_mode,
    pas_segregated_deallocation_mode deallocation_mode)
{
    if (!thread_local_cache)
        return;

    pas_lock_lock(&thread_local_cache->node->scavenger_lock);

    switch (deallocation_mode) {
    case pas_segregated_deallocation_direct_mode:
        /* If we're doing a direct flush then try to share code with flush_deallocation_log_for_scavenger,
           which does the same thing we would do. */
        flush_deallocation_log_for_scavenger(thread_local_cache);
        break;
    case pas_segregated_deallocation_to_view_cache_mode:
        flush_deallocation_log_without_resetting(
            thread_local_cache, pas_segregated_deallocation_to_view_cache_mode);
        break;
    }
    
    thread_local_cache->deallocation_log_index = 0;
    thread_local_cache->num_logged_bytes = 0;

    /* Dirtying the deallocation log is a signal to the scavenger that we are actively operating on
       this log. */
    thread_local_cache->deallocation_log_dirty = true;

    pas_lock_unlock(&thread_local_cache->node->scavenger_lock);
    
    if (heap_lock_hold_mode == pas_lock_is_not_held)
        pas_scavenger_notify_eligibility_if_needed();
}

void pas_thread_local_cache_flush_deallocation_log(pas_thread_local_cache* thread_local_cache,
                                                   pas_lock_hold_mode heap_lock_hold_mode)
{
    flush_deallocation_log_with_mode(
        thread_local_cache, heap_lock_hold_mode, pas_segregated_deallocation_to_view_cache_mode);
}

void pas_thread_local_cache_flush_deallocation_log_direct(pas_thread_local_cache* thread_local_cache,
                                                          pas_lock_hold_mode heap_lock_hold_mode)
{
    flush_deallocation_log_with_mode(
        thread_local_cache, heap_lock_hold_mode, pas_segregated_deallocation_direct_mode);
}

typedef struct scavenger_thread_suspend_data {
    bool did_suspend;
    bool is_scavenger_itself;
#if PAS_OS(DARWIN)
    mach_port_t mach_thread;
#endif
} scavenger_thread_suspend_data;

static scavenger_thread_suspend_data scavenger_thread_suspend_data_create(void)
{
    scavenger_thread_suspend_data thread_suspend_data;
    thread_suspend_data.did_suspend = false;
    thread_suspend_data.is_scavenger_itself = false;
    return thread_suspend_data;
}

#if PAS_OS(DARWIN)

static void suspend(pas_thread_local_cache* cache, scavenger_thread_suspend_data* thread_suspend_data)
{
    static const bool verbose = false;

    pthread_t thread;
    kern_return_t result;

    if (thread_suspend_data->did_suspend)
        return;

    thread_suspend_data->did_suspend = true;
    
    if (verbose)
        printf("Suspending TLC %p with thread %p.\n", cache, cache->thread);

    thread = cache->thread;
    PAS_ASSERT(thread);
    
    thread_suspend_data->is_scavenger_itself = thread == pthread_self();
    if (thread_suspend_data->is_scavenger_itself)
        return;

    thread_suspend_data->mach_thread = pthread_mach_thread_np(thread);
    result = thread_suspend(thread_suspend_data->mach_thread);
    
    /* Fun fact: it's impossible for us to try to suspend a thread that has exited, since
       thread exit for any thread with a TLC needs to grab the heap lock and we hold the
       heap lock. */

    if (result != KERN_SUCCESS) {
        pas_log("[%d] Failed to suspend pthread %p (mach thread %d) associated with TLC %p: %d\n",
                getpid(), thread, thread_suspend_data->mach_thread, cache, result);
        dump_thread_diagnostics(thread);
        PAS_ASSERT(result == KERN_SUCCESS);
    }
}

static void resume(pas_thread_local_cache* cache, scavenger_thread_suspend_data* thread_suspend_data)
{
    static const bool verbose = false;
    
    kern_return_t result;

    if (verbose)
        pas_log("Resuming TLC %p with thread %p.\n", cache, cache->thread);
    
    if (thread_suspend_data->is_scavenger_itself)
        return;

    result = thread_resume(thread_suspend_data->mach_thread);
    
    PAS_ASSERT(result == KERN_SUCCESS);
}

#endif

static void decommit_allocator_range(pas_thread_local_cache* cache,
                                     pas_thread_local_cache_layout_segment* begin_segment,
                                     uintptr_t begin_node_index,
                                     uintptr_t start_of_possible_decommit,
                                     uintptr_t end_of_possible_decommit,
                                     pas_thread_local_cache_layout_segment* end_segment,
                                     uintptr_t end_node_index)
{
#if PAS_OS(DARWIN)
    static const bool verbose = false;
    
    uintptr_t begin_page_index;
    uintptr_t end_page_index;
    pas_thread_local_cache_layout_node inner_node;
    uintptr_t page_index;
    size_t page_size;
    size_t page_size_shift;
    pas_range decommit_range;

    pas_heap_lock_assert_held();
    pas_lock_assert_held(&cache->node->scavenger_lock);

    page_size = pas_page_malloc_alignment();
    page_size_shift = pas_page_malloc_alignment_shift();
    
    begin_page_index = pas_round_up_to_power_of_2(
        start_of_possible_decommit, page_size) >> page_size_shift;
    end_page_index = end_of_possible_decommit >> page_size_shift;

    if (begin_page_index >= end_page_index)
        return;

    decommit_range = pas_range_create(begin_page_index << page_size_shift,
                                      end_page_index << page_size_shift);

    if (begin_segment) {
        pas_thread_local_cache_layout_segment* segment = begin_segment;
        uintptr_t node_index = begin_node_index;
        for (inner_node = pas_thread_local_cache_layout_segment_get_node(segment, node_index); !(segment == end_segment && node_index == end_node_index); inner_node = pas_thread_local_cache_layout_segment_next_node(&segment, &node_index))
            pas_thread_local_cache_layout_node_prepare_to_decommit(inner_node, cache, decommit_range);
    }
    
    for (page_index = begin_page_index; page_index < end_page_index; ++page_index) {
        PAS_ASSERT(pas_bitvector_get(cache->pages_committed, page_index));
        pas_bitvector_set(cache->pages_committed, page_index, false);
    }

    if (verbose)
        pas_log("Decommitting %p...%p\n", (void*)decommit_range.begin, (void*)decommit_range.end);

    pas_page_malloc_decommit_without_mprotect(
        (char*)cache + decommit_range.begin, pas_range_size(decommit_range), pas_may_mmap);

    if (verbose) {
        pas_log("Num committed pages in the range we just decommitted: %zu\n",
                pas_count_committed_pages(
                    (char*)cache + decommit_range.begin,
                    pas_range_size(decommit_range),
                    &pas_large_utility_free_heap_allocation_config));
    }
#else /* PAS_OS(DARWIN) -> so !PAS_OS(DARWIN) */
    PAS_UNUSED_PARAM(cache);
    PAS_UNUSED_PARAM(begin_segment);
    PAS_UNUSED_PARAM(begin_node_index);
    PAS_UNUSED_PARAM(start_of_possible_decommit);
    PAS_UNUSED_PARAM(end_of_possible_decommit);
    PAS_UNUSED_PARAM(end_segment);
    PAS_UNUSED_PARAM(end_node_index);
#endif /* PAS_OS(DARWIN) -> so end of !PAS_OS(DARWIN) */
}

static void stop_allocator(pas_thread_local_cache* cache,
                           pas_allocator_scavenge_action allocator_action,
                           pas_allocator_index allocator_index,
                           pas_local_allocator_scavenger_data* scavenger_data,
                           bool* result,
                           scavenger_thread_suspend_data* thread_suspend_data)
{
    static const bool verbose = false;
    
    if (allocator_action == pas_allocator_scavenge_request_stop_action) {
        uint8_t should_stop_count;
                    
        if (verbose)
            pas_log("Doing the request thingy.\n");
                    
        if (scavenger_data->dirty) {
            if (verbose)
                pas_log("It was dirty.\n");
            scavenger_data->dirty = false;
            *result = true;
            return;
        }

        /* Bail out if it's no longer active. */
        if (pas_local_allocator_scavenger_data_is_stopped(scavenger_data)) {
            if (verbose)
                pas_log("It was empty.\n");
            return;
        }

        should_stop_count = scavenger_data->should_stop_count;
        if (should_stop_count < pas_local_allocator_should_stop_count_for_suspend) {
            scavenger_data->should_stop_count = ++should_stop_count;
                        
            PAS_ASSERT(should_stop_count
                       <= pas_local_allocator_should_stop_count_for_suspend);
                        
            pas_bitvector_set(cache->should_stop_bitvector, allocator_index, true);
            cache->should_stop_some = true;
                        
            *result = true;
                        
            if (verbose)
                pas_log("Told it to stop.\n");
                        
            return;
        }
                    
        PAS_ASSERT(should_stop_count
                   == pas_local_allocator_should_stop_count_for_suspend);
    } else {
        if (verbose)
            pas_log("Doing actual stop.\n");
                    
        PAS_ASSERT(allocator_action == pas_allocator_scavenge_force_stop_action);
    }

    if (pas_local_allocator_scavenger_data_is_stopped(scavenger_data)) {
        if (verbose)
            pas_log("It was empty.\n");
        return;
    }

#if PAS_OS(DARWIN)
    if (verbose)
        pas_log("Need to suspend for allocator %p\n", scavenger_data);

    suspend(cache, thread_suspend_data);

    PAS_ASSERT(thread_suspend_data->did_suspend);
                
    if (scavenger_data->is_in_use) {
        *result = true;
        return;
    }

    pas_compiler_fence();

    PAS_ASSERT(!scavenger_data->is_in_use);

    if (verbose)
        pas_log("Stopping allocator %p while suspended\n", scavenger_data);
    if (!pas_local_allocator_scavenger_data_stop(
            scavenger_data, pas_lock_lock_mode_try_lock, pas_lock_is_held))
        *result = true;
#else
    PAS_UNUSED_PARAM(thread_suspend_data);
#endif
}

bool pas_thread_local_cache_for_all(pas_allocator_scavenge_action allocator_action,
                                    pas_deallocator_scavenge_action deallocator_action,
                                    pas_thread_local_cache_decommit_action thread_local_cache_decommit_action)
{
    pas_thread_local_cache_node* node;
    bool result;
    size_t page_size;
    size_t page_size_shift;

    page_size = pas_page_malloc_alignment();
    page_size_shift = pas_page_malloc_alignment_shift();

    /* Set this to true if we encounter a situation where we think that more memory could
       be returned if this was called again. */
    result = false;

    /* The thread suspend lock ensures that thread suspention can be done only from one thread at a time.
       This avoids dead-lock where multiple threads suspend / resume each other. */
    pas_thread_suspend_lock_lock();

    /* The heap lock protects two things:
       
       - The iteration over thread local caches. Otherwise we wouldn't be able to ask if a cache
         node has an actual cache.
       
       - The local allocators themselves. You're allowed to do things to the local allocator only
         if you:
         
         A) are the thread that owns the cache and you have set the is_in_use bit.
         
         B) are the thread that owns the cache and you hold the heap lock.
         
         C) you hold the heap lock and you have checked that the is_in_use bit is not set. */
    pas_heap_lock_lock();
    
    for (node = pas_thread_local_cache_node_first; node; node = node->next) {
        pas_thread_local_cache* cache;
        
        cache = node->cache;
        if (!cache)
            continue;

        pas_lock_lock(&node->scavenger_lock);

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
            scavenger_thread_suspend_data thread_suspend_data;
            pas_thread_local_cache_layout_node layout_node;
            pas_thread_local_cache_layout_segment* segment;
            pas_thread_local_cache_layout_segment* begin_segment;
            uintptr_t start_of_possible_decommit;
            uintptr_t page_index;
            uintptr_t node_index;
            uintptr_t begin_node_index;
            
            thread_suspend_data = scavenger_thread_suspend_data_create();

            start_of_possible_decommit =
                pas_thread_local_cache_offset_of_allocator(PAS_LOCAL_ALLOCATOR_UNSELECTED_NUM_INDICES);

            PAS_ASSERT(!thread_suspend_data.did_suspend);

            begin_segment = NULL;
            begin_node_index = 0;
            segment = pas_thread_local_cache_layout_first_segment;
            node_index = 0;
            for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR_WITH_SEGMENT_AND_INDEX(layout_node, segment, node_index)) {
                pas_allocator_index allocator_index;
                pas_local_allocator_scavenger_data* scavenger_data;

                allocator_index =
                    pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node);

                if (allocator_index >= cache->allocator_index_upper_bound)
                    break;

                scavenger_data = (pas_local_allocator_scavenger_data*)
                    pas_thread_local_cache_get_local_allocator_direct(cache, allocator_index);

                if (thread_local_cache_decommit_action == pas_thread_local_cache_decommit_if_possible_action) {
                    pas_allocator_index end_allocator_index = allocator_index + pas_thread_local_cache_layout_node_num_allocator_indices(layout_node);
                    pas_decommit_exclusion_range decommit_exclusion_range = pas_thread_local_cache_compute_decommit_exclusion_range(cache, allocator_index, end_allocator_index);
                    PAS_ASSERT(start_of_possible_decommit <= decommit_exclusion_range.start_of_possible_decommit);
                    PAS_ASSERT(start_of_possible_decommit <= decommit_exclusion_range.end_of_possible_decommit);

                    if (pas_decommit_exclusion_range_is_contiguous(decommit_exclusion_range)) {
                        stop_allocator(
                            cache, allocator_action, allocator_index, scavenger_data, &result, &thread_suspend_data);

                        if (pas_local_allocator_scavenger_data_is_stopped(scavenger_data)) {
                            if (!begin_segment) {
                                begin_segment = segment;
                                begin_node_index = node_index;
                            } else {
                                PAS_TESTING_ASSERT(
                                    pas_thread_local_cache_layout_node_get_allocator_index_generic(pas_thread_local_cache_layout_segment_get_node(begin_segment, begin_node_index))
                                    < allocator_index);
                            }
                        } else {
                            decommit_exclusion_range =
                                pas_decommit_exclusion_range_create_inverted(decommit_exclusion_range);
                        }
                    }

                    if (pas_decommit_exclusion_range_is_inverted(decommit_exclusion_range)) {
                        decommit_allocator_range(
                            cache, begin_segment, begin_node_index, start_of_possible_decommit,
                            decommit_exclusion_range.end_of_possible_decommit, segment, node_index);
                        start_of_possible_decommit = decommit_exclusion_range.start_of_possible_decommit;
                        begin_segment = NULL;
                        begin_node_index = 0;
                    }
                } else
                    stop_allocator(cache, allocator_action, allocator_index, scavenger_data, &result, &thread_suspend_data);
            }
            
#if PAS_OS(DARWIN)
            if (thread_suspend_data.did_suspend)
                resume(cache, &thread_suspend_data);
#endif

            if (thread_local_cache_decommit_action == pas_thread_local_cache_decommit_if_possible_action) {
                uintptr_t begin_page_index =
                    pas_round_up_to_power_of_2(start_of_possible_decommit, page_size)
                    >> page_size_shift;
                uintptr_t end_page_index =
                    pas_thread_local_cache_size_for_allocator_index_capacity(cache->allocator_index_capacity)
                    >> page_size_shift;

                for (page_index = begin_page_index; page_index < end_page_index; ++page_index) {
                    if (!pas_bitvector_get(cache->pages_committed, page_index)) {
                        decommit_allocator_range(
                            cache, begin_segment, begin_node_index, start_of_possible_decommit,
                            page_index << page_size_shift,
                            segment, node_index);
                        start_of_possible_decommit = (page_index + 1) << page_size_shift;
                        begin_segment = NULL;
                        begin_node_index = 0;
                    }
                }

                decommit_allocator_range(
                    cache, begin_segment, begin_node_index, start_of_possible_decommit,
                    pas_thread_local_cache_size_for_allocator_index_capacity(cache->allocator_index_capacity),
                    segment, node_index);
            }
        }
        
        pas_lock_unlock(&node->scavenger_lock);
    }
    
    pas_heap_lock_unlock();
    pas_thread_suspend_lock_unlock();

    return result;
}

PAS_NEVER_INLINE void pas_thread_local_cache_append_deallocation_slow(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin,
    pas_segregated_page_config_kind_and_role kind_and_role)
{
    unsigned index;

    index = thread_local_cache->deallocation_log_index;
    PAS_ASSERT(index < PAS_DEALLOCATION_LOG_SIZE);
    thread_local_cache->deallocation_log[index++] = pas_thread_local_cache_encode_object(begin, kind_and_role);
    thread_local_cache->deallocation_log_index = index;

    pas_thread_local_cache_flush_deallocation_log(thread_local_cache, pas_lock_is_not_held);
}

void pas_thread_local_cache_shrink(pas_thread_local_cache* thread_local_cache,
                                   pas_lock_hold_mode heap_lock_hold_mode)
{
    if (!thread_local_cache)
        return;
    pas_thread_local_cache_flush_deallocation_log_direct(thread_local_cache, heap_lock_hold_mode);
    pas_thread_local_cache_stop_local_allocators(thread_local_cache, heap_lock_hold_mode);
    if (heap_lock_hold_mode == pas_lock_is_not_held)
        pas_scavenger_notify_eligibility_if_needed();
}

PAS_END_EXTERN_C;

#endif /* LIBPAS_ENABLED */
