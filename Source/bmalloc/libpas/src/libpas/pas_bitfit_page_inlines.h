/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_BITFIT_PAGE_INLINES_H
#define PAS_BITFIT_PAGE_INLINES_H

#include "pas_bitfit_allocation_result.h"
#include "pas_bitfit_page.h"
#include "pas_bitfit_view.h"
#include "pas_commit_span.h"
#include "pas_heap_config.h"
#include "pas_mte.h"
#include "pas_page_base_inlines.h"
#include "pas_page_sharing_pool.h"
#include "pas_thread.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE uintptr_t pas_bitfit_page_compute_offset(
    uintptr_t word_index, uintptr_t bit_index, pas_bitfit_page_config page_config)
{
    return (PAS_BITVECTOR_BIT_INDEX64(word_index) + bit_index)
        << page_config.base.min_align_shift;
}

static PAS_ALWAYS_INLINE bool pas_bitfit_page_allocation_satisfies_alignment(
    /* The word+bit indices of the start of the allocation. */
    uintptr_t* word_index,
    uintptr_t* fixed_start_bit_index,

    /* The word+bit indices of the end of the allocation. */
    uintptr_t other_word_index,
    uintptr_t fixed_end_bit_index,

    uintptr_t size,
    uintptr_t alignment,
    pas_bitfit_page_config page_config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_BITFIT_HEAPS);
    
    uintptr_t begin_offset;
    uintptr_t end_offset;
    uintptr_t aligned_offset;

    PAS_ASSERT(page_config.base.is_enabled);
    
    if (PAS_LIKELY(alignment <= pas_page_base_config_min_align(page_config.base)))
        return true;

    begin_offset = pas_bitfit_page_compute_offset(*word_index, *fixed_start_bit_index, page_config);
    end_offset = pas_bitfit_page_compute_offset(other_word_index, fixed_end_bit_index, page_config);

    aligned_offset = PAS_ROUND_UP_TO_POWER_OF_2(begin_offset, alignment);

    if (verbose) {
        pas_log("begin_offset = %zu, end_offset = %zu, size = %zu\n",
                begin_offset, end_offset, size);
    }

    if (end_offset - aligned_offset < size)
        return false;

    *word_index = PAS_BITVECTOR_WORD64_INDEX(
        aligned_offset >> page_config.base.min_align_shift);
    *fixed_start_bit_index = PAS_BITVECTOR_BIT_SHIFT64(
        aligned_offset >> page_config.base.min_align_shift);

    /* If we're in an alignment situation, we'll call this for each word until we find one. So
       there's no way for this computation to find an earlier word. If it did then we would have
       triggered on that one. */
    PAS_ASSERT(other_word_index == PAS_BITVECTOR_WORD64_INDEX(
                   (aligned_offset + size - 1) >> page_config.base.min_align_shift));
    return true;
}

/* Returns zero if we don't need to reloop, which could happen if any of these are true:
 
   - There is only one granule in the page according to the page config.

   - None of the granules were decommitted.

   - We hold the commit lock already.

   FIXME: We can save a lot of code size by making this outline (either specialized or generic,
   probably doesn't matter that much which). */
