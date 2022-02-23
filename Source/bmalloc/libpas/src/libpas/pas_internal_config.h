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

#ifndef PAS_INTERNAL_CONFIG_H
#define PAS_INTERNAL_CONFIG_H

#include "pas_config.h"

#define PAS_SUBPAGE_SIZE                 4096

#define PAS_USE_SMALL_SEGREGATED_OVERRIDE     true
#define PAS_USE_MEDIUM_SEGREGATED_OVERRIDE    true
#define PAS_USE_SMALL_BITFIT_OVERRIDE         true
#define PAS_USE_MEDIUM_BITFIT_OVERRIDE        true
#define PAS_USE_MARGE_BITFIT_OVERRIDE         true

/* The OS may have a smaller page size. That's OK. */
#define PAS_SMALL_PAGE_DEFAULT_SHIFT     14
#define PAS_SMALL_PAGE_DEFAULT_SIZE      ((size_t)1 << PAS_SMALL_PAGE_DEFAULT_SHIFT)

#define PAS_SMALL_BITFIT_PAGE_DEFAULT_SHIFT   14
#define PAS_SMALL_BITFIT_PAGE_DEFAULT_SIZE    ((size_t)1 << PAS_SMALL_BITFIT_PAGE_DEFAULT_SHIFT)

#define PAS_MEDIUM_PAGE_DEFAULT_SHIFT    17
#define PAS_MEDIUM_PAGE_DEFAULT_SIZE     ((size_t)1 << PAS_MEDIUM_PAGE_DEFAULT_SHIFT)

#define PAS_MARGE_PAGE_DEFAULT_SHIFT     22
#define PAS_MARGE_PAGE_DEFAULT_SIZE      ((size_t)1 << PAS_MARGE_PAGE_DEFAULT_SHIFT)

#if PAS_ARM64 || PAS_PLATFORM(PLAYSTATION)
#define PAS_GRANULE_DEFAULT_SHIFT        14
#else
#define PAS_GRANULE_DEFAULT_SHIFT        12
#endif
#define PAS_GRANULE_DEFAULT_SIZE         ((size_t)1 << PAS_GRANULE_DEFAULT_SHIFT)

#define PAS_FAST_MEGAPAGE_SHIFT          24
#define PAS_FAST_MEGAPAGE_SIZE           ((size_t)1 << PAS_FAST_MEGAPAGE_SHIFT)

#define PAS_MEDIUM_MEGAPAGE_SHIFT        24
#define PAS_MEDIUM_MEGAPAGE_SIZE         ((size_t)1 << PAS_MEDIUM_MEGAPAGE_SHIFT)

#define PAS_MIN_ALIGN_SHIFT              4
#define PAS_MIN_ALIGN                    ((size_t)1 << PAS_MIN_ALIGN_SHIFT)

/* This is the same as PAS_BITVECTOR_WORD_SHIFT, which is a nice performance optimization but
   isn't necessary. It's a performance optimization because there are specialized fast code
   paths for sharing_shift == PAS_BITVECTOR_WORD_SHIFT. */
#define PAS_SMALL_SHARING_SHIFT          5

#define PAS_APPROXIMATE_LARGE_SIZE_SHIFT 10
#define PAS_APPROXIMATE_LARGE_SIZE       ((size_t)1 << PAS_APPROXIMATE_LARGE_SIZE_SHIFT)

/* This is the minimum object size in small allocators and we rely on this to lay out alloc
   bits. */
#define PAS_MIN_OBJECT_SIZE_SHIFT        4
#define PAS_MIN_OBJECT_SIZE              ((size_t)1 << PAS_MIN_OBJECT_SIZE_SHIFT)

#define PAS_MIN_MEDIUM_ALIGN_SHIFT       9
#define PAS_MIN_MEDIUM_ALIGN             ((size_t)1 << PAS_MIN_MEDIUM_ALIGN_SHIFT)

#define PAS_MIN_MARGE_ALIGN_SHIFT        12
#define PAS_MIN_MARGE_ALIGN              ((size_t)1 << PAS_MIN_MARGE_ALIGN_SHIFT)

#define PAS_MEDIUM_SHARING_SHIFT         3

#define PAS_MIN_OBJECTS_PER_PAGE         6

/* Expresses the probability using integers in the range 0..UINT_MAX (inclusive) */
#define PAS_PROBABILITY_OF_SHARED_PAGE_INELIGIBILITY 0x1fffffff

#ifdef PAS_LIBMALLOC
#define PAS_DEALLOCATION_LOG_SIZE        10
#define PAS_DEALLOCATION_LOG_MAX_BYTES   1000
#else
#define PAS_DEALLOCATION_LOG_SIZE        1000
#define PAS_DEALLOCATION_LOG_MAX_BYTES   50000
#endif

#define PAS_NUM_STATIC_FAST_MEGAPAGE_TABLES   0
#define PAS_USE_DYNAMIC_FAST_MEGAPAGE_TABLE   1

#define PAS_NUM_FAST_FAST_MEGAPAGE_BITS  524288

#define PAS_MAX_OBJECT_SIZE(payload_size) ((size_t)(payload_size) / (size_t)PAS_MIN_OBJECTS_PER_PAGE)

#define PAS_INTRINSIC_SMALL_LOOKUP_SIZE_UPPER_BOUND 10000
#define PAS_SMALL_LOOKUP_SIZE_UPPER_BOUND 500
#define PAS_UTILITY_LOOKUP_SIZE_UPPER_BOUND 1400

