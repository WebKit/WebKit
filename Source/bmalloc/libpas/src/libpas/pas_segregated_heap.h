/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_HEAP_H
#define PAS_SEGREGATED_HEAP_H

#include "pas_allocator_scavenge_action.h"
#include "pas_compact_atomic_allocator_index_ptr.h"
#include "pas_compact_atomic_ptr.h"
#include "pas_compact_atomic_segregated_size_directory_ptr.h"
#include "pas_compact_heap_ptr.h"
#include "pas_config.h"
#include "pas_hashtable.h"
#include "pas_heap_config.h"
#include "pas_heap_runtime_config.h"
#include "pas_heap_summary.h"
#include "pas_large_map_entry.h"
#include "pas_local_allocator.h"
#include "pas_mutation_count.h"
#include "pas_segregated_heap_lookup_kind.h"
#include "pas_segregated_size_directory_creation_mode.h"
#include "pas_page_sharing_mode.h"
#include "pas_segregated_page.h"
#include "pas_size_lookup_mode.h"

PAS_BEGIN_EXTERN_C;

#ifndef pas_heap
#define pas_heap __pas_heap
#endif

enum pas_segregated_heap_medium_size_directory_search_mode {
    /* Given a requested index, the returned directory has to be begin_index <= index <= end_index. */
    pas_segregated_heap_medium_size_directory_search_within_size_class_progression,

    /* Given a requested index, the returned directory has to be index <= end_index and it has to be the
       directory with minimum end_index that meets that property. So, it's the smallest-indexed directory
       whose advertised index is greater-than-or-equal to the index we are asking for. */
    pas_segregated_heap_medium_size_directory_search_least_greater_equal
};

typedef enum pas_segregated_heap_medium_size_directory_search_mode pas_segregated_heap_medium_size_directory_search_mode;

struct pas_heap;
struct pas_segregated_size_directory;
struct pas_segregated_heap;
struct pas_segregated_heap_medium_directory_tuple;
struct pas_segregated_heap_rare_data;
typedef struct pas_heap pas_heap;
typedef struct pas_segregated_size_directory pas_segregated_size_directory;
typedef struct pas_segregated_heap pas_segregated_heap;
typedef struct pas_segregated_heap_medium_directory_tuple pas_segregated_heap_medium_directory_tuple;
typedef struct pas_segregated_heap_rare_data pas_segregated_heap_rare_data;

PAS_DEFINE_COMPACT_ATOMIC_PTR(pas_segregated_heap_rare_data, pas_segregated_heap_rare_data_ptr);
PAS_DEFINE_COMPACT_ATOMIC_PTR(pas_segregated_heap_medium_directory_tuple,
                              pas_segregated_heap_medium_directory_tuple_ptr);

struct pas_segregated_heap {
    pas_heap_runtime_config* runtime_config;
    
    pas_allocator_index* index_to_small_allocator_index;
    pas_compact_atomic_segregated_size_directory_ptr* index_to_small_size_directory;

    /* This is the head of the linked list of segregated size directories. Also, if there is
       a basic size directory (the one for index == 1 or index == *cached_index), then it will
       be at the head of the list with the is_basic_size_directory bit set. */
    pas_compact_atomic_segregated_size_directory_ptr basic_size_directory_and_head;
    
    pas_segregated_heap_rare_data_ptr rare_data;

    pas_compact_atomic_bitfit_heap_ptr bitfit_heap;

    /* Below this index (exclusive), we can look up the allocator index and directiry directly in
       the index_to_XYZ tables. Above this index (inclusive), we have to use the rare data and it
       becomes a binary search.
    
       NOTE: The directories below this bound _may_ be medium-sized if our heuristics say that this
       is a better fit. That's rare but could happen.
    
       Also, confusingly, this field being non-zero means "we have decided to cache small index
       lookup". Note that it _is_ OK for this to be zero but for us to have a rare_data with a
       non-zero number of medium directories. */
    unsigned small_index_upper_bound;
};

typedef unsigned pas_segregated_heap_medium_directory_index;

struct pas_segregated_heap_medium_directory_tuple {
    /* NOTE: This directory is always going to be medium sized because this data structure is only
       used for sizes that are beyond the max object size for small pages. But nobody strongly
       relies on that property. */
    pas_compact_atomic_segregated_size_directory_ptr directory;

    pas_allocator_index allocator_index;
    
    /* This is the `index` we would use to do a lookup. A medium directory tuple represents a
       contiguous range of indices that map to a single directory. */
    pas_segregated_heap_medium_directory_index begin_index; /* inclusive, but cannot be zero, so we can
                                                               detect races (we could relax this if we just
                                                               made this "begin_index_plus_one" or something
                                                               like that) */
    pas_segregated_heap_medium_directory_index end_index; /* inclusive */
};

