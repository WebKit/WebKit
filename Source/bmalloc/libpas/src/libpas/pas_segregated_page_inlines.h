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

#ifndef PAS_SEGREGATED_PAGE_INLINES_H
#define PAS_SEGREGATED_PAGE_INLINES_H

#include "pas_config.h"
#include "pas_log.h"
#include "pas_page_base_inlines.h"
#include "pas_segregated_deallocation_mode.h"
#include "pas_segregated_exclusive_view_inlines.h"
#include "pas_segregated_page.h"
#include "pas_segregated_partial_view.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_shared_handle.h"
#include "pas_segregated_shared_handle_inlines.h"
#include "pas_thread_local_cache_node.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE bool pas_segregated_page_initialize_full_use_counts(
    pas_segregated_size_directory* directory,
    pas_page_granule_use_count* use_counts,
    uintptr_t end_granule_index,
    pas_segregated_page_config page_config)
{
    uintptr_t num_granules;
    uintptr_t end_granule_offset;
    uintptr_t start_of_payload;
    uintptr_t end_of_payload;
    uintptr_t start_of_first_object;
    uintptr_t end_of_last_object;
    uintptr_t object_size;
    uintptr_t offset;
    pas_segregated_size_directory_data* data;

    PAS_ASSERT(page_config.base.is_enabled);
    PAS_ASSERT(directory);
    
    num_granules = page_config.base.page_size / page_config.base.granule_size;
    PAS_ASSERT(end_granule_index <= num_granules);

    end_granule_offset = end_granule_index * page_config.base.granule_size;

    pas_zero_memory(use_counts, end_granule_index * sizeof(pas_page_granule_use_count));
    
    /* If there are any bytes in the page not made available for allocation then make sure
       that the use counts know about it. */
    start_of_payload =
        page_config.exclusive_payload_offset;
    end_of_payload =
        pas_segregated_page_config_payload_end_offset_for_role(
            page_config, pas_segregated_page_exclusive_role);

    if (start_of_payload) {
        if (!end_granule_offset)
            return false;
        
        pas_page_granule_increment_uses_for_range(
            use_counts, 0, PAS_MIN(start_of_payload, end_granule_offset),
            page_config.base.page_size, page_config.base.granule_size);
    }

    if (start_of_payload > end_granule_offset)
        return false;

    data = pas_segregated_size_directory_data_ptr_load_non_null(&directory->data);

    start_of_first_object = data->offset_from_page_boundary_to_first_object;
    end_of_last_object = data->offset_from_page_boundary_to_end_of_last_object;

    object_size = directory->object_size;
    
    for (offset = start_of_first_object; offset < end_of_last_object; offset += object_size) {
        if (offset >= end_granule_offset)
            return false;
        
        pas_page_granule_increment_uses_for_range(
            use_counts, offset, PAS_MIN(offset + object_size, end_granule_offset),
            page_config.base.page_size, page_config.base.granule_size);

        if (offset + object_size > end_granule_offset)
            return false;
    }

    if (page_config.base.page_size > end_of_payload) {
        if (end_of_payload >= end_granule_offset)
            return false;
        
        pas_page_granule_increment_uses_for_range(
            use_counts, end_of_payload, PAS_MIN(page_config.base.page_size, end_granule_offset),
            page_config.base.page_size, page_config.base.granule_size);

        if (page_config.base.page_size > end_granule_offset)
            return false;
    }
    
    return true;
}

PAS_API bool pas_segregated_page_lock_with_unbias_impl(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock* lock_ptr);

static PAS_ALWAYS_INLINE bool pas_segregated_page_lock_with_unbias_not_utility(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock* lock_ptr)
{
    PAS_TESTING_ASSERT(lock_ptr);
    
    *held_lock = lock_ptr;
    
    if (PAS_LIKELY(pas_lock_try_lock(lock_ptr)))
        return lock_ptr == page->lock_ptr;

    return pas_segregated_page_lock_with_unbias_impl(page, held_lock, lock_ptr);
}

static PAS_ALWAYS_INLINE bool pas_segregated_page_lock_with_unbias(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock* lock_ptr,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return true;
    }

    PAS_TESTING_ASSERT(lock_ptr);

    return pas_segregated_page_lock_with_unbias_not_utility(page, held_lock, lock_ptr);
}

static PAS_ALWAYS_INLINE void pas_segregated_page_lock(
    pas_segregated_page* page,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return;
    }
    
    for (;;) {
        pas_lock* lock_ptr;
        pas_lock* held_lock_ignored;
        
        lock_ptr = page->lock_ptr;

        if (pas_segregated_page_lock_with_unbias(page, &held_lock_ignored, lock_ptr, page_config))
            return;
        
        pas_lock_unlock(lock_ptr);
    }
}

