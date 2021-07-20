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

#ifndef PAS_SEGREGATED_PAGE_INLINES_H
#define PAS_SEGREGATED_PAGE_INLINES_H

#include "pas_config.h"
#include "pas_log.h"
#include "pas_magazine.h"
#include "pas_page_base_inlines.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_page.h"
#include "pas_segregated_partial_view.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_segregated_shared_handle.h"
#include "pas_segregated_shared_handle_inlines.h"
#include "pas_segregated_view_inlines.h"

PAS_BEGIN_EXTERN_C;

/* OK to pass NULL page if you pass non-NULL directory. */
static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_offset_from_page_boundary_to_first_object(
    pas_segregated_page* page,
    pas_segregated_global_size_directory* directory,
    pas_segregated_page_config page_config)
{
    uintptr_t result;

    PAS_ASSERT(page_config.base.is_enabled);
    
    if (!page || pas_segregated_view_is_exclusive_ish(page->owner)) {
        if (!page)
            PAS_ASSERT(directory);
        else {
            directory = pas_compact_segregated_global_size_directory_ptr_load_non_null(
                &pas_segregated_view_get_exclusive(page->owner)->directory);
        }

        return pas_segregated_global_size_directory_data_ptr_load_non_null(&directory->data)
            ->offset_from_page_boundary_to_first_object;
    }

    result = pas_round_up_to_power_of_2(
        page_config.base.page_object_payload_offset,
        pas_segregated_page_config_min_align(page_config));

    PAS_ASSERT((unsigned)result == result);
    return (unsigned)result;
}

/* Returns true if it covered everything. OK to pass NULL page. Passing NULL for the directory
   means that we're interested in leaving zeroes where the objects go, which is useful for
   bringing up shared views. */
static PAS_ALWAYS_INLINE bool pas_segregated_page_initialize_full_use_counts(
    pas_segregated_page* page,
    pas_segregated_global_size_directory* directory,
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
    pas_segregated_global_size_directory_data* data;

    PAS_ASSERT(page_config.base.is_enabled);
    
    num_granules = page_config.base.page_size / page_config.base.granule_size;
    PAS_ASSERT(end_granule_index <= num_granules);

    end_granule_offset = end_granule_index * page_config.base.granule_size;

    pas_zero_memory(use_counts, end_granule_index * sizeof(pas_page_granule_use_count));
    
    /* If there are any bytes in the page not made available for allocation then make sure
       that the use counts know about it. */
    start_of_payload =
        page_config.base.page_object_payload_offset;
    end_of_payload =
        page_config.base.page_object_payload_offset + page_config.base.page_object_payload_size;

    if (start_of_payload) {
        if (!end_granule_offset)
            return false;
        
        pas_page_granule_increment_uses_for_range(
            use_counts, 0, PAS_MIN(start_of_payload, end_granule_offset),
            page_config.base.page_size, page_config.base.granule_size);
    }

    if (start_of_payload > end_granule_offset)
        return false;

    if (!directory)
        return end_granule_offset == page_config.base.page_size;

    data = pas_segregated_global_size_directory_data_ptr_load_non_null(&directory->data);

    start_of_first_object =
        pas_segregated_page_offset_from_page_boundary_to_first_object(
            page, directory, page_config);
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

static PAS_ALWAYS_INLINE bool pas_segregated_page_switch_lock_with_mode(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_lock_lock_mode lock_mode,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return true;
    }

    switch (lock_mode) {
    case pas_lock_lock_mode_try_lock:
        return pas_lock_switch_with_mode(held_lock, page->lock_ptr, pas_lock_lock_mode_try_lock);

    case pas_lock_lock_mode_lock: {
        pas_lock* held_lock_value;
        pas_lock* page_lock;

        held_lock_value = *held_lock;
        page_lock = page->lock_ptr;
        
        if (PAS_LIKELY(held_lock_value == page_lock)) {
            if (verbose)
                pas_log("Getting the same lock as before (%p).\n", page_lock);
            return true;
        }

        *held_lock = pas_segregated_page_switch_lock_slow(page, held_lock_value, page_lock);
        return true;
    } }
    PAS_ASSERT(!"Should not be reached");
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

PAS_API void pas_segregated_page_switch_lock_and_rebias_to_magazine_while_ineligible_impl(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_magazine* magazine);

static PAS_ALWAYS_INLINE void
pas_segregated_page_switch_lock_and_rebias_to_magazine_while_ineligible(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_magazine* magazine,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_compiler_fence();
        return;
    }

    pas_segregated_page_switch_lock_and_rebias_to_magazine_while_ineligible_impl(
        page, held_lock, magazine);
}

PAS_API void pas_segregated_page_verify_granules(pas_segregated_page* page);

PAS_API PAS_NO_RETURN PAS_USED void pas_segregated_page_deallocation_did_fail(uintptr_t begin);

