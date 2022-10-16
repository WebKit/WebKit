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

#ifndef PAS_THREAD_LOCAL_CACHE_H
#define PAS_THREAD_LOCAL_CACHE_H

#include "pas_allocator_index.h"
#include "pas_allocator_scavenge_action.h"
#include "pas_darwin_spi.h"
#include "pas_deallocator_scavenge_action.h"
#include "pas_decommit_exclusion_range.h"
#include "pas_fast_tls.h"
#include "pas_internal_config.h"
#include "pas_local_allocator.h"
#include "pas_local_allocator_result.h"
#include "pas_malloc_stack_logging.h"
#include "pas_segregated_page_config_kind_and_role.h"
#include "pas_utils.h"
#include <pthread.h>

#define PAS_THREAD_LOCAL_CACHE_DESTROYED 1

PAS_BEGIN_EXTERN_C;

struct pas_magazine;
struct pas_thread_local_cache;
struct pas_thread_local_cache_node;
typedef struct pas_magazine pas_magazine;
typedef struct pas_thread_local_cache pas_thread_local_cache;
typedef struct pas_thread_local_cache_node pas_thread_local_cache_node;

struct pas_thread_local_cache {
    uintptr_t deallocation_log[PAS_DEALLOCATION_LOG_SIZE];

    unsigned deallocation_log_index;

    /* A dirty allocation log is one that has been flushed by the thread recently. A clean one is
       one that hasn't been touched by the thread (except maybe by appending more things to it). */
    bool deallocation_log_dirty;

    size_t num_logged_bytes; /* This undercounts since when we append small objects we don't
                                touch this. */

    /* This part of the TLC is allocated separately so that it does not move. */
    pas_thread_local_cache_node* node;

    unsigned* should_stop_bitvector;
    unsigned* pages_committed; /* This is a bitvector, protected by the scavenger lock. */
    
    pthread_t thread;
    
    bool should_stop_some;
    
    unsigned allocator_index_upper_bound;
    unsigned allocator_index_capacity;
    uint64_t local_allocators[1]; /* This is variable-length. */
};

PAS_API extern pas_fast_tls pas_thread_local_cache_fast_tls;

#if PAS_HAVE_PTHREAD_MACHDEP_H
#define PAS_THREAD_LOCAL_KEY __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY4
#endif

#if PAS_HAVE_THREAD_KEYWORD
PAS_API extern __thread void* pas_thread_local_cache_pointer;
#define PAS_THREAD_LOCAL_KEY pas_thread_local_cache_pointer
#endif

static PAS_ALWAYS_INLINE pas_thread_local_cache* pas_thread_local_cache_try_get_impl(void)
{
    return (pas_thread_local_cache*)PAS_FAST_TLS_GET(PAS_THREAD_LOCAL_KEY, &pas_thread_local_cache_fast_tls);
}

static inline pas_thread_local_cache* pas_thread_local_cache_try_get(void)
{
    pas_thread_local_cache* cache = pas_thread_local_cache_try_get_impl();
#if !PAS_OS(DARWIN)
    if (((uintptr_t)cache) == PAS_THREAD_LOCAL_CACHE_DESTROYED)
        return NULL;
#endif
    return cache;
}

static inline bool pas_thread_local_cache_can_set(void)
{
#if PAS_OS(DARWIN)
    return !pthread_self_is_exiting_np() && !pas_msl_is_enabled();
#else
    return ((uintptr_t)pas_thread_local_cache_try_get_impl()) != PAS_THREAD_LOCAL_CACHE_DESTROYED;
#endif
}

static inline void pas_thread_local_cache_set_impl(pas_thread_local_cache* thread_local_cache)
{
    PAS_FAST_TLS_SET(PAS_THREAD_LOCAL_KEY, &pas_thread_local_cache_fast_tls, thread_local_cache);
}

