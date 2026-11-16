/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_BITFIT_DIRECTORY_INLINES_H
#define PAS_BITFIT_DIRECTORY_INLINES_H

#include "pas_bitfit_directory.h"
#include "pas_bitfit_page_config.h"

PAS_BEGIN_EXTERN_C;

typedef struct {
    unsigned num_bits;
} pas_bitfit_directory_find_first_free_for_num_bits_iterate_data;

static PAS_ALWAYS_INLINE bool
pas_bitfit_directory_find_first_free_for_num_bits_iterate_callback(
    pas_bitfit_max_free* entry_ptr,
    size_t index,
    void* arg)
{
    pas_bitfit_directory_find_first_free_for_num_bits_iterate_data* data;
    pas_bitfit_max_free entry;

    PAS_UNUSED_PARAM(index);

    data = arg;

    entry = *entry_ptr;

    /* Return true if we want to keep going. */
    if (entry < data->num_bits)
        return true;
    
    if (entry == PAS_BITFIT_MAX_FREE_EMPTY)
        return true;
    
    return false;
}

static PAS_ALWAYS_INLINE pas_found_index
pas_bitfit_directory_find_first_free_for_num_bits(pas_bitfit_directory* directory,
                                                  unsigned start_index,
                                                  unsigned num_bits)
{
    pas_bitfit_directory_find_first_free_for_num_bits_iterate_data data;
    
    PAS_TESTING_ASSERT(num_bits <= (unsigned)UINT8_MAX);

    data.num_bits = num_bits;

    return pas_bitfit_directory_max_free_vector_iterate(
        &directory->max_frees, start_index,
        pas_bitfit_directory_find_first_free_for_num_bits_iterate_callback, &data);
}

static PAS_ALWAYS_INLINE pas_found_index
pas_bitfit_directory_find_first_free(pas_bitfit_directory* directory,
                                     unsigned start_index,
                                     unsigned size,
                                     pas_bitfit_page_config page_config)
{
    return pas_bitfit_directory_find_first_free_for_num_bits(
        directory, start_index, size >> page_config.base.min_align_shift);
}

static PAS_ALWAYS_INLINE bool
pas_bitfit_directory_find_first_empty_iterate_callback(
    pas_bitfit_max_free* entry_ptr,
    size_t index,
    void* arg)
{
    PAS_UNUSED_PARAM(index);
    PAS_UNUSED_PARAM(arg);
    /* Return true if we want to keep going. */
    return *entry_ptr != PAS_BITFIT_MAX_FREE_EMPTY;
}

static PAS_ALWAYS_INLINE pas_found_index
pas_bitfit_directory_find_first_empty(pas_bitfit_directory* directory,
                                      unsigned start_index)
{
    return pas_bitfit_directory_max_free_vector_iterate(
        &directory->max_frees, start_index,
        pas_bitfit_directory_find_first_empty_iterate_callback, NULL);
}

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_DIRECTORY_INLINES_H */

