/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_DIRECTORY_INLINES_H
#define PAS_SEGREGATED_DIRECTORY_INLINES_H

#include "pas_log.h"
#include "pas_segregated_directory.h"
#include "pas_segregated_directory_bit_reference.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_directory_iterate_config;
typedef struct pas_segregated_directory_iterate_config pas_segregated_directory_iterate_config;

struct pas_segregated_directory_iterate_config {
    pas_segregated_directory* directory;
    
    size_t index; /* This gets mutated so you know where the iteration ended up. */
    size_t index_end; /* Only need to set this if you're calling iterate_forward directly. */

    /* This stuff gets populated for you. */
    size_t first_considered;
    pas_segregated_directory_bit_reference bit_reference;
    pas_segregated_directory_bitvector_segment segment;

    /* Each bit in the _bits variables tells a fact about a page. There are up to 32 pages. You return
       a mask describing which pages to look at.
       
       You don't know how many or which of the 32 bits represent actual pages, but fake pages will have
       all zero bits. It doesn't matter what bit value you return for bits that don't represent actual
       pages. Garbage in, garbage out.
       
       This should be simple arithmetic. Or, if you want to just look at every page, return UINT_MAX
       unconditionally. */
    unsigned (*should_consider_view_parallel)(
        pas_segregated_directory_bitvector_segment segment,
        pas_segregated_directory_iterate_config* config);

    /* If this is NULL, it's assumed to return true. */
    bool (*consider_view)(pas_segregated_directory_iterate_config* config);

    void* arg; /* Use this as you like. */
};

static PAS_ALWAYS_INLINE bool
pas_segregated_directory_iterate_iterate_callback(
    pas_segregated_directory_bitvector_segment* segment_ptr,
    size_t word_index,
    void* arg,
    bool is_forward)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    pas_segregated_directory_iterate_config* config;
    pas_segregated_directory_bitvector_segment segment;
    size_t base_index;
    unsigned combined_word;

    config = (pas_segregated_directory_iterate_config*)arg;

    segment = *segment_ptr;

    base_index = PAS_BITVECTOR_BIT_INDEX(word_index);

    if (verbose)
        pas_log("Looking at word_index = %zu, base_index = %zu.\n", word_index, base_index);

    combined_word = config->should_consider_view_parallel(segment, config);

    if (verbose)
        pas_log("%zu: combined_word = %u\n", word_index, combined_word);
    
    while (combined_word) {
        size_t index_offset;
        size_t index;
        unsigned mask;
        
        if (verbose)
            pas_log("%zu: now combined_word = %u\n", word_index, combined_word);
    
        if (is_forward)
            index_offset = (size_t)__builtin_ctz(combined_word);
        else
            index_offset = 31lu - (size_t)__builtin_clz(combined_word);
        index = base_index + index_offset;

        mask = PAS_BITVECTOR_BIT_MASK(index_offset);

        config->index = index + 1;
        config->segment = segment;
        config->bit_reference =
            pas_segregated_directory_bit_reference_create_out_of_line(index + 1, segment_ptr, mask);

        if (config->first_considered == SIZE_MAX) {
            if (verbose)
                pas_log("%p: setting first_considered to %zu\n", config->directory, config->index);
            config->first_considered = config->index;
        } else {
            if (verbose) {
                pas_log("%p: not setting first_considered because it is %zu\n",
                        config->directory, config->first_considered);
            }
        }

        if (config->index >= config->index_end
            || !config->consider_view || config->consider_view(config))
            return false; /* Found what we needed so we can stop. */

        combined_word &= ~mask;
    }

    return true; /* Yes, we want the segmented vector iterate to continue. */
}

static PAS_ALWAYS_INLINE bool
pas_segregated_directory_iterate_forward_iterate_callback(
    pas_segregated_directory_bitvector_segment* segment_ptr,
    size_t word_index,
    void* arg)
{
    static const bool is_forward = true;
    return pas_segregated_directory_iterate_iterate_callback(
        segment_ptr, word_index, arg, is_forward);
}

static PAS_ALWAYS_INLINE bool
pas_segregated_directory_iterate_forward(
    pas_segregated_directory_iterate_config* config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    pas_segregated_directory_data* data;
    pas_found_index found_index;

    config->first_considered = SIZE_MAX;
    config->segment = PAS_SEGREGATED_DIRECTORY_BITVECTOR_SEGMENT_INITIALIZER;
    config->bit_reference = pas_segregated_directory_bit_reference_create_null();
    
    if (!config->index) {
        pas_segregated_directory_bitvector_segment segment;
        segment = pas_segregated_directory_spoof_inline_segment(config->directory);
        if (config->should_consider_view_parallel(segment, config)) {
            config->index = 0;
            config->segment = segment;
            config->bit_reference = pas_segregated_directory_bit_reference_create_inline();
            if (verbose)
                pas_log("%p: setting first_considered to 0\n", config->directory);
            config->first_considered = 0;
            if (config->index >= config->index_end
                || !config->consider_view || config->consider_view(config))
                return true;
        }
    }

    data = pas_segregated_directory_data_ptr_load(&config->directory->data);
    if (!data)
        return false;

    if (verbose) {
        pas_log("Actually using segmented bitvector iteration.\n");
        pas_log("Segmented bitvector size = %u\n", data->bitvectors.size);
    }
    found_index = pas_segregated_directory_segmented_bitvectors_iterate(
        &data->bitvectors, PAS_BITVECTOR_WORD_INDEX(PAS_MAX((size_t)1, config->index) - 1),
        pas_segregated_directory_iterate_forward_iterate_callback, config);
    
    return found_index.did_succeed;
}