static PAS_ALWAYS_INLINE unsigned pas_bitfit_page_allocation_commit_granules_or_reloop(
    pas_bitfit_page* page,
    pas_bitfit_view* view,
    uintptr_t word_index,
    uint64_t fixed_start_bit_index,
    uintptr_t size,
    pas_bitfit_page_config page_config,
    pas_lock_hold_mode commit_lock_hold_mode,
    size_t* bytes_committed)
{
    uintptr_t offset_in_page;
    uintptr_t index_of_first_granule;
    uintptr_t index_of_last_granule;
    uintptr_t granule_index;
    pas_page_granule_use_count* use_counts;
    unsigned result;

    if (page_config.base.page_size == page_config.base.granule_size)
        return 0;

    use_counts = pas_bitfit_page_get_granule_use_counts(page, page_config);
    
    offset_in_page = pas_bitfit_page_compute_offset(word_index, fixed_start_bit_index, page_config);

    PAS_TESTING_ASSERT(offset_in_page >= pas_bitfit_page_offset_to_first_object(page_config));
    PAS_TESTING_ASSERT(offset_in_page + size <= page_config.base.page_size);

    pas_page_granule_get_indices(
        offset_in_page,
        offset_in_page + size,
        page_config.base.page_size,
        page_config.base.granule_size,
        &index_of_first_granule,
        &index_of_last_granule);

    PAS_ASSERT(page_config.base.page_size > page_config.base.granule_size);

    if (commit_lock_hold_mode == pas_lock_is_held) {
        pas_commit_span commit_span;
        
        PAS_ASSERT(view->is_owned);
        pas_lock_assert_held(&view->commit_lock);
        
        pas_commit_span_construct(&commit_span, page_config.base.heap_config_ptr->mmap_capability);
        
        for (granule_index = index_of_first_granule;
             granule_index <= index_of_last_granule;
             ++granule_index) {
            if (use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED) {
                pas_commit_span_add_to_change(&commit_span, granule_index);
                use_counts[granule_index] = 1;
                continue;
            }
            
            pas_commit_span_add_unchanged_and_commit(
                &commit_span, &page->base, granule_index, page_config.base.page_config_ptr);
            
            use_counts[granule_index]++;
            PAS_ASSERT(use_counts[granule_index] != PAS_PAGE_GRANULE_DECOMMITTED);
        }
        PAS_ASSERT(granule_index == index_of_last_granule + 1);
        
        pas_commit_span_add_unchanged_and_commit(
            &commit_span, &page->base, granule_index, page_config.base.page_config_ptr);

        *bytes_committed += commit_span.total_bytes;

        pas_compiler_fence();
        return 0;
    }

    result = 0;
    for (granule_index = index_of_first_granule;
         granule_index <= index_of_last_granule;
         ++granule_index) {
        if (use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED)
            result++;
    }

    if (!result) {
        for (granule_index = index_of_first_granule;
             granule_index <= index_of_last_granule;
             ++granule_index) {
            PAS_ASSERT(use_counts[granule_index] != PAS_PAGE_GRANULE_DECOMMITTED);
            
            use_counts[granule_index]++;
            PAS_ASSERT(use_counts[granule_index] != PAS_PAGE_GRANULE_DECOMMITTED);
        }
        pas_compiler_fence();
    }

    return result;
}

static PAS_ALWAYS_INLINE pas_bitfit_allocation_result pas_bitfit_page_finish_allocation(
    pas_bitfit_page* page,
    pas_bitfit_view* view,
    uintptr_t word_index,
    uint64_t fixed_start_bit_index,
    uintptr_t size,
    pas_allocation_mode allocation_mode,
    pas_bitfit_page_config page_config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_BITFIT_HEAPS);
    
    bool did_overflow;
    uintptr_t offset_in_page;
    uintptr_t begin;
    uint16_t num_live_bits;

    PAS_ASSERT(page_config.base.is_enabled);

    num_live_bits = page->num_live_bits;
    if (!num_live_bits)
        pas_bitfit_view_note_nonemptiness(view);

    did_overflow = __builtin_add_overflow(num_live_bits,
                                          size >> page_config.base.min_align_shift,
                                          &num_live_bits);
    PAS_ASSERT(!did_overflow);
    page->num_live_bits = num_live_bits;

    offset_in_page = pas_bitfit_page_compute_offset(word_index, fixed_start_bit_index, page_config);

    PAS_TESTING_ASSERT(offset_in_page >= pas_bitfit_page_offset_to_first_object(page_config));
    PAS_TESTING_ASSERT(offset_in_page + size <= page_config.base.page_size);

    begin = (uintptr_t)pas_bitfit_page_boundary(page, page_config) + offset_in_page;

    if (verbose) {
        pas_log("%p: bitfit allocated %p of size %zu in %p\n",
                (void*)pthread_self(), (void*)begin, size, page);
    }

    if (verbose) {
        pas_log("Bits after allocating %p (size %zu, offset %zu in %p):\n",
                (void*)begin, size, offset_in_page, page);
        pas_bitfit_page_log_bits(page, offset_in_page, offset_in_page + size);
    }
    
    pas_bitfit_page_testing_verify(page);

    PAS_PROFILE(BITFIT_ALLOCATION, &page_config, begin, size, allocation_mode);
    PAS_MTE_HANDLE(BITFIT_ALLOCATION, page_config, begin, size, allocation_mode);

    return pas_bitfit_allocation_result_create_success(begin);
}

