/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#ifndef JS_MARKED_BLOCK_HEAP_CONFIG_H
#define JS_MARKED_BLOCK_HEAP_CONFIG_H

#include "pas_config.h"

#if LIBPAS_ENABLED

#if PAS_ENABLE_JS_MARKED_BLOCK

#include "js_marked_block_heap.h"
#include "pas_heap_config_utils.h"
#include "pas_megapage_cache.h"
#include "pas_segregated_page.h"
#include "pas_segregated_page_config_utils.h"
#include "pas_simple_type.h"

PAS_BEGIN_EXTERN_C;

/* The page config that serves JS_MARKED_BLOCK_SIZE. See js_marked_block_heap.h for what the heap is
   and why it exists. */

/* How many blocks a page holds. Two things put a floor under it: pas_segregated_page_deallocate_impl
   indexes a page's alloc bits by word, computing the word count as page_size >> (min_align_shift + 5)
   and requiring it to be a power of two, so a page needs at least 32 min-aligns; and
   PAS_MIN_OBJECTS_PER_PAGE caps an object at page_size/11, so a block needs a page of at least 11
   blocks. With the minimum alignment at the block size, the first of those binds at 32 blocks, and
   this leaves a spare alloc word above it. Sizes from 16 to 128 blocks measure the same on
   JetStream3, in both throughput and footprint. */
#define JS_MARKED_BLOCK_BLOCKS_PER_PAGE 64

#define JS_MARKED_BLOCK_PAGE_SIZE (JS_MARKED_BLOCK_BLOCKS_PER_PAGE * JS_MARKED_BLOCK_SIZE)

#define JS_MARKED_BLOCK_USE_MEDIUM_SEGREGATED true
#define JS_MARKED_BLOCK_USE_MEDIUM_BITFIT false

#define JS_MARKED_BLOCK_MEDIUM_SEGREGATED_MIN_ALIGN_SHIFT JS_MARKED_BLOCK_SHIFT
#define JS_MARKED_BLOCK_MEDIUM_SEGREGATED_PAGE_SIZE JS_MARKED_BLOCK_PAGE_SIZE

/* Idle, but the template computes header sizes for every slot whether or not it is enabled, and
   dividing by a zero page size would not compile. */
#define JS_MARKED_BLOCK_MEDIUM_BITFIT_MIN_ALIGN_SHIFT PAS_MIN_MEDIUM_ALIGN_SHIFT
#define JS_MARKED_BLOCK_MEDIUM_BITFIT_PAGE_SIZE PAS_MEDIUM_BITFIT_PAGE_DEFAULT_SIZE

/* The small segregated slot cannot be turned off, and nothing this heap allocates could fit in it
   anyway. It keeps an ordinary shape because the config's large_alignment is derived from its minimum
   alignment, and both probabilistic guard malloc and the designated intrinsic heap assume that
   number is small. */
#define JS_MARKED_BLOCK_SMALL_MINALIGN_SHIFT ((size_t)4)

PAS_API void js_marked_block_heap_config_activate(void);

#define JS_MARKED_BLOCK_HEAP_CONFIG PAS_BASIC_HEAP_CONFIG( \
    js_marked_block, \
    .activate = js_marked_block_heap_config_activate, \
    .get_type_size = pas_simple_type_as_heap_type_get_type_size, \
    .get_type_alignment = pas_simple_type_as_heap_type_get_type_alignment, \
    .dump_type = pas_simple_type_as_heap_type_dump, \
    .check_deallocation = true, \
    .small_segregated_min_align_shift = JS_MARKED_BLOCK_SMALL_MINALIGN_SHIFT, \
    .small_segregated_sharing_shift = PAS_SMALL_SHARING_SHIFT, \
    .small_segregated_page_size = PAS_SMALL_PAGE_DEFAULT_SIZE, \
    .small_segregated_wasteage_handicap = PAS_SMALL_PAGE_HANDICAP, \
    .small_exclusive_segregated_logging_mode = pas_segregated_deallocation_size_oblivious_logging_mode, \
    .small_exclusive_segregated_enable_empty_word_eligibility_optimization = false, \
    .small_segregated_use_reversed_current_word = PAS_ARM64, \
    .enable_view_cache = false, \
    .use_small_bitfit = false, \
    .small_bitfit_min_align_shift = JS_MARKED_BLOCK_SMALL_MINALIGN_SHIFT, \
    .small_bitfit_page_size = PAS_SMALL_BITFIT_PAGE_DEFAULT_SIZE, \
    .medium_segregated_page_size = JS_MARKED_BLOCK_MEDIUM_SEGREGATED_PAGE_SIZE, \
    .medium_bitfit_page_size = JS_MARKED_BLOCK_MEDIUM_BITFIT_PAGE_SIZE, \
    .granule_size = JS_MARKED_BLOCK_SIZE, \
    .use_medium_segregated = JS_MARKED_BLOCK_USE_MEDIUM_SEGREGATED, \
    .medium_segregated_min_align_shift = JS_MARKED_BLOCK_MEDIUM_SEGREGATED_MIN_ALIGN_SHIFT, \
    .medium_segregated_sharing_shift = PAS_MEDIUM_SHARING_SHIFT, \
    .medium_segregated_wasteage_handicap = PAS_MEDIUM_PAGE_HANDICAP, \
    .medium_exclusive_segregated_logging_mode = pas_segregated_deallocation_size_oblivious_logging_mode, \
    .use_medium_bitfit = JS_MARKED_BLOCK_USE_MEDIUM_BITFIT, \
    .medium_bitfit_min_align_shift = JS_MARKED_BLOCK_MEDIUM_BITFIT_MIN_ALIGN_SHIFT, \
    .use_marge_bitfit = false, \
    .marge_bitfit_min_align_shift = PAS_MIN_MARGE_ALIGN_SHIFT, \
    .marge_bitfit_page_size = PAS_MARGE_PAGE_DEFAULT_SIZE, \
    .pgm_enabled = false, \
    .delegate_large_user_allocations = false, \
    .large_map_variant = pas_default_large_map_variant, \
    .allow_mte_tagging = false)

PAS_API extern const pas_heap_config js_marked_block_heap_config;

PAS_BASIC_HEAP_CONFIG_DECLARATIONS(js_marked_block, JS_MARKED_BLOCK);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JS_MARKED_BLOCK */

#endif /* LIBPAS_ENABLED */
#endif /* JS_MARKED_BLOCK_HEAP_CONFIG_H */
