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

#include "pas_enumerator.h"

#include "pas_enumerate_bitfit_heaps.h"
#include "pas_enumerate_initially_unaccounted_pages.h"
#include "pas_enumerate_large_heaps.h"
#include "pas_enumerate_segregated_heaps.h"
#include "pas_enumerate_unaccounted_pages_as_meta.h"
#include "pas_enumerator_internal.h"
#include "pas_enumerator_region.h"
#include "pas_ptr_hash_set.h"
#include "pas_root.h"

static void* allocate(size_t size, const char* name, pas_allocation_kind allocation_kind, void* arg)
{
    pas_enumerator* enumerator;

    PAS_UNUSED_PARAM(name);
    PAS_UNUSED_PARAM(allocation_kind);

    enumerator = arg;

    return pas_enumerator_allocate(enumerator, size);
}

static void deallocate(void* ptr, size_t size, pas_allocation_kind allocation_kind, void* arg)
{
    PAS_UNUSED_PARAM(ptr);
    PAS_UNUSED_PARAM(size);
    PAS_UNUSED_PARAM(allocation_kind);
    PAS_UNUSED_PARAM(arg);

    /* We ignore deallocations because the enumerator runs in an environment where we don't really have
       much of an allocator at all. */
}

pas_enumerator* pas_enumerator_create(pas_root* remote_root_address,
                                      pas_enumerator_reader reader,
                                      void* reader_arg,
                                      pas_enumerator_recorder recorder,
                                      void* recorder_arg,
                                      pas_enumerator_meta_recording_mode record_meta,
                                      pas_enumerator_payload_recording_mode record_payload,
                                      pas_enumerator_object_recording_mode record_object)
{
    pas_enumerator* result;
    uintptr_t* compact_heap_base;
    size_t* compact_heap_size;
    size_t* compact_heap_guard_size;
    pas_enumerator_region* region;
    pas_heap_config** configs;
    pas_heap_config_kind config_kind;

    region = NULL;

    result = pas_enumerator_region_allocate(&region, sizeof(pas_enumerator));
    if (!result)
        return NULL;

    result->region = region;

    result->allocation_config.allocate = allocate;
    result->allocation_config.deallocate = deallocate;
    result->allocation_config.arg = result;

    result->heap_config_datas = pas_enumerator_allocate(
        result, sizeof(void*) * pas_heap_config_kind_num_kinds);
    pas_zero_memory(result->heap_config_datas, sizeof(void*) * pas_heap_config_kind_num_kinds);

    result->root = reader(result, remote_root_address, sizeof(pas_root), reader_arg);
    if (!result->root)
        goto fail;

    PAS_ASSERT(result->root->magic == PAS_ROOT_MAGIC);
    PAS_ASSERT(result->root->num_heap_configs == pas_heap_config_kind_num_kinds);

    compact_heap_base = reader(
        result, result->root->compact_heap_reservation_base, sizeof(uintptr_t), reader_arg);
    if (!compact_heap_base)
        goto fail;

    compact_heap_size = reader(
        result, result->root->compact_heap_reservation_size, sizeof(size_t), reader_arg);
    if (!compact_heap_size)
        goto fail;

    compact_heap_guard_size = reader(
        result, result->root->compact_heap_reservation_guard_size, sizeof(size_t), reader_arg);
    if (!compact_heap_size)
        goto fail;

    result->compact_heap_remote_base = (void*)*compact_heap_base;
    result->compact_heap_copy_base = (void*)(
        (uintptr_t)reader(
            result, (void*)(*compact_heap_base + *compact_heap_guard_size), *compact_heap_size,
            reader_arg)
        - *compact_heap_guard_size);
    if (!result->compact_heap_copy_base)
        goto fail;
    
    result->compact_heap_size = *compact_heap_size;
    result->compact_heap_guard_size = *compact_heap_guard_size;

    result->unaccounted_pages = pas_enumerator_allocate(result, sizeof(pas_ptr_hash_set));
    pas_ptr_hash_set_construct(result->unaccounted_pages);

    result->reader = reader;
    result->reader_arg = reader_arg;
    result->recorder = recorder;
    result->recorder_arg = recorder_arg;
    result->record_meta = record_meta;
    result->record_payload = record_payload;
    result->record_object = record_object;

    configs = reader(
        result,
        result->root->heap_configs,
        sizeof(pas_heap_config*) * pas_heap_config_kind_num_kinds,
        reader_arg);
    if (!configs)
        goto fail;
    
    for (PAS_EACH_HEAP_CONFIG_KIND(config_kind)) {
        pas_heap_config* config;
        pas_heap_config* remote_config;

        if (config_kind == pas_heap_config_kind_null)
            continue;

        config = pas_heap_config_kind_get_config(config_kind);

        PAS_ASSERT(config);

        remote_config = reader(result, configs[config->kind], sizeof(pas_heap_config), reader_arg);
        if (!remote_config)
            goto fail;

        PAS_ASSERT(remote_config->kind == config->kind);

        if (!config->prepare_to_enumerate)
            continue;

        result->heap_config_datas[config_kind] = config->prepare_to_enumerate(result);
        if (!result->heap_config_datas[config_kind])
            goto fail;
    }

    return result;

fail:
    pas_enumerator_destroy(result);
    return NULL;
}