struct pas_segregated_heap_rare_data {
    /* This code is counter-locked. Note that odd values mean that we are mutating right now. */
    pas_mutation_count mutation_count;

    /* This contains all of the medium page directories in sorted order. Note that it must be
       allocated from a heap that never returns memory to the OS; otherwise our counter-synchronized
       reading code could crash. We currently achieve this by never freeing what we allocate
       here. */
    pas_segregated_heap_medium_directory_tuple_ptr medium_directories;

    unsigned num_medium_directories;
    unsigned medium_directories_capacity;
};

PAS_API extern unsigned pas_segregated_heap_num_size_lookup_rematerializations;

/* NOTE: it's possible to construct a pas_segregated_heap outside a pas_heap. Such a
   segregated_heap will assume type_size == 1. */
PAS_API void pas_segregated_heap_construct(pas_segregated_heap* segregated_heap,
                                           pas_heap* parent_heap,
                                           pas_heap_config* config,
                                           pas_heap_runtime_config* runtime_config);

PAS_API pas_bitfit_heap* pas_segregated_heap_get_bitfit(pas_segregated_heap* heap,
                                                        pas_heap_config* heap_config,
                                                        pas_lock_hold_mode heap_lock_hold_mode);

static PAS_ALWAYS_INLINE size_t
pas_segregated_heap_index_for_size(size_t size, pas_heap_config config)
{
    size_t min_align_shift;
    size_t min_align;
    min_align_shift = pas_heap_config_segregated_heap_min_align_shift(config);
    min_align = (size_t)1 << min_align_shift;
    return (size + min_align - 1) >> min_align_shift;
}

static PAS_ALWAYS_INLINE size_t
pas_segregated_heap_size_for_index(size_t index, pas_heap_config config)
{
    return index << pas_heap_config_segregated_heap_min_align_shift(config);
}

/* These functions help with dealing with pas_heap_ref::allocator_index. Can pass a NULL cached_index, in which
   case we check if the index corresponds to the index of the type_size. Note that if cached_index is not NULL
   then there are actually three possibilities:

   1) cached_index is not set.
   2) cached_index is set and matches the index.
   3) cached_index is set and does not match the index. */
PAS_API size_t pas_segregated_heap_get_cached_index_for_heap_type(pas_segregated_heap* heap,
                                                                  pas_heap_config* config);
PAS_API bool pas_segregated_heap_cached_index_is_set(unsigned* cached_index);
PAS_API size_t pas_segregated_heap_get_cached_index(pas_segregated_heap* heap,
                                                    unsigned* cached_index,
                                                    pas_heap_config* config);
PAS_API bool pas_segregated_heap_index_is_cached_index_and_cached_index_is_set(pas_segregated_heap* heap,
                                                                               unsigned* cached_index,
                                                                               size_t index,
                                                                               pas_heap_config* config);
PAS_API bool pas_segregated_heap_index_is_cached_index_or_cached_index_is_unset(pas_segregated_heap* heap,
                                                                                unsigned* cached_index,
                                                                                size_t index,
                                                                                pas_heap_config* config);
PAS_API bool pas_segregated_heap_index_is_not_cached_index_and_cached_index_is_set(pas_segregated_heap* heap,
                                                                                   unsigned* cached_index,
                                                                                   size_t index,
                                                                                   pas_heap_config* config);
PAS_API bool pas_segregated_heap_index_is_greater_than_cached_index_and_cached_index_is_set(
    pas_segregated_heap* heap,
    unsigned* cached_index,
    size_t index,
    pas_heap_config* config);
PAS_API bool pas_segregated_heap_index_is_greater_equal_cached_index_and_cached_index_is_set(
    pas_segregated_heap* heap,
    unsigned* cached_index,
    size_t index,
    pas_heap_config* config);

