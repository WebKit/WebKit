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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_biasing_directory.h"

#include "pas_all_biasing_directories.h"
#include "pas_biasing_directory_inlines.h"
#include "pas_bitfit_global_directory.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_global_size_directory.h"

void pas_biasing_directory_construct(pas_biasing_directory* directory,
                                     pas_biasing_directory_kind kind,
                                     unsigned magazine_index)
{
    pas_versioned_field_construct(&directory->eligible_high_watermark, 0);
    
    pas_page_sharing_participant_payload_with_use_epoch_construct(&directory->bias_sharing_payload);

    directory->current_high_watermark = 0;
    directory->previous_high_watermark = 0;

    directory->index_in_all = 0;
    
    directory->kind = kind;

    pas_all_biasing_directories_append(directory);

    pas_page_sharing_pool_add_at_index(
        pas_biasing_directory_get_sharing_pool(directory),
        pas_page_sharing_participant_create(
            directory, pas_page_sharing_participant_biasing_directory),
        magazine_index);
}

pas_page_sharing_pool* pas_biasing_directory_get_sharing_pool(pas_biasing_directory* directory)
{
    if (pas_is_segregated_biasing_directory(directory)) {
        return pas_compact_atomic_page_sharing_pool_ptr_load_non_null(
            &pas_segregated_global_size_directory_data_ptr_load_non_null(
                &pas_compact_segregated_global_size_directory_ptr_load_non_null(
                    &pas_unwrap_segregated_biasing_directory(
                        directory)->parent_global)->data)->bias_sharing_pool);
    }

    return pas_compact_atomic_page_sharing_pool_ptr_load_non_null(
        &pas_compact_bitfit_global_directory_ptr_load_non_null(
            &pas_unwrap_bitfit_biasing_directory(
                directory)->parent_global)->bias_sharing_pool);
}

void pas_biasing_directory_did_create_delta(pas_biasing_directory* directory)
{
    pas_page_sharing_pool* pool;
    
    pool = pas_biasing_directory_get_sharing_pool(directory);
    
    pas_page_sharing_pool_did_create_delta(
        pool,
        pas_page_sharing_participant_create(
            directory,
            pas_page_sharing_participant_biasing_directory));
}

pas_page_sharing_pool_take_result pas_biasing_directory_take_last_unused(
    pas_biasing_directory* directory)
{
    if (pas_is_segregated_biasing_directory(directory)) {
        return pas_segregated_biasing_directory_take_last_unused(
            pas_unwrap_segregated_biasing_directory(directory));
    }

    return pas_bitfit_biasing_directory_take_last_unused(
        pas_unwrap_bitfit_biasing_directory(directory));
}

void pas_biasing_directory_did_use_index_slow(pas_biasing_directory* directory,
                                              size_t index)
{
    size_t large_index_plus_one;
    unsigned index_plus_one;
    bool did_overflow;

    did_overflow = __builtin_add_overflow(index, 1, &large_index_plus_one);
    PAS_ASSERT(!did_overflow);
    index_plus_one = (unsigned)large_index_plus_one;
    PAS_ASSERT(index_plus_one == large_index_plus_one);
    directory->current_high_watermark = index_plus_one;
    pas_fence();
    pas_all_biasing_directories_activate(directory);
}

void pas_biasing_directory_index_did_become_eligible(pas_biasing_directory* directory,
                                                     size_t index)
{
    size_t watermark_index;
    bool did_overflow;
    bool should_notify_delta;

    did_overflow = __builtin_add_overflow(index, 1, &watermark_index);
    PAS_ASSERT(!did_overflow);

    should_notify_delta =
        pas_versioned_field_maximize(
            &directory->eligible_high_watermark, watermark_index)
        < watermark_index;

    if (should_notify_delta) {
        pas_page_sharing_pool* pool;

        pool = pas_biasing_directory_get_sharing_pool(directory);

        pas_page_sharing_pool_did_create_delta(
            pool,
            pas_page_sharing_participant_create(
                directory,
                pas_page_sharing_participant_biasing_directory));
    }
}

#endif /* LIBPAS_ENABLED */
