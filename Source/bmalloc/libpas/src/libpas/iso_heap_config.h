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

#ifndef ISO_HEAP_CONFIG_H
#define ISO_HEAP_CONFIG_H

#include "pas_config.h"

#if PAS_ENABLE_ISO

#include "pas_heap_config_utils.h"
#include "pas_megapage_cache.h"
#include "pas_simple_type.h"
#include "pas_segregated_page.h"
#include "pas_segregated_page_config_utils.h"

PAS_BEGIN_EXTERN_C;

#define ISO_MINALIGN_SHIFT ((size_t)4)
#define ISO_MINALIGN_SIZE ((size_t)1 << ISO_MINALIGN_SHIFT)

#define ISO_HEAP_CONFIG PAS_BASIC_HEAP_CONFIG( \
    iso, \
    .activate = pas_heap_config_utils_null_activate, \
    .get_type_size = pas_simple_type_as_heap_type_get_type_size, \
    .get_type_alignment = pas_simple_type_as_heap_type_get_type_alignment, \
    .dump_type = pas_simple_type_as_heap_type_dump, \
    .check_deallocation = true, \
    .small_segregated_min_align_shift = ISO_MINALIGN_SHIFT, \
    .small_segregated_sharing_shift = PAS_SMALL_SHARING_SHIFT, \
    .small_segregated_page_size = PAS_SMALL_PAGE_DEFAULT_SIZE, \
    .small_segregated_wasteage_handicap = PAS_SMALL_PAGE_HANDICAP, \
    .small_exclusive_segregated_logging_mode = pas_segregated_deallocation_size_oblivious_logging_mode, \
    .small_shared_segregated_logging_mode = pas_segregated_deallocation_no_logging_mode, \
    .small_exclusive_segregated_enable_empty_word_eligibility_optimization = false, \
    .small_shared_segregated_enable_empty_word_eligibility_optimization = false, \
    .small_segregated_use_reversed_current_word = PAS_ARM64, \
    .enable_view_cache = false, \
    .use_small_bitfit = true, \
    .small_bitfit_min_align_shift = ISO_MINALIGN_SHIFT, \
    .small_bitfit_page_size = PAS_SMALL_BITFIT_PAGE_DEFAULT_SIZE, \
    .medium_page_size = PAS_MEDIUM_PAGE_DEFAULT_SIZE, \
    .granule_size = PAS_GRANULE_DEFAULT_SIZE, \
    .use_medium_segregated = true, \
    .medium_segregated_min_align_shift = PAS_MIN_MEDIUM_ALIGN_SHIFT, \
    .medium_segregated_sharing_shift = PAS_MEDIUM_SHARING_SHIFT, \
    .medium_segregated_wasteage_handicap = PAS_MEDIUM_PAGE_HANDICAP, \
    .medium_exclusive_segregated_logging_mode = pas_segregated_deallocation_size_aware_logging_mode, \
    .medium_shared_segregated_logging_mode = pas_segregated_deallocation_no_logging_mode, \
    .use_medium_bitfit = true, \
    .medium_bitfit_min_align_shift = PAS_MIN_MEDIUM_ALIGN_SHIFT, \
    .use_marge_bitfit = true, \
    .marge_bitfit_min_align_shift = PAS_MIN_MARGE_ALIGN_SHIFT, \
    .marge_bitfit_page_size = PAS_MARGE_PAGE_DEFAULT_SIZE, \
    .pgm_enabled = false)

PAS_API extern const pas_heap_config iso_heap_config;

PAS_BASIC_HEAP_CONFIG_DECLARATIONS(iso, ISO);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_ISO */

#endif /* ISO_HEAP_CONFIG_H */