static PAS_ALWAYS_INLINE bool
pas_segregated_directory_iterate_backward_iterate_callback(
    pas_segregated_directory_bitvector_segment* segment_ptr,
    size_t word_index,
    void* arg)
{
    static const bool is_forward = false;
    return pas_segregated_directory_iterate_iterate_callback(
        segment_ptr, word_index, arg, is_forward);
}

static PAS_ALWAYS_INLINE bool
pas_segregated_directory_iterate_backward(
    pas_segregated_directory_iterate_config* config)
{
    pas_segregated_directory_data* data;
    pas_segregated_directory_bitvector_segment segment;
    
    config->first_considered = SIZE_MAX;
    config->index_end = config->index + 1;
    config->segment = PAS_SEGREGATED_DIRECTORY_BITVECTOR_SEGMENT_INITIALIZER;
    config->bit_reference = pas_segregated_directory_bit_reference_create_null();
    
    data = pas_segregated_directory_data_ptr_load(&config->directory->data);
    if (data && config->index) {
        bool found_index;
        
        found_index = pas_segregated_directory_segmented_bitvectors_iterate_backward(
            &data->bitvectors, PAS_BITVECTOR_WORD_INDEX(config->index - 1),
            pas_segregated_directory_iterate_backward_iterate_callback, config);

        if (found_index)
            return true;
    }

    segment = pas_segregated_directory_spoof_inline_segment(config->directory);
    if (config->should_consider_view_parallel(segment, config)) {
        config->index = 0;
        config->segment = segment;
        config->bit_reference = pas_segregated_directory_bit_reference_create_inline();
        if (config->first_considered == SIZE_MAX)
            config->first_considered = 0;
        
        if (!config->consider_view || config->consider_view(config))
            return true;
    }

    return false;
}

static PAS_ALWAYS_INLINE bool
pas_segregated_directory_iterate_forward_to_take_first_eligible(
    pas_segregated_directory_iterate_config* config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    static const bool simple_eligibility = false;

    if (verbose)
        pas_log("%p: Doing take_first_eligible.\n", config->directory);
    for (;;) {
        pas_versioned_field first_eligible;
        size_t previous_size;
        bool did_find_something;

        previous_size = pas_segregated_directory_size(config->directory);
        if (verbose)
            pas_log("previous_size = %zu\n", previous_size);

        first_eligible = pas_segregated_directory_watch_first_eligible(config->directory);

        if (simple_eligibility)
            config->index = 0;
        else
            config->index = first_eligible.value;
        
        if (verbose)
            pas_log("starting from index = %zu\n", config->index);
        if (config->index == previous_size)
            return false;

        config->index_end = previous_size;

        pas_fence();

        did_find_something = pas_segregated_directory_iterate_forward(config);
        PAS_ASSERT(config->first_considered == SIZE_MAX
                   || config->first_considered <= config->index);

        if (did_find_something && config->index >= previous_size)
            did_find_something = false;

        if (!did_find_something) {
            pas_fence();
            
            if (pas_segregated_directory_size(config->directory) != previous_size)
                continue;

            config->index = previous_size;
            config->first_considered = PAS_MIN(previous_size, config->first_considered);

            if (verbose)
                pas_log("Didn't find anything.\n");
        }

        if (verbose) {
            pas_log("%p: Returning index = %zu, first_considered = %zu\n",
                    config->directory, config->index, config->first_considered);
        }

        PAS_ASSERT(config->first_considered <= previous_size);
        
        pas_segregated_directory_update_first_eligible_after_search(
            config->directory,
            first_eligible,
            config->first_considered);
        
        return did_find_something;
    }
}

static PAS_ALWAYS_INLINE bool
pas_segregated_directory_iterate_backward_to_take_last_empty(
    pas_segregated_directory_iterate_config* config)
{
    pas_versioned_field last_empty_plus_one;
    bool did_find_something;
    size_t new_last_empty_plus_one;
    
    last_empty_plus_one = pas_segregated_directory_watch_last_empty_plus_one(
        config->directory);

    if (!last_empty_plus_one.value)
        return false;
    
    config->index = last_empty_plus_one.value - 1;

    did_find_something = pas_segregated_directory_iterate_backward(config);

    if (!did_find_something)
        config->index = 0;

    if (config->first_considered == SIZE_MAX)
        new_last_empty_plus_one = 0;
    else
        new_last_empty_plus_one = config->first_considered + 1;
    
    pas_segregated_directory_update_last_empty_plus_one_after_search(config->directory,
                                                                     last_empty_plus_one,
                                                                     new_last_empty_plus_one);

    return did_find_something;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_DIRECTORY_INLINES_H */

