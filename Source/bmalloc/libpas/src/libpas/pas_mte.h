/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#ifndef PAS_MTE_H
#define PAS_MTE_H

#include "pas_mte_config.h"

#if PAS_OS(DARWIN)
#include <dispatch/dispatch.h>
#if PAS_USE_APPLE_INTERNAL_SDK
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <mach/vm_page_size.h>
#include <mach/vm_statistics.h>
#endif // PAS_USE_APPLE_INTERNAL_SDK
#endif // PAS_OS(DARWIN)
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#if !PAS_OS(WINDOWS)
#include "unistd.h"
#endif

#if defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#if PAS_ENABLE_MTE

#include "pas_utils.h"
#include "pas_page_base_config.h"
#include "pas_allocation_mode.h"

PAS_IGNORE_WARNINGS_BEGIN("unsafe-buffer-usage")

#define PAS_MTE_TAG_MASK 0x0f00000000000000ull
#define PAS_MTE_CANONICAL_MASK ((0x1ull << 48) - 1)

#if __has_include(<malloc_private.h>)
#include <malloc_private.h>
#endif

/*
 * This must be kept in sync with the value of PAS_SMALL_PAGE_DEFAULT_SHIFT
 * from OpenSource's pas_internal_config.h -- we cannot use it directly as
 * pas_utils.h is too high up in the include hierarchy.
 */
#define PAS_MTE_SMALL_PAGE_DEFAULT_SHIFT (14)
#define PAS_MTE_SMALL_PAGE_NO_MASK (0x0000ffffffffffffull & ~((1 << PAS_MTE_SMALL_PAGE_DEFAULT_SHIFT) - 1))
#define PAS_MTE_SMALL_PAGE_NO(ptr) (((uintptr_t)ptr) & PAS_MTE_SMALL_PAGE_NO_MASK)

#define PAS_MTE_GET_MTAG(ptr) do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "ldg %0, [%0]" \
            : "+r"(ptr) \
            : \
            : \
        ); \
    } while (0)
#define PAS_MTE_SET_TAG(ptr) do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "stg %0, [%0]" \
            : \
            : "r"(ptr) \
            : \
        ); \
    } while (0)
#define PAS_MTE_SET_TAG_PAIR(ptr) do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "st2g %0, [%0]" \
            : \
            : "r"(ptr) \
            : \
        ); \
    } while (0)
#define PAS_MTE_SET_TAG_WITH_OFFSET(ptr, offset) do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "stg %0, [%0, #" #offset "]" \
            : \
            : "r"(ptr) \
            : \
        ); \
    } while (0)
#define PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(ptr, offset) do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "st2g %0, [%0, #" #offset "]" \
            : \
            : "r"(ptr) \
            : \
        ); \
    } while (0)
#define PAS_MTE_SET_TAG_POSTINDEX(ptr) do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "stg %0, [%0], #16" \
            : "+r"(ptr) \
            : \
            : \
        ); \
    } while (0)
#define PAS_MTE_SET_TAG_PAIR_POSTINDEX(ptr) do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "st2g %0, [%0], #32" \
            : "+r"(ptr) \
            : \
            : \
        ); \
    } while (0)
#define PAS_MTE_CREATE_RANDOM_TAG(ptr, mask) do { \
        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_ZERO_TAG_ALL)) { \
            ptr &= (uintptr_t)~PAS_MTE_TAG_MASK; \
            break; \
        } \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "irg %0, %0, %1" \
            : "+r"(ptr) \
            : "r"((uintptr_t)(mask)) \
            : \
        ); \
    } while (0)

/*
 * DC GVA writes tags for a contiguous range of addresses in bulk. The size of this
 * range, and whether or not DC GVA is enabled in hardware, is controlled by the
 * DCZID_EL0 register. Technically, if we wanted to be maximally robust, we would
 * query this register to detect if DC GVA is enabled and if so, how much memory it
 * can tag at once. In practice, DC GVA should always be enabled on PAS_MTE-compatible
 * Apple hardware, with a size of 64 bytes. Because tagging code is so critical to
 * PAS_MTE performance, we assume both of these things are true, saving us the cost of
 * needing to remember enablement and granule size dynamically.
 *
 * In addition, DC GVA requires at least 16-byte alignment, and really ideally
 * 64-byte alignment as far as I am aware. Our usage of this instruction should be
 * careful to respect 64-byte alignment.
 */
#define DC_GVA_GRANULE_SIZE 64
#define PAS_MTE_SET_TAGS_USING_DC_GVA(ptr) do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "dc gva, %0" \
            : \
            : "r"(ptr) \
            : "memory" \
        ); \
    } while (0)

// We call an allocator of taggable objects "homogeneous" if all taggable
// objects allocated by the allocator are the same size, e.g. like is the
// case with any slab allocator.
enum pas_allocator_homogeneity {
  pas_mte_homogeneous_allocator,
  pas_mte_nonhomogeneous_allocator,
};

typedef enum pas_allocator_homogeneity pas_allocator_homogeneity;

enum pas_allocation_initiality {
  pas_initial_allocation,
  pas_maybe_initial_allocation,
  pas_non_initial_allocation,
};

typedef enum pas_allocation_initiality pas_allocation_initiality;

enum pas_mte_tag_constraint {
  pas_mte_any_nonzero_tag = 0x0001,
  pas_mte_odd_tag = 0x5555,
  pas_mte_nonzero_even_tag = 0xaaab,
};

PAS_IGNORE_WARNINGS_BEGIN("implicit-fallthrough")

inline __attribute__((always_inline)) void pas_mte_tag_st2g_loop(uint8_t* begin, size_t size)
{
    uint8_t* end = begin + size;
    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
        printf("[MTE]\t    Tagging initial 16 bytes %p to %p\n", begin, begin + 16);
    PAS_MTE_SET_TAG(begin);

    // Ensure begin is a multiple of 32 bytes from the end.
    begin += size % 32;

    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG) && begin < end)
        printf("[MTE]\t    Doing ST2G loop from %p to %p\n", begin, end);
    while (begin < end)
        PAS_MTE_SET_TAG_PAIR_POSTINDEX(begin);
}