static PAS_ALWAYS_INLINE void
pas_segregated_page_deallocate_with_page(pas_segregated_page* page,
                                         uintptr_t begin,
                                         pas_segregated_page_config page_config)
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
                count_partial, count_exclusive);
    }
    
    word = page->alloc_bits[word_index];

    if (page_config.check_deallocation) {
#if !PAS_ARM
        new_word = word;
        
        asm volatile (
            "btrl %1, %0\n\t"
            "jc 0f\n\t"
            "movq %2, %%rdi\n\t"
            "call _pas_segregated_page_deallocation_did_fail\n\t"
            "0:"
            : "+r"(new_word)
            : "r"((unsigned)bit_index_unmasked), "r"(begin)
            : "memory");
#else /* !PAS_ARM -> so PAS_ARM */
        unsigned bit_mask;
        bit_mask = PAS_BITVECTOR_BIT_MASK(bit_index_unmasked);
        
        if (PAS_UNLIKELY(!(word & bit_mask)))
            pas_segregated_page_deallocation_did_fail(begin);
        
        new_word = word & ~bit_mask;
#endif /* !PAS_ARM -> so end of PAS_ARM */
    } else
        new_word = word & ~PAS_BITVECTOR_BIT_MASK(bit_index_unmasked);
    
    page->alloc_bits[word_index] = new_word;

    if (verbose)
        pas_log("at word_index = %zu, new_word = %u\n", word_index, new_word);

    if (!page_config.enable_empty_word_eligibility_optimization || !new_word) {
        pas_segregated_view owner;
        owner = page->owner;
        
        if (!pas_segregated_view_is_eligible_kind(owner)) {
            if (pas_segregated_view_is_exclusive_ish(owner)) {
                if (verbose)
                    pas_log("Notifying exclusive-ish eligibility on view %p.\n", owner);
                pas_segregated_view_note_eligibility(owner, page);
            } else {
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
            }
        }
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

        if (pas_segregated_view_is_exclusive_ish(owner))
            object_size = page->object_size;
        else {
            object_size = pas_compact_segregated_global_size_directory_ptr_load_non_null(
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
    pas_segregated_page_config page_config)
{
    pas_segregated_page* page;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    page = pas_segregated_page_for_address_and_page_config(begin, page_config);
    pas_segregated_page_switch_lock(page, held_lock, page_config);
    pas_segregated_page_deallocate_with_page(page, begin, page_config);
}

static PAS_ALWAYS_INLINE bool
pas_segregated_page_qualifies_for_decommit(
    pas_segregated_page* page,
    pas_segregated_page_config page_config)
{
    pas_page_granule_use_count* use_counts;
    uintptr_t num_granules;
    uintptr_t granule_index;
    
    PAS_ASSERT(page_config.base.is_enabled);

    if (!page->num_non_empty_words)
        return true;
    
    if (page_config.base.page_size == page_config.base.granule_size)
        return false;
    
    use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
    num_granules = page_config.base.page_size / page_config.base.granule_size;
    
    for (granule_index = num_granules; granule_index--;) {
        if (!use_counts[granule_index])
            return true;
    }

    return false;
}

static PAS_ALWAYS_INLINE pas_segregated_global_size_directory*
pas_segregated_page_get_directory_for_address_in_page(pas_segregated_page* page,
                                                      uintptr_t begin,
                                                      pas_segregated_page_config page_config)
{
    pas_segregated_view owning_view;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    owning_view = page->owner;

    switch (pas_segregated_view_get_kind(owning_view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(owning_view)->directory);

    case pas_segregated_shared_handle_kind:
        return pas_compact_segregated_global_size_directory_ptr_load(
            &pas_segregated_shared_handle_partial_view_for_object(
                pas_segregated_view_get_shared_handle(owning_view), begin, page_config)->directory);

    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

static PAS_ALWAYS_INLINE pas_segregated_global_size_directory*
pas_segregated_page_get_directory_for_address_and_page_config(uintptr_t begin,
                                                              pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    return pas_segregated_page_get_directory_for_address_in_page(
        pas_segregated_page_for_address_and_page_config(begin, page_config),
        begin, page_config);
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_get_object_size_for_address_in_page(pas_segregated_page* page,
                                                        uintptr_t begin,
                                                        pas_segregated_page_config page_config)
{
    pas_segregated_view owning_view;
    
    PAS_ASSERT(page_config.base.is_enabled);
    
    owning_view = page->owner;

    switch (pas_segregated_view_get_kind(owning_view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return page->object_size;

    case pas_segregated_shared_handle_kind:
        return pas_compact_segregated_global_size_directory_ptr_load(
            &pas_segregated_shared_handle_partial_view_for_object(
                pas_segregated_view_get_shared_handle(owning_view),
                begin, page_config)->directory)->object_size;

    default:
        PAS_ASSERT(!"Should not be reached");
        return 0;
    }
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_get_object_size_for_address_and_page_config(
    uintptr_t begin,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    return pas_segregated_page_get_object_size_for_address_in_page(
        pas_segregated_page_for_address_and_page_config(begin, page_config),
        begin, page_config);
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_INLINES_H */

