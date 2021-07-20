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

#ifndef PAS_SEGREGATED_DIRECTORY_H
#define PAS_SEGREGATED_DIRECTORY_H

#include "pas_bitvector.h"
#include "pas_bootstrap_free_heap.h"
#include "pas_compact_atomic_segregated_view.h"
#include "pas_config.h"
#include "pas_immutable_vector.h"
#include "pas_lock.h"
#include "pas_page_sharing_mode.h"
#include "pas_page_sharing_participant.h"
#include "pas_segmented_vector.h"
#include "pas_segregated_directory_first_eligible_kind.h"
#include "pas_segregated_page_config_kind.h"
#include "pas_segmented_vector.h"
#include "pas_segregated_view.h"
#include "pas_versioned_field.h"

PAS_BEGIN_EXTERN_C;

#ifndef pas_heap
#define pas_heap __pas_heap
#endif

struct pas_heap;
struct pas_segregated_heap;
struct pas_segregated_page;
struct pas_segregated_page_config;
struct pas_segregated_directory;
struct pas_segregated_directory_bitvector_segment;
struct pas_segregated_directory_simple_bitvector_segment;
struct pas_segregated_directory_data;
struct pas_page_sharing_pool;
typedef struct pas_heap pas_heap;
typedef struct pas_segregated_heap pas_segregated_heap;
typedef struct pas_segregated_page pas_segregated_page;
typedef struct pas_segregated_page_config pas_segregated_page_config;
typedef struct pas_segregated_directory pas_segregated_directory;
typedef struct pas_segregated_directory_bitvector_segment pas_segregated_directory_bitvector_segment;
typedef struct pas_segregated_directory_simple_bitvector_segment pas_segregated_directory_simple_bitvector_segment;
typedef struct pas_segregated_directory_data pas_segregated_directory_data;
typedef struct pas_page_sharing_pool pas_page_sharing_pool;

struct pas_segregated_directory_bitvector_segment {
    /* Eligible means that the page is available for taking, either to:
       - Allocate in,
       - Decommit, or
       - Commit and then allocate in. */
    unsigned eligible_bits;
    
    /* Empty means that the page has stuff that can be decommitted. However, this bit has no meaning if
       the eligible bit is not set. */
    unsigned empty_bits;

    /* Tabled means that we have previously found this page to be totally free of objects, so we'd like
       to defer allocating in the page until we allocate in pages that aren't totally free. */
    unsigned tabled_bits;
};

/* FIXME: Attempt to introduce code to pack the bitvector if we don't need the empty bits. Currently
   this is not used, but should be. */
struct pas_segregated_directory_simple_bitvector_segment {
    unsigned eligible_bits;
    unsigned tabled_bits;
};

#define PAS_SEGREGATED_DIRECTORY_BITVECTOR_SEGMENT_INITIALIZER \
    ((pas_segregated_directory_bitvector_segment){ \
        .eligible_bits = 0, \
        .empty_bits = 0, \
        .tabled_bits = 0 \
    })

#define PAS_SEGREGATED_DIRECTORY_SIMPLE_BITVECTOR_SEGMENT_INITIALIZER \
    ((pas_segregated_directory_simple_bitvector_segment){ \
        .eligible_bits = 0, \
        .tabled_bits = 0 \
    })

PAS_DECLARE_SEGMENTED_VECTOR(pas_segregated_directory_segmented_bitvectors,
                             pas_segregated_directory_bitvector_segment,
                             4);
PAS_DECLARE_SEGMENTED_VECTOR(pas_segregated_directory_simple_segmented_bitvectors,
                             pas_segregated_directory_simple_bitvector_segment,
                             16);
PAS_DECLARE_IMMUTABLE_VECTOR(pas_segregated_directory_view_vector,
                             pas_compact_atomic_segregated_view);

PAS_DEFINE_COMPACT_ATOMIC_PTR(pas_segregated_directory_data, pas_segregated_directory_data_ptr);

PAS_DEFINE_COMPACT_TAGGED_ATOMIC_PTR(uintptr_t, pas_segregated_directory_sharing_payload_ptr);