inline __attribute__((always_inline)) void pas_mte_tag_st2g_switching(uint8_t* begin, size_t size)
{
    uint8_t* end = begin + size;
    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
        printf("[MTE]\t    Tagging initial 16 bytes %p to %p\n", begin, begin + 16);
    PAS_MTE_SET_TAG(begin);

    // Ensure begin is a multiple of 32 bytes from the end.
    begin += size % 32;

    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
        printf("[MTE]\t    Doing ST2G loop from %p to %p\n", begin, end);
    while (begin < end) {
        uintptr_t num_granules_to_st2g = (uintptr_t)(end - begin) / (uintptr_t)32 & 15;
        if (!num_granules_to_st2g)
            num_granules_to_st2g = 16;
        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG)) {
            size_t tagged_size = num_granules_to_st2g * 32;
            printf("[MTE]\t        Tagging %zu bytes from %p to %p\n", tagged_size, begin, begin + tagged_size);
        }
        switch (num_granules_to_st2g) {
        case 16: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 480);
        case 15: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 448);
        case 14: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 416);
        case 13: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 384);
        case 12: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 352);
        case 11: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 320);
        case 10: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 288);
        case 9: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 256);
        case 8: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 224);
        case 7: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 192);
        case 6: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 160);
        case 5: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 128);
        case 4: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 96);
        case 3: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 64);
        case 2: PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 32);
        case 1: PAS_MTE_SET_TAG_PAIR(begin); break;
        default: __builtin_unreachable();
        }
        begin += num_granules_to_st2g * 32;
    }
}

inline __attribute__((always_inline)) void pas_mte_tag_dc_gva_loop(uint8_t* begin, size_t size)
{
    /* Get the small-object case out of the way. */
    if (size <= 48) {
        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
            printf("[MTE]\t    Tagging small object with size %zu from %p to %p\n", size, begin, begin + size);
        PAS_MTE_SET_TAG(begin);
        if (size <= 16)
            return;
        PAS_MTE_SET_TAG_PAIR(begin);
        if (size > 32)
            PAS_MTE_SET_TAG_WITH_OFFSET(begin, 32);
        return;
    }

    /* Now that we know the size is at least 64 bytes, we can use DC GVA. */
    /* First, we handle the first 64 bytes, which may not be aligned to 64 bytes. */
    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
        printf("[MTE]\t    Tagging initial 64 bytes from %p to %p\n", begin, begin + 64);
    PAS_MTE_SET_TAG_PAIR(begin);
    PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 32);

    uint8_t* end = begin + size;

    if (size > 128) {
        /* Next, we align our begin and end, in preparation for doing a DC GVA loop. */
        begin = (uint8_t*)((uintptr_t)begin + DC_GVA_GRANULE_SIZE - 1 & (intptr_t)-DC_GVA_GRANULE_SIZE);
        uint8_t* end_aligned = (uint8_t*)((uintptr_t)end & (intptr_t)-DC_GVA_GRANULE_SIZE);

        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
            printf("[MTE]\t    Doing aligned DC GVA loop from %p to %p\n", begin, end_aligned);
        while (begin < end_aligned) {
            PAS_MTE_SET_TAGS_USING_DC_GVA(begin);
            begin += DC_GVA_GRANULE_SIZE;
        }
    }

    /* Handle the last 64 bytes, covering the unaligned remainder we may have */
    /* missed in our DC GVA loop. */
    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
        printf("[MTE]\t    Tagging final 64 bytes from %p to %p\n", end - 64, end);
    PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(end, -64);
    PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(end, -32);
}

inline __attribute__((always_inline)) void pas_mte_tag_dc_gva_known_medium(uint8_t* begin, size_t size)
{
    uint8_t* end = begin + size;
    while (begin < end) {
        PAS_MTE_SET_TAGS_USING_DC_GVA(begin);
        PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE);
        PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 2);
        PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 3);
        PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 4);
        PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 5);
        PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 6);
        PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 7);
        begin += DC_GVA_GRANULE_SIZE * 8;
    }
}

inline __attribute__((always_inline)) void pas_mte_tag_dc_gva_switching(uint8_t* begin, size_t size)
{
    /* Get the small-object case out of the way. */
    if (size <= 48) {
        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
            printf("[MTE]\t    Tagging small object with size %zu from %p to %p\n", size, begin, begin + size);
        PAS_MTE_SET_TAG(begin);
        if (size <= 16)
            return;
        PAS_MTE_SET_TAG_PAIR(begin);
        if (size > 32)
            PAS_MTE_SET_TAG_WITH_OFFSET(begin, 32);
        return;
    }

    /* Now that we know the size is at least 64 bytes, we can use DC GVA. */
    /* First, we handle the first 64 bytes, which may not be aligned to 64 bytes. */
    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
        printf("[MTE]\t    Tagging initial 64 bytes from %p to %p\n", begin, begin + 64);
    PAS_MTE_SET_TAG_PAIR(begin);
    PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(begin, 32);

    uint8_t* end = begin + size;

    if (size > 128) {
        /* Next, we align our begin and end, in preparation for doing a DC GVA loop. */
        begin = (uint8_t*)((uintptr_t)begin + 63 & (intptr_t)-64);
        uint8_t* end_aligned = (uint8_t*)((uintptr_t)end & (intptr_t)-64);

        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
            printf("[MTE]\t    Doing aligned DC GVA loop from %p to %p\n", begin, end_aligned);
        while (begin < end_aligned) {
            uintptr_t num_granules_to_gva = (uintptr_t)(end_aligned - begin) / (uintptr_t)DC_GVA_GRANULE_SIZE % 16;
            if (!num_granules_to_gva)
                num_granules_to_gva = 16;
            if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG)) {
                size_t tagged_size = num_granules_to_gva * DC_GVA_GRANULE_SIZE;
                printf("[MTE]\t        Tagging %zu bytes from %p to %p\n", tagged_size, begin, begin + tagged_size);
            }
            switch (num_granules_to_gva) {
            case 16: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 15);
            case 15: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 14);
            case 14: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 13);
            case 13: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 12);
            case 12: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 11);
            case 11: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 10);
            case 10: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 9);
            case 9: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 8);
            case 8: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 7);
            case 7: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 6);
            case 6: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 5);
            case 5: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 4);
            case 4: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 3);
            case 3: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE * 2);
            case 2: PAS_MTE_SET_TAGS_USING_DC_GVA(begin + DC_GVA_GRANULE_SIZE);
            case 1: PAS_MTE_SET_TAGS_USING_DC_GVA(begin); break;
            default: __builtin_unreachable();
            }
            begin += num_granules_to_gva * DC_GVA_GRANULE_SIZE;
        }
    }

    /* Handle the last 64 bytes, covering the unaligned remainder we may have */
    /* missed in our DC GVA loop. */
    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
        printf("[MTE]\t    Tagging final 64 bytes from %p to %p\n", end - 64, end);
    PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(end, -64);
    PAS_MTE_SET_TAG_PAIR_WITH_OFFSET(end, -32);
}

