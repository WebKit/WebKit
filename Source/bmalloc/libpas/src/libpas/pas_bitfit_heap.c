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

#include "pas_bitfit_heap.h"

#include "pas_bitfit_global_directory.h"
#include "pas_bitfit_global_size_class.h"
#include "pas_bitfit_page.h"
#include "pas_bitfit_view.h"
#include "pas_heap_config.h"
#include "pas_immortal_heap.h"
#include "pas_segregated_global_size_directory.h"

pas_bitfit_heap* pas_bitfit_heap_create(pas_segregated_heap* segregated_heap,
                                        pas_heap_config* heap_config)
{
    pas_bitfit_heap* result;
    pas_bitfit_page_config_variant variant;

    result = pas_immortal_heap_allocate_with_alignment(
        sizeof(pas_bitfit_heap),
        sizeof(pas_versioned_field),
        "pas_bitfit_heap",
        pas_object_allocation);

    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(variant)) {
        pas_bitfit_global_directory_construct(
            pas_bitfit_heap_get_directory(result, variant),
            pas_heap_config_bitfit_page_config_ptr_for_variant(heap_config, variant),
            segregated_heap);
    }

    return result;
}

pas_bitfit_variant_selection pas_bitfit_heap_select_variant(size_t requested_object_size,
                                                            pas_heap_config* config)
{
    pas_bitfit_page_config_variant variant;
    pas_bitfit_page_config_variant best_variant;
    size_t best_object_size;
    pas_bitfit_variant_selection result;
    bool did_find_best;

    best_variant = (pas_bitfit_page_config_variant)0;
    best_object_size = 0;
    did_find_best = false;

    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(variant)) {
        pas_bitfit_page_config page_config;
        size_t object_size;

        page_config = *pas_heap_config_bitfit_page_config_ptr_for_variant(config, variant);
        if (!pas_bitfit_page_config_is_enabled(page_config))
            continue;

        object_size = pas_round_up_to_power_of_2(
            requested_object_size, pas_page_base_config_min_align(page_config.base));

        if (object_size <= page_config.base.max_object_size) {
            PAS_ASSERT(
                object_size
                < ((size_t)PAS_BITFIT_MAX_FREE_UNPROCESSED << page_config.base.min_align_shift));
            PAS_ASSERT(
                object_size
                <= ((size_t)PAS_BITFIT_MAX_FREE_MAX_VALID << page_config.base.min_align_shift));
            
            best_variant = variant;
            best_object_size = object_size;
            did_find_best = true;
            break;
        }
    }

    PAS_ASSERT(did_find_best);
    PAS_ASSERT(best_object_size);

    result.object_size = (unsigned)best_object_size;
    PAS_ASSERT(result.object_size == best_object_size);
    result.variant = best_variant;
    return result;
}

pas_bitfit_global_size_class*
pas_bitfit_heap_ensure_global_size_class(pas_bitfit_heap* heap,
                                         pas_segregated_global_size_directory* directory,
                                         pas_heap_config* config,
                                         pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_bitfit_global_size_class* result;
    pas_bitfit_variant_selection best;

    if (verbose) {
        pas_log("Considering creating size class for bitfit heap %p, segregated size directory %p "
                "(directory size = %u).\n",
                heap, directory, directory->object_size);
    }

    best = pas_bitfit_heap_select_variant(directory->object_size, config);

    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    pas_heap_lock_assert_held();
    if (verbose) {
        pas_log("Trying to create size class for bitfit heap %p, segregated size directory %p "
                "(directory size = %u, best object size = %u).\n",
                heap, directory, directory->object_size, best.object_size);
    }
    result = pas_compact_atomic_bitfit_global_size_class_ptr_load(&directory->bitfit_size_class);
    if (!result) {
        pas_compact_atomic_bitfit_size_class_ptr* insertion_point;
        pas_bitfit_global_directory* bitfit_directory;
        pas_bitfit_size_class* size_class;

        if (verbose)
            pas_log("It doesn't have one already.\n");

        bitfit_directory = pas_bitfit_heap_get_directory(heap, best.variant);

        insertion_point = pas_bitfit_size_class_find_insertion_point(
            &bitfit_directory->base, best.object_size);
        if (insertion_point)
            size_class = pas_compact_atomic_bitfit_size_class_ptr_load(insertion_point);
        else
            size_class = NULL;

        if (size_class && size_class->size == best.object_size) {
            if (verbose)
                pas_log("Actually it does!\n");
            result = (pas_bitfit_global_size_class*)size_class;
        } else {
            result = pas_bitfit_global_size_class_create(
                best.object_size, bitfit_directory, insertion_point);
        }
        
        pas_compact_atomic_bitfit_global_size_class_ptr_store(
            &directory->bitfit_size_class, result);
    }
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);

    return result;
}

pas_heap_summary pas_bitfit_heap_compute_summary(pas_bitfit_heap* heap)
{
    pas_heap_summary result;
    pas_bitfit_page_config_variant variant;

    result = pas_heap_summary_create_empty();

    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(variant)) {
        result = pas_heap_summary_add(
            result,
            pas_bitfit_directory_compute_summary(
                &pas_bitfit_heap_get_directory(heap, variant)->base));
    }

    return result;
}

typedef struct {
    pas_bitfit_heap* heap;
    pas_bitfit_heap_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_callback(
    pas_bitfit_view* view,
    uintptr_t begin,
    size_t size,
    void* arg)
{
    for_each_live_object_data* data;

    data = arg;

    return data->callback(data->heap, view, begin, size, data->arg);
}

bool pas_bitfit_heap_for_each_live_object(
    pas_bitfit_heap* heap,
    pas_bitfit_heap_for_each_live_object_callback callback,
    void* arg)
{
    pas_bitfit_page_config_variant variant;
    for_each_live_object_data data;
    
    data.heap = heap;
    data.callback = callback;
    data.arg = arg;

    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(variant)) {
        pas_bitfit_global_directory* directory;
        size_t index;

        directory = pas_bitfit_heap_get_directory(heap, variant);

        for (index = 0; index < pas_bitfit_directory_size(&directory->base); ++index) {
            pas_bitfit_view* view;

            view = pas_bitfit_directory_get_view(&directory->base, index);
            if (!view)
                continue;

            if (!pas_bitfit_view_for_each_live_object(view, for_each_live_object_callback, &data))
                return false;
        }
    }

    return true;
}

#endif /* LIBPAS_ENABLED */
