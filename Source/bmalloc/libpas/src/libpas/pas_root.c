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

#include "pas_root.h"

#include "pas_all_heaps.h"
#include "pas_baseline_allocator_table.h"
#include "pas_compact_heap_reservation.h"
#include "pas_enumerable_page_malloc.h"
#include "pas_immortal_heap.h"
#include "pas_large_heap_physical_page_sharing_cache.h"
#include "pas_large_map.h"
#include "pas_large_sharing_pool.h"
#include "pas_payload_reservation_page_list.h"
#include "pas_scavenger.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_thread_local_cache_node.h"

#if PAS_OS(DARWIN)
#include <mach/mach_init.h>
#endif

static bool count_static_heaps_callback(pas_heap* heap, void* arg)
{
    size_t* count;

    PAS_UNUSED_PARAM(heap);

    count = arg;

    (*count)++;

    return true;
}

typedef struct {
    pas_root* root;
    size_t index;
} collect_static_heaps_data;

static bool collect_static_heaps_callback(pas_heap* heap, void* arg)
{
    collect_static_heaps_data* data;

    data = arg;

    PAS_ASSERT(data->index < data->root->num_static_heaps);
    PAS_ASSERT(heap);

    data->root->static_heaps[data->index++] = heap;

    return true;
}

void pas_root_construct(pas_root* root)
{
    collect_static_heaps_data data;
    size_t index;
    pas_heap_config_kind config_kind;

    root->magic = PAS_ROOT_MAGIC;
    root->compact_heap_reservation_base = &pas_compact_heap_reservation_base;
    root->compact_heap_reservation_size = &pas_compact_heap_reservation_size;
    root->compact_heap_reservation_guard_size = &pas_compact_heap_reservation_guard_size;
    root->compact_heap_reservation_available_size = &pas_compact_heap_reservation_available_size;
    root->compact_heap_reservation_bump = &pas_compact_heap_reservation_bump;
    root->enumerable_page_malloc_page_list = &pas_enumerable_page_malloc_page_list;
    root->large_heap_physical_page_sharing_cache_page_list =
        &pas_large_heap_physical_page_sharing_cache_page_list;
    root->payload_reservation_page_list = &pas_payload_reservation_page_list;
    root->thread_local_cache_node_first = &pas_thread_local_cache_node_first;
    root->thread_local_cache_layout_first_segment = &pas_thread_local_cache_layout_first_segment;
    root->all_heaps_first_heap = &pas_all_heaps_first_heap;
    
    root->num_static_heaps = 0;
    pas_all_heaps_for_each_static_heap(count_static_heaps_callback, &root->num_static_heaps);
    
    root->static_heaps = pas_immortal_heap_allocate(
        sizeof(pas_heap*) * root->num_static_heaps,
        "pas_root/static_heaps",
        pas_object_allocation);

    data.root = root;
    data.index = 0;
    pas_all_heaps_for_each_static_heap(collect_static_heaps_callback, &data);
    PAS_ASSERT(data.index == root->num_static_heaps);

    for (index = root->num_static_heaps; index--;)
        PAS_ASSERT(root->static_heaps[index]);

    root->large_map_hashtable_instance = &pas_large_map_hashtable_instance;
    root->large_map_hashtable_instance_in_flux_stash = &pas_large_map_hashtable_instance_in_flux_stash;
    root->small_large_map_hashtable_instance = &pas_small_large_map_hashtable_instance;
    root->small_large_map_hashtable_instance_in_flux_stash =
        &pas_small_large_map_hashtable_instance_in_flux_stash;
    root->tiny_large_map_hashtable_instance = &pas_tiny_large_map_hashtable_instance;
    root->tiny_large_map_hashtable_instance_in_flux_stash =
        &pas_tiny_large_map_hashtable_instance_in_flux_stash;
    root->tiny_large_map_second_level_hashtable_in_flux_stash_instance =
        &pas_tiny_large_map_second_level_hashtable_in_flux_stash_instance;

    root->heap_configs = pas_immortal_heap_allocate(
        sizeof(const pas_heap_config*) * pas_heap_config_kind_num_kinds,
        "pas_root/heap_configs",
        pas_object_allocation);
    for (PAS_EACH_HEAP_CONFIG_KIND(config_kind))
        root->heap_configs[config_kind] = pas_heap_config_kind_get_config(config_kind);
    root->num_heap_configs = pas_heap_config_kind_num_kinds;

    root->large_sharing_tree = &pas_large_sharing_tree;
    root->large_sharing_tree_jettisoned_nodes = &pas_large_sharing_tree_jettisoned_nodes;

    root->page_malloc_alignment = pas_page_malloc_alignment();

    root->baseline_allocator_table = &pas_baseline_allocator_table;
    root->num_baseline_allocators = PAS_NUM_BASELINE_ALLOCATORS;
}