PAS_IGNORE_WARNINGS_END

#define ASSERT_PRIOR_TAG_IS_DISJOINT(ptr) do { \
        uint8_t* prev_ptr = (uint8_t*)((uintptr_t)ptr - 16); \
        uint8_t* curr_ptr = (uint8_t*)ptr; \
        if (PAS_MTE_SMALL_PAGE_NO(prev_ptr) == PAS_MTE_SMALL_PAGE_NO(curr_ptr)) { \
            PAS_MTE_GET_MTAG(prev_ptr); \
            PAS_MTE_GET_MTAG(curr_ptr); \
            uintptr_t prev_tag = (uintptr_t)prev_ptr & PAS_MTE_TAG_MASK; \
            uintptr_t curr_tag = (uintptr_t)curr_ptr & PAS_MTE_TAG_MASK; \
            if (prev_tag == curr_tag && !curr_tag) \
                printf("[MTE]\tAdjacent tag collision between %p and %p: crashing\n", prev_ptr, curr_ptr); \
            PAS_ASSERT(prev_tag != curr_tag || !curr_tag); \
        } \
    } while (0)

#define TAG_REGION_FROM_POINTER(ptr, size, is_known_medium) do { \
        uint8_t* pas_mte_begin = (uint8_t*)(ptr); \
        size_t pas_mte_size = (size_t)(size); \
        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG)) { \
            void* purified_begin = pas_mte_begin; \
            PAS_MTE_GET_MTAG(purified_begin); \
            printf("[MTE]\tTagging %zu bytes from %p to %p (old tag is %p)\n", pas_mte_size, pas_mte_begin, pas_mte_begin + pas_mte_size, purified_begin); \
        } \
        if (is_known_medium) \
            pas_mte_tag_dc_gva_known_medium(pas_mte_begin, pas_mte_size); \
        else \
            pas_mte_tag_st2g_loop(pas_mte_begin, pas_mte_size); \
    } while (0)

// Purify is used to reload the correct tag for a pointer from tag
// memory. We generally use this when we add to or round down a pointer,
// and need to modify memory at that new address, such as page headers.

#define PAS_MTE_PURIFY(a) do { \
        if (PAS_USE_MTE) { \
            if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_PURIFY)) \
                printf("[MTE]\tPurified %p", (void*)(a)); \
            PAS_MTE_GET_MTAG(a); \
            if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_PURIFY)) \
                printf(" to %p\n", (void*)(a)); \
        } \
    } while (0)

// Clear is used to canonicalize (zero out) the tag bits of a pointer.
// We generally use this when we want to treat the address itself as
// an integer or key, and don't intend to load from it directly.
// We don't check for PAS_MTE enablement in these cases, since on non-PAS_MTE
// hardware, the tag should be zero anyway, and masking off the bits
// should be faster than branching on g_config.

#define PAS_MTE_CLEAR(a) do { \
        a &= ~PAS_MTE_TAG_MASK; \
    } while (0)

#define PAS_MTE_CLEAR_PAIR(a, b) do { \
        a &= ~PAS_MTE_TAG_MASK; \
        b &= ~PAS_MTE_TAG_MASK; \
    } while (0)

// Use these to configure the tagging policy for different sizes. Currently we only
// tag small and medium allocations, in both segregated and bitfit pages. Medium
// allocations should be additionally guarded at runtime by PAS_MTE_MEDIUM_TAGGING_ENABLED.
#define PAS_MTE_ALLOW_TAG_SMALL 1
#define PAS_MTE_ALLOW_TAG_MEDIUM 1

#if PAS_MTE_ALLOW_TAG_SMALL && PAS_MTE_ALLOW_TAG_MEDIUM
#define PAS_MTE_SHOULD_TAG_ALLOCATOR(allocator) (allocator)->is_mte_tagged
#define PAS_MTE_DECIDE_PAGE_CONFIG_TAGGEDNESS(size_category) \
    (size_category == pas_page_config_size_category_small || size_category == pas_page_config_size_category_medium)
// TODO: once we drop support for runtime-differentiating medium tagging, we can
// drop the second half of this statement
#define PAS_MTE_SHOULD_TAG_PAGE(page_config) ((page_config).base.allow_mte_tagging && \
                                          (PAS_MTE_MEDIUM_TAGGING_ENABLED || (page_config).base.page_config_size_category != pas_page_config_size_category_medium))
#define PAS_MTE_IS_KNOWN_MEDIUM_BUMP(allocator) !(allocator)->is_small
#define PAS_MTE_IS_KNOWN_MEDIUM_PAGE(page_config_base) (page_config_base.page_config_size_category == pas_page_config_size_category_medium)
#define PAS_MTE_SHOULD_TAG_SEGREGATED_HEAP(segregated_heap) (segregated_heap->parent_heap && segregated_heap->parent_heap->is_non_compact_heap)
#elif PAS_MTE_ALLOW_TAG_SMALL
#define PAS_MTE_DECIDE_PAGE_CONFIG_TAGGEDNESS(size_category) (size_category == pas_page_config_size_category_small)
#define PAS_MTE_SHOULD_TAG_ALLOCATOR(allocator) (allocator)->is_mte_tagged
#define PAS_MTE_SHOULD_TAG_PAGE(page_config) ((page_config).base.allow_mte_tagging)
#define PAS_MTE_IS_KNOWN_MEDIUM_BUMP(allocator) 0
#define PAS_MTE_IS_KNOWN_MEDIUM_PAGE(page_config) 0
#else
#define PAS_MTE_DECIDE_PAGE_CONFIG_TAGGEDNESS(size_category) (false)