void pas_enumerator_destroy(pas_enumerator* enumerator)
{
    pas_enumerator_region_destroy(enumerator->region);
}

void* pas_enumerator_allocate(pas_enumerator* enumerator,
                              size_t size)
{
    return pas_enumerator_region_allocate(&enumerator->region, size);
}

void* pas_enumerator_read_compact(pas_enumerator* enumerator,
                                  void* remote_address)
{
    if ((uintptr_t)remote_address < (uintptr_t)PAS_INTERNAL_MIN_ALIGN)
        return remote_address;
    
    PAS_ASSERT(remote_address >= (void*)((uintptr_t)enumerator->compact_heap_remote_base +
                                         enumerator->compact_heap_guard_size));
    PAS_ASSERT(remote_address < (void*)((uintptr_t)enumerator->compact_heap_remote_base +
                                        enumerator->compact_heap_size));
    return (void*)(
        (uintptr_t)enumerator->compact_heap_copy_base
        + (uintptr_t)remote_address - (uintptr_t)enumerator->compact_heap_remote_base);
}

void* pas_enumerator_read(pas_enumerator* enumerator,
                          void* remote_address,
                          size_t size)
{
    void* compact_heap_end;

    PAS_ASSERT(remote_address);

    compact_heap_end = (void*)(
        (uintptr_t)enumerator->compact_heap_remote_base + enumerator->compact_heap_size);
    
    if (remote_address >= enumerator->compact_heap_remote_base
        && remote_address < compact_heap_end) {
        PAS_ASSERT((uintptr_t)remote_address + size <= (uintptr_t)compact_heap_end);
        return pas_enumerator_read_compact(enumerator, remote_address);
    }

    if (!size)
        return &enumerator->dummy_byte;
    
    return enumerator->reader(enumerator, remote_address, size, enumerator->reader_arg);
}

void pas_enumerator_add_unaccounted_pages(pas_enumerator* enumerator,
                                          void* remote_address,
                                          size_t size)
{
    size_t offset;

    PAS_ASSERT(pas_is_aligned((uintptr_t)remote_address, enumerator->root->page_malloc_alignment));
    PAS_ASSERT(pas_is_aligned(size, enumerator->root->page_malloc_alignment));

    /* Catch bogus sizes, in case we did some overflow or weird subtraction. */
    PAS_ASSERT((uint64_t)size < ((uint64_t)1 << PAS_ADDRESS_BITS));

    for (offset = 0; offset < size; offset += enumerator->root->page_malloc_alignment) {
        pas_ptr_hash_set_set(enumerator->unaccounted_pages,
                             (void*)((uintptr_t)remote_address + offset),
                             NULL, &enumerator->allocation_config);
    }
}

bool pas_enumerator_exclude_accounted_page(pas_enumerator* enumerator,
                                           void* remote_address)
{
    PAS_ASSERT(pas_is_aligned((uintptr_t)remote_address, enumerator->root->page_malloc_alignment));
    return pas_ptr_hash_set_remove(
        enumerator->unaccounted_pages, remote_address, NULL, &enumerator->allocation_config);
}

void pas_enumerator_exclude_accounted_pages(pas_enumerator* enumerator,
                                            void* remote_address,
                                            size_t size)
{
    size_t offset;

    PAS_ASSERT(pas_is_aligned((uintptr_t)remote_address, enumerator->root->page_malloc_alignment));
    PAS_ASSERT(pas_is_aligned(size, enumerator->root->page_malloc_alignment));

    /* Catch bogus sizes, in case we did some overflow or weird subtraction. */
    PAS_ASSERT((uint64_t)size < ((uint64_t)1 << PAS_ADDRESS_BITS));

    for (offset = 0; offset < size; offset += enumerator->root->page_malloc_alignment)
        pas_enumerator_exclude_accounted_page(enumerator, (void*)((uintptr_t)remote_address + offset));
}

void pas_enumerator_record(pas_enumerator* enumerator,
                           void* remote_address,
                           size_t size,
                           pas_enumerator_record_kind kind)
{
    if (!size)
        return;

    /* Catch bogus sizes, in case we did some overflow or weird subtraction. */
    PAS_ASSERT((uint64_t)size < ((uint64_t)1 << PAS_ADDRESS_BITS));
    
    switch (kind) {
    case pas_enumerator_meta_record:
        if (!enumerator->record_meta)
            return;
        break;
    case pas_enumerator_payload_record:
        if (!enumerator->record_payload)
            return;
        break;
    case pas_enumerator_object_record:
        if (!enumerator->record_object)
            return;
        break;
    }

    enumerator->recorder(enumerator, remote_address, size, kind, enumerator->recorder_arg);
}