struct pas_segregated_directory {
    pas_compact_atomic_segregated_view first_view;
    pas_segregated_directory_data_ptr data;
    unsigned bits;
    pas_segregated_page_config_kind page_config_kind : 8;
    pas_page_sharing_mode sharing_mode : 8;
    pas_segregated_directory_kind directory_kind : 8;
    bool is_basic_size_directory; /* This field is owned by the segregated_heap. */
};

#define PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_SHIFT       0u
#define PAS_SEGREGATED_DIRECTORY_BITS_EMPTY_SHIFT          1u
#define PAS_SEGREGATED_DIRECTORY_BITS_TABLED_SHIFT         2u

#define PAS_SEGREGATED_DIRECTORY_BITS_MISC_SHIFT           3u
#define PAS_SEGREGATED_DIRECTORY_BITS_OTHER_MISC_SHIFT     4u

#define PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_MASK \
    (1u << PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_SHIFT)
#define PAS_SEGREGATED_DIRECTORY_BITS_EMPTY_MASK \
    (1u << PAS_SEGREGATED_DIRECTORY_BITS_EMPTY_SHIFT)
#define PAS_SEGREGATED_DIRECTORY_BITS_TABLED_MASK \
    (1u << PAS_SEGREGATED_DIRECTORY_BITS_TABLED_SHIFT)

#define PAS_SEGREGATED_DIRECTORY_BITS_VIEW_MASK \
    (PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_MASK | \
     PAS_SEGREGATED_DIRECTORY_BITS_EMPTY_MASK | \
     PAS_SEGREGATED_DIRECTORY_BITS_TABLED_MASK)

#define PAS_SEGREGATED_DIRECTORY_BITS_MISC_MASK \
    (1u << PAS_SEGREGATED_DIRECTORY_BITS_MISC_SHIFT)
#define PAS_SEGREGATED_DIRECTORY_BITS_OTHER_MISC_MASK \
    (1u << PAS_SEGREGATED_DIRECTORY_BITS_OTHER_MISC_SHIFT)

/* This makes the GET_BIT/SET_BIT macros work. */
#define PAS_SEGREGATED_DIRECTORY_BITS_eligible_MASK \
    PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_MASK
#define PAS_SEGREGATED_DIRECTORY_BITS_empty_MASK \
    PAS_SEGREGATED_DIRECTORY_BITS_EMPTY_MASK
#define PAS_SEGREGATED_DIRECTORY_BITS_tabled_MASK \
    PAS_SEGREGATED_DIRECTORY_BITS_TABLED_MASK

#define PAS_SEGREGATED_DIRECTORY_BITS_eligible_AVAILABLE_IN_BIASING     true
#define PAS_SEGREGATED_DIRECTORY_BITS_empty_AVAILABLE_IN_BIASING  false
#define PAS_SEGREGATED_DIRECTORY_BITS_tabled_AVAILABLE_IN_BIASING       true

struct PAS_ALIGNED(sizeof(pas_versioned_field)) pas_segregated_directory_data {
    pas_versioned_field first_eligible_but_not_tabled;
    pas_versioned_field first_eligible_and_tabled;
    pas_versioned_field last_empty_plus_one; /* Zero means there aren't any. */

    pas_segregated_directory_segmented_bitvectors bitvectors;
    pas_segregated_directory_view_vector views;
    pas_segregated_directory_sharing_payload_ptr sharing_payload;
};

#define PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT 1

#define PAS_SEGREGATED_DIRECTORY_INITIALIZER(page_config_kind_argument, sharing_mode_argument, directory_kind_argument) \
    ((pas_segregated_directory){ \
        .page_config_kind = page_config_kind_argument, \
        .first_view = PAS_COMPACT_ATOMIC_PTR_INITIALIZER, \
        .data = PAS_COMPACT_ATOMIC_PTR_INITIALIZER, \
        .bits = 0, \
        .sharing_mode = (sharing_mode_argument), \
        .directory_kind = (directory_kind_argument), \
        .is_basic_size_directory = false \
    })