#define PAS_MTE_SHOULD_TAG_ALLOCATOR(allocator) 0
#define PAS_MTE_SHOULD_TAG_PAGE(page_config) 0
#define PAS_MTE_IS_KNOWN_MEDIUM_BUMP(allocator) 0
#define PAS_MTE_IS_KNOWN_MEDIUM_PAGE(page_config) 0
#endif

// Tagging is what actually applies an PAS_MTE tag to an allocation. If the
// pas_allocation_mode passed to this macro is compact, we zero the upper
// bits of the pointer and tag the object with a zero tag. Otherwise, we
// randomly choose a nonzero tag. It's assumed that this macro will be
// invoked with a size that's a multiple of 16, and it's really important
// that the size passed be the allocation size of the object, not the
// actual size.
#define PAS_MTE_TAG_REGION(ptr, size, mode, is_allocator_homogeneous, is_known_medium) do { \
        if (PAS_MTE_SHOULD_STORE_TAG) { \
            if (mode != pas_non_compact_allocation_mode) \
                ptr &= ~PAS_MTE_TAG_MASK; \
            else { \
                if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION) && is_allocator_homogeneous == pas_mte_homogeneous_allocator) { \
                    if ((((uintptr_t)ptr & PAS_MTE_CANONICAL_MASK) / size) & 0x1) \
                        PAS_MTE_CREATE_RANDOM_TAG(ptr, pas_mte_odd_tag); \
                    else \
                        PAS_MTE_CREATE_RANDOM_TAG(ptr, pas_mte_nonzero_even_tag); \
                } else \
                    PAS_MTE_CREATE_RANDOM_TAG(ptr, pas_mte_any_nonzero_tag); \
            } \
            if (mode != pas_always_compact_allocation_mode) { \
                TAG_REGION_FROM_POINTER(ptr, size, is_known_medium); \
                if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION) \
                    && PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_ASSERT_ADJACENT_TAGS_ARE_DISJOINT) \
                    && is_allocator_homogeneous == pas_mte_homogeneous_allocator) { \
                    ASSERT_PRIOR_TAG_IS_DISJOINT(ptr); \
                    ASSERT_PRIOR_TAG_IS_DISJOINT(ptr + size); \
                } \
            } \
        } \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

PAS_ALWAYS_INLINE uintptr_t
pas_mte_maybe_tag_allocated_region(
    uintptr_t begin,
    size_t size,
    pas_allocation_mode mode,
    pas_allocator_homogeneity homogeneity,
    pas_allocation_initiality initiality,
    bool is_known_medium);

PAS_ALWAYS_INLINE uintptr_t
pas_mte_retag_freed_region_if_tagged(
    uintptr_t begin,
    size_t size,
    pas_page_base_config page_config,
    pas_allocator_homogeneity homogeneity);

#ifdef __cplusplus
}
#endif

/*
 * MTE can be used to protect against both spatial (e.g. out-of-bounds) and
 * temporal (e.g. use-after-free) memory safety violations.
 *
 * Spatial protections are simple: we just need to ensure that, at some point
 * before an allocated object is returned by the allocator, we have tagged the
 * bytes corresponding to that allocated object with a memory tag 'reasonably
 * likely' to be distinct from other allocations. For libpas that means choosing
 * a random tag via irg + filtering out the tags of the allocations immediately
 * before and after it.
 *
 * Temporal protections are slightly more complicated, because we have more
 * choices for when to tag objects. To prevent all use-after-frees, an allocator
 * should ideally retag the bytes corresponding to a freed object before
 * returning from the call to free().
 * Considering a series of allocations and frees for a single object, where
 * '[' denotes allocation, ']' deallocation, and '-' the lifetime of the object,
 * and '^' denotes the point at which an allocation policy tags the object:
 *                         [-----]    [--------------]            [-----]
 *      Tag-on-Alloc:      ^          ^                           ^
 *      Tag-on-Free:             ^                   ^                  ^
 * Clearly both policies are deficient -- Tag-on-Alloc does not protect against
 * UaFs prior to the subsequent allocation, while Tag-on-Free does not protect
 * against anything for the first allocation in a slot. The pessimizing solution
 * would be to do something like
 *      Tag-on-Alloc/Free: ^     ^    ^              ^            ^     ^
 * But not all of these allocations are necessary, and we can save some overhead
 * by slightly relaxing our tagging policy. Rather than always tagging on alloc,
 * we only tag the first allocation for a given slot, then thereafter re-tag on
 * every free of an object in that slot, while skipping the tagging step for
 * all subsequent allocations (which are pas_non_initial_allocation). A la:
 *      Retag-on-Free:     ^     ^                   ^                  ^
 * This is safe because we do not give the user knowledge of the correct tag
 * for the retagged slot until the slot is used for a subsequent allocation,
 * so at allocation-time we can safely re-use the tag without impairing our
 * ability to catch use-after-frees.
 *
 * Of course, as alluded to earlier, the 'Retag-on-Free' policy is only usable
 * when allocating in a slot freed by an object of the same size. Bitfit
 * objects do not generally hold to this property, and while it would be
 * possible to track metadata to determine when it is, doing so would be more
 * costly than it would be worth, so instead we conservatively tag them on
 * both allocation and free.
 * As such, the effective tagging policies are as such:
 *                         [-----]    [--------------]            [-----]
 *      Segregated heaps:  ^     ^                   ^                  ^
 *      Bitfit heaps:      ^     ^    ^              ^            ^     ^
 *
 * N.b.: the current implementation (as the name RETAG_ON_SCAVENGE
 * implies) does not retag-on-free, but on scavenge. This somewhat weakens
 * the ability of this approach to catch use-after-frees, but otherwise does
 * not break the validity of the tag/no-tag orderings above.
 *
 * the point of view of the orderings mentioned above, as scavenging always
 * happens at some point before a subsequent allocation.
 */
