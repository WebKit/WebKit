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

#ifndef PAS_THREAD_LOCAL_CACHE_H
#define PAS_THREAD_LOCAL_CACHE_H

#include "pas_allocator_scavenge_action.h"
#include "pas_deallocator_scavenge_action.h"
#include "pas_fast_tls.h"
#include "pas_internal_config.h"
#include "pas_local_allocator.h"
#include "pas_local_allocator_result.h"
#include "pas_utils.h"
#include <pthread.h>

#if defined(__has_include) && __has_include(<pthread/private.h>)
#include <pthread/private.h>
#define PAS_HAVE_PTHREAD_PRIVATE 1
#else
#define PAS_HAVE_PTHREAD_PRIVATE 0
#endif

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

    size_t num_logged_bytes; /* This undercounts since when we append small objects we don't
                                touch this. */

    /* This part of the TLC is allocated separately so that it does not move. */
    pas_thread_local_cache_node* node;

    unsigned* should_stop_bitvector;
    
    pthread_t thread;
    
    bool should_stop_some;
    
    unsigned allocator_index_upper_bound;
    unsigned allocator_index_capacity;
    uint64_t local_allocators[1]; /* This is variable-length. */
};

PAS_API extern pas_fast_tls pas_thread_local_cache_fast_tls;

#define PAS_THREAD_LOCAL_KEY __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY4

static inline pas_thread_local_cache* pas_thread_local_cache_try_get(void)
{
    return (pas_thread_local_cache*)PAS_FAST_TLS_GET(
        PAS_THREAD_LOCAL_KEY, &pas_thread_local_cache_fast_tls);
}

#if PAS_HAVE_PTHREAD_PRIVATE
#define PAS_THREAD_LOCAL_CACHE_CAN_DETECT_THREAD_EXIT \
    __builtin_available(macos 10.16, ios 14.0, tvos 14.0, watchos 7.0, *)

static inline bool pas_thread_local_cache_is_guaranteed_to_destruct(void)
{
    if (PAS_THREAD_LOCAL_CACHE_CAN_DETECT_THREAD_EXIT)
        return true;
    return false;
}

static inline bool pas_thread_local_cache_can_set(void)
{
    if (PAS_THREAD_LOCAL_CACHE_CAN_DETECT_THREAD_EXIT)
        return !pthread_self_is_exiting_np();
    return true;
}
#else /* PAS_HAVE_PTHREAD_PRIVATE -> so !PAS_HAVE_PTHREAD_PRIVATE */
static inline bool pas_thread_local_cache_is_guaranteed_to_destruct(void)
{
    return false;
}

static inline bool pas_thread_local_cache_can_set(void)
{
    return true;
}
#endif /* PAS_HAVE_PTHREAD_PRIVATE -> so end of !PAS_HAVE_PTHREAD_PRIVATE */

static inline void pas_thread_local_cache_set_impl(pas_thread_local_cache* thread_local_cache)
{
    PAS_ASSERT(pas_thread_local_cache_can_set() || pas_thread_local_cache_try_get());
    PAS_FAST_TLS_SET(PAS_THREAD_LOCAL_KEY, &pas_thread_local_cache_fast_tls, thread_local_cache);
}

PAS_API size_t pas_thread_local_cache_size_for_allocator_index_capacity(unsigned allocator_index_capacity);

PAS_API pas_thread_local_cache* pas_thread_local_cache_create(void);