/* This has the special ability to unlock and relock the ownership lock! */
static PAS_ALWAYS_INLINE pas_bitfit_allocation_result pas_bitfit_page_allocate(
    pas_bitfit_page* page,
    pas_bitfit_view* owner,
    uintptr_t size,
    uintptr_t alignment,
    pas_allocation_mode allocation_mode,
    pas_bitfit_page_config page_config,
    pas_lock_hold_mode commit_lock_hold_mode,
    size_t* bytes_committed)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_BITFIT_HEAPS);
    
    uintptr_t word_index;
    uint64_t* free_words;
    uint64_t* object_end_words;
    uintptr_t num_desired_bits;
    uintptr_t largest_available_bits;

    if (verbose)
        pas_log("In page %p allocating size = %zu, alignment = %zu.\n", page, size, alignment);

    PAS_ASSERT(page_config.base.is_enabled);
    PAS_TESTING_ASSERT(pas_is_aligned(size, pas_page_base_config_min_align(page_config.base)));

    pas_lock_testing_assert_held(&owner->ownership_lock);
    PAS_TESTING_ASSERT(pas_page_base_get_kind(&page->base)
                       == pas_page_kind_for_bitfit_variant(page_config.variant));
    PAS_TESTING_ASSERT(pas_compact_atomic_bitfit_view_ptr_load_non_null(&page->owner) == owner);

    pas_bitfit_page_testing_verify(page);

    free_words = (uint64_t*)pas_bitfit_page_free_bits(page);
    object_end_words = (uint64_t*)pas_bitfit_page_object_end_bits(page, page_config);
    num_desired_bits = size >> page_config.base.min_align_shift;
    largest_available_bits = 0;

    /* A note about how to do this concurrently to enumeration:
       
       The enumerator can ignore "objects" that comprise a span of clear free bits with no end bit, so
       long as they are followed by a set free bit.
       
       The enumerator can also ignore end bits that don't have a correspondingly clear free bit.
       
       So to allocate we:
       
       1. Clear all free bits except the ones in the word that has the end bit.
       2. Set the end bit.
       3. Clear the free bits in the word that has the end bit.
       
       And to deallocate we:
       
       1. Set the free bits in the word that has the end bit.
       2. Clear the end bit.
       3. Set the rest of the free bits.
       
       This way, the enumerator will either see (and ignore) an end bit with a set free bit, or it will
       see (and ignore) a span of clear free bits that end in a set free bit but not end bit. */

    for (word_index = 0;
         word_index < pas_bitfit_page_config_num_alloc_words64(page_config);
         word_index++) {
        uint64_t free_word;
        uint64_t shifted_free_word;
        uintptr_t num_lost_bits;

        free_word = free_words[word_index];
        shifted_free_word = free_word;

        num_lost_bits = 0;
        
        while (shifted_free_word) {
            uintptr_t start_bit_index;
            uintptr_t num_available_bits;
            uint64_t remaining_word;
            uintptr_t end_bit_index;
            uintptr_t fixed_start_bit_index;
            
            start_bit_index = (uintptr_t)__builtin_ctzll(shifted_free_word);
            fixed_start_bit_index = start_bit_index + num_lost_bits;

            remaining_word = ~(shifted_free_word >> start_bit_index);

            if (remaining_word)
                num_available_bits = (uintptr_t)__builtin_ctzll(remaining_word);
            else
                num_available_bits = PAS_BITVECTOR_BITS_PER_WORD64;

            PAS_TESTING_ASSERT(
                num_available_bits + fixed_start_bit_index <= PAS_BITVECTOR_BITS_PER_WORD64);

            if (num_available_bits >= num_desired_bits &&
                pas_bitfit_page_allocation_satisfies_alignment(
                    &word_index,
                    &fixed_start_bit_index,
                    word_index,
                    fixed_start_bit_index + num_available_bits,
                    size, alignment, page_config)) {
                unsigned pages_to_commit_on_reloop;
                pages_to_commit_on_reloop =
                    pas_bitfit_page_allocation_commit_granules_or_reloop(
                        page, owner, word_index, fixed_start_bit_index, size, page_config,
                        commit_lock_hold_mode, bytes_committed);
                if (pages_to_commit_on_reloop) {
                    return pas_bitfit_allocation_result_create_need_to_lock_commit_lock(
                        pages_to_commit_on_reloop);
                }
                object_end_words[word_index] |=
                    PAS_BITVECTOR_BIT_MASK64(fixed_start_bit_index + num_desired_bits - 1);
                pas_compiler_fence();
                free_words[word_index] =
                    free_word & ~(pas_make_mask64(num_desired_bits) << fixed_start_bit_index);
                return pas_bitfit_page_finish_allocation(
                    page, owner, word_index, fixed_start_bit_index, size, allocation_mode, page_config);
            }

            if (num_available_bits + fixed_start_bit_index >= PAS_BITVECTOR_BITS_PER_WORD64) {
                uint64_t num_remaining_needed_bits;
                uintptr_t other_word_index;

                num_remaining_needed_bits = num_desired_bits - num_available_bits;

                if (verbose) {
                    pas_log("Need to do a search starting at word_index = %lu + 1, "
                            "num_remaining_needed_bits = %llu\n",
                            (unsigned long)word_index, (unsigned long long)num_remaining_needed_bits);
                }

                for (other_word_index = word_index + 1; ; ++other_word_index) {
                    uint64_t other_free_word;
                    uint64_t other_alloc_word;
                    uint64_t num_available_leading_bits;
                    uint64_t num_upper_bits_to_clear;
                    uintptr_t intermediate_word_index;
                    uintptr_t index;
                    unsigned pages_to_commit_on_reloop;

                    if (verbose) {
                        pas_log("At other_word_index = %lu, num_remaining_needed_bits = %llu\n",
                                (unsigned long)other_word_index, (unsigned long long)num_remaining_needed_bits);
                    }

                    if (other_word_index >= pas_bitfit_page_config_num_alloc_words64(page_config)) {
                        PAS_TESTING_ASSERT(
                            other_word_index
                            == pas_bitfit_page_config_num_alloc_words64(page_config));
                        return pas_bitfit_allocation_result_create_failure(
                            PAS_MAX(largest_available_bits,
                                    PAS_BITVECTOR_BIT_INDEX64(other_word_index)
                                    - PAS_BITVECTOR_BIT_INDEX64(word_index)
                                    - fixed_start_bit_index)
                            << page_config.base.min_align_shift);
                    }

                    other_free_word = free_words[other_word_index];
                    other_alloc_word = ~other_free_word;

                    if (other_alloc_word) {
                        num_available_leading_bits = (uint64_t)__builtin_ctzll(other_alloc_word);
                        if (num_available_leading_bits < num_remaining_needed_bits ||
                            !pas_bitfit_page_allocation_satisfies_alignment(
                                &word_index,
                                &fixed_start_bit_index,
                                other_word_index,
                                num_available_leading_bits,
                                size, alignment, page_config)) {
                            largest_available_bits = PAS_MAX(
                                largest_available_bits,
                                PAS_BITVECTOR_BIT_INDEX64(other_word_index)
                                + num_available_leading_bits
                                - PAS_BITVECTOR_BIT_INDEX64(word_index)
                                - fixed_start_bit_index);
                            word_index = other_word_index;
                            free_word = other_free_word;
                            shifted_free_word = free_word >> num_available_leading_bits;
                            num_lost_bits = num_available_leading_bits;
                            break;
                        }
                    } else {
                        num_available_leading_bits = PAS_BITVECTOR_BITS_PER_WORD64;
                        if (num_remaining_needed_bits > PAS_BITVECTOR_BITS_PER_WORD64) {
                            num_remaining_needed_bits -= PAS_BITVECTOR_BITS_PER_WORD64;
                            continue;
                        }
                        if (!pas_bitfit_page_allocation_satisfies_alignment(
                                &word_index,
                                &fixed_start_bit_index,
                                other_word_index,
                                PAS_BITVECTOR_BITS_PER_WORD64,
                                size, alignment, page_config))
                            continue;
                    }

                    /* Recompute other_word_index and num_remaining_needed_bits, since these values
                       may have been changed due to alignment stuff. */
                    index = PAS_BITVECTOR_BIT_INDEX64(word_index) + fixed_start_bit_index;
                    PAS_TESTING_ASSERT(other_word_index == PAS_BITVECTOR_WORD64_INDEX(
                                           index + num_desired_bits - 1));
                    num_remaining_needed_bits =
                        PAS_BITVECTOR_BIT_SHIFT64(index + num_desired_bits - 1) + 1;
                    PAS_TESTING_ASSERT(num_remaining_needed_bits <= num_available_leading_bits);

                    num_upper_bits_to_clear = PAS_BITVECTOR_BITS_PER_WORD64 - fixed_start_bit_index;

                    pages_to_commit_on_reloop =
                        pas_bitfit_page_allocation_commit_granules_or_reloop(
                            page, owner, word_index, fixed_start_bit_index, size, page_config,
                            commit_lock_hold_mode, bytes_committed);
                    if (pages_to_commit_on_reloop) {
                        return pas_bitfit_allocation_result_create_need_to_lock_commit_lock(
                            pages_to_commit_on_reloop);
                    }

                    if (num_upper_bits_to_clear == PAS_BITVECTOR_BITS_PER_WORD64)
                        free_words[word_index] = 0;
                    else {
                        free_words[word_index] =
                            free_word << num_upper_bits_to_clear >> num_upper_bits_to_clear;
                    }
                    for (intermediate_word_index = word_index + 1;
                         intermediate_word_index < other_word_index;
                         intermediate_word_index++)
                        free_words[intermediate_word_index] = 0;
                    pas_compiler_fence();
                    object_end_words[other_word_index] |=
                        PAS_BITVECTOR_BIT_MASK64(num_remaining_needed_bits - 1);
                    pas_compiler_fence();
                    if (num_remaining_needed_bits == PAS_BITVECTOR_BITS_PER_WORD64)
                        free_words[other_word_index] = 0;
                    else {
                        free_words[other_word_index] =
                            other_free_word
                            >> num_remaining_needed_bits
                            << num_remaining_needed_bits;
                    }
                    PAS_TESTING_ASSERT(
                        pas_bitvector_get(
                            (unsigned*)object_end_words,
                            ((pas_bitfit_page_compute_offset(
                                  word_index, fixed_start_bit_index, page_config) + size)
                             >> page_config.base.min_align_shift) - 1));
                    return pas_bitfit_page_finish_allocation(
                        page, owner, word_index, fixed_start_bit_index, size, allocation_mode, page_config);
                }

                /* NOTE - it's important that if we get here, we've set word_index, free_word,
                   num_lost_bits, and shifted_free_word. */
            } else {
                largest_available_bits = PAS_MAX(largest_available_bits, num_available_bits);
                end_bit_index = start_bit_index + num_available_bits;
                num_lost_bits += end_bit_index;
                shifted_free_word >>= end_bit_index;
            }
        }
    }

    return pas_bitfit_allocation_result_create_failure(
        largest_available_bits << page_config.base.min_align_shift);
}