#define PAS_NUM_INTRINSIC_SIZE_CLASSES \
    ((PAS_INTRINSIC_SMALL_LOOKUP_SIZE_UPPER_BOUND >> PAS_MIN_ALIGN_SHIFT) + 1)

#define PAS_NUM_UTILITY_SIZE_CLASSES \
    ((PAS_UTILITY_LOOKUP_SIZE_UPPER_BOUND >> PAS_INTERNAL_MIN_ALIGN_SHIFT) + 1)

#define PAS_SIZE_CLASS_PROGRESSION       1.3

#define PAS_SMALL_PAGE_HANDICAP          1.
#define PAS_MEDIUM_PAGE_HANDICAP         1.05

#define PAS_SMALL_BITFIT_PAGE_HANDICAP   1.
#define PAS_MEDIUM_BITFIT_PAGE_HANDICAP  1.05

#define PAS_MAX_LARGE_ALIGNMENT_WASTEAGE 1.3

#define PAS_BITS_TTL                     16

#define PAS_NUM_BASELINE_ALLOCATORS      32u

#define PAS_MAX_OBJECTS_PER_PAGE         2048

#define PAS_MPROTECT_DECOMMITTED         PAS_ENABLE_TESTING

/* Currently, enabling page balancing is likely to be super bad for performance. I've done a bunch of
   optimizations that make the !page_balancing case work much better and those optimizations have likely
   made !!page_balancing a lot worse in terms of time performance. */
#ifdef PAS_LIBMALLOC
#define PAS_PAGE_BALANCING_ENABLED            true
#else
#define PAS_PAGE_BALANCING_ENABLED            false
#endif

#define PAS_SMALL_SHARED_PAGE_LOG_SHIFT     31
#define PAS_MEDIUM_SHARED_PAGE_LOG_SHIFT    31

/* Must be zero; utility doesn't support partials right now (though it could if we hacked deallocation and
   added a shared page directory). */
#define PAS_UTILITY_BOUND_FOR_PARTIAL_VIEWS                     0
#define PAS_UTILITY_BOUND_FOR_BASELINE_ALLOCATORS               0
#define PAS_UTILITY_BOUND_FOR_NO_VIEW_CACHE                     UINT_MAX
#define PAS_UTILITY_MAX_SEGREGATED_OBJECT_SIZE                  UINT_MAX
#define PAS_UTILITY_MAX_BITFIT_OBJECT_SIZE                      0

/* Intrinsic heaps are for global, singleton, process-wide heaps -- so the "not isoheaped" heap, the
   thing you get from regular malloc. */
#define PAS_INTRINSIC_BOUND_FOR_PARTIAL_VIEWS                   0
#define PAS_INTRINSIC_BOUND_FOR_BASELINE_ALLOCATORS             0
#define PAS_INTRINSIC_BOUND_FOR_NO_VIEW_CACHE                   0
#define PAS_INTRINSIC_MAX_SEGREGATED_OBJECT_SIZE                UINT_MAX
#define PAS_INTRINSIC_MAX_BITFIT_OBJECT_SIZE                    UINT_MAX

/* This is for heaps that hold primitives but should be kept separate from the main heap. So, the
   tuning has to be such that we are efficient for lots of small heaps. But, there's no restriction
   on whether the data in the heap is reused in any particular way -- we run this with a type size
   of 1. */
#define PAS_PRIMITIVE_BOUND_FOR_PARTIAL_VIEWS                   0
#define PAS_PRIMITIVE_BOUND_FOR_BASELINE_ALLOCATORS             0
#define PAS_PRIMITIVE_BOUND_FOR_NO_VIEW_CACHE                   UINT_MAX
#define PAS_PRIMITIVE_MAX_SEGREGATED_OBJECT_SIZE                UINT_MAX
#define PAS_PRIMITIVE_MAX_BITFIT_OBJECT_SIZE                    UINT_MAX

/* This is for heaps that hold typed objects. These objects have a certain size. We may allocate
   arrays of these objects. The type may tell us an alignment requirement in addition to a size.
   The alignment requirement is taken together with the minalign argument (the allocator uses
   whichever is bigger), but it's a bit more optimal to convey alignment using the alignment part
   of the type than by passing it to minalign. */
#define PAS_TYPED_BOUND_FOR_PARTIAL_VIEWS                       10
#define PAS_TYPED_BOUND_FOR_BASELINE_ALLOCATORS                 0
#define PAS_TYPED_BOUND_FOR_NO_VIEW_CACHE                       UINT_MAX
#define PAS_TYPED_MAX_SEGREGATED_OBJECT_SIZE                    UINT_MAX
#define PAS_TYPED_MAX_BITFIT_OBJECT_SIZE                        0

/* This is for heaps of flexible array member objects. These are variable-sized objects of a certain
   type. We should assume that different-sized instances can only be overlapped at the header.
   Implementing these as segregated heaps is valid since sizes get segregated and different-sized
   objects will only overlap at the header. However, FIXME: we need flex heaps to use a large heap
   that won't overlap objects in these heaps as if they were arrays. */
#define PAS_FLEX_BOUND_FOR_PARTIAL_VIEWS                        10
#define PAS_FLEX_BOUND_FOR_BASELINE_ALLOCATORS                  0
#define PAS_FLEX_BOUND_FOR_NO_VIEW_CACHE                        UINT_MAX
#define PAS_FLEX_MAX_SEGREGATED_OBJECT_SIZE                     UINT_MAX
#define PAS_FLEX_MAX_BITFIT_OBJECT_SIZE                         0

#endif /* PAS_INTERNAL_CONFIG_H */

