/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_PAGE_H
#define PAS_SEGREGATED_PAGE_H

#include "pas_bitvector.h"
#include "pas_config.h"
#include "pas_free_range_kind.h"
#include "pas_heap_config.h"
#include "pas_heap_summary.h"
#include "pas_local_allocator.h"
#include "pas_lock.h"
#include "pas_range.h"
#include "pas_range_locked_mode.h"
#include "pas_page_base.h"
#include "pas_page_granule_use_count.h"
#include "pas_segregated_page_and_config.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* "Small pages" are pages that hold objects that are smaller than PAS_MAX_OBJECT_SIZE (i.e.
   "small" objects). */

struct pas_deferred_decommit_log;
struct pas_segregated_heap;
struct pas_segregated_page;
typedef struct pas_deferred_decommit_log pas_deferred_decommit_log;
typedef struct pas_segregated_heap pas_segregated_heap;
typedef struct pas_segregated_page pas_segregated_page;

/* This struct lives somewhere in the small page. Where it lives, and how big the page is, are
   controlled by the pas_segregated_page_config. */
struct pas_segregated_page {
    pas_page_base base;
    
    /* This bit is only used by exclusive pages. */
    bool is_in_use_for_allocation;

    /* This bit also accurately tells eligibility at the moment that we are about to stop
       allocating in a page. But after we stop, it will be false.
    
       This bit is only used by exclusive pages. */
    bool eligibility_notification_has_been_deferred;

    /* This is handy for debugging. */
    bool is_committing_fully : 1;

    bool avoid_line_allocator : 1;

    unsigned object_size; /* Caching this here is great for performance. */

    pas_lock* lock_ptr;

    uintptr_t use_epoch;

    /* Can be one of:
       
       exclusive: this is owned by some view directory, so it's a simple segregated storage page.
           This also means that the page has noted eligibility.
       
       ineligible_exclusive: same as exclusive, but eligibility has not been noted.
       
       shared_handle: it's shared by the shared page directory and possibly some number of view
           directories. Eligibility is noted by whether the partial directories appear in the
           eligibility table.
    
       Note that in both cases, this field kinda implicitly gives us a "eligibility_has_been_noted"
       field. Here's what that means. Eligible = has at least one free object.
       
       This is sort of like the eligible bit for this page in the page directory, but it will
       be set sooner: it may get set while the page is being allocated from.
       
       Eligibility is cleared when we start allocating. When we are about to stop allocating
       (i.e. we are a local allocator and we know we are about to stop allocating in this
       page), this bit accurately tells eligiblity.
    
       This bit also protects us against making a page eligibility in the time between when when
       we take_first_eligible and when we actually start allocating. For that reason, it's
       important that a page only gets its eligible bit set once it's marked eligible here, and
       this doesn't get cleared until we mark the page as being in_use_for_allocation. */
    pas_segregated_view owner;
    
    uint16_t num_non_empty_words;

    unsigned alloc_bits[1];
};

enum pas_segregated_page_hugging_mode {
    pas_segregated_page_hug_left,
    pas_segregated_page_hug_right
};

typedef enum pas_segregated_page_hugging_mode pas_segregated_page_hugging_mode;

extern PAS_API bool pas_segregated_page_deallocate_should_verify_granules;

#define PAS_SEGREGATED_PAGE_BASE_HEADER_SIZE PAS_OFFSETOF(pas_segregated_page, alloc_bits)

#define PAS_SEGREGATED_PAGE_HEADER_SIZE(num_alloc_bits, num_granules) \
    (PAS_SEGREGATED_PAGE_BASE_HEADER_SIZE + \
     PAS_ROUND_UP_TO_POWER_OF_2( \
         PAS_SEGREGATED_PAGE_CONFIG_NUM_ALLOC_BYTES(num_alloc_bits) + \
         PAS_PAGE_BASE_CONFIG_NUM_GRANULE_BYTES(num_granules), \
         sizeof(uint64_t)))