enum pas_bitfit_page_deallocate_with_page_impl_mode {
    pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode,
    pas_bitfit_page_deallocate_with_page_impl_deallocate_mode,
    pas_bitfit_page_deallocate_with_page_impl_shrink_mode
};

typedef enum pas_bitfit_page_deallocate_with_page_impl_mode pas_bitfit_page_deallocate_with_page_impl_mode;

static inline const char* pas_bitfit_page_deallocate_with_page_impl_mode_get_string(
    pas_bitfit_page_deallocate_with_page_impl_mode mode)
{
    switch (mode) {
    case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
        return "get_allocation_size";
    case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
        return "deallocate";
    case pas_bitfit_page_deallocate_with_page_impl_shrink_mode:
        return "shrink";
    }
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

/* Returns the number of bits used by the object. */
static PAS_ALWAYS_INLINE uintptr_t pas_bitfit_page_deallocate_with_page_impl(
    pas_bitfit_page* page,
    uintptr_t begin,
    pas_bitfit_page_config page_config,
    pas_bitfit_page_deallocate_with_page_impl_mode mode,
    size_t new_size)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_BITFIT_HEAPS);
    
    uintptr_t offset;
    uintptr_t bit_index;
    uintptr_t word_index;
    uintptr_t bit_index_in_word;
    uintptr_t other_word_index;
    uint64_t* free_words;
    uint64_t* object_end_words;
    uint64_t object_end_word;
    uint64_t shifted_object_end_word;
    uintptr_t num_bits;
    uintptr_t new_num_bits;
    pas_bitfit_view* owner;
    bool did_find_empty_granule;

    PAS_ASSERT(page_config.base.is_enabled);

    offset = pas_modulo_power_of_2(begin, page_config.base.page_size);
    
    owner = pas_compact_atomic_bitfit_view_ptr_load(&page->owner);

    if (verbose) {
        pas_log("Bits before deallocate_impl (mode = %s) of %p (offset = %zu in %p), "
                "num_live_bits = %u\n",
                pas_bitfit_page_deallocate_with_page_impl_mode_get_string(mode), (void*)begin, offset,
                page, page->num_live_bits);
        pas_bitfit_page_log_bits(page, offset, offset + 1);
    }

    switch (mode) {
    case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
    case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
        PAS_ASSERT(!new_size);
        new_num_bits = 0;
        break;
        
    case pas_bitfit_page_deallocate_with_page_impl_shrink_mode:
        /* FIXME: Maybe it would be better if we rounded up the size to the type alignment. But,
           currently, type size/alignment don't do much for us in the bitfit heap anyway, so maybe
           it's OK that we don't. */
        if (!new_size)
            new_num_bits = 1;
        else {
            new_num_bits =
                pas_round_up_to_power_of_2(new_size, pas_page_base_config_min_align(page_config.base))
                >> page_config.base.min_align_shift;
        }

        if (verbose)
            pas_log("Shrinking to new_size = %zu, new_num_bits = %zu\n", new_size, new_num_bits);
        
        break;
    }

    bit_index = offset >> page_config.base.min_align_shift;

    switch (mode) {
    case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
        break;
        
    case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
    case pas_bitfit_page_deallocate_with_page_impl_shrink_mode:
        pas_lock_lock(&owner->ownership_lock);
        
        pas_bitfit_page_testing_verify(page);

        if (offset < pas_bitfit_page_offset_to_first_object(page_config))
            pas_deallocation_did_fail("attempt to free bitfit page header", begin);
        
        if (offset > pas_bitfit_page_offset_to_first_object(page_config)
            && (!pas_bitvector_get(pas_bitfit_page_free_bits(page), bit_index - 1)
                && !pas_bitvector_get(pas_bitfit_page_object_end_bits(page, page_config),
                                      bit_index - 1))) {
            pas_bitfit_page_deallocation_did_fail(
                page, page_config.kind, begin, offset, "previous bit is not free or end of object");
        }
        break;
    }

    word_index = PAS_BITVECTOR_WORD64_INDEX(bit_index);
    bit_index_in_word = PAS_BITVECTOR_BIT_SHIFT64(bit_index);
    free_words = (uint64_t*)pas_bitfit_page_free_bits(page);
    object_end_words = (uint64_t*)pas_bitfit_page_object_end_bits(page, page_config);

    switch (mode) {
    case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
        break;
        
    case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
    case pas_bitfit_page_deallocate_with_page_impl_shrink_mode:
        if (pas_bitvector_get((unsigned*)free_words, bit_index)) {
            pas_bitfit_page_deallocation_did_fail(
                page, page_config.kind, begin, offset, "free bit set");
        }
        break;
    }

    object_end_word = object_end_words[word_index];
    shifted_object_end_word = object_end_word >> bit_index_in_word;

    if (shifted_object_end_word) {
        uint64_t object_end_bit_index;

        object_end_bit_index = (uint64_t)__builtin_ctzll(shifted_object_end_word);
        num_bits = object_end_bit_index + 1;

        if (verbose) {
            pas_log("Taking the same-word fast path with object_end_bit_index = %llu\n",
                    (unsigned long long)object_end_bit_index);
        }

        switch (mode) {
        case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
            break;
            
        case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
            free_words[word_index] |= pas_make_mask64(num_bits) << bit_index_in_word;
            pas_compiler_fence();
            object_end_words[word_index] =
                object_end_word & ~PAS_BITVECTOR_BIT_MASK64(bit_index_in_word +
                                                            object_end_bit_index);
            break;
            
        case pas_bitfit_page_deallocate_with_page_impl_shrink_mode:
            if (new_num_bits > num_bits)
                pas_deallocation_did_fail("attempt to shrink to a larger size", begin);
            if (new_num_bits == num_bits)
                break;
            free_words[word_index] |=
                pas_make_mask64(num_bits - new_num_bits) << (bit_index_in_word + new_num_bits);
            pas_compiler_fence();
            object_end_words[word_index] =
                (object_end_word & ~PAS_BITVECTOR_BIT_MASK64(bit_index_in_word +
                                                             object_end_bit_index))
                | PAS_BITVECTOR_BIT_MASK64(bit_index_in_word + new_num_bits - 1);
            break;
        }
    } else {
        other_word_index = word_index;
        for (;;) {
            ++other_word_index;
            switch (mode) {
            case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
                break;
                
            case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
            case pas_bitfit_page_deallocate_with_page_impl_shrink_mode:
                if (other_word_index >= pas_bitfit_page_config_num_alloc_words64(page_config)) {
                    pas_bitfit_page_deallocation_did_fail(
                        page, page_config.kind, begin, offset, "object falls off end of page");
                }
                break;
            }

            object_end_word = object_end_words[other_word_index];
            if (object_end_word) {
                uint64_t object_end_bit_index;
                uintptr_t intermediate_word_index;

                object_end_bit_index = (uint64_t)__builtin_ctzll(object_end_word);

                if (verbose) {
                    pas_log("Found end bit word at %lu, bit index %llu\n",
                            (unsigned long)other_word_index, (unsigned long long)object_end_bit_index);
                }

                num_bits =
                    PAS_BITVECTOR_BITS_PER_WORD64 - bit_index_in_word
                    + (other_word_index - word_index - 1) * PAS_BITVECTOR_BITS_PER_WORD64
                    + object_end_bit_index + 1;

                switch (mode) {
                case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
                    break;
                    
                case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
                case pas_bitfit_page_deallocate_with_page_impl_shrink_mode: {
                    uintptr_t modified_word_index;
                    uintptr_t modified_bit_index_in_word;
                    bool should_break;
                    
                    PAS_ASSERT(other_word_index >= word_index + 1);

                    should_break = false;
                    
                    switch (mode) {
                    case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
                        modified_word_index = word_index;
                        modified_bit_index_in_word = bit_index_in_word;
                        break;
                        
                    case pas_bitfit_page_deallocate_with_page_impl_shrink_mode: {
                        uintptr_t start_of_free;
                        
                        if (new_num_bits > num_bits)
                            pas_deallocation_did_fail("attempt to shrink to a larger size", begin);
                        if (new_num_bits == num_bits) {
                            should_break = true;
                            /* Tell the compiler to chill out. */
                            modified_word_index = 0;
                            modified_bit_index_in_word = 0;
                            break;
                        }
                        
                        start_of_free =
                            word_index * PAS_BITVECTOR_BITS_PER_WORD64 +
                            bit_index_in_word + new_num_bits;

                        if (verbose)
                            pas_log("start_of_free = %zu\n", start_of_free);

                        modified_word_index = PAS_BITVECTOR_WORD64_INDEX(start_of_free);
                        modified_bit_index_in_word = PAS_BITVECTOR_BIT_SHIFT64(start_of_free);

                        /* For enumeration, the best that we can do is to make the memory we are about
                           to "shrink out" momentarily appear as its own object. */
                        pas_bitvector_set(
                            pas_bitfit_page_object_end_bits(page, page_config),
                            start_of_free - 1, true);
                        pas_compiler_fence();
                        object_end_word = object_end_words[other_word_index];
                        pas_compiler_fence();

                        if (modified_word_index == other_word_index) {
                            PAS_ASSERT(object_end_bit_index + 1 - modified_bit_index_in_word
                                       == num_bits - new_num_bits);
                            PAS_ASSERT(num_bits - new_num_bits <= PAS_BITVECTOR_BITS_PER_WORD64);
                            
                            free_words[modified_word_index] |=
                                pas_make_mask64(num_bits - new_num_bits) << modified_bit_index_in_word;
                            pas_compiler_fence();
                            object_end_words[modified_word_index] =
                                object_end_word & ~PAS_BITVECTOR_BIT_MASK64(object_end_bit_index);
                            should_break = true;
                        }
                        break;
                    }

                    default:
                        PAS_ASSERT_NOT_REACHED();
                        /* Tell the compiler to chill out. */
                        modified_word_index = 0;
                        modified_bit_index_in_word = 0;
                        break;
                    }

                    if (should_break)
                        break;

                    free_words[other_word_index] |= pas_make_mask64(object_end_bit_index + 1);
                    pas_compiler_fence();
                    object_end_words[other_word_index] =
                        object_end_word & ~PAS_BITVECTOR_BIT_MASK64(object_end_bit_index);
                    pas_compiler_fence();
                    free_words[modified_word_index] |= UINT64_MAX << modified_bit_index_in_word;
                    for (intermediate_word_index = modified_word_index + 1;
                         intermediate_word_index < other_word_index;
                         intermediate_word_index++)
                        free_words[intermediate_word_index] = UINT64_MAX;
                    if (verbose) {
                        pas_log("object_end_bit_index = %llu, mask = %llu\n",
                                (unsigned long long)object_end_bit_index, (unsigned long long)pas_make_mask64(object_end_bit_index + 1));
                    }
                    break;
                } }

                if (verbose) {
                    pas_log("word_index = %lu, bit_index_in_word = %lu, other_word_index = %lu, "
                            "object_end_bit_index = %llu\n",
                            (unsigned long)word_index, (unsigned long)bit_index_in_word, (unsigned long)other_word_index, (unsigned long long)object_end_bit_index);
                }

                if (verbose)
                    pas_log("num_bits = %zu\n", num_bits);
                break;
            }
        }
    }

    switch (mode) {
    case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
        break;
        
    case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
    case pas_bitfit_page_deallocate_with_page_impl_shrink_mode: {
        bool did_overflow;
        uintptr_t size;
        uintptr_t modified_offset;

        /* new_num_bits is zero in deallocate_mode (see top of this function). */
        size = (num_bits - new_num_bits) << page_config.base.min_align_shift;
        modified_offset = offset + (new_num_bits << page_config.base.min_align_shift);

        if (verbose) {
            pas_log("%p: bitfit deallocated %p of size %zu in %p with modified_offset = %zu\n",
                    (void*)pthread_self(), (void*)begin, size, page, modified_offset);
        }
        
        if (page_config.base.page_size > page_config.base.granule_size) {
            pas_range range;

            switch (mode) {
            case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode:
                range = pas_range_create(offset, offset + size);
                break;

            case pas_bitfit_page_deallocate_with_page_impl_shrink_mode:
                range = pas_range_create_forgiving(
                    pas_round_up_to_power_of_2(modified_offset, page_config.base.granule_size),
                    modified_offset + size);
                break;

            default:
                PAS_ASSERT_NOT_REACHED();
                range = pas_range_create_empty();
                break;
            }
            
            did_find_empty_granule = pas_page_base_free_granule_uses_in_range(
                pas_bitfit_page_get_granule_use_counts(page, page_config),
                range.begin,
                range.end,
                page_config.base);
        } else
            did_find_empty_granule = false;
        
        if (!page->did_note_max_free) {
            pas_bitfit_view_note_max_free(owner);
            page->did_note_max_free = true;
        }
        
        did_overflow = __builtin_sub_overflow(
            page->num_live_bits, num_bits - new_num_bits, &page->num_live_bits);
        PAS_ASSERT(!did_overflow);
        if (!page->num_live_bits)
            pas_bitfit_view_note_full_emptiness(owner, page);
        else if (did_find_empty_granule)
            pas_bitfit_view_note_partial_emptiness(owner, page);
        
        if (verbose) {
            pas_log("Bits afer deallocate_impl (mode = %s) with size %zu, offset = %zu, "
                    "modified_offset = %zu in %p\n",
                    pas_bitfit_page_deallocate_with_page_impl_mode_get_string(mode), size, offset,
                    modified_offset, page);
            pas_bitfit_page_log_bits(
                page, modified_offset, modified_offset + size);
        }

        pas_bitfit_page_testing_verify(page);

        pas_lock_unlock(&owner->ownership_lock);
        break;
    } }

    switch (mode) {
    case pas_bitfit_page_deallocate_with_page_impl_deallocate_mode: {
        PAS_PROFILE(BITFIT_PAGE_DEALLOCATION, page_config, begin, (num_bits << page_config.base.min_align_shift));
        PAS_MTE_HANDLE(BITFIT_PAGE_DEALLOCATION, page_config, begin, (num_bits << page_config.base.min_align_shift));
        break;
    case pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode:
    case pas_bitfit_page_deallocate_with_page_impl_shrink_mode:
        break;
    } }

    return num_bits;
}