static inline size_t pas_segregated_directory_size(pas_segregated_directory* directory)
{
    pas_segregated_directory_data* data;
    if (!pas_compact_atomic_segregated_view_load(&directory->first_view))
        return 0;
    data = pas_segregated_directory_data_ptr_load(&directory->data);
    if (data)
        return data->views.size + 1;
    return 1;
}

static inline bool pas_segregated_directory_bits_get_by_mask(
    pas_segregated_directory* directory, unsigned mask)
{
    return !!(directory->bits & mask);
}

static inline bool pas_segregated_directory_bits_set_by_mask(
    pas_segregated_directory* directory,
    unsigned mask,
    bool value)
{
    for (;;) {
        unsigned old_bits;
        unsigned new_bits;

        old_bits = directory->bits;
        
        if (value)
            new_bits = old_bits | mask;
        else
            new_bits = old_bits & ~mask;

        if (new_bits == old_bits)
            return false;

        if (pas_compare_and_swap_uint32_weak(&directory->bits, old_bits, new_bits))
            return true;
    }
}

static inline bool pas_segregated_directory_get_misc_bit(pas_segregated_directory* directory)
{
    return pas_segregated_directory_bits_get_by_mask(
        directory, PAS_SEGREGATED_DIRECTORY_BITS_MISC_MASK);
}

static inline bool pas_segregated_directory_set_misc_bit(pas_segregated_directory* directory,
                                                         bool value)
{
    return pas_segregated_directory_bits_set_by_mask(
        directory, PAS_SEGREGATED_DIRECTORY_BITS_MISC_MASK, value);
}

static inline bool pas_segregated_directory_get_other_misc_bit(pas_segregated_directory* directory)
{
    return pas_segregated_directory_bits_get_by_mask(
        directory, PAS_SEGREGATED_DIRECTORY_BITS_OTHER_MISC_MASK);
}

static inline bool pas_segregated_directory_set_other_misc_bit(pas_segregated_directory* directory,
                                                               bool value)
{
    return pas_segregated_directory_bits_set_by_mask(
        directory, PAS_SEGREGATED_DIRECTORY_BITS_OTHER_MISC_MASK, value);
}

static PAS_ALWAYS_INLINE pas_segregated_directory_bitvector_segment
pas_segregated_directory_spoof_inline_segment(pas_segregated_directory* directory)
{
    pas_segregated_directory_bitvector_segment segment;
    unsigned bits;
    
    bits = directory->bits;
    
    segment.eligible_bits = (bits >> PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_SHIFT) & 1u;
    segment.empty_bits = (bits >> PAS_SEGREGATED_DIRECTORY_BITS_EMPTY_SHIFT) & 1u;
    segment.tabled_bits = (bits >> PAS_SEGREGATED_DIRECTORY_BITS_TABLED_SHIFT) & 1u;

    return segment;
}

#define PAS_SEGREGATED_DIRECTORY_GET_BIT(directory, index, bit_name) ({ \
        pas_segregated_directory* _directory; \
        size_t _index; \
        unsigned* _word_ptr; \
        bool _result; \
        \
        _directory = (directory); \
        \
        _index = (index); \
        PAS_TESTING_ASSERT(_index < pas_segregated_directory_size(directory)); \
        \
        if (!_index) { \
            _result = pas_segregated_directory_bits_get_by_mask( \
                _directory, PAS_SEGREGATED_DIRECTORY_BITS_##bit_name##_MASK); \
        } else { \
            \
            --_index; \
            _word_ptr = &pas_segregated_directory_segmented_bitvectors_get_ptr( \
                &pas_segregated_directory_data_ptr_load_non_null(&_directory->data)->bitvectors, \
                PAS_BITVECTOR_WORD_INDEX(_index))->bit_name##_bits; \
            _result = pas_bitvector_get_from_word(*_word_ptr, _index); \
        } \
        _result; \
    })