PAS_API void pas_thread_local_cache_destroy(pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_thread_local_cache* pas_thread_local_cache_get_slow(
    pas_heap_config* config, pas_lock_hold_mode heap_lock_hold_mode);

static inline pas_thread_local_cache* pas_thread_local_cache_get_already_initialized(void)
{
    pas_thread_local_cache* result;

    result = pas_thread_local_cache_try_get();

    PAS_ASSERT(result);

    return result;
}

static inline pas_thread_local_cache*
pas_thread_local_cache_get_with_heap_lock_hold_mode(pas_heap_config* config,
                                                    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_thread_local_cache* result;

    result = pas_thread_local_cache_try_get();
    
    if (PAS_LIKELY(result))
        return result;
    
    return pas_thread_local_cache_get_slow(config, heap_lock_hold_mode);
}

static inline pas_thread_local_cache* pas_thread_local_cache_get(pas_heap_config* config)
{
    return pas_thread_local_cache_get_with_heap_lock_hold_mode(config, pas_lock_is_not_held);
}

static inline pas_thread_local_cache* pas_thread_local_cache_get_holding_heap_lock(
    pas_heap_config* config)
{
    return pas_thread_local_cache_get_with_heap_lock_hold_mode(config, pas_lock_is_held);
}

PAS_API pas_local_allocator_result pas_thread_local_cache_get_local_allocator_slow(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index,
    pas_lock_hold_mode heap_lock_hold_mode);

static PAS_ALWAYS_INLINE pas_local_allocator*
pas_thread_local_cache_get_local_allocator_impl(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t allocator_index)
{
    return (pas_local_allocator*)(thread_local_cache->local_allocators + allocator_index);
}

static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_get_local_allocator(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    if (PAS_UNLIKELY(allocator_index >= thread_local_cache->allocator_index_upper_bound)) {
        return pas_thread_local_cache_get_local_allocator_slow(
            thread_local_cache, allocator_index, heap_lock_hold_mode);
    }
    
    return pas_local_allocator_result_create_success(
        pas_thread_local_cache_get_local_allocator_impl(thread_local_cache, allocator_index));
}

static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_try_get_local_allocator(
    pas_thread_local_cache* thread_local_cache,
    unsigned allocator_index)
{
    if (PAS_UNLIKELY(allocator_index >= thread_local_cache->allocator_index_upper_bound))
        return pas_local_allocator_result_create_failure();
    
    return pas_local_allocator_result_create_success(
        pas_thread_local_cache_get_local_allocator_impl(thread_local_cache, allocator_index));
}

PAS_API pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_if_can_set_cache_slow(unsigned allocator_index,
                                                                 pas_heap_config* heap_config);

static PAS_ALWAYS_INLINE pas_local_allocator_result
pas_thread_local_cache_get_local_allocator_if_can_set_cache(unsigned allocator_index,
                                                            pas_heap_config* heap_config)
{
    pas_thread_local_cache* cache;

    cache = pas_thread_local_cache_try_get();

    if (PAS_UNLIKELY(!cache)) {
        return pas_thread_local_cache_get_local_allocator_if_can_set_cache_slow(
            allocator_index, heap_config);
    }

    return pas_thread_local_cache_get_local_allocator(cache, allocator_index, pas_lock_is_not_held);
}

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

/* Returns true if it's possible that we'll be able to return more memory if this was called
   again. */
PAS_API bool pas_thread_local_cache_for_all(pas_allocator_scavenge_action allocator_action,
                                            pas_deallocator_scavenge_action deallocator_action,
                                            pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_thread_local_cache_append_deallocation_slow(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin,
    pas_segregated_page_config_kind kind);

static PAS_ALWAYS_INLINE uintptr_t pas_thread_local_cache_encode_object(
    uintptr_t begin,
    pas_segregated_page_config_kind kind)
{
    return (begin << PAS_SEGREGATED_PAGE_CONFIG_KIND_NUM_BITS) | (uintptr_t)kind;
}

static PAS_ALWAYS_INLINE void pas_thread_local_cache_append_deallocation(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin,
    pas_segregated_page_config_kind kind)
{
    unsigned index;

    index = thread_local_cache->deallocation_log_index;
    if (PAS_UNLIKELY(index >= PAS_DEALLOCATION_LOG_SIZE - 1)) {
        pas_thread_local_cache_append_deallocation_slow(thread_local_cache, begin, kind);
        return;
    }
    
    thread_local_cache->deallocation_log[index++] = pas_thread_local_cache_encode_object(begin, kind);
    thread_local_cache->deallocation_log_index = index;
}

static PAS_ALWAYS_INLINE void pas_thread_local_cache_append_deallocation_with_size(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin, size_t object_size,
    pas_segregated_page_config_kind kind)
{
    size_t new_num_logged_bytes;

    new_num_logged_bytes = thread_local_cache->num_logged_bytes + object_size;
    if (new_num_logged_bytes > PAS_DEALLOCATION_LOG_MAX_BYTES) {
        pas_thread_local_cache_append_deallocation_slow(thread_local_cache, begin, kind);
        return;
    }

    thread_local_cache->num_logged_bytes = new_num_logged_bytes;
    
    pas_thread_local_cache_append_deallocation(thread_local_cache, begin, kind);
}

PAS_API void pas_thread_local_cache_shrink(pas_thread_local_cache* thread_local_cache,
                                           pas_lock_hold_mode heap_lock_hold_mode);

PAS_END_EXTERN_C;

#endif /* PAS_THREAD_LOCAL_CACHE_H */