PAS_ALWAYS_INLINE uintptr_t
pas_mte_maybe_tag_allocated_region(
    uintptr_t begin,
    size_t size,
    pas_allocation_mode mode,
    pas_allocator_homogeneity homogeneity,
    pas_allocation_initiality initiality,
    bool is_known_medium)
{
    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_RETAG_ON_SCAVENGE)
        && initiality == pas_non_initial_allocation
        && mode == pas_non_compact_allocation_mode) {
        /* can assume: size >= 16 && begin % 16 == 0 */
        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG))
            printf("[MTE]\tSkipping alloc-tagging %zu bytes from %p to %p\n", size, (uint8_t*)begin, (uint8_t*)begin + size);
        PAS_MTE_PURIFY(begin);
    } else {
        if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG) && initiality != pas_non_initial_allocation) {
            const char* qualifier = (initiality == pas_initial_allocation) ? "First" : "Maybe first";
            printf("[MTE]\t%s time tagging region: alloc-tagging %zu bytes from %p to %p\n", qualifier, size, (uint8_t*)begin, (uint8_t*)begin + size);
        }
        PAS_MTE_TAG_REGION(begin, size, mode, homogeneity, is_known_medium);
    }
    return begin;
}

PAS_ALWAYS_INLINE uintptr_t
pas_mte_retag_freed_region_if_tagged(
    uintptr_t begin,
    size_t size,
    pas_page_base_config page_config,
    pas_allocator_homogeneity homogeneity)
{
    if (!PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_RETAG_ON_SCAVENGE))
        return begin;
    uintptr_t tag = begin;
    PAS_MTE_GET_MTAG(tag);
    tag &= PAS_MTE_TAG_MASK;
    /* libpas ensures that all allocations which reside in pages with
     * backing tag memory are tagged with a non-zero tag at all points
     * in time after they've been allocated, so we can use this to see
     * whether the allocation should be retagged or not.
     * In the future it would be better to pipe the information through
     * so that we can save the LDG, but that will require moving the
     * source-of-truth out of the local allocator. */
    if (tag)
        PAS_MTE_TAG_REGION(begin, size, pas_non_compact_allocation_mode, homogeneity, PAS_MTE_IS_KNOWN_MEDIUM_PAGE(page_config));
    return begin;
}

// We leave the majority of the view to be tagged as individual segregated
// allocations are slab-allocated from within it. All we need to do here is
// zero-tag the trailing-buffer which the shared view shared-allocator leaves
// at the end of the new partial view.
#define PAS_MTE_TAG_BUMP_ALLOCATION_FOR_PARTIAL_VIEW(page_config, page, view, bump, mode) do { \
        if (mode != pas_always_compact_allocation_mode) { \
            uintptr_t page_boundary = (uintptr_t)pas_page_base_boundary(&page->base, page_config.base); \
            uintptr_t ptr = page_boundary + (bump.new_bump - 16); \
            TAG_REGION_FROM_POINTER(ptr, 16, PAS_MTE_IS_KNOWN_MEDIUM_PAGE(page_config.base)); \
            if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_ON_TAG)) { \
                uintptr_t bump_base = page_boundary + bump.old_bump; \
                printf("[MTE]\tTagging 16 bytes from %p for trailing-buffer of partial view %p, bump starting at %p\n", (void*)ptr, view, (void*)bump_base); \
            } \
        } \
    } while (0)

// When zeroing out memory we need to be careful to not clear its tagged status.
// Neither memset nor mach_vm_behavior_set will do so, but re-mapping the page
// with mmap or mach_vm_map will do so unless we force it to use PAS_VM_MTE.
// This has the side effect of making *non*-PAS_MTE pages into tagged memory, but
// the only side effect of that should be a small hit to performance -- which
// will have to suffice until we can start using mach_vm_behavior_set.

#if PAS_OS(DARWIN)

#define PAS_MTE_ZERO_FILL_PAGE(ptr, size, flags, tag) do { \
        (void)flags; \
        if (PAS_USE_MTE) { \
            const vm_inherit_t childProcessInheritance = VM_INHERIT_DEFAULT; \
            const bool copy = false; \
            /* FIXME: use mach_vm_behavior_set instead rdar://160813532 */ \
            kern_return_t vm_map_result = mach_vm_map(mach_task_self(), \
                (mach_vm_address_t*)&ptr, \
                (size), \
                0, \
                VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE | PAS_VM_MTE | (tag), MEMORY_OBJECT_NULL, \
                0, \
                copy, \
                VM_PROT_DEFAULT, \
                VM_PROT_ALL, \
                childProcessInheritance); \
            if (vm_map_result != KERN_SUCCESS) \
                errno = 0; \
            PAS_ASSERT(vm_map_result == KERN_SUCCESS); \
            /* Early exit from caller function since we've done the zero-fill ourselves */ \
            return; \
        } \
    } while (false) \

#else
#define PAS_MTE_ZERO_FILL_PAGE(ptr, size, flags, tag) do { \
          (void)ptr; \
          (void)size; \
          (void)flags; \
          (void)tag; \
      } while (false)
#endif // PAS_OS(DARWIN)

// This is no longer needed as the pointer is already tagged in preparation for
// being returned to the caller of the allocation function.
#define PAS_MTE_HANDLE_ZERO_ALLOCATION_RESULT(a) do { (void)a; } while (false)

// Used to zero an existing page allocation without clearing the tagged-memory
// bit in its page-table entries.
#define PAS_MTE_HANDLE_ZERO_FILL_PAGE(ptr, size, flags, tag) PAS_MTE_ZERO_FILL_PAGE(ptr, size, flags, tag)

// Used to clear the tag before we look up the address in the megapage table when reallocating.
#define PAS_MTE_HANDLE_REALLOCATE(a) PAS_MTE_CLEAR(a)

// Used to restore the correct tag when reallocating something to a new address before copying it.
#define PAS_MTE_HANDLE_TRY_REALLOCATE_AND_COPY(ptr, old_ptr, size) do { \
        if (PAS_USE_MTE) { \
            PAS_MTE_CHECK_TAG_AND_SET_TCO(ptr); \
            memcpy((void*)ptr, old_ptr, size); \
            PAS_MTE_CLEAR_TCO; \
            if (verbose) { \
                pas_log("\t...done copying size %zu from %p to %p\n", size, old_ptr, (void*)ptr); \
            } \
            /* Early exit from caller function since \
             * we've done the copy ourselves */ \
            return result; \
        } \
    } while (false)

