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

#ifndef PAS_BITFIT_DIRECTORY_H
#define PAS_BITFIT_DIRECTORY_H

#include "pas_bitfit_max_free.h"
#include "pas_bitfit_page_config_kind.h"
#include "pas_bitfit_view_and_index.h"
#include "pas_compact_atomic_bitfit_size_class_ptr.h"
#include "pas_compact_atomic_bitfit_view_ptr.h"
#include "pas_heap_summary.h"
#include "pas_page_sharing_participant.h"
#include "pas_segmented_vector.h"
#include "pas_versioned_field.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_directory;
struct pas_bitfit_directory_bitvector_segment;
struct pas_bitfit_size_class;
struct pas_bitfit_page;
struct pas_bitfit_view;
struct pas_segregated_heap;
typedef struct pas_bitfit_directory pas_bitfit_directory;
typedef struct pas_bitfit_directory_bitvector_segment pas_bitfit_directory_bitvector_segment;
typedef struct pas_bitfit_size_class pas_bitfit_size_class;
typedef struct pas_bitfit_page pas_bitfit_page;
typedef struct pas_bitfit_view pas_bitfit_view;
typedef struct pas_segregated_heap pas_segregated_heap;

#define PAS_BITFIT_DIRECTORY_VERBOSE_MAX_FREE false

PAS_DECLARE_SEGMENTED_VECTOR(pas_bitfit_directory_max_free_vector,
                             pas_bitfit_max_free,
                             128);

PAS_DECLARE_SEGMENTED_VECTOR(pas_bitfit_directory_view_vector,
                             pas_compact_atomic_bitfit_view_ptr,
                             8);

struct pas_bitfit_directory_bitvector_segment {
    unsigned empty_bits;
};

#define PAS_BITFIT_DIRECTORY_BITVECTOR_SEGMENT_INITIALIZER \
    ((pas_bitfit_directory_bitvector_segment){ \
         .empty_bits = 0, \
     })

PAS_DECLARE_SEGMENTED_VECTOR(pas_bitfit_directory_segmented_bitvectors,
                             pas_bitfit_directory_bitvector_segment,
                             4);

struct PAS_ALIGNED(sizeof(pas_versioned_field)) pas_bitfit_directory {
    pas_versioned_field first_unprocessed_free;
    pas_versioned_field first_empty;
    pas_versioned_field last_empty_plus_one; /* Zero means there aren't any. */
    pas_segregated_heap* heap;
    pas_bitfit_directory_segmented_bitvectors bitvectors;
    pas_bitfit_directory_max_free_vector max_frees;
    pas_bitfit_directory_view_vector views;
    pas_page_sharing_participant_payload physical_sharing_payload;
    pas_compact_atomic_bitfit_size_class_ptr largest_size_class;
    pas_bitfit_page_config_kind config_kind : 8;
};

PAS_API void pas_bitfit_directory_construct(pas_bitfit_directory* directory,
                                            pas_bitfit_page_config* config,
                                            pas_segregated_heap* heap);

static inline unsigned pas_bitfit_directory_size(pas_bitfit_directory* directory)
{
    return directory->views.size;
}

static inline pas_bitfit_max_free* pas_bitfit_directory_get_max_free_ptr(
    pas_bitfit_directory* directory,
    size_t index)
{
    return pas_bitfit_directory_max_free_vector_get_ptr_checked(&directory->max_frees, index);
}

static inline pas_bitfit_max_free pas_bitfit_directory_get_max_free(pas_bitfit_directory* directory,
                                                                    size_t index)
{
    return *pas_bitfit_directory_get_max_free_ptr(directory, index);
}

static inline void pas_bitfit_directory_set_max_free_unchecked(
    pas_bitfit_directory* directory,
    size_t index,
    pas_bitfit_max_free max_free,
    const char* reason)
{
    if (PAS_BITFIT_DIRECTORY_VERBOSE_MAX_FREE)
        pas_log("%p:%zu: setting max_free unchecked to %u (%s)\n", directory, index, max_free, reason);
    *pas_bitfit_directory_get_max_free_ptr(directory, index) = max_free;
}

/* We maintain the convention that you should only do this while holding the ownership lock. */
static inline void pas_bitfit_directory_set_max_free_not_empty_impl(pas_bitfit_directory* directory,
                                                                    size_t index,
                                                                    pas_bitfit_max_free max_free)
{
    pas_bitfit_max_free* ptr;
    ptr = pas_bitfit_directory_get_max_free_ptr(directory, index);
    if (*ptr == PAS_BITFIT_MAX_FREE_EMPTY) {
        pas_log("%p:%zu: found empty when setting max_free\n", directory, index);
        PAS_ASSERT(*ptr != PAS_BITFIT_MAX_FREE_EMPTY);
    }
    PAS_ASSERT(max_free != PAS_BITFIT_MAX_FREE_EMPTY);
    *ptr = max_free;
}

static inline void pas_bitfit_directory_set_processed_max_free(pas_bitfit_directory* directory,
                                                               size_t index,
                                                               pas_bitfit_max_free max_free,
                                                               const char* reason)
{
    if (PAS_BITFIT_DIRECTORY_VERBOSE_MAX_FREE)
        pas_log("%p:%zu: setting max_free to processed %u (%s)\n", directory, index, max_free, reason);
    PAS_ASSERT(max_free < (size_t)PAS_BITFIT_MAX_FREE_UNPROCESSED);
    PAS_ASSERT(PAS_BITFIT_MAX_FREE_UNPROCESSED < PAS_BITFIT_MAX_FREE_EMPTY);
    pas_bitfit_directory_set_max_free_not_empty_impl(directory, index, max_free);
}

