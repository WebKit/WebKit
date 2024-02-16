/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#include "pas_heap.h"

#include "pas_all_heaps.h"
#include "pas_heap_config.h"
#include "pas_heap_inlines.h"
#include "pas_heap_ref.h"
#include "pas_heap_table.h"
#include "pas_immortal_heap.h"
#include "pas_log.h"
#include "pas_monotonic_time.h"
#include "pas_primitive_heap_ref.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_segregated_size_directory.h"

pas_heap* pas_heap_create(pas_heap_ref* heap_ref,
                          pas_heap_ref_kind heap_ref_kind,
                          const pas_heap_config* config,
                          pas_heap_runtime_config* runtime_config)
{
    static const bool verbose = false;
    pas_heap* heap;
    uintptr_t begin;

    if (verbose) {
        pas_log("Creating heap for size = %lu, alignment = %lu.\n",
                config->get_type_size(heap_ref->type),
                config->get_type_alignment(heap_ref->type));
    }
    
    PAS_ASSERT(config->get_type_size(heap_ref->type) >= 1);
    PAS_ASSERT(pas_is_power_of_2(config->get_type_alignment(heap_ref->type)));
    PAS_ASSERT(pas_is_aligned(config->get_type_size(heap_ref->type),
                              config->get_type_alignment(heap_ref->type)));
    
    heap = pas_immortal_heap_allocate(sizeof(pas_heap), "pas_heap", pas_object_allocation);

    begin = (uintptr_t)heap;
    PAS_PROFILE(CREATE_HEAP, begin);
    heap = (void*)begin;

    pas_zero_memory(heap, sizeof(pas_heap));
    heap->type = heap_ref->type;
    pas_segregated_heap_construct(
        &heap->segregated_heap, heap, config, runtime_config);
    pas_large_heap_construct(&heap->large_heap);
    heap->heap_ref = heap_ref;
    heap->heap_ref_kind = heap_ref_kind;
    heap->config_kind = config->kind;

    // PGM being enabled in the config does not guarantee it will be called during runtime.
    if (config->pgm_enabled)
        pas_probabilistic_guard_malloc_initialize_pgm();
    
    pas_all_heaps_add_heap(heap);
    
    return heap;
}

size_t pas_heap_get_type_size(pas_heap* heap)
{
    pas_heap_config_kind kind;
    const pas_heap_config* config;
    if (!heap)
        return 1;
    kind = heap->config_kind;
    PAS_ASSERT(kind != pas_heap_config_kind_null);
    config = pas_heap_config_kind_get_config(kind);
    PAS_ASSERT(config);
    return config->get_type_size(heap->type);
}

size_t pas_heap_get_type_alignment(pas_heap* heap)
{
    if (!heap)
        return 1;
    return pas_heap_config_kind_get_config(heap->config_kind)->get_type_alignment(heap->type);
}

size_t pas_heap_get_num_free_bytes(pas_heap* heap)
{
    return
        pas_segregated_heap_get_num_free_bytes(&heap->segregated_heap)
        + pas_large_heap_get_num_free_bytes(&heap->large_heap);
}

typedef struct {
    pas_heap* heap;
    pas_heap_for_each_live_object_callback callback;
    void* arg;
} for_each_live_object_data;

static bool for_each_live_object_small_object_callback(pas_segregated_heap* heap,
                                                       uintptr_t begin,
                                                       size_t size,
                                                       void* arg)
{
    for_each_live_object_data* data;
    
    data = arg;
    
    PAS_ASSERT(&data->heap->segregated_heap == heap);
    
    return data->callback(data->heap, begin, size, data->arg);
}

static bool for_each_live_object_large_object_callback(pas_large_heap* heap,
                                                       uintptr_t begin,
                                                       uintptr_t end,
                                                       void* arg)
{
    for_each_live_object_data* data;
    
    data = arg;
    
    PAS_ASSERT(&data->heap->large_heap == heap);
    
    return data->callback(data->heap, begin, end - begin, data->arg);
}

bool pas_heap_for_each_live_object(pas_heap* heap,
                                   pas_heap_for_each_live_object_callback callback,
                                   void* arg,
                                   pas_lock_hold_mode heap_lock_hold_mode)
{
    for_each_live_object_data data;
    bool result;
    
    data.heap = heap;
    data.callback = callback;
    data.arg = arg;
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    
    result =
        pas_segregated_heap_for_each_live_object(&heap->segregated_heap,
                                                 for_each_live_object_small_object_callback,
                                                 &data) &&
        pas_large_heap_for_each_live_object(&heap->large_heap,
                                            for_each_live_object_large_object_callback,
                                            &data);

    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    
    return result;
}

pas_heap_summary pas_heap_compute_summary(pas_heap* heap,
                                          pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_heap_summary result;
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    result = pas_heap_summary_add(
        pas_segregated_heap_compute_summary(&heap->segregated_heap),
        pas_large_heap_compute_summary(&heap->large_heap));
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    return result;
}

void pas_heap_reset_heap_ref(pas_heap* heap)
{
    if (!heap->heap_ref)
        return;
    
    heap->heap_ref->heap = NULL;
    heap->heap_ref->allocator_index = 0;
    switch (heap->heap_ref_kind) {
    case pas_normal_heap_ref_kind:
        return;
    case pas_primitive_heap_ref_kind:
        ((pas_primitive_heap_ref*)heap->heap_ref)->cached_index = UINT_MAX;
        return;
    case pas_fake_heap_ref_kind:
        PAS_ASSERT(!"Should not be reached");
        return;
    }

    PAS_ASSERT(!"Should not be reached");
}

pas_segregated_size_directory*
pas_heap_ensure_size_directory_for_size_slow(
    pas_heap* heap,
    size_t size,
    size_t alignment,
    pas_size_lookup_mode force_size_lookup,
    const pas_heap_config* config,
    unsigned* cached_index)
{
    pas_segregated_size_directory* result;
    
    pas_heap_lock_lock();
    result = pas_segregated_heap_ensure_size_directory_for_size(
        &heap->segregated_heap, size, alignment, force_size_lookup, config, cached_index,
        pas_segregated_size_directory_full_creation_mode);
    pas_heap_lock_unlock();
    
    return result;
}

#endif /* LIBPAS_ENABLED */