#define PAS_SEGREGATED_DIRECTORY_SET_BIT(directory, index, bit_name, value) ({ \
        const bool _verbose = false; \
        \
        pas_segregated_directory* _directory; \
        size_t _index; \
        unsigned* _word_ptr; \
        bool _value; \
        bool _result; \
        \
        _directory = (directory); \
        _index = (index); \
        _value = (value); \
        \
        PAS_TESTING_ASSERT(_index < pas_segregated_directory_size(directory)); \
        PAS_TESTING_ASSERT( \
            !_value \
            || _directory->directory_kind != pas_segregated_biasing_directory_kind \
            || PAS_SEGREGATED_DIRECTORY_BITS_##bit_name##_AVAILABLE_IN_BIASING); \
        \
        if (!_index) { \
            _result = pas_segregated_directory_bits_set_by_mask( \
                _directory, PAS_SEGREGATED_DIRECTORY_BITS_##bit_name##_MASK, _value); \
        } else {\
            size_t _adjusted_index; \
            _adjusted_index = _index - 1; \
            _word_ptr = &pas_segregated_directory_segmented_bitvectors_get_ptr( \
                &pas_segregated_directory_data_ptr_load_non_null(&_directory->data)->bitvectors, \
                PAS_BITVECTOR_WORD_INDEX(_adjusted_index))->bit_name##_bits; \
            _result = pas_bitvector_set_atomic_in_word(_word_ptr, _adjusted_index, _value); \
        } \
        \
        if (_verbose && _result) { \
            pas_log("%p[%s, %zu]: set to %s at %s:%d %s\n", \
                    _directory, #bit_name, _index, _value ? "true" : "false", \
                    __FILE__, __LINE__, __PRETTY_FUNCTION__); \
        } \
        \
        _result; \
    })

static inline bool pas_segregated_directory_get_empty_bit(
    pas_segregated_directory* directory,
    size_t page_index)
{
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(directory, page_index, empty);
}

static inline bool pas_segregated_directory_set_empty_bit(
    pas_segregated_directory* directory,
    size_t page_index,
    bool value)
{
    return PAS_SEGREGATED_DIRECTORY_SET_BIT(directory, page_index, empty, value);
}

PAS_API void pas_segregated_directory_construct(
    pas_segregated_directory* directory,
    pas_segregated_page_config_kind page_config_kind,
    pas_page_sharing_mode page_sharing_mode,
    pas_segregated_directory_kind directory_kind);

PAS_API pas_segregated_directory_data*
pas_segregated_directory_get_data_slow(pas_segregated_directory* directory,
                                       pas_lock_hold_mode heap_lock_hold_mode);

static inline pas_segregated_directory_data*
pas_segregated_directory_get_data(pas_segregated_directory* directory,
                                  pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_directory_data* data;
    data = pas_segregated_directory_data_ptr_load(&directory->data);
    if (data)
        return data;
    return pas_segregated_directory_get_data_slow(directory, heap_lock_hold_mode);
}

static inline bool
pas_segregated_directory_can_do_sharing(pas_segregated_directory* directory)
{
    return pas_page_sharing_mode_does_sharing(directory->sharing_mode);
}

static inline bool
pas_segregated_directory_is_doing_sharing(pas_segregated_directory* directory)
{
    pas_segregated_directory_data* data;
    data = pas_segregated_directory_data_ptr_load(&directory->data);
    return data
        && (pas_segregated_directory_sharing_payload_ptr_load(&data->sharing_payload)
            & PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT);
}

static inline pas_page_sharing_participant_payload*
pas_segregated_directory_data_try_get_sharing_payload(pas_segregated_directory_data* data)
{
    uintptr_t value;

    value = pas_segregated_directory_sharing_payload_ptr_load(&data->sharing_payload);

    return (pas_page_sharing_participant_payload*)(
        value & ~PAS_SEGREGATED_DIRECTORY_SHARING_PAYLOAD_IS_INITIALIZED_BIT);
}

PAS_API uint64_t pas_segregated_directory_get_use_epoch(pas_segregated_directory* directory);

PAS_API pas_page_sharing_participant_payload*
pas_segregated_directory_get_sharing_payload(pas_segregated_directory* directory,
                                             pas_lock_hold_mode heap_lock_hold_mode);

