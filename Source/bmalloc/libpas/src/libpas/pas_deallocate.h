/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_DEALLOCATE_H
#define PAS_DEALLOCATE_H

#include "pas_bitfit_page_inlines.h"
#include "pas_deallocation_mode.h"
#include "pas_debug_heap.h"
#include "pas_get_page_base_and_kind_for_small_other_in_fast_megapage.h"
#include "pas_heap_config.h"
#include "pas_heap_lock.h"
#include "pas_heap_ref.h"
#include "pas_large_heap.h"
#include "pas_malloc_stack_logging.h"
#include "pas_segregated_page_inlines.h"
#include "pas_thread_local_cache.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

PAS_API bool pas_try_deallocate_slow(uintptr_t begin,
                                     const pas_heap_config* config,
                                     pas_deallocation_mode deallocation_mode);

PAS_API bool pas_try_deallocate_slow_no_cache(void* ptr,
                                              const pas_heap_config* config_ptr,
                                              pas_deallocation_mode deallocation_mode);

static PAS_ALWAYS_INLINE void
pas_deallocate_known_segregated(void* ptr,
                                pas_segregated_page_config page_config,
                                pas_segregated_page_role role)
{
    pas_thread_local_cache* thread_local_cache;

    thread_local_cache = pas_thread_local_cache_try_get();
    if (!thread_local_cache) {
        pas_try_deallocate_slow_no_cache(ptr, page_config.base.heap_config_ptr, pas_deallocate_mode);
        return;
    }
    
    pas_segregated_page_log_or_deallocate((uintptr_t)ptr, thread_local_cache, page_config, role);
}

PAS_API bool pas_try_deallocate_known_large(void* ptr,
                                            const pas_heap_config* config,
                                            pas_deallocation_mode deallocation_mode);

PAS_API void pas_deallocate_known_large(void* ptr,
                                        const pas_heap_config* config);

static PAS_ALWAYS_INLINE bool pas_try_deallocate_not_small_exclusive_segregated(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin,
    pas_heap_config config,
    pas_deallocation_mode deallocation_mode,
    pas_fast_megapage_kind megapage_kind)
{
    pas_page_base* page_base;

    if (PAS_LIKELY(megapage_kind == pas_small_other_fast_megapage_kind)) {
        pas_page_base_and_kind page_and_kind;
        page_and_kind = pas_get_page_base_and_kind_for_small_other_in_fast_megapage(begin, config);
        switch (page_and_kind.page_kind) {
        case pas_small_shared_segregated_page_kind:
            pas_segregated_page_log_or_deallocate(
                begin, thread_local_cache, config.small_segregated_config, pas_segregated_page_shared_role);
            return true;
        case pas_small_bitfit_page_kind:
            config.small_bitfit_config.specialized_page_deallocate_with_page(
                pas_page_base_get_bitfit(page_and_kind.page_base),
                begin);
            return true;
        default:
            PAS_ASSERT(!"Should not be reached");
            return false;
        }
    }

    if (pas_debug_heap_is_enabled(config.kind)) {
        pas_debug_heap_free((void*)begin);
        return true;
    }
    pas_msl_free_logging((void*)begin);

    page_base = config.page_header_func(begin);
    if (page_base) {
        switch (pas_page_base_get_kind(page_base)) {
        case pas_small_shared_segregated_page_kind:
            PAS_ASSERT(!config.small_segregated_is_in_megapage);
            pas_segregated_page_log_or_deallocate(
                begin, thread_local_cache, config.small_segregated_config, pas_segregated_page_shared_role);
            return true;
        case pas_small_exclusive_segregated_page_kind:
            PAS_ASSERT(!config.small_segregated_is_in_megapage);
            pas_segregated_page_log_or_deallocate(
                begin, thread_local_cache, config.small_segregated_config, pas_segregated_page_exclusive_role);
            return true;
        case pas_small_bitfit_page_kind:
            PAS_ASSERT(!config.small_bitfit_is_in_megapage);
            config.small_bitfit_config.specialized_page_deallocate_with_page(
                pas_page_base_get_bitfit(page_base),
                begin);
            return true;
        case pas_medium_shared_segregated_page_kind:
            pas_segregated_page_log_or_deallocate(
                begin, thread_local_cache, config.medium_segregated_config, pas_segregated_page_shared_role);
            return true;
        case pas_medium_exclusive_segregated_page_kind:
            pas_segregated_page_log_or_deallocate(
                begin, thread_local_cache, config.medium_segregated_config, pas_segregated_page_exclusive_role);
            return true;
        case pas_medium_bitfit_page_kind:
            config.medium_bitfit_config.specialized_page_deallocate_with_page(
                pas_page_base_get_bitfit(page_base),
                begin);
            return true;
        case pas_marge_bitfit_page_kind:
            config.marge_bitfit_config.specialized_page_deallocate_with_page(
                pas_page_base_get_bitfit(page_base),
                begin);
            return true;
        }
        PAS_ASSERT(!"Wrong page kind");
        return false;
    }
    
    return pas_try_deallocate_slow(begin, config.config_ptr, deallocation_mode);
}

static PAS_ALWAYS_INLINE bool pas_try_deallocate_impl(pas_thread_local_cache* thread_local_cache,
                                                      void* ptr,
                                                      pas_heap_config config,
                                                      pas_deallocation_mode deallocation_mode)
{
    uintptr_t begin;
    pas_fast_megapage_kind megapage_kind;

    begin = (uintptr_t)ptr;
    
    megapage_kind = config.fast_megapage_kind_func(begin);
    if (PAS_LIKELY(megapage_kind == pas_small_exclusive_segregated_fast_megapage_kind)) {
        pas_segregated_page_log_or_deallocate(
            begin, thread_local_cache, config.small_segregated_config, pas_segregated_page_exclusive_role);
        return true;
    }

    return config.specialized_try_deallocate_not_small_exclusive_segregated(
        thread_local_cache, begin, deallocation_mode, megapage_kind);
}

/* This returns true if the object was successfully deallocated.
   
   This returns false if the object pointer definitely does not into any heap for this config.
   
   This may crash or return false if the object pointer points into a heap of this config, but not
   to a valid object start address. Specifically, this currently will crash if it's an invalid
   object pointer with an address inside pages managed by segregated or bitfit, but returns false
   if the address points inside pages managed by the large heap.

   Passing deallocation_mode = pas_deallocate_mode means that the false cases are crashes
   instead. */
static PAS_ALWAYS_INLINE bool pas_try_deallocate(void* ptr,
                                                 pas_heap_config config,
                                                 pas_deallocation_mode deallocation_mode)
{
    static const bool verbose = false;
    
    pas_thread_local_cache* thread_local_cache;

    if (verbose)
        pas_log("try_deallocate for %p\n", ptr);
    
    thread_local_cache = pas_thread_local_cache_try_get();
    if (PAS_UNLIKELY(!thread_local_cache)) {
        if (verbose)
            pas_log("Going slow because no TLC.\n");
        return pas_try_deallocate_slow_no_cache(ptr, config.config_ptr, deallocation_mode);
    }

    if (verbose)
        pas_log("Going fast because have TLC = %p\n", thread_local_cache);
    
    return pas_try_deallocate_impl(thread_local_cache, ptr, config, deallocation_mode);
}

static PAS_ALWAYS_INLINE void pas_deallocate(void* ptr,
                                             pas_heap_config config)
{
    pas_try_deallocate(ptr, config, pas_deallocate_mode);
}

PAS_END_EXTERN_C;

#endif /* PAS_DEALLOCATE_H */