PAS_API pas_segregated_heap_medium_directory_tuple*
pas_segregated_heap_medium_directory_tuple_for_index(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API unsigned pas_segregated_heap_medium_allocator_index_for_index(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_segregated_size_directory* pas_segregated_heap_medium_size_directory_for_index(
    pas_segregated_heap* heap,
    size_t index,
    pas_segregated_heap_medium_size_directory_search_mode search_mode,
    pas_lock_hold_mode heap_lock_hold_mode);

static PAS_ALWAYS_INLINE unsigned
pas_segregated_heap_allocator_index_for_index_inline_only(pas_segregated_heap* heap,
                                                          size_t index)
{
    pas_allocator_index* index_to_allocator_index;

    if (PAS_UNLIKELY(index >= (size_t)heap->small_index_upper_bound))
        return 0;
    
    index_to_allocator_index = heap->index_to_small_allocator_index;
    if (!index_to_allocator_index)
        return 0; /* Guard against races. */
    
    return index_to_allocator_index[index];
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_heap_allocator_index_for_index(pas_segregated_heap* heap,
                                              size_t index,
                                              pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_allocator_index* index_to_allocator_index;

    if (PAS_UNLIKELY(index >= (size_t)heap->small_index_upper_bound)) {
        return pas_segregated_heap_medium_allocator_index_for_index(
            heap, index,
            pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
            heap_lock_hold_mode);
    }
    
    index_to_allocator_index = heap->index_to_small_allocator_index;
    if (!index_to_allocator_index)
        return 0; /* Guard against races. */
    
    return index_to_allocator_index[index];
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_heap_allocator_index_for_size_inline_only(pas_segregated_heap* heap,
                                                         size_t size,
                                                         pas_heap_config heap_config)
{
    size_t index;
    
    index = pas_segregated_heap_index_for_size(size, heap_config);
    
    return pas_segregated_heap_allocator_index_for_index_inline_only(heap, index);
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_heap_allocator_index_for_size(pas_segregated_heap* heap,
                                             size_t size,
                                             pas_heap_config heap_config)
{
    size_t index;
    
    index = pas_segregated_heap_index_for_size(size, heap_config);
    
    return pas_segregated_heap_allocator_index_for_index(heap, index,
                                                         pas_lock_is_not_held);
}

PAS_API unsigned
pas_segregated_heap_ensure_allocator_index(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
    size_t size,
    pas_size_lookup_mode size_lookup_mode,
    pas_heap_config* config,
    unsigned* cached_index);

/* This may return UINT_MAX if it determines that the wasteage from allocating this size with
   the small heap is greater than the wasteage from rounding up to large alignment and allocating
   with the large allocator.

   This will try to avoid creating new size classes if there is a size class that is pretty close
   in size to the one you ask for, except if that size class fails to obey the alignment
   requirement.

   Need to hold heap lock to call this.

   NOTE: force_size_lookup == true means "it's not good enough for me to have this cached in
   basic_size_directory since I will directly do a lookup in index_to_small_allocator_index or
   the rare_data". force_size_lookup == false means "please assume that if this can be cached in
   the basic_size_directory then I will want this cached in the heap_ref".

   FIXME: Merge these two comments. */
/* May return NULL; see comment for
   pas_segregated_heap_ensure_allocator_index_for_size. Need to hold heap lock to
   call this. OK to pass NULL for cached_index; only primitive_heap_ref's use
   that. */
PAS_API pas_segregated_size_directory*
pas_segregated_heap_ensure_size_directory_for_size(
    pas_segregated_heap* heap,
    size_t size, size_t alignment,
    pas_size_lookup_mode size_lookup_mode,
    pas_heap_config* config,
    unsigned* cached_index,
    pas_segregated_size_directory_creation_mode creation_mode);

PAS_API size_t pas_segregated_heap_get_num_free_bytes(pas_segregated_heap* heap);

typedef bool (*pas_segregated_heap_for_each_size_directory_callback)(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
    void* arg);

PAS_API bool pas_segregated_heap_for_each_size_directory(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_size_directory_callback callback,
    void* arg);

typedef bool (*pas_segregated_heap_for_each_committed_view_callback)(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
    pas_segregated_view view,
    void* arg);

PAS_API bool pas_segregated_heap_for_each_committed_view(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_committed_view_callback callback,
    void* arg);

typedef bool (*pas_segregated_heap_for_each_view_index_callback)(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
    size_t index,
    pas_segregated_view viewp,
    void* arg);

PAS_API bool pas_segregated_heap_for_each_view_index(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_view_index_callback callback,
    void* arg);

typedef bool (*pas_segregated_heap_for_each_live_object_callback)(
    pas_segregated_heap* heap,
    uintptr_t begin,
    size_t size,
    void* arg);

PAS_API bool pas_segregated_heap_for_each_live_object(
    pas_segregated_heap* heap,
    pas_segregated_heap_for_each_live_object_callback callback,
    void *arg);

PAS_API size_t pas_segregated_heap_num_committed_views(pas_segregated_heap* heap);
PAS_API size_t pas_segregated_heap_num_empty_views(pas_segregated_heap* heap);
PAS_API size_t pas_segregated_heap_num_empty_granules(pas_segregated_heap* heap);
PAS_API size_t pas_segregated_heap_num_views(pas_segregated_heap* heap);

PAS_API pas_heap_summary pas_segregated_heap_compute_summary(pas_segregated_heap* heap);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_HEAP_H */