static inline void
pas_segregated_directory_start_sharing_if_necessary(pas_segregated_directory* directory,
                                                    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_segregated_directory_get_sharing_payload(directory, heap_lock_hold_mode);
}

static inline pas_versioned_field* pas_segregated_directory_data_get_first_eligible_ptr(
    pas_segregated_directory_data* data,
    pas_segregated_directory_first_eligible_kind kind)
{
    switch (kind) {
    case pas_segregated_directory_first_eligible_but_not_tabled_kind:
        return &data->first_eligible_but_not_tabled;
    case pas_segregated_directory_first_eligible_and_tabled_kind:
        return &data->first_eligible_and_tabled;
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_API void pas_segregated_directory_minimize_first_eligible(
    pas_segregated_directory* directory,
    pas_segregated_directory_first_eligible_kind kind,
    size_t index);

PAS_API void pas_segregated_directory_update_first_eligible_after_search(
    pas_segregated_directory* directory,
    pas_segregated_directory_first_eligible_kind kind,
    pas_versioned_field first_eligible,
    size_t new_value);

PAS_API bool pas_segregated_directory_view_did_become_eligible_at_index_without_biasing_update(
    pas_segregated_directory* directory,
    size_t index);
PAS_API bool pas_segregated_directory_view_did_become_eligible_without_biasing_update(
    pas_segregated_directory* directory,
    pas_segregated_view view);
PAS_API bool pas_segregated_directory_view_did_become_eligible_at_index(
    pas_segregated_directory* directory,
    size_t index);
PAS_API bool pas_segregated_directory_view_did_become_eligible(
    pas_segregated_directory* directory,
    pas_segregated_view view);
PAS_API bool pas_segregated_directory_view_did_become_empty_at_index(
    pas_segregated_directory* directory,
    size_t index);
PAS_API bool pas_segregated_directory_view_did_become_empty(
    pas_segregated_directory* directory,
    pas_segregated_view view);

static inline bool pas_segregated_directory_is_eligible(
    pas_segregated_directory* directory, size_t index)
{
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(directory, index, eligible);
}

static inline bool pas_segregated_directory_is_empty(
    pas_segregated_directory* directory, size_t index)
{
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(directory, index, empty);
}

static inline bool pas_segregated_directory_is_tabled(
    pas_segregated_directory* directory, size_t index)
{
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(directory, index, tabled);
}

PAS_API bool pas_segregated_directory_is_committed(
    pas_segregated_directory* directory, size_t index);

PAS_API size_t pas_segregated_directory_num_committed_views(
    pas_segregated_directory* directory);

PAS_API size_t pas_segregated_directory_num_empty_views(
    pas_segregated_directory* directory);

PAS_API size_t pas_segregated_directory_num_empty_granules(
    pas_segregated_directory* directory);

static inline pas_versioned_field pas_segregated_directory_get_first_eligible_impl(
    pas_segregated_directory* directory,
    pas_segregated_directory_first_eligible_kind kind,
    pas_versioned_field (*read)(pas_versioned_field* field))
{
    pas_segregated_directory_data* data;
    unsigned expected;

    data = pas_segregated_directory_data_ptr_load(&directory->data);
    if (data)
        return read(pas_segregated_directory_data_get_first_eligible_ptr(data, kind));

    switch (kind) {
    case pas_segregated_directory_first_eligible_but_not_tabled_kind:
        expected = PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_MASK;
        break;
    case pas_segregated_directory_first_eligible_and_tabled_kind:
        expected =
            PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_MASK |
            PAS_SEGREGATED_DIRECTORY_BITS_TABLED_MASK;
        break;
    }
    
    if ((directory->bits & (PAS_SEGREGATED_DIRECTORY_BITS_ELIGIBLE_MASK |
                            PAS_SEGREGATED_DIRECTORY_BITS_TABLED_MASK))
        == expected)
        return pas_versioned_field_create_with_invalid_version(0);

    return !pas_compact_atomic_segregated_view_is_null(&directory->first_view)
        ? pas_versioned_field_create_with_invalid_version(1)
        : pas_versioned_field_create_with_invalid_version(0);
}

static inline pas_versioned_field pas_segregated_directory_get_first_eligible(
    pas_segregated_directory* directory,
    pas_segregated_directory_first_eligible_kind kind)
{
    return pas_segregated_directory_get_first_eligible_impl(
        directory, kind, pas_versioned_field_read);
}

static inline pas_versioned_field pas_segregated_directory_get_first_eligible_torn(
    pas_segregated_directory* directory,
    pas_segregated_directory_first_eligible_kind kind)
{
    return pas_segregated_directory_get_first_eligible_impl(
        directory, kind, pas_versioned_field_read_torn);
}

static inline pas_versioned_field pas_segregated_directory_watch_first_eligible(
    pas_segregated_directory* directory,
    pas_segregated_directory_first_eligible_kind kind)
{
    return pas_segregated_directory_get_first_eligible_impl(
        directory, kind, pas_versioned_field_read_to_watch);
}

static inline pas_versioned_field pas_segregated_directory_get_last_empty_plus_one_impl(
    pas_segregated_directory* directory,
    pas_versioned_field (*read)(pas_versioned_field* field))
{
    pas_segregated_directory_data* data;

    data = pas_segregated_directory_data_ptr_load(&directory->data);

    if (data)
        return read(&data->last_empty_plus_one);
    
    if (directory->bits & PAS_SEGREGATED_DIRECTORY_BITS_EMPTY_MASK)
        return pas_versioned_field_create_with_invalid_version(1);

    return pas_versioned_field_create_with_invalid_version(0);
}

static inline pas_versioned_field pas_segregated_directory_get_last_empty_plus_one(
    pas_segregated_directory* directory)
{
    return pas_segregated_directory_get_last_empty_plus_one_impl(
        directory, pas_versioned_field_read);
}

static inline uintptr_t pas_segregated_directory_get_last_empty_plus_one_value(
    pas_segregated_directory* directory)
{
    return pas_segregated_directory_get_last_empty_plus_one_impl(
        directory, pas_versioned_field_read_torn).value;
}

static inline pas_versioned_field pas_segregated_directory_watch_last_empty_plus_one(
    pas_segregated_directory* directory)
{
    return pas_segregated_directory_get_last_empty_plus_one_impl(
        directory, pas_versioned_field_read_to_watch);
}

PAS_API void pas_segregated_directory_update_last_empty_plus_one_after_search(
    pas_segregated_directory* directory,
    pas_versioned_field last_empty_plus_one,
    size_t new_value);

static inline pas_versioned_field* pas_segregated_directory_last_empty_plus_one(
    pas_segregated_directory* directory)
{
    pas_segregated_directory_data* data;

    data = pas_segregated_directory_data_ptr_load(&directory->data);
    if (!data)
        return NULL;

    return &data->last_empty_plus_one;
}

static inline pas_segregated_view pas_segregated_directory_get(
    pas_segregated_directory* directory, size_t index)
{
    PAS_ASSERT(index < pas_segregated_directory_size(directory));
    if (!index)
        return pas_compact_atomic_segregated_view_load(&directory->first_view);
    return pas_compact_atomic_segregated_view_load(
        pas_segregated_directory_view_vector_get_ptr(
            &pas_segregated_directory_data_ptr_load_non_null(&directory->data)->views,
            index - 1));
}

/* Must call this requesting to append at index == size. We use this pattern because we expect
   that everything is locked when we do this and we have already constructed the view at least
   somewhat and may have locked in the expectation that it'll be at this index. */
PAS_API void pas_segregated_directory_append(
    pas_segregated_directory* directory, size_t index, pas_segregated_view view);

/* This has a maximally inclusion way of treating memory that multiple views may have dibs on. For
   example, shared page directories and partial views in size directories may report on the same
   bytes of memory. Exclusive views in a global size directory may report on the same memory as
   biasing views in biasing directories. */
PAS_API pas_heap_summary
pas_segregated_directory_compute_summary(pas_segregated_directory* directory);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_DIRECTORY_H */