static PAS_ALWAYS_INLINE void pas_bitfit_page_deallocate_with_page(
    pas_bitfit_page* page,
    uintptr_t begin,
    pas_bitfit_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    pas_bitfit_page_deallocate_with_page_impl(
        page, begin, page_config, pas_bitfit_page_deallocate_with_page_impl_deallocate_mode, 0);
}

static PAS_ALWAYS_INLINE void pas_bitfit_page_deallocate(
    uintptr_t begin,
    pas_bitfit_page_config page_config)
{
    pas_bitfit_page* page;
    PAS_ASSERT(page_config.base.is_enabled);
    page = pas_bitfit_page_for_address_and_page_config(begin, page_config);
    page_config.specialized_page_deallocate_with_page(page, begin);
}

static PAS_ALWAYS_INLINE size_t pas_bitfit_page_get_allocation_size_with_page(
    pas_bitfit_page* page,
    uintptr_t begin,
    pas_bitfit_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    return pas_bitfit_page_deallocate_with_page_impl(
        page, begin, page_config, pas_bitfit_page_deallocate_with_page_impl_get_allocation_size_mode ,0)
        << page_config.base.min_align_shift;
}

static PAS_ALWAYS_INLINE size_t pas_bitfit_page_get_allocation_size(
    uintptr_t begin,
    pas_bitfit_page_config page_config)
{
    pas_bitfit_page* page;
    PAS_ASSERT(page_config.base.is_enabled);
    page = pas_bitfit_page_for_address_and_page_config(begin, page_config);
    return page_config.specialized_page_get_allocation_size_with_page(page, begin);
}

static PAS_ALWAYS_INLINE void pas_bitfit_page_shrink_with_page(
    pas_bitfit_page* page,
    uintptr_t begin,
    size_t new_size,
    pas_bitfit_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    pas_bitfit_page_deallocate_with_page_impl(
        page, begin, page_config, pas_bitfit_page_deallocate_with_page_impl_shrink_mode, new_size);
}

static PAS_ALWAYS_INLINE void pas_bitfit_page_shrink(
    uintptr_t begin,
    size_t new_size,
    pas_bitfit_page_config page_config)
{
    pas_bitfit_page* page;
    PAS_ASSERT(page_config.base.is_enabled);
    page = pas_bitfit_page_for_address_and_page_config(begin, page_config);
    return page_config.specialized_page_shrink_with_page(page, begin, new_size);
}

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_PAGE_INLINES_H */