pas_root* pas_root_create(void)
{
    pas_root* result;

    result = pas_immortal_heap_allocate(
        sizeof(pas_root),
        "pas_root",
        pas_object_allocation);

    pas_root_construct(result);

    return result;
}

#if PAS_OS(DARWIN)

static kern_return_t default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr)
{
    PAS_UNUSED_PARAM(task);
    PAS_UNUSED_PARAM(size);
    *ptr = (void*)address;
    return KERN_SUCCESS;
}

typedef struct {
    memory_reader_t* reader_callback;
    vm_range_recorder_t* recorder_callback;
    task_t task;
    void* context;
    kern_return_t err;
} enumerator_data;

static void* enumerator_reader(pas_enumerator* enumerator, void* remote_address, size_t size, void* arg)
{
    enumerator_data* data;
    void* result;

    PAS_UNUSED_PARAM(enumerator);

    data = arg;

    /* We should not be calling into the reader again after an error. */
    PAS_ASSERT(!data->err);

    data->err = data->reader_callback(data->task, (vm_address_t)remote_address, size, &result);
    if (data->err)
        return NULL;

    return result;
}

static void enumerator_recorder(pas_enumerator* enumerator, void* remote_address, size_t size,
                                pas_enumerator_record_kind kind, void* arg)
{
    enumerator_data* data;
    unsigned type;
    vm_range_t range;
    
    PAS_UNUSED_PARAM(enumerator);

    data = arg;

    switch (kind) {
    case pas_enumerator_meta_record:
        type = MALLOC_ADMIN_REGION_RANGE_TYPE;
        break;
    case pas_enumerator_payload_record:
        type = MALLOC_PTR_REGION_RANGE_TYPE;
        break;
    case pas_enumerator_object_record:
        type = MALLOC_PTR_IN_USE_RANGE_TYPE;
        break;
    }

    range.address = (vm_address_t)remote_address;
    range.size = size;

    return data->recorder_callback(data->task, data->context, type, &range, 1);
}

kern_return_t pas_root_enumerate_for_libmalloc(pas_root* remote_root,
                                               task_t task,
                                               void* context,
                                               unsigned type_mask,
                                               memory_reader_t reader,
                                               vm_range_recorder_t recorder)
{
    bool is_enumerating_self;
    enumerator_data data;
    pas_enumerator* enumerator;

    is_enumerating_self = !task || task == mach_task_self() || !reader;

    if (!reader)
        reader = default_reader;

    if (is_enumerating_self)
        pas_scavenger_suspend();

    data.reader_callback = reader;
    data.recorder_callback = recorder;
    data.context = context;
    data.task = task;
    data.err = KERN_SUCCESS;

    enumerator = pas_enumerator_create(
        remote_root,
        enumerator_reader,
        &data,
        enumerator_recorder,
        &data,
        (type_mask & MALLOC_ADMIN_REGION_RANGE_TYPE)
        ? pas_enumerator_record_meta_records : pas_enumerator_do_not_record_meta_records,
        (type_mask & (MALLOC_PTR_REGION_RANGE_TYPE | MALLOC_ADMIN_REGION_RANGE_TYPE))
        ? pas_enumerator_record_payload_records : pas_enumerator_do_not_record_payload_records,
        (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE)
        ? pas_enumerator_record_object_records : pas_enumerator_do_not_record_object_records);
    if (!enumerator)
        goto fail;

    if (!pas_enumerator_enumerate_all(enumerator))
        goto fail;

    pas_enumerator_destroy(enumerator);

    if (is_enumerating_self)
        pas_scavenger_resume();
    return KERN_SUCCESS;

fail:
    if (is_enumerating_self)
        pas_scavenger_resume();
    return data.err;
}

