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

#include "pas_bitfit_size_class.h"

#include "pas_bitfit_directory.h"
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"

pas_compact_atomic_bitfit_size_class_ptr*
pas_bitfit_size_class_find_insertion_point(pas_bitfit_directory* directory,
                                           unsigned size)
{
    pas_compact_atomic_bitfit_size_class_ptr* next_ptr;

    next_ptr = &directory->largest_size_class;
    for (;;) {
        pas_bitfit_size_class* current_size_class;

        current_size_class = pas_compact_atomic_bitfit_size_class_ptr_load(next_ptr);
        
        if (!current_size_class || size >= current_size_class->size)
            return next_ptr;

        next_ptr = &current_size_class->next_smaller;
    }
}

void pas_bitfit_size_class_construct(pas_bitfit_size_class* size_class,
                                     unsigned size,
                                     pas_bitfit_directory* directory,
                                     pas_compact_atomic_bitfit_size_class_ptr* insertion_point)
{
    pas_bitfit_size_class* next_size_class;
    
    pas_heap_lock_assert_held();
    PAS_ASSERT(insertion_point);
    
    pas_versioned_field_construct(&size_class->first_free, 0);
    size_class->size = size;
    pas_compact_bitfit_directory_ptr_store(&size_class->directory, directory);
    pas_compact_atomic_bitfit_size_class_ptr_store(&size_class->next_smaller, NULL);

    next_size_class = pas_compact_atomic_bitfit_size_class_ptr_load(insertion_point);
    PAS_ASSERT(!next_size_class || size > next_size_class->size);
    
    pas_compact_atomic_bitfit_size_class_ptr_store(
        &size_class->next_smaller, next_size_class);
    pas_compact_atomic_bitfit_size_class_ptr_store(insertion_point, size_class);
}

pas_bitfit_size_class* pas_bitfit_size_class_create(unsigned size,
                                                    pas_bitfit_directory* directory)
{
    pas_bitfit_size_class* result;

    result = pas_immortal_heap_allocate_with_alignment(
        sizeof(pas_bitfit_size_class),
        alignof(pas_bitfit_size_class),
        "pas_bitfit_size_class",
        pas_object_allocation);

    pas_bitfit_size_class_construct(
        result, size, directory,
        pas_bitfit_size_class_find_insertion_point(directory, size));

    return result;
}

pas_bitfit_view*
pas_bitfit_size_class_get_first_free_view(pas_bitfit_size_class* size_class,
                                          pas_bitfit_global_size_class* global_size_class,
                                          pas_bitfit_page_config* page_config)
{
    pas_bitfit_directory* directory;
    pas_versioned_field first_free_for_size;
    pas_versioned_field first_unprocessed_free;
    pas_bitfit_view_and_index view_and_index;
    uintptr_t start_index;

    directory = pas_compact_bitfit_directory_ptr_load_non_null(&size_class->directory);

    first_free_for_size = pas_versioned_field_read_to_watch(&size_class->first_free);
    first_unprocessed_free = pas_versioned_field_read_to_watch(&directory->first_unprocessed_free);

    start_index = PAS_MIN(first_free_for_size.value,
                          first_unprocessed_free.value);
    PAS_ASSERT((unsigned)start_index == start_index);
    
    view_and_index = pas_bitfit_directory_get_first_free_view(
        directory, global_size_class, (unsigned)start_index, size_class->size, page_config);

    PAS_ASSERT(view_and_index.view);

    /* So, this is the first view that had enough stuff for our requested size. That means it also
       has to be the first unprocessed or the first unprocessed has a higher index - since otherwise
       we would have picked the lower index here. */

    pas_versioned_field_maximize_watched(
        &size_class->first_free, first_free_for_size, view_and_index.index);
    pas_versioned_field_maximize_watched(
        &directory->first_unprocessed_free, first_unprocessed_free, view_and_index.index);

    return view_and_index.view;
}

#endif /* LIBPAS_ENABLED */
