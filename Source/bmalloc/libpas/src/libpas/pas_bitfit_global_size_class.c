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

#include "pas_bitfit_global_size_class.h"

#include "pas_biasing_directory_inlines.h"
#include "pas_bitfit_biasing_directory.h"
#include "pas_bitfit_global_directory.h"
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_magazine.h"
#include "pas_page_sharing_participant.h"
#include "pas_page_sharing_pool.h"

pas_bitfit_global_size_class* pas_bitfit_global_size_class_create(
    unsigned size,
    pas_bitfit_global_directory* directory,
    pas_compact_atomic_bitfit_size_class_ptr* insertion_point)
{
    pas_bitfit_global_size_class* result;

    result = pas_immortal_heap_allocate_with_alignment(
        sizeof(pas_bitfit_global_size_class),
        alignof(pas_bitfit_global_size_class),
        "pas_bitfit_global_size_class",
        pas_object_allocation);

    pas_bitfit_size_class_construct(&result->base, size, &directory->base, insertion_point);
    pas_bitfit_global_size_class_biasing_vector_construct(&result->biasing_vector);

    return result;
}

pas_bitfit_size_class*
pas_bitfit_global_size_class_select_for_magazine(pas_bitfit_global_size_class* global_size_class,
                                                 pas_magazine* magazine)
{
    pas_bitfit_directory* directory;
    pas_bitfit_global_directory* global_directory;
    pas_page_sharing_participant participant;
    pas_bitfit_size_class* result;
    pas_page_sharing_pool* pool;
    pas_bitfit_biasing_directory* biasing_directory;
    pas_compact_atomic_bitfit_size_class_ptr* size_class_ptr;
    unsigned magazine_index;

    directory = pas_compact_bitfit_directory_ptr_load(&global_size_class->base.directory);
    global_directory = (pas_bitfit_global_directory*)directory;
    
    if (!global_directory->contention_did_trigger_explosion || !magazine)
        return &global_size_class->base;
    
    PAS_ASSERT(pas_bitfit_global_directory_can_explode);
    magazine_index = magazine->magazine_index;

    if (magazine_index < global_size_class->biasing_vector.size) {
        result = pas_compact_atomic_bitfit_size_class_ptr_load(
            pas_bitfit_global_size_class_biasing_vector_get_ptr(
                &global_size_class->biasing_vector, magazine_index));
        if (result)
            return result;
    }

    pool = pas_compact_atomic_page_sharing_pool_ptr_load(&global_directory->bias_sharing_pool);
    if (!pool) {
        pas_heap_lock_lock();

        pool = pas_compact_atomic_page_sharing_pool_ptr_load(&global_directory->bias_sharing_pool);
        if (!pool) {
            pool = pas_immortal_heap_allocate_with_alignment(
                sizeof(pas_page_sharing_pool),
                alignof(pas_page_sharing_pool),
                "pas_bitfit_global_size_class/bias_sharing_pool",
                pas_object_allocation);
            pas_page_sharing_pool_construct(pool);
            pas_compact_atomic_page_sharing_pool_ptr_store(
                &global_directory->bias_sharing_pool, pool);
        }

        pas_heap_lock_unlock();

        PAS_ASSERT(
            pas_compact_atomic_page_sharing_pool_ptr_load(&global_directory->bias_sharing_pool)
            == pool);
    }

    PAS_ASSERT(pool);

    participant = pas_page_sharing_pool_get_participant(pool, magazine_index);
    if (!participant) {
        biasing_directory = NULL;
        pas_heap_lock_lock();
        participant = pas_page_sharing_pool_get_participant(pool, magazine_index);
        if (!participant) {
            biasing_directory = pas_bitfit_biasing_directory_create(
                global_directory, magazine_index);
        }
        pas_heap_lock_unlock();

        if (!participant) {
            participant = pas_page_sharing_pool_get_participant(pool, magazine_index);
            PAS_ASSERT(
                participant = pas_page_sharing_participant_create(
                    &biasing_directory->biasing_base,
                    pas_page_sharing_participant_biasing_directory));
        }
    }
    
    PAS_ASSERT(pas_page_sharing_participant_get_kind(participant)
               == pas_page_sharing_participant_biasing_directory);

    biasing_directory = pas_unwrap_bitfit_biasing_directory(
        pas_page_sharing_participant_get_ptr(participant));

    pas_heap_lock_lock();

    while (magazine_index >= global_size_class->biasing_vector.size) {
        pas_compact_atomic_bitfit_size_class_ptr null_ptr = PAS_COMPACT_ATOMIC_PTR_INITIALIZER;
        pas_bitfit_global_size_class_biasing_vector_append(
            &global_size_class->biasing_vector, null_ptr, pas_lock_is_held);
    }

    size_class_ptr = pas_bitfit_global_size_class_biasing_vector_get_ptr(
        &global_size_class->biasing_vector, magazine_index);
    result = pas_compact_atomic_bitfit_size_class_ptr_load(size_class_ptr);
    if (!result) {
        result = pas_bitfit_size_class_create(
            global_size_class->base.size, &biasing_directory->base);
        pas_fence();
        pas_compact_atomic_bitfit_size_class_ptr_store(size_class_ptr, result);
    }

    pas_heap_lock_unlock();

    return result;
}

#endif /* LIBPAS_ENABLED */