static PAS_ALWAYS_INLINE size_t pas_segregated_page_header_size(
    pas_segregated_page_config page_config)
{
    return PAS_SEGREGATED_PAGE_HEADER_SIZE(
        page_config.num_alloc_bits,
        page_config.base.page_size / page_config.base.granule_size);
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_offset_from_page_boundary_to_first_object_for_hugging_mode(
    unsigned object_size,
    pas_segregated_page_config page_config,
    pas_segregated_page_hugging_mode mode)
{
    PAS_ASSERT(pas_is_aligned(object_size, pas_segregated_page_config_min_align(page_config)));
    PAS_ASSERT(object_size <= page_config.base.max_object_size);

    switch (mode) {
    case pas_segregated_page_hug_left:
        return (unsigned)(
            ((page_config.base.page_object_payload_offset + object_size - 1) /
             object_size) * object_size);
    case pas_segregated_page_hug_right:
        return (unsigned)(
            page_config.base.page_size - 
            ((page_config.base.page_size - page_config.base.page_object_payload_offset) /
             object_size) * object_size);
    }
}

static PAS_ALWAYS_INLINE unsigned 
pas_segregated_page_offset_from_page_boundary_to_end_of_last_object_for_hugging_mode(
    unsigned object_size,
    pas_segregated_page_config page_config,
    pas_segregated_page_hugging_mode mode)
{
    PAS_ASSERT(pas_is_aligned(object_size, pas_segregated_page_config_min_align(page_config)));
    PAS_ASSERT(object_size <= page_config.base.max_object_size);

    switch (mode) {
    case pas_segregated_page_hug_left:
        return (unsigned)(
            (pas_segregated_page_config_object_payload_end_offset_from_boundary(page_config) /
             object_size) * object_size);
    case pas_segregated_page_hug_right:
        return (unsigned)(
            page_config.base.page_size -
            ((page_config.base.page_size -
              pas_segregated_page_config_object_payload_end_offset_from_boundary(page_config) +
              object_size - 1) /
             object_size) * object_size);
    }
}

static PAS_ALWAYS_INLINE unsigned pas_segregated_page_useful_object_payload_size_for_hugging_mode(
    unsigned object_size,
    pas_segregated_page_config page_config,
    pas_segregated_page_hugging_mode mode)
{
    return
        pas_segregated_page_offset_from_page_boundary_to_end_of_last_object_for_hugging_mode(
            object_size,
            page_config,
            mode) -
        pas_segregated_page_offset_from_page_boundary_to_first_object_for_hugging_mode(
            object_size,
            page_config,
            mode);
}

static PAS_ALWAYS_INLINE pas_segregated_page_hugging_mode
pas_segregated_page_best_hugging_mode(unsigned object_size,
                                      pas_segregated_page_config page_config)
{
    if (pas_segregated_page_useful_object_payload_size_for_hugging_mode(
            object_size, page_config,
            pas_segregated_page_hug_left) >=
        pas_segregated_page_useful_object_payload_size_for_hugging_mode(
            object_size, page_config,
            pas_segregated_page_hug_right))
        return pas_segregated_page_hug_left;
    return pas_segregated_page_hug_right;
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_offset_from_page_boundary_to_first_object_exclusive(
    unsigned object_size,
    pas_segregated_page_config page_config)
{
    return pas_segregated_page_offset_from_page_boundary_to_first_object_for_hugging_mode(
        object_size, page_config, pas_segregated_page_best_hugging_mode(object_size, page_config));
}

static PAS_ALWAYS_INLINE unsigned 
pas_segregated_page_offset_from_page_boundary_to_end_of_last_object_exclusive(
    unsigned object_size,
    pas_segregated_page_config page_config)
{
    return pas_segregated_page_offset_from_page_boundary_to_end_of_last_object_for_hugging_mode(
        object_size, page_config, pas_segregated_page_best_hugging_mode(object_size, page_config));
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_useful_object_payload_size(unsigned object_size,
                                               pas_segregated_page_config page_config)
{
    return pas_segregated_page_useful_object_payload_size_for_hugging_mode(
        object_size, page_config, pas_segregated_page_best_hugging_mode(object_size, page_config));
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_page_number_of_objects(unsigned object_size,
                                      pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    unsigned result;
    
    result = pas_segregated_page_useful_object_payload_size(object_size, page_config) / object_size;
    
    if (verbose) {
        pas_log("first object offset = %u\n", pas_segregated_page_offset_from_page_boundary_to_first_object_exclusive(object_size, page_config));
        pas_log("end of last object offset = %u\n", pas_segregated_page_offset_from_page_boundary_to_end_of_last_object_exclusive(object_size, page_config));
        pas_log("payload offset = %lu\n", page_config.base.page_object_payload_offset);
        pas_log("object_size = %u, so number_of_objects = %u\n", object_size, result);
    }
    
    return result;
}

PAS_API extern double pas_segregated_page_extra_wasteage_handicap_for_config_variant[];

static PAS_ALWAYS_INLINE double
pas_segregated_page_bytes_dirtied_per_object(unsigned object_size,
                                             pas_segregated_page_config page_config)
{
    double extra_handicap;

    PAS_ASSERT((unsigned)page_config.variant < PAS_NUM_SEGREGATED_PAGE_CONFIG_VARIANTS);
    extra_handicap = pas_segregated_page_extra_wasteage_handicap_for_config_variant[
        page_config.variant];
    
    return (double)page_config.base.page_size /
        pas_segregated_page_number_of_objects(object_size, page_config) *
        page_config.wasteage_handicap * extra_handicap;
}

/* NOTE: for the security properties to work, the page must start out zero-initialized. However,
   it's OK if it's not zero-initialized if we're re-constructing a page that was previously used
   with the same size class (and same heap). */
PAS_API void pas_segregated_page_construct(pas_segregated_page* page,
                                           pas_segregated_view owner,
                                           bool was_stolen,
                                           pas_segregated_page_config* page_config);

static PAS_ALWAYS_INLINE pas_page_granule_use_count*
pas_segregated_page_get_granule_use_counts(pas_segregated_page* page,
                                           pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.page_size > page_config.base.granule_size);
    return (pas_page_granule_use_count*)(
        page->alloc_bits + pas_segregated_page_config_num_alloc_words(page_config));
}

static PAS_ALWAYS_INLINE pas_segregated_page*
pas_segregated_page_for_boundary(void* boundary,
                                 pas_segregated_page_config page_config)
{
    return pas_page_base_get_segregated(pas_page_base_for_boundary(boundary, page_config.base));
}

static PAS_ALWAYS_INLINE pas_segregated_page*
pas_segregated_page_for_boundary_or_null(void* boundary,
                                         pas_segregated_page_config page_config)
{
    return pas_page_base_get_segregated(
        pas_page_base_for_boundary_or_null(boundary, page_config.base));
}

static PAS_ALWAYS_INLINE pas_segregated_page*
pas_segregated_page_for_boundary_unchecked(void* boundary,
                                           pas_segregated_page_config page_config)
{
    return (pas_segregated_page*)pas_page_base_for_boundary(boundary, page_config.base);
}

static PAS_ALWAYS_INLINE void*
pas_segregated_page_boundary(pas_segregated_page* page,
                             pas_segregated_page_config page_config)
{
    return pas_page_base_boundary(&page->base, page_config.base);
}

static PAS_ALWAYS_INLINE void*
pas_segregated_page_boundary_or_null(pas_segregated_page* page,
                                     pas_segregated_page_config page_config)
{
    return pas_page_base_boundary_or_null(&page->base, page_config.base);
}

static PAS_ALWAYS_INLINE pas_segregated_page*
pas_segregated_page_for_address_and_page_config(uintptr_t begin,
                                                pas_segregated_page_config page_config)
{
    return pas_page_base_get_segregated(
        pas_page_base_for_address_and_page_config(begin, page_config.base));
}

static PAS_ALWAYS_INLINE bool
pas_segregated_page_is_allocated_with_page(pas_segregated_page* page,
                                           uintptr_t begin,
                                           pas_segregated_page_config page_config)
{
    uintptr_t offset_in_page;
    size_t bit_index;
    unsigned word;
    size_t word_index;
    unsigned bit_mask;
    
    offset_in_page = begin - (uintptr_t)pas_segregated_page_boundary(page, page_config);
    bit_index = pas_page_base_index_of_object_at_offset_from_page_boundary(
        offset_in_page, page_config.base);
    
    word_index = PAS_BITVECTOR_WORD_INDEX(bit_index);
    bit_mask = PAS_BITVECTOR_BIT_MASK(bit_index);
    
    word = page->alloc_bits[word_index];
    return word & bit_mask;
}

static PAS_ALWAYS_INLINE bool
pas_segregated_page_is_allocated(uintptr_t begin,
                                 pas_segregated_page_config page_config)
{
    pas_segregated_page* page;
    bool result;
    
    page = pas_segregated_page_for_address_and_page_config(begin, page_config);
    result = pas_segregated_page_is_allocated_with_page(page, begin, page_config);
    return result;
}

PAS_API void pas_segregated_page_note_emptiness(pas_segregated_page* page);

typedef enum {
    pas_commit_fully_holding_page_lock,
    pas_commit_fully_holding_page_and_commit_locks
} pas_commit_fully_lock_hold_mode;

PAS_API void pas_segregated_page_commit_fully(
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_commit_fully_lock_hold_mode lock_hold_mode);

/* This will try to take the page lock if you're not already holding it. Then it may or may
   not still be holding it after it's done. */
PAS_API bool pas_segregated_page_take_empty_granules(
    pas_segregated_page* page,
    pas_deferred_decommit_log* decommit_log,
    pas_lock** held_lock,
    pas_range_locked_mode range_locked_mode,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API bool pas_segregated_page_take_physically(
    pas_segregated_page* page,
    pas_deferred_decommit_log* decommit_log,
    pas_range_locked_mode range_locked_mode,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API size_t pas_segregated_page_get_num_empty_granules(pas_segregated_page* page);
PAS_API size_t pas_segregated_page_get_num_committed_granules(pas_segregated_page* page);

PAS_API pas_segregated_page_config* pas_segregated_page_get_config(pas_segregated_page* page);

PAS_API void pas_segregated_page_add_commit_range(pas_segregated_page* page,
                                                  pas_heap_summary* result,
                                                  pas_range range);

PAS_API pas_segregated_page_and_config
pas_segregated_page_and_config_for_address_and_heap_config(uintptr_t begin,
                                                           pas_heap_config* config);

static inline pas_segregated_page*
pas_segregated_page_for_address_and_heap_config(uintptr_t begin, pas_heap_config* config)
{
    return pas_segregated_page_and_config_for_address_and_heap_config(begin, config).page;
}

PAS_API void pas_segregated_page_verify_num_non_empty_words(pas_segregated_page* page,
                                                            pas_segregated_page_config* page_config);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_H */