static PAS_ALWAYS_INLINE void pas_segregated_page_unlock(
    pas_segregated_page* page,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return;
    }
    
    pas_lock_unlock(page->lock_ptr);
}

PAS_API pas_lock* pas_segregated_page_switch_lock_slow(
    pas_segregated_page* page,
    pas_lock* held_lock,
    pas_lock* page_lock);

static PAS_ALWAYS_INLINE void pas_segregated_page_switch_lock_impl(
    pas_segregated_page* page,
    pas_lock** held_lock)
{
    static const bool verbose = false;
    
    pas_lock* held_lock_value;
    pas_lock* page_lock;
    
    held_lock_value = *held_lock;
    page_lock = page->lock_ptr;

    PAS_TESTING_ASSERT(page_lock);
    
    if (PAS_LIKELY(held_lock_value == page_lock)) {
        if (verbose)
            pas_log("Getting the same lock as before (%p).\n", page_lock);
        return;
    }
    
    *held_lock = pas_segregated_page_switch_lock_slow(page, held_lock_value, page_lock);
}

static PAS_ALWAYS_INLINE bool pas_segregated_page_switch_lock_with_mode(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock_lock_mode lock_mode,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return true;
    }

    switch (lock_mode) {
    case pas_lock_lock_mode_try_lock: {
        pas_lock* page_lock;
        pas_compiler_fence();
        page_lock = page->lock_ptr;
        PAS_TESTING_ASSERT(page_lock);
        if (*held_lock == page_lock) {
            pas_compiler_fence();
            return true;
        }
        pas_compiler_fence();
        if (*held_lock)
            pas_lock_unlock(*held_lock);
        pas_compiler_fence();
        for (;;) {
            pas_lock* new_page_lock;
            if (!pas_lock_try_lock(page_lock)) {
                *held_lock = NULL;
                pas_compiler_fence();
                return false;
            }
            new_page_lock = page->lock_ptr;
            if (page_lock == new_page_lock) {
                *held_lock = page_lock;
                pas_compiler_fence();
                return true;
            }
            pas_lock_unlock(page_lock);
            page_lock = new_page_lock;
        }
    }

    case pas_lock_lock_mode_lock: {
        pas_segregated_page_switch_lock_impl(page, held_lock);
        return true;
    } }
    PAS_ASSERT(!"Should not be reached");
    return true;
}

static PAS_ALWAYS_INLINE void pas_segregated_page_switch_lock(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_segregated_page_config page_config)
{
    bool result;

    PAS_ASSERT(page_config.base.is_enabled);
    
    result = pas_segregated_page_switch_lock_with_mode(
        page, held_lock, pas_lock_lock_mode_lock, page_config);
    PAS_ASSERT(result);
}

PAS_API void pas_segregated_page_switch_lock_and_rebias_while_ineligible_impl(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_thread_local_cache_node* cache_node);

static PAS_ALWAYS_INLINE void
pas_segregated_page_switch_lock_and_rebias_while_ineligible(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_thread_local_cache_node* cache_node,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return;
    }

    pas_segregated_page_switch_lock_and_rebias_while_ineligible_impl(
        page, held_lock, cache_node);
}

PAS_API void pas_segregated_page_verify_granules(pas_segregated_page* page);

PAS_API PAS_NO_RETURN PAS_USED void pas_segregated_page_deallocation_did_fail(uintptr_t begin);