// Used to clear the tag from a pointer into a page, since the page header should
// be zero-tagged.
#define PAS_MTE_HANDLE_PAGE_BASE_FROM_BOUNDARY(a) PAS_MTE_CLEAR(a)

// Used to clear pointer tag bits before looking it up in the page header table.
#define PAS_MTE_HANDLE_PAGE_HEADER_TABLE_GET(a) PAS_MTE_CLEAR(a)

// Used to clear pointer tag bits before adding it to the page header table.
#define PAS_MTE_HANDLE_PAGE_HEADER_TABLE_ADD(a) PAS_MTE_CLEAR(a)

// Used to clear key tag bits before looking up a pointer in the large map.
#define PAS_MTE_HANDLE_LARGE_MAP_FIND(a) PAS_MTE_CLEAR(a)

// Used to clear key tag bits before inserting a pointer range into the large map.
#define PAS_MTE_HANDLE_LARGE_MAP_ADD(a, b) PAS_MTE_CLEAR(a)

// Used to clear key tag bits before taking a pointer from the large map.
#define PAS_MTE_HANDLE_LARGE_MAP_TAKE(a) PAS_MTE_CLEAR(a)

// Used to restore the correct tag of a large map entry key after looking it up.
// Takes a pas_heap_config*
#define PAS_MTE_HANDLE_LARGE_MAP_FOUND_ENTRY(config, a, b) PAS_MTE_PURIFY(a)

// Used to restore the correct tag of a large map entry key after taking it from the map.
// Takes a pas_heap_config*
#define PAS_MTE_HANDLE_LARGE_MAP_TOOK_ENTRY(config, a, b) PAS_MTE_PURIFY(a)

// Used to clear pointer tag bits before taking a pointer from the large map.
// Takes a pas_heap_config*
#define PAS_MTE_HANDLE_PGM_ALLOCATE(config, a) PAS_MTE_CLEAR(a)

// Used to clear pointer tag bits before taking a pointer from the large map.
#define PAS_MTE_HANDLE_PGM_DEALLOCATE(a) PAS_MTE_CLEAR(a)

// Used to clear pointer tag bits before putting a pointer to the megapage table.
#define PAS_MTE_HANDLE_MEGAPAGE_SET(a) PAS_MTE_CLEAR(a)

// Used to clear pointer tag bits before getting a pointer from the megapage table.
#define PAS_MTE_HANDLE_MEGAPAGE_GET(a) PAS_MTE_CLEAR(a)

// Used to clear pointer tag bits freeing a range in the large sharing pool.
#define PAS_MTE_HANDLE_LARGE_SHARING_POOL_BOOT_FREE(a, b) PAS_MTE_CLEAR_PAIR(a, b)

// Used to clear pointer tag bits freeing a range in the large sharing pool.
#define PAS_MTE_HANDLE_LARGE_SHARING_POOL_FREE(a, b) PAS_MTE_CLEAR_PAIR(a, b)

// Used to clear pointer tag bits allocating a range in the large sharing pool.
#define PAS_MTE_HANDLE_LARGE_SHARING_POOL_ALLOCATE_AND_COMMIT(a, b) PAS_MTE_CLEAR_PAIR(a, b)

// Used to clear pointer tag bits when summarizing a range in the large sharing pool.
#define PAS_MTE_HANDLE_LARGE_SHARING_POOL_COMPUTE_SUMMARY(a, b) PAS_MTE_CLEAR_PAIR(a, b)

#define PAS_SHOULD_MTE_TAG_BASIC_HEAP_PAGE(size_category) PAS_MTE_DECIDE_PAGE_CONFIG_TAGGEDNESS(size_category)

struct __pas_heap;

#ifdef __cplusplus
extern "C" {
#endif
extern struct __pas_heap bmalloc_common_primitive_heap;
#ifdef __cplusplus
}
#endif

// It's possible for users to allocate memory from a pas_heap prior to ever
// inducing libpas to go down the pas_page_malloc path -- e.g. if they only use
// the system allocator, or heaps which use memory allocated by the user.
// However, all such allocations have to go down the pas-heap initialization
// path -- so we can intercept here to catch those cases.
// This is not sufficient on its own, however, as it's theoretically possible
// for libpas to allocate PAS_MTE memory on its own, e.g. via the utility heap.
// N.b.: `heap` is empty (nullptr) at the time when this macro is used, so we
// cannot actually make use of it. We take it as a parameter as a bandaid over
// a spurious unused-variable warning that clang sometimes throws otherwise:
// see rdar://157158045
#define PAS_MTE_HANDLE_ENSURE_HEAP_SLOW(heap, heap_ref, heap_ref_kind, heap_config, runtime_config) do { \
        (void)heap; \
        (void)heap_ref; \
        (void)heap_ref_kind; \
        (void)heap_config; \
        (void)runtime_config; \
        pas_mte_ensure_initialized(); \
    } while (false)

// Used to set up whether a local allocator should tag its allocations.
#define PAS_MTE_HANDLE_SET_UP_LOCAL_ALLOCATOR(page_config, segregated_heap, allocator) do { \
        if (PAS_USE_MTE && PAS_MTE_SHOULD_TAG_SEGREGATED_HEAP(segregated_heap)) { \
            allocator->is_mte_tagged = PAS_MTE_SHOULD_TAG_PAGE(page_config); \
            allocator->is_small = (page_config).base.page_config_size_category == pas_page_config_size_category_small; \
        } else \
            allocator->is_mte_tagged = false; \
    } while (false)

// Used to tag bump allocations from a local allocator.
#define PAS_MTE_HANDLE_LOCAL_BUMP_ALLOCATION(heap_config, allocator, ptr, size, mode) do { \
        if (PAS_MTE_SHOULD_TAG_ALLOCATOR(allocator)) \
            ptr = pas_mte_maybe_tag_allocated_region(ptr, (size_t)size, mode, pas_mte_homogeneous_allocator, pas_initial_allocation, PAS_MTE_IS_KNOWN_MEDIUM_BUMP(allocator)); \
    } while (false)