static inline void pas_thread_local_cache_set(pas_thread_local_cache* thread_local_cache)
{
    PAS_ASSERT(pas_thread_local_cache_can_set() || pas_thread_local_cache_try_get());
    pas_thread_local_cache_set_impl(thread_local_cache);
}

PAS_API size_t pas_thread_local_cache_size_for_allocator_index_capacity(unsigned allocator_index_capacity);

PAS_API pas_thread_local_cache* pas_thread_local_cache_create(void);

PAS_API void pas_thread_local_cache_destroy(pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_thread_local_cache* pas_thread_local_cache_get_slow(
    const pas_heap_config* config, pas_lock_hold_mode heap_lock_hold_mode);

static inline pas_thread_local_cache* pas_thread_local_cache_get_already_initialized(void)
{
    pas_thread_local_cache* result;

    result = pas_thread_local_cache_try_get();

    PAS_ASSERT(result);

    return result;
}

static inline pas_thread_local_cache*
pas_thread_local_cache_get_with_heap_lock_hold_mode(const pas_heap_config* config,
                                                    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_thread_local_cache* result;

    result = pas_thread_local_cache_try_get();
    
    if (PAS_LIKELY(result))
        return result;
    
    return pas_thread_local_cache_get_slow(config, heap_lock_hold_mode);
}

static inline pas_thread_local_cache* pas_thread_local_cache_get(const pas_heap_config* config)
{
    return pas_thread_local_cache_get_with_heap_lock_hold_mode(config, pas_lock_is_not_held);
}

static inline pas_thread_local_cache* pas_thread_local_cache_get_holding_heap_lock(
    const pas_heap_config* config)
{
    return pas_thread_local_cache_get_with_heap_lock_hold_mode(config, pas_lock_is_held);
}

PAS_API pas_local_allocator_result pas_thread_local_cache_get_local_allocator_slow(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index,
    pas_lock_hold_mode heap_lock_hold_mode);

static PAS_ALWAYS_INLINE uintptr_t pas_thread_local_cache_offset_of_allocator(pas_allocator_index index)
{
    return PAS_OFFSETOF(pas_thread_local_cache, local_allocators) + (uintptr_t)index * sizeof(uint64_t);
}

PAS_API bool pas_thread_local_cache_is_committed(pas_thread_local_cache* thread_local_cache,
                                                 pas_allocator_index begin_allocator_index,
                                                 pas_allocator_index end_allocator_index);

/* Returns a description of what parts of the allocator are available for decommit, in the sense that they
   are committed and are legal to decommit. The values in the range are offsets from the base of the cache.
   
   If this allocator is totally committed: we'll get a contiguous range with start = start of allocator, end
   = end of allocator.
   
   If this allocator is partly decommitted: we'll get an inverted range with start = the first byte after the
   last decommitted byte in the page, end = the first decommitted byte in the page.

   So either pas_decommit_exclusion_range_is_contiguous() or !pas_decommit_exclusion_range_is_inverted() tells
   us if the allocator is committed. It's impossible for the returned range to be empty. */
PAS_API pas_decommit_exclusion_range pas_thread_local_cache_compute_decommit_exclusion_range(
    pas_thread_local_cache* thread_local_cache,
    pas_allocator_index begin_allocator_index,
    pas_allocator_index end_allocator_index);

/* Must hold the scavenger_lock if this is called and there is something to commit. */
PAS_API void pas_thread_local_cache_ensure_committed(pas_thread_local_cache* thread_local_cache,
                                                     pas_allocator_index begin_allocator_index,
                                                     pas_allocator_index end_allocator_index);

PAS_API void pas_thread_local_cache_did_recommit_accidentally(pas_thread_local_cache* thread_local_cache,
                                                              pas_allocator_index begin_allocator_index,
                                                              pas_allocator_index end_allocator_index);

static PAS_ALWAYS_INLINE void*
pas_thread_local_cache_get_local_allocator_direct_without_any_checks_whatsoever(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t allocator_index)
{
    return (void*)(thread_local_cache->local_allocators + allocator_index);
}

static PAS_ALWAYS_INLINE void*
pas_thread_local_cache_get_local_allocator_direct_for_initialization(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t allocator_index)
{
    PAS_ASSERT(allocator_index < thread_local_cache->allocator_index_capacity);
    return pas_thread_local_cache_get_local_allocator_direct_without_any_checks_whatsoever(
        thread_local_cache, allocator_index);
}

static PAS_ALWAYS_INLINE void*
pas_thread_local_cache_get_local_allocator_direct_unchecked(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t allocator_index)
{
    PAS_TESTING_ASSERT(allocator_index < thread_local_cache->allocator_index_upper_bound);
    return pas_thread_local_cache_get_local_allocator_direct_without_any_checks_whatsoever(
        thread_local_cache, allocator_index);
}

static PAS_ALWAYS_INLINE void*
pas_thread_local_cache_get_local_allocator_direct(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t allocator_index)
{
    PAS_ASSERT(allocator_index < thread_local_cache->allocator_index_upper_bound);
    return pas_thread_local_cache_get_local_allocator_direct_unchecked(thread_local_cache, allocator_index);
}

static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_for_possibly_uninitialized_but_not_unselected_index(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    PAS_TESTING_ASSERT(allocator_index);
    
    if (PAS_UNLIKELY(allocator_index >= thread_local_cache->allocator_index_upper_bound)) {
        /* It could be that we just don't have an allocator index yet. */
        if (allocator_index >= (pas_allocator_index)UINT_MAX)
            return pas_local_allocator_result_create_failure();
    
        return pas_thread_local_cache_get_local_allocator_slow(
            thread_local_cache, allocator_index, heap_lock_hold_mode);
    }
    
    return pas_local_allocator_result_create_success(
        pas_thread_local_cache_get_local_allocator_direct_unchecked(thread_local_cache, allocator_index));
}

static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_for_initialized_index(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    PAS_TESTING_ASSERT(allocator_index != (pas_allocator_index)UINT_MAX);
    PAS_TESTING_ASSERT(allocator_index != UINT_MAX);

    return pas_thread_local_cache_get_local_allocator_for_possibly_uninitialized_but_not_unselected_index(
        thread_local_cache, allocator_index, heap_lock_hold_mode);
}

static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_for_possibly_uninitialized_index(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    if (!allocator_index)
        return pas_local_allocator_result_create_failure();

    return pas_thread_local_cache_get_local_allocator_for_initialized_index(
        thread_local_cache, allocator_index, heap_lock_hold_mode);
}

/* This is appropriate for use when we're going to take one of the inline_only paths, which can "handle" the
   unselected allocator by failing. */
static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_try_get_local_allocator_or_unselected_for_uninitialized_index(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index)
{
    if (PAS_UNLIKELY(allocator_index >= thread_local_cache->allocator_index_upper_bound))
        return pas_local_allocator_result_create_failure();
    
    return pas_local_allocator_result_create_success(
        pas_thread_local_cache_get_local_allocator_direct_unchecked(thread_local_cache, allocator_index));
}

static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_try_get_local_allocator_for_possibly_uninitialized_but_not_unselected_index(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index)
{
    PAS_TESTING_ASSERT(allocator_index);

    return pas_thread_local_cache_try_get_local_allocator_or_unselected_for_uninitialized_index(
        thread_local_cache, allocator_index);
}

PAS_API pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_if_can_set_cache_for_possibly_uninitialized_index_slow(
    unsigned allocator_index,
    const pas_heap_config* heap_config);

static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_if_can_set_cache_for_possibly_uninitialized_index(
    unsigned allocator_index,
    const pas_heap_config* heap_config)
{
    pas_thread_local_cache* cache;

    cache = pas_thread_local_cache_try_get();

    if (PAS_UNLIKELY(!cache)) {
        return
            pas_thread_local_cache_get_local_allocator_if_can_set_cache_for_possibly_uninitialized_index_slow(
                allocator_index, heap_config);
    }

    return pas_thread_local_cache_get_local_allocator_for_possibly_uninitialized_index(
        cache, allocator_index, pas_lock_is_not_held);
}

PAS_API pas_allocator_index pas_thread_local_cache_allocator_index_for_allocator(pas_thread_local_cache* cache,
                                                                                 void* allocator);

PAS_API unsigned pas_thread_local_cache_get_num_allocators(pas_thread_local_cache* cache);

PAS_API void pas_thread_local_cache_stop_local_allocators(
    pas_thread_local_cache* thread_local_cache,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_thread_local_cache_stop_local_allocators_if_necessary(
    pas_thread_local_cache* thread_local_cache,
    pas_local_allocator* requesting_allocator,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_thread_local_cache_flush_deallocation_log(
    pas_thread_local_cache* thread_local_cache,
    pas_lock_hold_mode heap_lock_hold_mode);
PAS_API void pas_thread_local_cache_flush_deallocation_log_direct(
    pas_thread_local_cache* thread_local_cache,
    pas_lock_hold_mode heap_lock_hold_mode);

enum pas_thread_local_cache_decommit_action {
    pas_thread_local_cache_decommit_no_action,
    pas_thread_local_cache_decommit_if_possible_action,
};
typedef enum pas_thread_local_cache_decommit_action pas_thread_local_cache_decommit_action;

/* Returns true if it's possible that we'll be able to return more memory if this was called
   again. */
PAS_API bool pas_thread_local_cache_for_all(pas_allocator_scavenge_action allocator_action,
                                            pas_deallocator_scavenge_action deallocator_action,
                                            pas_thread_local_cache_decommit_action thread_local_cache_decommit_action);

PAS_API PAS_NEVER_INLINE void pas_thread_local_cache_append_deallocation_slow(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin,
    pas_segregated_page_config_kind_and_role kind_and_role);

static PAS_ALWAYS_INLINE uintptr_t pas_thread_local_cache_encode_object(
    uintptr_t begin,
    pas_segregated_page_config_kind_and_role kind_and_role)
{
    return (begin << PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_NUM_BITS) | (uintptr_t)kind_and_role;
}

static PAS_ALWAYS_INLINE void pas_thread_local_cache_append_deallocation(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin,
    pas_segregated_page_config_kind_and_role kind_and_role)
{
    unsigned index;

    index = thread_local_cache->deallocation_log_index;
    if (PAS_UNLIKELY(index >= PAS_DEALLOCATION_LOG_SIZE - 1)) {
        pas_thread_local_cache_append_deallocation_slow(thread_local_cache, begin, kind_and_role);
        return;
    }
    
    thread_local_cache->deallocation_log[index++] = pas_thread_local_cache_encode_object(begin, kind_and_role);
    thread_local_cache->deallocation_log_index = index;
}

static PAS_ALWAYS_INLINE void pas_thread_local_cache_append_deallocation_with_size(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin, size_t object_size,
    pas_segregated_page_config_kind_and_role kind_and_role)
{
    size_t new_num_logged_bytes;

    new_num_logged_bytes = thread_local_cache->num_logged_bytes + object_size;
    if (new_num_logged_bytes > PAS_DEALLOCATION_LOG_MAX_BYTES) {
        pas_thread_local_cache_append_deallocation_slow(thread_local_cache, begin, kind_and_role);
        return;
    }

    thread_local_cache->num_logged_bytes = new_num_logged_bytes;
    
    pas_thread_local_cache_append_deallocation(thread_local_cache, begin, kind_and_role);
}

PAS_API void pas_thread_local_cache_shrink(pas_thread_local_cache* thread_local_cache,
                                           pas_lock_hold_mode heap_lock_hold_mode);

PAS_END_EXTERN_C;

#endif /* PAS_THREAD_LOCAL_CACHE_H */

