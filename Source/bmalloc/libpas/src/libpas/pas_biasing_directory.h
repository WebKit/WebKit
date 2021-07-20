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

#ifndef PAS_BIASING_DIRECTORY_H
#define PAS_BIASING_DIRECTORY_H

#include "pas_biasing_directory_kind.h"
#include "pas_page_sharing_participant.h"
#include "pas_versioned_field.h"

PAS_BEGIN_EXTERN_C;

struct pas_biasing_directory;
struct pas_bitfit_biasing_directory;
struct pas_segregated_biasing_directory;
typedef struct pas_biasing_directory pas_biasing_directory;
typedef struct pas_bitfit_biasing_directory pas_bitfit_biasing_directory;
typedef struct pas_segregated_biasing_directory pas_segregated_biasing_directory;

struct pas_biasing_directory {
    pas_versioned_field eligible_high_watermark;
    pas_page_sharing_participant_payload_with_use_epoch bias_sharing_payload;
    unsigned current_high_watermark;
    unsigned previous_high_watermark;
    unsigned index_in_all : 31;
    pas_biasing_directory_kind kind : 1;
};

PAS_API void pas_biasing_directory_construct(pas_biasing_directory* directory,
                                             pas_biasing_directory_kind kind,
                                             unsigned magazine_index);

static inline bool pas_is_segregated_biasing_directory(pas_biasing_directory* directory)
{
    return directory->kind == pas_biasing_directory_segregated_kind;
}

static inline bool pas_is_bitfit_biasing_directory(pas_biasing_directory* directory)
{
    return !pas_is_segregated_biasing_directory(directory);
}

static inline unsigned pas_biasing_directory_magazine_index(pas_biasing_directory* directory)
{
    return directory->bias_sharing_payload.base.index_in_sharing_pool;
}

/* All views at indices below the ownership threshold are going to be kept from the global
   directory. */
static inline unsigned pas_biasing_directory_ownership_threshold(pas_biasing_directory* directory)
{
    return PAS_MAX(directory->current_high_watermark,
                   directory->previous_high_watermark);
}

static inline unsigned pas_biasing_directory_unused_span_size(pas_biasing_directory* directory)
{
    unsigned ownership_threshold;
    size_t eligible_threshold;
    unsigned small_eligible_threshold;

    /* This is written so that it returns something sensible even in case of races. */
    ownership_threshold = pas_biasing_directory_ownership_threshold(directory);
    eligible_threshold = directory->eligible_high_watermark.value;

    small_eligible_threshold = (unsigned)eligible_threshold;
    PAS_ASSERT(small_eligible_threshold == eligible_threshold);

    if (ownership_threshold > small_eligible_threshold)
        return 0; /* This is totally possible - ownership and eligiblity float independently. */

    return small_eligible_threshold - ownership_threshold;
}

PAS_API pas_page_sharing_pool* pas_biasing_directory_get_sharing_pool(pas_biasing_directory* directory);

PAS_API void pas_biasing_directory_did_create_delta(pas_biasing_directory* directory);

PAS_API pas_page_sharing_pool_take_result pas_biasing_directory_take_last_unused(
    pas_biasing_directory* directory);

PAS_API void pas_biasing_directory_did_use_index_slow(pas_biasing_directory* directory,
                                                      size_t index);

static inline void pas_biasing_directory_did_use_index(pas_biasing_directory* directory,
                                                       size_t index,
                                                       uint64_t epoch)
{
    if (PAS_UNLIKELY(index >= directory->current_high_watermark))
        pas_biasing_directory_did_use_index_slow(directory, index);

    directory->bias_sharing_payload.use_epoch = epoch;
}

PAS_API void pas_biasing_directory_index_did_become_eligible(pas_biasing_directory* directory,
                                                             size_t index);

PAS_END_EXTERN_C;

#endif /* PAS_BIASING_DIRECTORY_H */