// Used to tag free-bit scanning allocations from a local allocator.
#define PAS_MTE_HANDLE_LOCAL_FREEBITS_ALLOCATION(page_config, ptr, allocator, mode) do { \
        if (PAS_MTE_SHOULD_TAG_ALLOCATOR(allocator)) \
            ptr = pas_mte_maybe_tag_allocated_region(ptr, (size_t)allocator->object_size, mode, pas_mte_homogeneous_allocator, pas_non_initial_allocation, PAS_MTE_IS_KNOWN_MEDIUM_PAGE(page_config.base)); \
    } while (false)

// Used to tag bitfit allocations.
#define PAS_MTE_HANDLE_BITFIT_ALLOCATION(page_config, ptr, size, mode) do { \
        if (PAS_USE_MTE && PAS_MTE_SHOULD_TAG_PAGE(page_config)) \
            ptr = pas_mte_maybe_tag_allocated_region(ptr, (size_t)size, mode, pas_mte_nonhomogeneous_allocator, pas_maybe_initial_allocation, PAS_MTE_IS_KNOWN_MEDIUM_PAGE(page_config.base)); \
    } while (false)

// Logic for tagging system heap (aka system malloc) allocations. These are used
// in production in some services/daemons to avoid using up memory for both
// libpas and system malloc metadata, but since the system malloc also supports
// PAS_MTE and some of these services have PAS_MTE enabled, we need to ensure
// allocations we expect to be zero-tagged under PAS_MTE are zero-tagged in this
// mode too.
//
// At the time of writing, malloc_zone_malloc_with_options_np with
// MALLOC_NP_OPTION_CANONICAL_TAG is the preferred means of doing this. Since
// we don't have an equivalent for realloc yet, we do our own malloc + copy +
// free sequence instead.

// Allowed argument values (as dictated by malloc_zone_malloc_with_options_np):
//  - alignment: 0 for unaligned, or a power of 2 >= sizeof(void*).
//  - size: any if alignment == 0, a multiple of the alignment otherwise.

inline __attribute__((always_inline)) void* pas_mte_system_heap_malloc_zero_tagged(malloc_zone_t* zone, size_t alignment, size_t size)
{
PAS_IGNORE_WARNINGS_BEGIN("deprecated-declarations")
    return malloc_zone_malloc_with_options_np(zone, alignment, size, MALLOC_NP_OPTION_CANONICAL_TAG);
PAS_IGNORE_WARNINGS_END
}

inline __attribute__((always_inline)) void* pas_mte_system_heap_zeroed_malloc_zero_tagged(malloc_zone_t* zone, size_t alignment, size_t size)
{
PAS_IGNORE_WARNINGS_BEGIN("deprecated-declarations")
    return malloc_zone_malloc_with_options_np(zone, alignment, size, (malloc_options_np_t)(MALLOC_NP_OPTION_CANONICAL_TAG | MALLOC_NP_OPTION_CLEAR));
PAS_IGNORE_WARNINGS_END
}

#ifdef __cplusplus
extern "C" {
#endif
void* pas_mte_system_heap_realloc_zero_tagged(malloc_zone_t* zone, void* ptr, size_t size);
#ifdef __cplusplus
}
#endif

#define PAS_MTE_HANDLE_SYSTEM_HEAP_ALLOCATION(systemHeap, size, alignment, mode) do { \
        if ((mode) != pas_non_compact_allocation_mode) \
            return pas_mte_system_heap_malloc_zero_tagged(systemHeap->zone(), (alignment), (size)); \
    } while (false)

#define PAS_MTE_HANDLE_SYSTEM_HEAP_REALLOCATION(systemHeap, ptr, size, mode) do { \
        if (mode != pas_non_compact_allocation_mode) \
            return pas_mte_system_heap_realloc_zero_tagged(systemHeap->zone(), (ptr), (size)); \
    } while (false)

// Used to tag bump allocations in the primordial heap.
// Non-homogeneous because this comes from a partial view, meaning other
// allocators can use the same page.
// Takes a pas_segregated_page_config
#define PAS_MTE_HANDLE_PRIMORDIAL_BUMP_ALLOCATION(page_config, ptr, size, mode) do { \
        /* Even though this is a bump allocation, because we have the page_config */ \
        /* handy, we use the page instead of the allocator for purposes of checking */ \
        /* if this allocation should be tagged. */ \
        if (PAS_USE_MTE && PAS_MTE_SHOULD_TAG_PAGE(page_config)) \
            pas_mte_maybe_tag_allocated_region(ptr, (size_t)size, mode, pas_mte_homogeneous_allocator, pas_initial_allocation, PAS_MTE_IS_KNOWN_MEDIUM_PAGE(page_config.base)); \
    } while (false)

// Used to bail from allocating megapages from the megapage large heap if PAS_MTE is disabled.
// The non-MTE default is to use the megapage large heap for any non-compact megapage
// allocation, which is what we want in an PAS_MTE world, but splitting up the page sources incurs
// a modest but significant performance/memory overhead when PAS_MTE is disabled. This is part of
// the inevitable overhead of enabling PAS_MTE, but we don't want to burden non-PAS_MTE hardware with
// this cost, so we inject this early return.
#define PAS_MTE_HANDLE_MEGAPAGES_ALLOCATION(heap, size, alignment, heap_config) do { \
        if (!PAS_USE_MTE) { \
            return pas_large_heap_try_allocate_and_forget( \
                &heap->large_heap, size, alignment, pas_non_compact_allocation_mode, \
                heap_config, transaction); \
        } \
    } while (false)

// Used to tag the trailing-buffer bytes of a partial view when it is first
// committed and becomes ready for use as an allocator.
#define PAS_MTE_HANDLE_POPULATE_PRIMORDIAL_PARTIAL_VIEW(page_config, page, view, bump_result, mode) do { \
        if (PAS_USE_MTE) { \
            if (PAS_MTE_SHOULD_TAG_PAGE(page_config)) \
                PAS_MTE_TAG_BUMP_ALLOCATION_FOR_PARTIAL_VIEW(page_config, page, view, bump_result, mode); \
        } \
    } while (false)

