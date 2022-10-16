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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_deallocate.h"

#include "pas_debug_heap.h"
#include "pas_malloc_stack_logging.h"
#include "pas_scavenger.h"
#include "pas_segregated_page_inlines.h"

bool pas_try_deallocate_known_large(void* ptr,
                                    const pas_heap_config* config,
                                    pas_deallocation_mode deallocation_mode)
{
    uintptr_t begin;

    begin = (uintptr_t)ptr;
    
    pas_heap_lock_lock();
    
    if (!pas_large_heap_try_deallocate(begin, config)) {
        switch (deallocation_mode) {
        case pas_try_deallocate_mode:
            pas_heap_lock_unlock();
            return false;

        case pas_deallocate_mode:
            pas_deallocation_did_fail("Large heap did not find object", begin);
            break;
        }
        PAS_ASSERT(!"Should not be reached");
    }
    
    pas_heap_lock_unlock();
    
    pas_scavenger_notify_eligibility_if_needed();
    return true;
}

void pas_deallocate_known_large(void* ptr,
                                const pas_heap_config* config)
{
    pas_try_deallocate_known_large(ptr, config, pas_deallocate_mode);
}

bool pas_try_deallocate_slow(uintptr_t begin,
                             const pas_heap_config* config,
                             pas_deallocation_mode deallocation_mode)
{
    if (!begin)
        return true;
    
    return pas_try_deallocate_known_large((void*)begin, config, deallocation_mode);
}

static void deallocate_segregated(uintptr_t begin,
                                  const pas_segregated_page_config* page_config,
                                  pas_segregated_page_role role)
{
    pas_lock* held_lock;
    held_lock = NULL;
    pas_segregated_page_deallocate(
        begin, &held_lock, pas_segregated_deallocation_direct_mode, NULL, *page_config, role);
    pas_lock_switch(&held_lock, NULL);
}

bool pas_try_deallocate_slow_no_cache(void* ptr,
                                      const pas_heap_config* config_ptr,
                                      pas_deallocation_mode deallocation_mode)
{
    static const bool verbose = false;
    
    uintptr_t begin;

    if (verbose)
        pas_log("Trying to deallocate %p.\n", ptr);
    if (PAS_UNLIKELY(pas_debug_heap_is_enabled(config_ptr->kind))) {
        if (verbose)
            pas_log("Deallocating %p with debug heap.\n", ptr);
        PAS_ASSERT(deallocation_mode == pas_deallocate_mode);
        pas_debug_heap_free(ptr);
        return true;
    }
    pas_msl_free_logging(ptr);

    if (verbose)
        pas_log("Deallocating %p normally.\n", ptr);
    
    if (pas_thread_local_cache_can_set()) {
        /* Make sure that future calls to this get a TLC. But the body of this slow path also needs to
           handle the cases where we cannot get a TLC, like if the thread is exiting. */
        pas_thread_local_cache_get(config_ptr);
    }
    
    begin = (uintptr_t)ptr;
        
    switch (config_ptr->fast_megapage_kind_func(begin)) {
    case pas_small_exclusive_segregated_fast_megapage_kind: {
        deallocate_segregated(begin, &config_ptr->small_segregated_config, pas_segregated_page_exclusive_role);
        return true;
    }

    case pas_small_other_fast_megapage_kind: {
        pas_page_base_and_kind page_and_kind;
        page_and_kind = pas_get_page_base_and_kind_for_small_other_in_fast_megapage(begin, *config_ptr);
        switch (page_and_kind.page_kind) {
        case pas_small_shared_segregated_page_kind:
            deallocate_segregated(begin, &config_ptr->small_segregated_config, pas_segregated_page_shared_role);
            return true;
        case pas_small_bitfit_page_kind:
            config_ptr->small_bitfit_config.specialized_page_deallocate_with_page(
                pas_page_base_get_bitfit(page_and_kind.page_base),
                begin);
            return true;
        default:
            PAS_ASSERT(!"Should not be reached");
            return false;
        }
    }

    case pas_not_a_fast_megapage_kind: {
        pas_page_base* page_base;
            
        page_base = config_ptr->page_header_func(begin);
        if (page_base) {
            switch (pas_page_base_get_kind(page_base)) {
            case pas_small_shared_segregated_page_kind:
                PAS_ASSERT(!config_ptr->small_segregated_is_in_megapage);
                deallocate_segregated(begin, &config_ptr->small_segregated_config,
                                      pas_segregated_page_shared_role);
                return true;
            case pas_small_exclusive_segregated_page_kind:
                PAS_ASSERT(!config_ptr->small_segregated_is_in_megapage);
                deallocate_segregated(begin, &config_ptr->small_segregated_config,
                                      pas_segregated_page_exclusive_role);
                return true;
            case pas_small_bitfit_page_kind:
                PAS_ASSERT(!config_ptr->small_bitfit_is_in_megapage);
                config_ptr->small_bitfit_config.specialized_page_deallocate_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin);
                return true;
            case pas_medium_shared_segregated_page_kind:
                deallocate_segregated(begin, &config_ptr->medium_segregated_config,
                                      pas_segregated_page_shared_role);
                return true;
            case pas_medium_exclusive_segregated_page_kind:
                deallocate_segregated(begin, &config_ptr->medium_segregated_config,
                                      pas_segregated_page_exclusive_role);
                return true;
            case pas_medium_bitfit_page_kind:
                config_ptr->medium_bitfit_config.specialized_page_deallocate_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin);
                return true;
            case pas_marge_bitfit_page_kind:
                config_ptr->marge_bitfit_config.specialized_page_deallocate_with_page(
                    pas_page_base_get_bitfit(page_base),
                    begin);
                return true;
            default:
                PAS_ASSERT(!"Wrong page kind");
                return false;
            }
        }

        return pas_try_deallocate_slow(begin, config_ptr, deallocation_mode);
    } }

    PAS_ASSERT(!"Should not be reached");
    return false;
}

#endif /* LIBPAS_ENABLED */