static inline void pas_bitfit_directory_set_unprocessed_max_free_unchecked(
    pas_bitfit_directory* directory,
    size_t index,
    const char* reason)
{
    if (PAS_BITFIT_DIRECTORY_VERBOSE_MAX_FREE)
        pas_log("%p:%zu: setting max_free to unprocessed unchecked (%s)\n", directory, index, reason);
    *pas_bitfit_directory_get_max_free_ptr(directory, index) = PAS_BITFIT_MAX_FREE_UNPROCESSED;
}

static inline void pas_bitfit_directory_set_empty_max_free(
    pas_bitfit_directory* directory,
    size_t index,
    const char* reason)
{
    if (PAS_BITFIT_DIRECTORY_VERBOSE_MAX_FREE)
        pas_log("%p:%zu: setting max_free to empty (%s)\n", directory, index, reason);
    *pas_bitfit_directory_get_max_free_ptr(directory, index) = PAS_BITFIT_MAX_FREE_EMPTY;
}

static inline void pas_bitfit_directory_set_unprocessed_max_free(pas_bitfit_directory* directory,
                                                                 size_t index,
                                                                 const char* reason)
{
    if (PAS_BITFIT_DIRECTORY_VERBOSE_MAX_FREE)
        pas_log("%p:%zu: setting max_free to unprocessed (%s)\n", directory, index, reason);
    pas_bitfit_directory_set_max_free_not_empty_impl(directory, index, PAS_BITFIT_MAX_FREE_UNPROCESSED);
}

PAS_API void pas_bitfit_directory_update_biasing_eligibility(pas_bitfit_directory* directory,
                                                             size_t index);

PAS_API void pas_bitfit_directory_max_free_did_become_unprocessed(pas_bitfit_directory* directory,
                                                                  size_t index,
                                                                  const char* reason);

PAS_API void pas_bitfit_directory_max_free_did_become_unprocessed_unchecked(
    pas_bitfit_directory* directory,
    size_t index,
    const char* reason);

PAS_API void pas_bitfit_directory_max_free_did_become_empty_without_biasing_update(
    pas_bitfit_directory* directory,
    size_t index,
    const char* reason);

PAS_API void pas_bitfit_directory_max_free_did_become_empty(pas_bitfit_directory* directory,
                                                            size_t index,
                                                            const char* reason);

static inline pas_compact_atomic_bitfit_view_ptr*
pas_bitfit_directory_get_view_ptr(pas_bitfit_directory* directory,
                                  size_t index)
{
    return pas_bitfit_directory_view_vector_get_ptr_checked(&directory->views, index);
}

static inline pas_bitfit_view* pas_bitfit_directory_get_view(pas_bitfit_directory* directory,
                                                             size_t index)
{
    return pas_compact_atomic_bitfit_view_ptr_load(
        pas_bitfit_directory_get_view_ptr(directory, index));
}

/* This does not update any indices held onto by the directory or size classes. To do that, use
   size_class's version of this function. */
PAS_API pas_bitfit_view_and_index
pas_bitfit_directory_get_first_free_view(pas_bitfit_directory* directory,
                                         unsigned start_index,
                                         unsigned size,
                                         pas_bitfit_page_config* page_config);

PAS_API pas_heap_summary pas_bitfit_directory_compute_summary(pas_bitfit_directory* directory);

typedef bool (*pas_bitfit_directory_for_each_live_object_callback)(
    pas_bitfit_directory* directory,
    pas_bitfit_view* view,
    uintptr_t begin,
    size_t size,
    void* arg);

PAS_API bool pas_bitfit_directory_for_each_live_object(
    pas_bitfit_directory* directory,
    pas_bitfit_directory_for_each_live_object_callback callback,
    void* arg);

PAS_API bool pas_bitfit_directory_does_sharing(pas_bitfit_directory* directory);

PAS_API uint64_t pas_bitfit_directory_get_use_epoch(pas_bitfit_directory* directory);

PAS_API bool pas_bitfit_directory_get_empty_bit_at_index(
    pas_bitfit_directory* directory,
    size_t index);

/* Returns whether we changed the value. */
PAS_API bool pas_bitfit_directory_set_empty_bit_at_index(
    pas_bitfit_directory* directory,
    size_t index,
    bool value);

PAS_API void pas_bitfit_directory_view_did_become_empty_at_index(
    pas_bitfit_directory* directory,
    size_t index);
PAS_API void pas_bitfit_directory_view_did_become_empty(
    pas_bitfit_directory* directory,
    pas_bitfit_view* view);

PAS_API pas_page_sharing_pool_take_result pas_bitfit_directory_take_last_empty(
    pas_bitfit_directory* directory,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_bitfit_directory_dump_reference(
    pas_bitfit_directory* directory, pas_stream* stream);

PAS_API void pas_bitfit_directory_dump_for_spectrum(pas_stream* stream, void* directory);

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_DIRECTORY_H */