// Used to redirect small megapage allocations when PAS_MTE is not enabled to the respective untagged megapage cache.
#define PAS_MTE_HANDLE_SMALL_SHARED_SEGREGATED_PAGE_ALLOCATION(heap, megapage_cache) do { \
        if (!PAS_USE_MTE || !heap->parent_heap->is_non_compact_heap) \
            megapage_cache = &page_caches->small_compact_other_megapage_cache; \
    } while (false)
#define PAS_MTE_HANDLE_SMALL_EXCLUSIVE_SEGREGATED_PAGE_ALLOCATION(heap, megapage_cache) do { \
        if (!PAS_USE_MTE || !heap->parent_heap->is_non_compact_heap) \
            megapage_cache = &page_caches->small_compact_exclusive_segregated_megapage_cache; \
    } while (false)
#define PAS_MTE_HANDLE_SMALL_BITFIT_PAGE_ALLOCATION(heap, megapage_cache) do { \
        if (!PAS_USE_MTE || !heap->parent_heap->is_non_compact_heap) \
            megapage_cache = &page_caches->small_compact_other_megapage_cache; \
    } while (false)

// Used to redirect medium megapage allocations when medium object tagging is not enabled to the respective untagged megapage cache.
#define PAS_MTE_HANDLE_MEDIUM_SEGREGATED_PAGE_ALLOCATION(heap, megapage_cache) do { \
        if (!PAS_MTE_MEDIUM_TAGGING_ENABLED || !heap->parent_heap->is_non_compact_heap) \
            megapage_cache = &page_caches->medium_compact_megapage_cache; \
    } while (false)
#define PAS_MTE_HANDLE_MEDIUM_BITFIT_PAGE_ALLOCATION(heap, megapage_cache) do { \
        if (!PAS_MTE_MEDIUM_TAGGING_ENABLED || !heap->parent_heap->is_non_compact_heap) \
            megapage_cache = &page_caches->medium_compact_megapage_cache; \
    } while (false)

// Used to tacitly redirect all marge megapage allocations to the untagged megapage cache.
#define PAS_MTE_HANDLE_MARGE_BITFIT_PAGE_ALLOCATION(heap, megapage_cache) do { \
        megapage_cache = &page_caches->medium_compact_megapage_cache; \
    } while (false)

// Used to tag the memory left behind by objects freed from bitfit heaps.
#define PAS_MTE_HANDLE_BITFIT_PAGE_DEALLOCATION(page_config, ptr, size) do { \
        (void)page_config; \
        (void)ptr; \
        (void)size; \
        if (PAS_USE_MTE && PAS_MTE_SHOULD_TAG_PAGE(page_config)) \
            ptr = pas_mte_retag_freed_region_if_tagged((uintptr_t)ptr, (size_t)size, page_config.base, pas_mte_nonhomogeneous_allocator); \
    } while (false)

// Used to tag the memory left behind by objects freed from segregated heaps.
#define PAS_MTE_HANDLE_SEGREGATED_PAGE_DEALLOCATION(page_config, ptr, size) do { \
        (void)page_config; \
        (void)ptr; \
        (void)size; \
        if (PAS_USE_MTE && PAS_MTE_SHOULD_TAG_PAGE(page_config)) \
            ptr = pas_mte_retag_freed_region_if_tagged((uintptr_t)ptr, (size_t)size, page_config.base, pas_mte_homogeneous_allocator); \
    } while (false)

#define PAS_MTE_HANDLE_SCAVENGER_THREAD_MAIN(data) do { \
        pas_mte_ensure_initialized(); \
    } while (false)

#if PAS_OS(DARWIN)
#define PAS_MTE_HANDLE_PAGE_ALLOCATION(size, is_small, tag) do { \
        pas_mte_ensure_initialized(); \
        if (PAS_USE_MTE && (is_small)) { \
            const vm_inherit_t childProcessInheritance = VM_INHERIT_DEFAULT; \
            const bool copy = false; \
            const vm_prot_t protections = VM_PROT_WRITE | VM_PROT_READ; \
            kern_return_t vm_map_result = mach_vm_map(mach_task_self(), (mach_vm_address_t*)&mmap_result, (size), pas_page_malloc_alignment() - 1, VM_FLAGS_ANYWHERE | PAS_VM_MTE | (tag), MEMORY_OBJECT_NULL, 0, copy, protections, protections, childProcessInheritance); \
            if (vm_map_result != KERN_SUCCESS) { \
                errno = 0; \
                if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_PAGE_ALLOC)) \
                    printf("[MTE]\tFailed to map %zu bytes with VM_FLAGS_MTE.\n", (size_t)(size)); \
                return NULL; \
            } \
            if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_LOG_PAGE_ALLOC)) \
                printf("[MTE]\tMapped %zu bytes from %p to %p with VM_FLAGS_MTE.\n", (size_t)(size), (void*)(mmap_result), (uint8_t*)(mmap_result) + (size)); \
            return mmap_result; \
        } \
    } while (false)
#else // PAS_OS(DARWIN) -> !PAS_OS(DARWIN)
#define PAS_MTE_HANDLE_PAGE_ALLOCATION(a, b) do { \
        (void)(a); \
        (void)(b); \
    } while (false)
#endif // !PAS_OS(DARWIN)

#define PAS_MTE_HANDLE(kind, ...) \
    PAS_MTE_HANDLE_ ## kind(__VA_ARGS__)

PAS_IGNORE_WARNINGS_END

#else // !PAS_ENABLE_MTE
#define PAS_MTE_HANDLE(kind, ...) PAS_UNUSED_V(__VA_ARGS__)
#define PAS_SHOULD_MTE_TAG_BASIC_HEAP_PAGE(size_category) (false)
#endif // PAS_ENABLE_MTE
#else // defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#define PAS_MTE_HANDLE(kind, ...) PAS_UNUSED_V(__VA_ARGS__)
#define PAS_SHOULD_MTE_TAG_BASIC_HEAP_PAGE(size_category) (false)
#endif // PAS_ENABLE_MTE
#endif // PAS_MTE_H
