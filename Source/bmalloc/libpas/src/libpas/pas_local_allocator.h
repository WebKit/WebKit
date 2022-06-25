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

#ifndef PAS_LOCAL_ALLOCATOR_H
#define PAS_LOCAL_ALLOCATOR_H

#include "pas_allocation_result.h"
#include "pas_allocator_scavenge_action.h"
#include "pas_bitfit_allocator.h"
#include "pas_config.h"
#include "pas_local_allocator_config_kind.h"
#include "pas_local_allocator_scavenger_data.h"
#include "pas_lock.h"
#include "pas_segregated_page_config.h"
#include "pas_segregated_view.h"
#include "pas_utils.h"
#include <stdio.h>

PAS_BEGIN_EXTERN_C;

struct pas_allocator_counts;
struct pas_local_allocator;
struct pas_segregated_heap;
struct pas_segregated_page;
typedef struct pas_allocator_counts pas_allocator_counts;
typedef struct pas_local_allocator pas_local_allocator;
typedef struct pas_segregated_heap pas_segregated_heap;
typedef struct pas_segregated_page pas_segregated_page;

struct pas_local_allocator {
    pas_local_allocator_scavenger_data scavenger_data;
    
    uint8_t alignment_shift;
    pas_local_allocator_config_kind config_kind : 8;
    bool current_word_is_valid; /* This is just used by enumeration. */

    /* This has to have a pointer to our index within the view. We can get to the view using
       page_ish. Maybe worth reconsidering that, but then again maybe it's good enough. 
    
       It's interesting that this increases the likelihood of restart. It's also interesting that
       this causes the allocator to blow over more memory before it has a chance to reuse memory. 
       But maybe that's better. */
       
    uintptr_t payload_end; /* payload_end != 0 means that we have or had a bump region; we may
                              have exhausted it. */
    unsigned remaining; /* remaining != 0 means that we still have a bump region and have not
                           exhausted it yet. */
    unsigned object_size;
    uintptr_t page_ish; /* page_ish != 0 means that we are allocating in that page. This is a pointer
                           to anywhere inside the page such that rounding down to page size gives the
                           page boundary. */
    unsigned current_offset; /* current_offset < end_offset means that we have bits to search. */
    unsigned end_offset;
    uint64_t current_word;
    pas_segregated_view view; /* points to a partial view if we're in partial mode or the size
                                 directory otherwise. This will point to a partial view in either
                                 primordial partial mode or in normal partial mode. */
    uint64_t bits[1];
};

#define PAS_LOCAL_ALLOCATOR_NULL_INITIALIZER ((pas_local_allocator){ \
        .scavenger_data = \
            PAS_LOCAL_ALLOCATOR_SCAVENGER_DATA_INITIALIZER(pas_local_allocator_allocator_kind), \
        .payload_end = 0, \
        .remaining = 0, \
        .object_size = 0, \
        .page_ish = 0, \
        .current_offset = 0, \
        .end_offset = 0, \
        .view = NULL, \
        .alignment_shift = 0, \
        .current_word_is_valid = false, \
        .current_word = 0, \
        .config_kind = pas_local_allocator_config_kind_null \
    })

#define PAS_LOCAL_ALLOCATOR_SIZE(num_alloc_bits) \
    (PAS_OFFSETOF(pas_local_allocator, bits) + \
     PAS_MAX_CONST( \
         sizeof(uint64_t) * PAS_BITVECTOR_NUM_WORDS64(num_alloc_bits), \
         PAS_ROUND_UP_TO_POWER_OF_2(sizeof(pas_bitfit_allocator), sizeof(uint64_t))))

#define PAS_LOCAL_ALLOCATOR_UNSELECTED_SIZE PAS_OFFSETOF(pas_local_allocator, bits)
#define PAS_LOCAL_ALLOCATOR_UNSELECTED_INDEX 0u
#define PAS_LOCAL_ALLOCATOR_UNSELECTED_NUM_INDICES (PAS_LOCAL_ALLOCATOR_UNSELECTED_SIZE / 8)

#define PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY 0

#if PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY
PAS_API extern double pas_local_allocator_refill_efficiency_sum;
PAS_API extern double pas_local_allocator_refill_efficiency_n;
PAS_DECLARE_LOCK(pas_local_allocator_refill_efficiency);
#endif /* PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY */

static inline bool pas_local_allocator_is_null(pas_local_allocator* allocator)
{
    return !allocator->view;
}

/* This doesn't capture "activity" in the sense of bitfit. That's fine since in bitfit mode, we don't
   exclusively hold any memory. */
static inline bool pas_local_allocator_is_active(pas_local_allocator* allocator)
{
    return !!allocator->page_ish;
}

static inline bool pas_local_allocator_has_bitfit(pas_local_allocator* allocator)
{
    return pas_local_allocator_config_kind_is_bitfit(allocator->config_kind);
}

static inline pas_bitfit_allocator* pas_local_allocator_get_bitfit(pas_local_allocator* allocator)
{
    PAS_TESTING_ASSERT(pas_local_allocator_has_bitfit(allocator));
    return (pas_bitfit_allocator*)allocator->bits;
}

static inline uintptr_t pas_local_allocator_page_boundary(pas_local_allocator* allocator,
                                                          pas_segregated_page_config page_config)
{
    return pas_round_down_to_power_of_2(allocator->page_ish,
                                        page_config.base.page_size);
}

PAS_API void pas_local_allocator_construct(pas_local_allocator* allocator,
                                           pas_segregated_size_directory* directory);

PAS_API void pas_local_allocator_construct_unselected(pas_local_allocator* allocator);

PAS_API void pas_local_allocator_reset(pas_local_allocator* allocator);

static inline size_t pas_local_allocator_alignment(pas_local_allocator* allocator)
{
    return (size_t)1 << (size_t)allocator->alignment_shift;
}

PAS_API void pas_local_allocator_move(pas_local_allocator* dst,
                                      pas_local_allocator* src);

PAS_API bool pas_local_allocator_stop(pas_local_allocator* allocator,
                                      pas_lock_lock_mode page_lock_mode,
                                      pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_lock_hold_mode pas_local_allocator_heap_lock_hold_mode(pas_local_allocator* allocator);

/* This is appropriate to call for allocators that are used under a lock and you're holding that
   lock. Allocators associated with TLCs use subtly different logic.

   Returns true if there is still more work for the scavenger. Will always return false if you use
   force. */
PAS_API bool pas_local_allocator_scavenge(pas_local_allocator* allocator,
                                          pas_allocator_scavenge_action action);

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_ALLOCATOR_H */

