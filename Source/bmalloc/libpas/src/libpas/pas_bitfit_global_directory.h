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

#ifndef PAS_BITFIT_GLOBAL_DIRECTORY_H
#define PAS_BITFIT_GLOBAL_DIRECTORY_H

#include "pas_bitfit_directory.h"
#include "pas_compact_atomic_page_sharing_pool_ptr.h"
#include "pas_page_sharing_participant.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_global_directory;
struct pas_bitfit_global_directory_bitvector_segment;
struct pas_segregated_heap;
typedef struct pas_bitfit_global_directory pas_bitfit_global_directory;
typedef struct pas_bitfit_global_directory_bitvector_segment pas_bitfit_global_directory_bitvector_segment;
typedef struct pas_segregated_heap pas_segregated_heap;

struct pas_bitfit_global_directory_bitvector_segment {
    unsigned empty_bits;
};

#define PAS_BITFIT_GLOBAL_DIRECTORY_BITVECTOR_SEGMENT_INITIALIZER \
    ((pas_bitfit_global_directory_bitvector_segment){ \
         .empty_bits = 0, \
     })

PAS_DECLARE_SEGMENTED_VECTOR(pas_bitfit_global_directory_segmented_bitvectors,
                             pas_bitfit_global_directory_bitvector_segment,
                             4);

struct PAS_ALIGNED(sizeof(pas_versioned_field)) pas_bitfit_global_directory {
    pas_bitfit_directory base;
    pas_segregated_heap* heap;
    pas_bitfit_global_directory_segmented_bitvectors bitvectors;
    pas_versioned_field last_empty_plus_one; /* Zero means there aren't any. */
    pas_page_sharing_participant_payload physical_sharing_payload;
    pas_compact_atomic_page_sharing_pool_ptr bias_sharing_pool;
    bool contention_did_trigger_explosion;
};

extern PAS_API bool pas_bitfit_global_directory_can_explode;
extern PAS_API bool pas_bitfit_global_directory_force_explode;

PAS_API void pas_bitfit_global_directory_construct(pas_bitfit_global_directory* directory,
                                                   pas_bitfit_page_config* config,
                                                   pas_segregated_heap* segregated_heap);

PAS_API bool pas_bitfit_global_directory_does_sharing(pas_bitfit_global_directory* directory);

PAS_API uint64_t pas_bitfit_global_directory_get_use_epoch(pas_bitfit_global_directory* directory);

PAS_API bool pas_bitfit_global_directory_get_empty_bit_at_index(
    pas_bitfit_global_directory* directory,
    size_t index);

/* Returns whether we changed the value. */
PAS_API bool pas_bitfit_global_directory_set_empty_bit_at_index(
    pas_bitfit_global_directory* directory,
    size_t index,
    bool value);

PAS_API void pas_bitfit_global_directory_view_did_become_empty_at_index(
    pas_bitfit_global_directory* directory,
    size_t index);
PAS_API void pas_bitfit_global_directory_view_did_become_empty(
    pas_bitfit_global_directory* directory,
    pas_bitfit_view* view);

PAS_API pas_page_sharing_pool_take_result pas_bitfit_global_directory_take_last_empty(
    pas_bitfit_global_directory* directory,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_bitfit_global_directory_dump_reference(
    pas_bitfit_global_directory* directory, pas_stream* stream);

PAS_API void pas_bitfit_global_directory_dump_for_spectrum(pas_stream* stream, void* directory);

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_GLOBAL_DIRECTORY_H */