static PAS_ALWAYS_INLINE void
pas_segregated_page_deallocate_with_page(pas_segregated_page* page,
                                         uintptr_t begin,
                                         pas_segregated_deallocation_mode deallocation_mode,
                                         pas_thread_local_cache* thread_local_cache,
                                         pas_segregated_page_config page_config,
                                         pas_segregated_page_role role)
{
    static const bool verbose = false;
    static const bool count_things = false;

    static uint64_t count_exclusive;
    static uint64_t count_partial;
    
    size_t bit_index_unmasked;
    size_t word_index;
    
    unsigned word;
    unsigned new_word;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (verbose) {
        pas_log("Freeing %p in %p(%s), num_non_empty_words = %u\n",
                (void*)begin, page,
                pas_segregated_page_config_kind_get_string(page_config.kind),
                page->num_non_empty_words);
    }

    bit_index_unmasked = begin >> page_config.base.min_align_shift;
    
    word_index = pas_modulo_power_of_2(
        (begin >> (page_config.base.min_align_shift + PAS_BITVECTOR_WORD_SHIFT)),
        page_config.base.page_size >> (page_config.base.min_align_shift + PAS_BITVECTOR_WORD_SHIFT));

    if (count_things) {
        pas_segregated_view owner;
        owner = page->owner;
        if (pas_segregated_view_is_shared_handle(owner))
            count_partial++;
        else
            count_exclusive++;
        pas_log("frees in partial = %llu, frees in exclusive = %llu.\n",
                (unsigned long long)count_partial, (unsigned long long)count_exclusive);
    }
    
    word = page->alloc_bits[word_index];

    if (page_config.check_deallocation) {
#if !PAS_ARM && !PAS_RISCV
        new_word = word;
        
        asm volatile (
            "btrl %1, %0\n\t"
            "jc 0f\n\t"
            "movq %2, %%rdi\n\t"
#if PAS_OS(DARWIN)
            "call _pas_segregated_page_deallocation_did_fail\n\t"
#else
            "call pas_segregated_page_deallocation_did_fail\n\t"
#endif
            "0:"
            : "+r"(new_word)
            : "r"((unsigned)bit_index_unmasked), "r"(begin)
            : "memory");
#else /* !PAS_ARM && !PAS_RISCV -> so PAS_ARM or PAS_RISCV */
        unsigned bit_mask;
        bit_mask = PAS_BITVECTOR_BIT_MASK(bit_index_unmasked);
        
        if (PAS_UNLIKELY(!(word & bit_mask)))
            pas_segregated_page_deallocation_did_fail(begin);
        
        new_word = word & ~bit_mask;
#endif /* !PAS_ARM && !PAS_RISCV-> so end of PAS_ARM or PAS_RISCV */
    } else
        new_word = word & ~PAS_BITVECTOR_BIT_MASK(bit_index_unmasked);
    
    page->alloc_bits[word_index] = new_word;

    if (verbose)
        pas_log("at word_index = %zu, new_word = %u\n", word_index, new_word);

    if (!pas_segregated_page_config_enable_empty_word_eligibility_optimization_for_role(page_config, role)
        || !new_word) {
        pas_segregated_view owner;
        owner = page->owner;

        switch (role) {
        case pas_segregated_page_exclusive_role: {
            if (pas_segregated_view_get_kind(owner) != pas_segregated_exclusive_view_kind) {
                PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(owner));
                if (verbose)
                    pas_log("Notifying exclusive-ish eligibility on view %p.\n", owner);
                /* NOTE: If this decides to cache the view then it's possible that we will release and
                   then reacquire this page's lock. */
                pas_segregated_exclusive_view_note_eligibility(
                    pas_segregated_view_get_exclusive(owner),
                    page, deallocation_mode, thread_local_cache, page_config);
            }
            break;
        }
            
        case pas_segregated_page_shared_role: {
            pas_segregated_shared_handle* shared_handle;
            pas_segregated_partial_view* partial_view;
            size_t offset_in_page;
            size_t bit_index;
            
            offset_in_page = pas_modulo_power_of_2(begin, page_config.base.page_size);
            bit_index = offset_in_page >> page_config.base.min_align_shift;
            
            shared_handle = pas_segregated_view_get_shared_handle(owner);
            
            partial_view = pas_segregated_shared_handle_partial_view_for_index(
                shared_handle, bit_index, page_config);
            
            if (verbose)
                pas_log("Notifying partial eligibility on view %p.\n", partial_view);
            
            if (!partial_view->eligibility_has_been_noted)
                pas_segregated_partial_view_note_eligibility(partial_view, page);
            break;
        } }
    }

    if (page_config.base.page_size > page_config.base.granule_size) {
        /* This is the partial decommit case. It's intended for medium pages. It requires doing
           more work, but it's a bounded amount of work, and it only happens when freeing
           medium objects. */

        uintptr_t object_size;
        pas_segregated_view owner;
        size_t offset_in_page;
        size_t bit_index;
        bool did_find_empty_granule;

        offset_in_page = pas_modulo_power_of_2(begin, page_config.base.page_size);
        bit_index = offset_in_page >> page_config.base.min_align_shift;
        owner = page->owner;

        if (pas_segregated_view_is_some_exclusive(owner))
            object_size = page->object_size;
        else {
            object_size = pas_compact_segregated_size_directory_ptr_load_non_null(
                &pas_segregated_shared_handle_partial_view_for_index(
                    pas_segregated_view_get_shared_handle(owner),
                    bit_index,
                    page_config)->directory)->object_size;
        }

        did_find_empty_granule = pas_page_base_free_granule_uses_in_range(
            pas_segregated_page_get_granule_use_counts(page, page_config),
            offset_in_page,
            offset_in_page + object_size,
            page_config.base);

        if (pas_segregated_page_deallocate_should_verify_granules)
            pas_segregated_page_verify_granules(page);

        if (did_find_empty_granule)
            pas_segregated_page_note_emptiness(page);
    }
    
    if (!new_word) {
        PAS_TESTING_ASSERT(page->num_non_empty_words);
        if (!--page->num_non_empty_words) {
            /* This has to happen last since it effectively unlocks the lock. That's due to
               the things that happen in switch_lock_and_try_to_take_bias. Specifically, its
               reliance on the fully_empty bit. */
            pas_segregated_page_note_emptiness(page);
        }
    }
}

