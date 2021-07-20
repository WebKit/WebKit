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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_all_biasing_directories.h"

#include "pas_biasing_directory_inlines.h"
#include "pas_page_sharing_pool.h"
#include "pas_scavenger.h"

pas_all_biasing_directories_active_bitvector pas_all_biasing_directories_active_bitvector_instance;
pas_all_biasing_directories_vector pas_all_biasing_directories_vector_instance;

void pas_all_biasing_directories_append(pas_biasing_directory* directory)
{
    size_t index;
    size_t word_index;
    pas_compact_atomic_biasing_directory_ptr directory_ptr;

    pas_heap_lock_assert_held();
    
    index = pas_all_biasing_directories_vector_instance.size;

    PAS_ASSERT(!directory->index_in_all);
    PAS_ASSERT((unsigned)index == index);
    directory->index_in_all = (unsigned)index;

    pas_compact_atomic_biasing_directory_ptr_store(&directory_ptr, directory);
    pas_all_biasing_directories_vector_append(
        &pas_all_biasing_directories_vector_instance, directory_ptr, pas_lock_is_held);
    
    /* We have to store to the bitvector second, so that when we see a bit set in the bitvector,
       we know that the thing in question is already appended to the vector. This is actually
       overkill since we only store the bit using a CAS and only after releasing the heap lock, but
       whatever - it's not worth it to try to be fast here. */
    pas_fence();

    word_index = PAS_BITVECTOR_WORD_INDEX(index);

    while (word_index >= pas_all_biasing_directories_active_bitvector_instance.size) {
        pas_all_biasing_directories_active_bitvector_append(
            &pas_all_biasing_directories_active_bitvector_instance, 0, pas_lock_is_held);
    }
}

bool pas_all_biasing_directories_activate(pas_biasing_directory* directory)
{
    size_t index;
    size_t word_index;
    bool did_activate;

    index = directory->index_in_all;
    
    word_index = PAS_BITVECTOR_WORD_INDEX(index);
    
    PAS_ASSERT(word_index < pas_all_biasing_directories_active_bitvector_instance.size);
    
    did_activate = pas_bitvector_set_atomic_in_word(
        pas_all_biasing_directories_active_bitvector_get_ptr(
            &pas_all_biasing_directories_active_bitvector_instance,
            word_index),
        index,
        true);

    if (did_activate)
        pas_scavenger_did_create_eligible();

    return did_activate;
}

typedef struct {
    bool should_go_again;
    unsigned* word_ptr;
} scavenge_data;

static unsigned scavenge_for_each_set_bit_bits_source(size_t word_index,
                                                      void* arg)
{
    scavenge_data* data;

    PAS_UNUSED_PARAM(word_index);

    data = arg;

    return *data->word_ptr;
}

static bool scavenge_for_each_set_bit_callback(pas_found_bit_index found_bit_index,
                                               void* arg)
{
    scavenge_data* data;
    unsigned index;
    unsigned word;
    unsigned* word_ptr;
    pas_biasing_directory* directory;
    unsigned current_high_watermark;
    uintptr_t large_eligible_high_watermark;
    unsigned eligible_high_watermark;

    data = arg;

    index = (unsigned)found_bit_index.index;
    PAS_ASSERT(index == found_bit_index.index);

    word = found_bit_index.word;
    word_ptr = data->word_ptr;

    PAS_ASSERT(index < pas_all_biasing_directories_vector_instance.size);
    directory = pas_compact_atomic_biasing_directory_ptr_load(
        pas_all_biasing_directories_vector_get_ptr(
            &pas_all_biasing_directories_vector_instance, index));

    current_high_watermark = directory->current_high_watermark;

    if (current_high_watermark)
        data->should_go_again = true;
    else {
        /* We've got to clear the bit before we do anything else. Otherwise there's a race where we
           might miss a notification about activation. */
        pas_bitvector_set_atomic_in_word(word_ptr, index, false);
    }
    
    large_eligible_high_watermark = directory->eligible_high_watermark.value;
    eligible_high_watermark = (unsigned)large_eligible_high_watermark;
    PAS_ASSERT(eligible_high_watermark == large_eligible_high_watermark);

    directory->previous_high_watermark = current_high_watermark;
    directory->current_high_watermark = 0;

    if (current_high_watermark < eligible_high_watermark)
        pas_biasing_directory_did_create_delta(directory);

    return true;
}

static bool scavenge_bitvector_word_callback(unsigned* word_ptr, size_t word_index, void* arg)
{
    scavenge_data* data;
    bool result;

    data = arg;
    data->word_ptr = word_ptr;
    
    result = pas_bitvector_for_each_set_bit(scavenge_for_each_set_bit_bits_source,
                                            word_index,
                                            word_index + 1,
                                            scavenge_for_each_set_bit_callback,
                                            data);
    PAS_ASSERT(result);
    
    return true;
}

bool pas_all_biasing_directories_scavenge(void)
{
    scavenge_data data;
    pas_found_index result;

    data.should_go_again = false;
    data.word_ptr = NULL;

    result = pas_all_biasing_directories_active_bitvector_iterate(
        &pas_all_biasing_directories_active_bitvector_instance,
        0,
        scavenge_bitvector_word_callback,
        &data);

    PAS_ASSERT(!result.did_succeed);

    return data.should_go_again;
}

#endif /* LIBPAS_ENABLED */