kern_return_t pas_root_enumerate_for_libmalloc_with_root_after_zone(
    task_t task,
    void *context,
    unsigned type_mask,
    vm_address_t zone_address,
    memory_reader_t reader,
    vm_range_recorder_t recorder)
{
    return pas_root_enumerate_for_libmalloc(
        (pas_root*)((malloc_zone_t*)zone_address + 1),
        task, context, type_mask, reader, recorder);
}

static size_t malloc_introspect_good_size(malloc_zone_t* zone, size_t size)
{
    PAS_UNUSED_PARAM(zone);
    PAS_UNUSED_PARAM(size);
    return size;
}

static boolean_t malloc_introspect_check(malloc_zone_t* zone)
{
    PAS_UNUSED_PARAM(zone);
    return true;
}

static void malloc_introspect_print(malloc_zone_t* zone, boolean_t verbose)
{
    PAS_UNUSED_PARAM(zone);
    PAS_UNUSED_PARAM(verbose);
}

static void malloc_introspect_log(malloc_zone_t* zone, void* arg)
{
    PAS_UNUSED_PARAM(zone);
    PAS_UNUSED_PARAM(arg);
}

static void malloc_introspect_force_lock(malloc_zone_t* zone)
{
    PAS_UNUSED_PARAM(zone);
}

static void malloc_introspect_force_unlock(malloc_zone_t* zone)
{
    PAS_UNUSED_PARAM(zone);
}

static void malloc_introspect_statistics(malloc_zone_t* zone, malloc_statistics_t* statistics)
{
    PAS_UNUSED_PARAM(zone);
    pas_zero_memory(statistics, sizeof(malloc_statistics_t));
}

static const malloc_introspection_t malloc_introspect = {
    .enumerator = pas_root_enumerate_for_libmalloc_with_root_after_zone,
    .good_size = malloc_introspect_good_size,
    .check = malloc_introspect_check,
    .print = malloc_introspect_print,
    .log = malloc_introspect_log,
    .force_lock = malloc_introspect_force_lock,
    .force_unlock = malloc_introspect_force_unlock,
    .statistics = malloc_introspect_statistics
};

static size_t malloc_introspect_size(malloc_zone_t* zone, const void* ptr)
{
    PAS_UNUSED_PARAM(zone);
    PAS_UNUSED_PARAM(ptr);

    /* When we're doing these shenanigans, we're not set up to respond to malloc calls. Folks are
       doing malloc/free by calling into the libpas API some other way. So, we tell libmalloc that
       we own no pointers. */
    
    return 0;
}

pas_root* pas_root_for_libmalloc_enumeration;

pas_root* pas_root_ensure_for_libmalloc_enumeration(void)
{
    malloc_zone_t* zone;
    pas_root* root;

    pas_heap_lock_assert_held();

    if (pas_root_for_libmalloc_enumeration)
        return pas_root_for_libmalloc_enumeration;

    zone = (malloc_zone_t*)pas_immortal_heap_allocate(
        sizeof(malloc_zone_t) + sizeof(pas_root),
        "pas_malloc_zone_and_root_for_libmalloc_introspection",
        pas_object_allocation);

    root = (pas_root*)(zone + 1);

    pas_zero_memory(zone, sizeof(malloc_zone_t));

    zone->size = malloc_introspect_size;
    zone->zone_name = "WebKit Malloc";
    zone->introspect = (malloc_introspection_t*)(uintptr_t)&malloc_introspect;
    zone->version = 4;

    pas_root_construct(root);
    
    malloc_zone_register(zone);

    pas_root_for_libmalloc_enumeration = root;

    return root;
}

#endif
#endif /* LIBPAS_ENABLED */