static PAS_ALWAYS_INLINE void pas_segregated_page_deallocate(
    uintptr_t begin,
    pas_lock** held_lock,
    pas_segregated_deallocation_mode deallocation_mode,
    pas_thread_local_cache* thread_local_cache,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    pas_segregated_page* page;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    page = pas_segregated_page_for_address_and_page_config(begin, page_config);
    pas_segregated_page_switch_lock(page, held_lock, page_config);
    pas_segregated_page_deallocate_with_page(
        page, begin, deallocation_mode, thread_local_cache, page_config, role);
}

static PAS_ALWAYS_INLINE pas_segregated_size_directory*
pas_segregated_page_get_directory_for_address_in_page(pas_segregated_page* page,
                                                      uintptr_t begin,
                                                      pas_segregated_page_config page_config,
                                                      pas_segregated_page_role role)
{
    pas_segregated_view owning_view;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    owning_view = page->owner;

    switch (role) {
    case pas_segregated_page_exclusive_role:
        return pas_compact_segregated_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(owning_view)->directory);

    case pas_segregated_page_shared_role:
        return pas_compact_segregated_size_directory_ptr_load(
            &pas_segregated_shared_handle_partial_view_for_object(
                pas_segregated_view_get_shared_handle(owning_view), begin, page_config)->directory);
    }
    
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static PAS_ALWAYS_INLINE pas_segregated_size_directory*
pas_segregated_page_get_directory_for_address_and_page_config(uintptr_t begin,
                                                              pas_segregated_page_config page_config,
                                                              pas_segregated_page_role role)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    return pas_segregated_page_get_directory_for_address_in_page(
        pas_segregated_page_for_address_and_page_config(begin, page_config),
        begin, page_config, role);
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_get_object_size_for_address_in_page(pas_segregated_page* page,
                                                        uintptr_t begin,
                                                        pas_segregated_page_config page_config,
                                                        pas_segregated_page_role role)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    switch (role) {
    case pas_segregated_page_exclusive_role:
        PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(page->owner));
        return page->object_size;

    case pas_segregated_page_shared_role: {
        pas_segregated_view owning_view;
        
        owning_view = page->owner;
    
        return pas_compact_segregated_size_directory_ptr_load(
            &pas_segregated_shared_handle_partial_view_for_object(
                pas_segregated_view_get_shared_handle(owning_view),
                begin, page_config)->directory)->object_size;
    } }
    
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_get_object_size_for_address_and_page_config(
    uintptr_t begin,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    return pas_segregated_page_get_object_size_for_address_in_page(
        pas_segregated_page_for_address_and_page_config(begin, page_config),
        begin, page_config, role);
}

static PAS_ALWAYS_INLINE void pas_segregated_page_log_or_deallocate(
    uintptr_t begin,
    pas_thread_local_cache* cache,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    pas_segregated_deallocation_logging_mode mode;
    
    mode = pas_segregated_page_config_logging_mode_for_role(page_config, role);

    if (!pas_segregated_deallocation_logging_mode_does_logging(mode)) {
        pas_lock* held_lock;
        held_lock = NULL;
        pas_segregated_page_deallocate(
            begin, &held_lock, pas_segregated_deallocation_direct_mode, NULL, page_config, role);
        pas_lock_switch(&held_lock, NULL);
        return;
    }

    /* This check happens here because if logging is disabled then whether we check deallocation is governed
       by the check_deallocation flag. */
    if (pas_segregated_deallocation_logging_mode_is_checked(mode)) {
        if (PAS_UNLIKELY(!pas_segregated_page_is_allocated(begin, page_config)))
            pas_deallocation_did_fail("Page bit not set", begin);
    }

    if (pas_segregated_deallocation_logging_mode_is_size_aware(mode)) {
        pas_thread_local_cache_append_deallocation_with_size(
            cache, begin,
            pas_segregated_page_get_object_size_for_address_and_page_config(begin, page_config, role),
            pas_segregated_page_config_kind_and_role_create(page_config.kind, role));
        return;
    }
    
    pas_thread_local_cache_append_deallocation(
        cache, begin, pas_segregated_page_config_kind_and_role_create(page_config.kind, role));
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_INLINES_H */