static void record_payload_span(pas_enumerator* enumerator,
                                uintptr_t page_boundary,
                                pas_range range)
{
    pas_enumerator_record(enumerator,
                          (void*)(page_boundary + range.begin),
                          pas_range_size(range),
                          pas_enumerator_payload_record);
}

void pas_enumerator_record_page_payload_and_meta(pas_enumerator* enumerator,
                                                 uintptr_t page_boundary,
                                                 uintptr_t page_size,
                                                 uintptr_t granule_size,
                                                 pas_page_granule_use_count* use_counts,
                                                 uintptr_t payload_begin,
                                                 uintptr_t payload_end)
{
    PAS_ASSERT(payload_begin < page_size);
    PAS_ASSERT(payload_end <= page_size);
    PAS_ASSERT(payload_begin < payload_end);

    /* We assume, correctly for now, that non-payload areas of the page are always committed if the page
       is committed. */
    pas_enumerator_record(enumerator,
                          (void*)page_boundary,
                          payload_begin,
                          pas_enumerator_meta_record);
    pas_enumerator_record(enumerator,
                          (void*)(page_boundary + payload_end),
                          page_size - payload_end,
                          pas_enumerator_meta_record);

    if (enumerator->record_payload) {
        if (page_size == granule_size) {
            PAS_ASSERT(!use_counts);
            pas_enumerator_record(enumerator,
                                  (void*)(page_boundary + payload_begin),
                                  payload_end - payload_begin,
                                  pas_enumerator_payload_record);
        } else {
            uintptr_t granule_index;
            pas_range span;

            PAS_ASSERT(page_size > granule_size);
            PAS_ASSERT(use_counts);

            span = pas_range_create(payload_begin, payload_begin);
            
            for (granule_index = 0; granule_index < page_size / granule_size; ++granule_index) {
                uintptr_t begin;
                uintptr_t end;
                
                begin = PAS_CLIP(granule_index * granule_size,
                                 payload_begin,
                                 payload_end);
                end = PAS_CLIP((granule_index + 1) * granule_size,
                               payload_begin,
                               payload_end);
                
                if (use_counts[granule_index] == PAS_PAGE_GRANULE_DECOMMITTED) {
                    record_payload_span(enumerator, page_boundary, span);
                    span.begin = end;
                }
                
                span.end = end;
            }
            
            record_payload_span(enumerator, page_boundary, span);
        }
    }
}

bool pas_enumerator_for_each_heap(pas_enumerator* enumerator,
                                  pas_enumerator_heap_callback callback,
                                  void* arg)
{
    size_t index;
    pas_heap* heap;
    pas_heap** first_heap;
    pas_heap** static_heaps;

    first_heap = pas_enumerator_read(enumerator,
                                     enumerator->root->all_heaps_first_heap,
                                     sizeof(pas_heap*));
    if (!first_heap)
        return false;

    for (heap = pas_enumerator_read_compact(enumerator, *first_heap);
         heap;
         heap = pas_compact_heap_ptr_load_remote(enumerator,
                                                 &heap->next_heap)) {
        if (!callback(enumerator, heap, arg))
            return false;
    }

    static_heaps = pas_enumerator_read(enumerator,
                                       enumerator->root->static_heaps,
                                       sizeof(pas_heap*) * enumerator->root->num_static_heaps);
    if (!static_heaps)
        return false;

    for (index = enumerator->root->num_static_heaps; index--;) {
        pas_heap* heap;

        heap = pas_enumerator_read(enumerator, static_heaps[index], sizeof(pas_heap));
        if (!heap)
            return false;
        
        if (!callback(enumerator, heap, arg))
            return false;
    }

    return true;
}

bool pas_enumerator_enumerate_all(pas_enumerator* enumerator)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("Enumerating initially unaccounted pages...\n");
    if (!pas_enumerate_initially_unaccounted_pages(enumerator))
        return false;

    if (verbose)
        pas_log("Enumerating large heaps...\n");
    if (!pas_enumerate_large_heaps(enumerator))
        return false;

    if (verbose)
        pas_log("Enumerating segregated heaps...\n");
    if (!pas_enumerate_segregated_heaps(enumerator))
        return false;

    if (verbose)
        pas_log("Enumerating bitfit heaps...\n");
    if (!pas_enumerate_bitfit_heaps(enumerator))
        return false;

    if (verbose)
        pas_log("Enumerating unaccounted pages as meta...\n");
    if (!pas_enumerate_unaccounted_pages_as_meta(enumerator))
        return false;

    if (verbose)
        pas_log("Done enumerating all!\n");
    
    return true;
}

#endif /* LIBPAS_ENABLED */
