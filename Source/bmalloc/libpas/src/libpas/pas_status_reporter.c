/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#include "pas_status_reporter.h"

#include "pas_all_heaps.h"
#include "pas_all_shared_page_directories.h"
#include "pas_baseline_allocator_table.h"
#include "pas_bitfit_directory.h"
#include "pas_bitfit_heap.h"
#include "pas_bitfit_size_class.h"
#include "pas_bitfit_view.h"
#include "pas_compact_bootstrap_free_heap.h"
#include "pas_compact_expendable_memory.h"
#include "pas_compact_large_utility_free_heap.h"
#include "pas_fd_stream.h"
#include "pas_heap.h"
#include "pas_heap_lock.h"
#include "pas_heap_summary.h"
#include "pas_heap_table.h"
#include "pas_large_expendable_memory.h"
#include "pas_large_heap.h"
#include "pas_large_map.h"
#include "pas_large_sharing_pool.h"
#include "pas_large_utility_free_heap.h"
#include "pas_log.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_shared_page_directory.h"
#include "pas_segregated_size_directory.h"
#include "pas_simple_type.h"
#include "pas_stream.h"
#include "pas_thread_local_cache.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_thread_local_cache_node.h"
#include "pas_utility_heap.h"
#include <pthread.h>
#include <unistd.h>

unsigned pas_status_reporter_enabled = 0;
unsigned pas_status_reporter_period_in_microseconds = 10 * 1000 * 1000;

static pthread_once_t once_control = PTHREAD_ONCE_INIT;

static void dump_ratio_initial(
    pas_stream* stream, const char* full, uintptr_t numerator, uintptr_t denominator)
{
    uintptr_t value;

    if (!denominator) {
        pas_stream_printf(stream, " ");
        return;
    }

    if (numerator == denominator) {
        pas_stream_printf(stream, "%s", full);
        return;
    }

    value = 10 * numerator / denominator;
    PAS_ASSERT(value <= 9);
    
    if (!value && numerator) {
        pas_stream_printf(stream, "e");
        return;
    }

    pas_stream_printf(stream, "%lu", value);
}

static void dump_occupancy_initial(pas_stream* stream, pas_heap_summary summary)
{
    dump_ratio_initial(stream, "F", summary.allocated, summary.allocated + summary.free);
}

static void dump_arrow(pas_stream* stream, uintptr_t index_of_arrow)
{
    uintptr_t index;
    
    for (index = 0; index < index_of_arrow; ++index)
        pas_stream_printf(stream, " ");
    pas_stream_printf(stream, "^");
}

static void report_bitfit_directory_contents(
    pas_stream* stream, pas_bitfit_directory* directory, const char* prefix)
{
    size_t index;
    pas_bitfit_size_class* size_class;

    if (!pas_bitfit_directory_size(directory))
        return;

    pas_stream_printf(stream, "%s    Occupancy: ", prefix);
    for (index = 0; index < pas_bitfit_directory_size(directory); ++index) {
        pas_bitfit_view* view;

        view = pas_bitfit_directory_get_view(directory, index);
        if (!view) {
            pas_stream_printf(stream, " ");
            continue;
        }

        dump_occupancy_initial(stream, pas_bitfit_view_compute_summary(view));
    }
    pas_stream_printf(stream, "\n");
    
    pas_stream_printf(stream, "%s     Max Free: ", prefix);
    for (index = 0; index < pas_bitfit_directory_size(directory); ++index) {
        pas_bitfit_max_free max_free;

        max_free = pas_bitfit_directory_get_max_free(directory, index);

        if (max_free == PAS_BITFIT_MAX_FREE_EMPTY)
            pas_stream_printf(stream, "E");
        else
            dump_ratio_initial(stream, "U", max_free, PAS_BITFIT_MAX_FREE_UNPROCESSED);
    }
    pas_stream_printf(stream, "\n");
    
    pas_stream_printf(stream, "%s  Empty (bit): ", prefix);
    for (index = 0; index < pas_bitfit_directory_size(directory); ++index) {
        if (pas_bitfit_directory_get_empty_bit_at_index(directory, index))
            pas_stream_printf(stream, "x");
        else
            pas_stream_printf(stream, " ");
    }
    pas_stream_printf(stream, "\n");
    
    pas_stream_printf(stream, "%s Last Empty+1: ", prefix);
    dump_arrow(stream, (uintptr_t)directory->last_empty_plus_one.value);
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "%s    Committed: ", prefix);
    for (index = 0; index < pas_bitfit_directory_size(directory); ++index) {
        pas_bitfit_view* view;
        
        view = pas_bitfit_directory_get_view(directory, index);
        if (!view) {
            pas_stream_printf(stream, " ");
            continue;
        }
        
        if (view->is_owned)
            pas_stream_printf(stream, "x");
        else
            pas_stream_printf(stream, " ");
    }
    pas_stream_printf(stream, "\n");

    for (size_class = pas_compact_atomic_bitfit_size_class_ptr_load(&directory->largest_size_class);
         size_class;
         size_class = pas_compact_atomic_bitfit_size_class_ptr_load(&size_class->next_smaller)) {
        uintptr_t index_of_first_free;

        index_of_first_free = size_class->first_free.value;
        
        pas_stream_printf(stream, "%s%7u Bytes: ", prefix, size_class->size);
        dump_arrow(stream, index_of_first_free);
        pas_stream_printf(stream, "\n");
    }

    pas_stream_printf(stream, "%s  Unprocessed: ", prefix);
    dump_arrow(stream, directory->first_unprocessed_free.value);
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "%s Empty (free): ", prefix);
    dump_arrow(stream, directory->first_empty.value);
    pas_stream_printf(stream, "\n");
}

void pas_status_reporter_dump_bitfit_directory(
    pas_stream* stream, pas_bitfit_directory* directory)
{
    if (directory->config_kind == pas_bitfit_page_config_kind_null)
        return;
    
    pas_stream_printf(stream, "            %s Global Dir (%p): ",
                      pas_bitfit_page_config_variant_get_capitalized_string(
                          pas_bitfit_page_config_kind_get_config(directory->config_kind)->variant),
                      directory);
    pas_heap_summary_dump(pas_bitfit_directory_compute_summary(directory), stream);
    pas_stream_printf(stream, "\n");

    report_bitfit_directory_contents(stream, directory, "                ");
}

static void report_segregated_directory_contents(
    pas_stream* stream, pas_segregated_directory* directory, const char* prefix)
{
    size_t index;

    if (!pas_segregated_directory_size(directory))
        return;
    
    pas_stream_printf(stream, "%s        Kind: ", prefix);
    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        pas_segregated_view view;
        
        view = pas_segregated_directory_get(directory, index);

        PAS_UNUSED_PARAM(view);

        pas_stream_printf(
            stream, "%c",
            pas_segregated_view_kind_get_character_code(
                pas_segregated_view_get_kind(pas_segregated_directory_get(directory, index))));
    }
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "%s   Occupancy: ", prefix);
    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        pas_segregated_view view;
        pas_heap_summary summary;

        view = pas_segregated_directory_get(directory, index);

        summary = pas_segregated_view_compute_summary(
            view, pas_segregated_page_config_kind_get_config(directory->page_config_kind));
        
        dump_occupancy_initial(stream, summary);
    }
    pas_stream_printf(stream, "\n");

    if (directory->directory_kind == pas_segregated_shared_page_directory_kind) {
        const pas_segregated_page_config* page_config;
        uintptr_t payload_begin;
        uintptr_t payload_end;

        page_config = pas_segregated_page_config_kind_get_config(directory->page_config_kind);

        payload_begin = pas_round_up_to_power_of_2(
            page_config->shared_payload_offset,
            pas_segregated_page_config_min_align(*page_config));
        payload_end = pas_segregated_page_config_payload_end_offset_for_role(
            *page_config, pas_segregated_page_shared_role);
        
        pas_stream_printf(stream, "%s        Bump: ", prefix);
        for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
            pas_segregated_view view;
            pas_segregated_shared_view* shared_view;
            unsigned bump_offset;
            
            view = pas_segregated_directory_get(directory, index);
            shared_view = pas_segregated_view_get_shared(view);

            bump_offset = shared_view->bump_offset;

            if (!bump_offset) {
                pas_stream_printf(stream, "0");
                continue;
            }

            PAS_ASSERT(bump_offset >= payload_begin);
            PAS_ASSERT(bump_offset <= payload_end);

            dump_ratio_initial(stream, "F", bump_offset - payload_begin, payload_end - payload_begin);
        }
        pas_stream_printf(stream, "\n");
    }

    pas_stream_printf(stream, "%s    Eligible: ", prefix);
    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        if (pas_segregated_directory_is_eligible(directory, index))
            pas_stream_printf(stream, "x");
        else
            pas_stream_printf(stream, " ");
    }
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "%s First Elgbl: ", prefix);
    dump_arrow(stream, (uintptr_t)pas_segregated_directory_get_first_eligible(directory).value);
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "%s       Empty: ", prefix);
    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        if (pas_segregated_directory_is_empty(directory, index))
            pas_stream_printf(stream, "x");
        else
            pas_stream_printf(stream, " ");
    }
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "%s Last Empt+1: ", prefix);
    dump_arrow(stream, (uintptr_t)pas_segregated_directory_get_last_empty_plus_one(directory).value);
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "%s   Committed: ", prefix);
    for (index = 0; index < pas_segregated_directory_size(directory); ++index) {
        if (pas_segregated_directory_is_committed(directory, index))
            pas_stream_printf(stream, "x");
        else
            pas_stream_printf(stream, " ");
    }
    pas_stream_printf(stream, "\n");
}

void pas_status_reporter_dump_segregated_size_directory(
    pas_stream* stream, pas_segregated_size_directory* directory)
{
    pas_heap_summary partial_summary;
    pas_heap_summary exclusive_summary;
    size_t index;
    pas_segregated_size_directory_data* data;
    
    pas_stream_printf(
        stream,
        "            Global Size Dir %p(%u/%s): Num Views: %zu",
        directory,
        directory->object_size,
        pas_segregated_page_config_kind_get_string(directory->base.page_config_kind),
        pas_segregated_directory_size(&directory->base));

    if (pas_segregated_directory_data_ptr_load(&directory->base.data))
        pas_stream_printf(stream, ", Has Base Data");
    data = pas_segregated_size_directory_data_ptr_load(&directory->data);
    if (data)
        pas_stream_printf(stream, ", Has Data");
    if (pas_segregated_size_directory_has_tlc_allocator(directory))
        pas_stream_printf(stream, ", Has TLA");
    if (pas_segregated_size_directory_are_exclusive_views_enabled(directory))
        pas_stream_printf(stream, ", Enabled Exclusives");
    pas_stream_printf(stream, "\n");

    partial_summary = pas_heap_summary_create_empty();
    exclusive_summary = pas_heap_summary_create_empty();
    for (index = 0; index < pas_segregated_directory_size(&directory->base); ++index) {
        pas_segregated_view view;
        pas_heap_summary view_summary;
        view = pas_segregated_directory_get(&directory->base, index);
        view_summary = pas_segregated_view_compute_summary(
            view, pas_segregated_page_config_kind_get_config(directory->base.page_config_kind));
        if (pas_segregated_view_is_partial(view))
            partial_summary = pas_heap_summary_add(partial_summary, view_summary);
        else {
            PAS_ASSERT(pas_segregated_view_is_some_exclusive(view));
            exclusive_summary = pas_heap_summary_add(exclusive_summary, view_summary);
        }
    }

    if (!pas_heap_summary_is_empty(partial_summary)) {
        pas_stream_printf(
            stream,
            "                Partials: ");
        pas_heap_summary_dump(partial_summary, stream);
        pas_stream_printf(stream, "\n");
    }

    if (!pas_heap_summary_is_empty(exclusive_summary)) {
        pas_stream_printf(
            stream,
            "                Exclusives: ");
        pas_heap_summary_dump(exclusive_summary, stream);
        pas_stream_printf(stream, "\n");
    }
    
    report_segregated_directory_contents(stream, &directory->base, "                ");
}

void pas_status_reporter_dump_segregated_shared_page_directory(
    pas_stream* stream, pas_segregated_shared_page_directory* directory)
{
    pas_heap_summary summary;
    
    pas_stream_printf(
        stream,
        "        Shared Page Dir %p(%s, ",
        directory,
        pas_segregated_page_config_kind_get_string(directory->base.page_config_kind));

    pas_segregated_page_config_kind_get_config(directory->base.page_config_kind)->base.heap_config_ptr
        ->dump_shared_page_directory_arg(stream, directory);

    pas_stream_printf(
        stream,
        "): Num Views: %zu, ",
        pas_segregated_directory_size(&directory->base));

    summary = pas_segregated_directory_compute_summary(&directory->base);
    pas_heap_summary_dump(summary, stream);

    pas_stream_printf(stream, "\n");

    if (pas_status_reporter_enabled >= 3)
        report_segregated_directory_contents(stream, &directory->base, "            ");
}

void pas_status_reporter_dump_large_heap(pas_stream* stream, pas_large_heap* heap)
{
    pas_heap_summary summary;
    
    pas_stream_printf(
        stream,
        "Large %p: ",
        heap);

    summary = pas_large_heap_compute_summary(heap);
    pas_heap_summary_dump(summary, stream);
}

void pas_status_reporter_dump_large_map(pas_stream* stream)
{
    pas_stream_printf(stream, "    Large Map:\n");
    pas_stream_printf(
        stream,
        "        Tiny Map: Num Entries: %u, Num Deleted: %u, Table Size: %u\n",
        pas_tiny_large_map_hashtable_instance.key_count,
        pas_tiny_large_map_hashtable_instance.deleted_count,
        pas_tiny_large_map_hashtable_instance.table_size);
    pas_stream_printf(
        stream,
        "        Small Fallback Map: Num Entries: %u, Num Deleted: %u, Table Size: %u\n",
        pas_small_large_map_hashtable_instance.key_count,
        pas_small_large_map_hashtable_instance.deleted_count,
        pas_small_large_map_hashtable_instance.table_size);
    pas_stream_printf(
        stream,
        "        Fallback Map: Num Entries: %u, Num Deleted: %u, Table Size: %u\n",
        pas_large_map_hashtable_instance.key_count,
        pas_large_map_hashtable_instance.deleted_count,
        pas_large_map_hashtable_instance.table_size);
}

void pas_status_reporter_dump_heap_table(pas_stream* stream)
{
    pas_stream_printf(
        stream,
        "Heap Table Size: %u",
        pas_heap_table_bump_index);
}

void pas_status_reporter_dump_immortal_heap(pas_stream* stream)
{
    pas_stream_printf(
        stream,
        "Alloc Internal: %zu, External: %zu",
        pas_immortal_heap_allocated_internal,
        pas_immortal_heap_allocated_external);
}

void pas_status_reporter_dump_compact_large_utility_free_heap(pas_stream* stream)
{
    pas_heap_summary summary;
    summary = pas_compact_large_utility_free_heap_compute_summary();
    pas_heap_summary_dump(summary, stream);
}

void pas_status_reporter_dump_large_utility_free_heap(pas_stream* stream)
{
    pas_heap_summary summary;
    summary = pas_large_utility_free_heap_compute_summary();
    pas_heap_summary_dump(summary, stream);
}

void pas_status_reporter_dump_compact_bootstrap_free_heap(pas_stream* stream)
{
    pas_stream_printf(
        stream,
        "Alloc: %zu, Peak Alloc: %zu, Mapped: %zu, Free: %zu",
        pas_compact_bootstrap_free_heap_num_allocated_object_bytes,
        pas_compact_bootstrap_free_heap_num_allocated_object_bytes_peak,
        pas_compact_bootstrap_free_heap.num_mapped_bytes,
        pas_compact_bootstrap_free_heap_get_num_free_bytes());
}

void pas_status_reporter_dump_bootstrap_free_heap(pas_stream* stream)
{
    pas_stream_printf(
        stream,
        "Alloc: %zu, Peak Alloc: %zu, Mapped: %zu, Free: %zu",
        pas_bootstrap_free_heap_num_allocated_object_bytes,
        pas_bootstrap_free_heap_num_allocated_object_bytes_peak,
        pas_bootstrap_free_heap.num_mapped_bytes,
        pas_bootstrap_free_heap_get_num_free_bytes());
}

static bool dump_segregated_heap_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
    void* arg)
{
    pas_stream* stream;

    PAS_UNUSED_PARAM(heap);

    stream = arg;

    pas_status_reporter_dump_segregated_size_directory(stream, directory);

    return true;
}

void pas_status_reporter_dump_bitfit_heap(pas_stream* stream, pas_bitfit_heap* heap)
{
    pas_stream_printf(stream, "        Bitfit Heap %p: ", heap);
    pas_heap_summary_dump(pas_bitfit_heap_compute_summary(heap), stream);
    pas_stream_printf(stream, "\n");

    if (pas_status_reporter_enabled >= 3) {
        pas_bitfit_page_config_variant variant;
        for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(variant)) {
            pas_status_reporter_dump_bitfit_directory(
                stream,
                pas_bitfit_heap_get_directory(heap, variant));
        }
    }
}

void pas_status_reporter_dump_segregated_heap(pas_stream* stream, pas_segregated_heap* heap)
{
    bool comma;
    pas_heap_summary summary;
    pas_bitfit_heap* bitfit_heap;
    
    pas_stream_printf(stream, "        Segregated Heap %p: ", heap);

    comma = false;
    if (pas_segregated_heap_rare_data_ptr_load(&heap->rare_data)) {
        pas_stream_print_comma(stream, &comma, ", ");
        pas_stream_printf(stream, "Has Rare Data");
    }
    if (heap->index_to_small_size_directory) {
        pas_stream_print_comma(stream, &comma, ", ");
        pas_stream_printf(stream, "Has Index Lookup");
    }
    pas_stream_printf(stream, ": ");
    summary = pas_segregated_heap_compute_summary(heap);
    pas_heap_summary_dump(summary, stream);
    pas_stream_printf(stream, "\n");

    if (pas_status_reporter_enabled >= 3) {
        pas_segregated_heap_for_each_size_directory(
            heap, dump_segregated_heap_directory_callback, stream);
    }
    
    bitfit_heap = pas_compact_atomic_bitfit_heap_ptr_load(&heap->bitfit_heap);
    if (bitfit_heap)
        pas_status_reporter_dump_bitfit_heap(stream, bitfit_heap);
}

void pas_status_reporter_dump_heap(pas_stream* stream, pas_heap* heap)
{
    const pas_heap_config* config;
    pas_heap_summary summary;

    config = pas_heap_config_kind_get_config(heap->config_kind);
    
    pas_stream_printf(stream, "    Heap %p:\n", heap);

    pas_stream_printf(stream, "        %s, ", pas_heap_config_kind_get_string(heap->config_kind));
    config->dump_type(heap->type, stream);
    pas_stream_printf(stream, "\n");
    
    summary = pas_heap_compute_summary(heap, pas_lock_is_held);
    pas_stream_printf(stream, "        Total Summary: ");
    pas_heap_summary_dump(summary, stream);
    pas_stream_printf(stream, "\n");

    pas_status_reporter_dump_segregated_heap(stream, &heap->segregated_heap);
    
    pas_stream_printf(stream, "        ");
    pas_status_reporter_dump_large_heap(stream, &heap->large_heap);
    pas_stream_printf(stream, "\n");
}

typedef struct {
    pas_stream* stream;
    size_t count;
} dump_all_heaps_data;

static bool dump_all_heaps_heap_callback(pas_heap* heap, void* arg)
{
    dump_all_heaps_data* data;

    data = arg;

    pas_status_reporter_dump_heap(data->stream, heap);
    data->count++;
    
    return true;
}

void pas_status_reporter_dump_all_heaps(pas_stream* stream)
{
    dump_all_heaps_data data;
    data.stream = stream;
    data.count = 0;
    pas_all_heaps_for_each_heap(dump_all_heaps_heap_callback, &data);
    pas_stream_printf(stream, "    Num Heaps: %zu\n", data.count);
}

static bool dump_all_shared_page_directories_directory_callback(
    pas_segregated_shared_page_directory* directory,
    void* arg)
{
    pas_stream* stream;

    stream = arg;

    pas_status_reporter_dump_segregated_shared_page_directory(stream, directory);

    return true;
}

void pas_status_reporter_dump_all_shared_page_directories(pas_stream* stream)
{
    pas_stream_printf(stream, "    Shared Page Directories:\n");
    pas_all_shared_page_directories_for_each(
        dump_all_shared_page_directories_directory_callback,
        stream);
}

void pas_status_reporter_dump_all_heaps_non_utility_summaries(pas_stream* stream)
{
    pas_stream_printf(stream, "    All Heaps Non-Utility Segregated Summary: ");
    pas_heap_summary_dump(pas_all_heaps_compute_total_non_utility_segregated_summary(), stream);
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "    All Heaps Non-Utility Bitfit Summary: ");
    pas_heap_summary_dump(pas_all_heaps_compute_total_non_utility_bitfit_summary(), stream);
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "    All Heaps Non-Utility Large Summary: ");
    pas_heap_summary_dump(pas_all_heaps_compute_total_non_utility_large_summary(), stream);
    pas_stream_printf(stream, "\n");
}

static bool dump_large_sharing_pool_node_callback(pas_large_sharing_node* node,
                                                  void* arg)
{
    pas_stream* stream;

    stream = arg;

    pas_stream_printf(stream, "        %p...%p: %s, %zu/%zu live (%.0lf%%), %llu",
                      (void*)node->range.begin,
                      (void*)node->range.end,
                      pas_commit_mode_get_string(node->is_committed),
                      node->num_live_bytes,
                      pas_range_size(node->range),
                      100. * (double)node->num_live_bytes / (double)pas_range_size(node->range),
                      (unsigned long long)node->use_epoch);

    if (node->synchronization_style != pas_physical_memory_is_locked_by_virtual_range_common_lock) {
        pas_stream_printf(
            stream, ", %s",
            pas_physical_memory_synchronization_style_get_string(node->synchronization_style));
    }
    if (node->mmap_capability != pas_may_mmap) {
        pas_stream_printf(
            stream, ", %s",
            pas_mmap_capability_get_string(node->mmap_capability));
    }

    pas_stream_printf(stream, "\n");

    return true;
}

void pas_status_reporter_dump_large_sharing_pool(pas_stream* stream)
{
    pas_stream_printf(stream, "    Large sharing pool contents:\n");
    pas_large_sharing_pool_for_each(dump_large_sharing_pool_node_callback, stream, pas_lock_is_held);
}

void pas_status_reporter_dump_utility_heap(pas_stream* stream)
{
    pas_stream_printf(stream, "    Utility Heap:\n");
    pas_status_reporter_dump_segregated_heap(stream, &pas_utility_segregated_heap);
}

#define SIZE_HISTOGRAM_MAX_SIZE 65536
#define SIZE_HISTOGRAM_BUCKET_SIZE 256
#define SIZE_HISTOGRAM_NUM_BUCKETS (SIZE_HISTOGRAM_MAX_SIZE / SIZE_HISTOGRAM_BUCKET_SIZE + 1)

typedef struct {
    size_t segregated_exclusive_fragmentation_size_histogram[SIZE_HISTOGRAM_NUM_BUCKETS];
    size_t segregated_partial_fragmentation_size_histogram[SIZE_HISTOGRAM_NUM_BUCKETS];
    size_t segregated_exclusive_fragmentation;
    size_t segregated_shared_fragmentation;
    size_t large_fragmentation;
} total_fragmentation_data;

static void add_to_size_histogram(size_t* histogram,
                                  size_t size,
                                  size_t value)
{
    PAS_ASSERT(size <= SIZE_HISTOGRAM_MAX_SIZE);
    histogram[size / SIZE_HISTOGRAM_BUCKET_SIZE] += value;
}

static void dump_histogram(pas_stream* stream, size_t* histogram)
{
    size_t size;
    
    for (size = 0; size <= SIZE_HISTOGRAM_MAX_SIZE; size += SIZE_HISTOGRAM_BUCKET_SIZE) {
        size_t value;
        value = histogram[size / SIZE_HISTOGRAM_BUCKET_SIZE];
        if (!value)
            continue;
        pas_stream_printf(stream,
                          "        %zu..%zu: %zu\n",
                          size, size + SIZE_HISTOGRAM_BUCKET_SIZE - 1,
                          value);
    }
}

static bool total_fragmentation_size_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
    void* arg)
{
    total_fragmentation_data* data;
    size_t index;

    PAS_UNUSED_PARAM(heap);

    data = arg;
    
    for (index = 0; index < pas_segregated_directory_size(&directory->base); ++index) {
        pas_segregated_view view;
        size_t fragmentation;
        view = pas_segregated_directory_get(&directory->base, index);
        fragmentation = pas_heap_summary_fragmentation(
            pas_segregated_view_compute_summary(
                view,
                pas_segregated_page_config_kind_get_config(directory->base.page_config_kind)));
        if (!pas_segregated_view_is_some_exclusive(view)) {
            PAS_ASSERT(pas_segregated_view_is_partial(view));
            add_to_size_histogram(data->segregated_partial_fragmentation_size_histogram,
                                  directory->object_size,
                                  fragmentation);
            continue;
        }
        data->segregated_exclusive_fragmentation += fragmentation;
        add_to_size_histogram(data->segregated_exclusive_fragmentation_size_histogram,
                              directory->object_size,
                              fragmentation);
    }

    return true;
}

static bool total_fragmentation_heap_callback(pas_heap* heap, void* arg)
{
    total_fragmentation_data* data;

    data = arg;

    pas_segregated_heap_for_each_size_directory(
        &heap->segregated_heap, total_fragmentation_size_directory_callback, data);

    data->large_fragmentation += pas_heap_summary_fragmentation(
        pas_large_heap_compute_summary(&heap->large_heap));

    return true;
}

static bool total_fragmentation_shared_page_directory_callback(
    pas_segregated_shared_page_directory* directory,
    void* arg)
{
    total_fragmentation_data* data;

    data = arg;

    data->segregated_shared_fragmentation += pas_heap_summary_fragmentation(
        pas_segregated_directory_compute_summary(&directory->base));

    return true;
}

void pas_status_reporter_dump_total_fragmentation(pas_stream* stream)
{
    total_fragmentation_data data;
    pas_zero_memory(&data, sizeof(data));
    pas_all_heaps_for_each_heap(total_fragmentation_heap_callback, &data);
    pas_all_shared_page_directories_for_each(
        total_fragmentation_shared_page_directory_callback, &data);
    pas_segregated_heap_for_each_size_directory(
        &pas_utility_segregated_heap, total_fragmentation_size_directory_callback, &data);
    data.large_fragmentation += pas_heap_summary_fragmentation(
        pas_large_utility_free_heap_compute_summary());
    pas_stream_printf(stream, "    Segregated Exclusive Fragmentation Histogram:\n");
    dump_histogram(stream, data.segregated_exclusive_fragmentation_size_histogram);
    pas_stream_printf(stream, "    Segregated Partial Fragmentation Histogram:\n");
    dump_histogram(stream, data.segregated_partial_fragmentation_size_histogram);
    pas_stream_printf(stream, "    Segregated Exclusive Fragmentation: %zu\n",
                      data.segregated_exclusive_fragmentation);
    pas_stream_printf(stream, "    Segregated Shared Fragmentation: %zu\n",
                      data.segregated_shared_fragmentation);
    pas_stream_printf(stream, "    Total Segregated Fragmentation: %zu\n",
                      data.segregated_exclusive_fragmentation +
                      data.segregated_shared_fragmentation);
    pas_stream_printf(stream, "    Large Fragmentation: %zu\n", data.large_fragmentation);
    pas_stream_printf(stream, "    Total Fragmentation: %zu\n",
                      data.segregated_exclusive_fragmentation +
                      data.segregated_shared_fragmentation +
                      data.large_fragmentation);
}

void pas_status_reporter_dump_view_stats(pas_stream* stream)
{
    pas_stream_printf(stream, "    Number of Partial Views: %zu\n", pas_segregated_partial_view_count);
    pas_stream_printf(stream, "    Number of Shared Views: %zu\n", pas_segregated_shared_view_count);
    pas_stream_printf(stream, "    Number of Exclusive Views: %zu\n", pas_segregated_exclusive_view_count);
}

typedef struct {
    size_t num_directories_with_data;
    size_t num_directories_with_tlas;
    size_t num_directories_with_exclusives;
    size_t num_directories;
    size_t num_heaps;
} tier_up_rate_data;

static bool tier_up_rate_size_directory_callback(
    pas_segregated_heap* heap,
    pas_segregated_size_directory* directory,
    void* arg)
{
    tier_up_rate_data* data;

    PAS_UNUSED_PARAM(heap);

    data = arg;

    if (pas_segregated_size_directory_data_ptr_load(&directory->data))
        data->num_directories_with_data++;
    if (pas_segregated_size_directory_has_tlc_allocator(directory))
        data->num_directories_with_tlas++;
    if (pas_segregated_size_directory_are_exclusive_views_enabled(directory))
        data->num_directories_with_exclusives++;
    data->num_directories++;

    return true;
}

static bool tier_up_rate_heap_callback(pas_heap* heap, void* arg)
{
    tier_up_rate_data* data;

    data = arg;

    pas_segregated_heap_for_each_size_directory(
        &heap->segregated_heap, tier_up_rate_size_directory_callback, data);

    data->num_heaps++;

    return true;
}

static void dump_directory_tier_up_rate(pas_stream* stream,
                                        const char* name,
                                        size_t count,
                                        tier_up_rate_data* data)
{
    pas_stream_printf(stream,
                      "    %s: %zu/%zu (%.0lf%%)\n",
                      name,
                      count,
                      data->num_directories,
                      100. * count / data->num_directories);
}

void pas_status_reporter_dump_tier_up_rates(pas_stream* stream)
{
    tier_up_rate_data data;
    pas_zero_memory(&data, sizeof(data));
    pas_all_heaps_for_each_heap(tier_up_rate_heap_callback, &data);
    dump_directory_tier_up_rate(
        stream, "Num Size Directories With Data", data.num_directories_with_data, &data);
    dump_directory_tier_up_rate(
        stream, "Num Size Directories With TLAs", data.num_directories_with_tlas, &data);
    dump_directory_tier_up_rate(
        stream, "Num Size Directories With Exclusives", data.num_directories_with_exclusives,
        &data);
}

static const char* allocator_state(pas_local_allocator* allocator)
{
    
    if (!pas_local_allocator_is_active(allocator))
        return "inactive";
    if (pas_segregated_view_is_partial(allocator->view))
        return "partial";
    return "exclusive";
}

static void dump_allocator_state(pas_stream* stream, pas_local_allocator* allocator)
{
    pas_segregated_view view;

    view = allocator->view;
    pas_stream_printf(stream, ", %s, view = %p, directory = %p, %s",
                      pas_local_allocator_config_kind_get_string(allocator->config_kind),
                      view, view ? pas_segregated_view_get_size_directory(view) : NULL,
                      allocator_state(allocator));
}

static void dump_scavenger_data_state(pas_stream* stream, pas_local_allocator_scavenger_data* data)
{
    pas_local_allocator_kind kind;

    kind = data->kind;
    pas_stream_printf(stream, "%s", pas_local_allocator_kind_get_string(kind));

    if (kind == pas_local_allocator_allocator_kind)
        dump_allocator_state(stream, (pas_local_allocator*)data);
}

void pas_status_reporter_dump_baseline_allocators(pas_stream* stream)
{
    size_t index;

    pas_stream_printf(stream, "    Baseline Allocators:\n");

    if (!pas_baseline_allocator_table) {
        pas_stream_printf(stream, "        N/A\n");
        return;
    }
    
    for (index = 0; index < PAS_NUM_BASELINE_ALLOCATORS; ++index) {
        pas_local_allocator* allocator;

        allocator = &pas_baseline_allocator_table[index].u.allocator;

        pas_stream_printf(stream, "         %zu: ", index);
        dump_allocator_state(stream, allocator);
        pas_stream_printf(stream, "\n");
    }
}

void pas_status_reporter_dump_thread_local_caches(pas_stream* stream)
{
    pas_thread_local_cache_node* node;
    pas_thread_local_cache_layout_node layout_node;
    size_t index;

    pas_stream_printf(stream, "    Thread Local Cache Layout:\n");
    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node)) {
        pas_stream_printf(stream, "        %u: %s, directory = %p\n",
                          pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node),
                          pas_thread_local_cache_layout_node_kind_get_string(
                              pas_thread_local_cache_layout_node_get_kind(layout_node)),
                          pas_thread_local_cache_layout_node_get_directory(layout_node));
    }

    pas_stream_printf(stream, "    Thread Local Caches:\n");
    for (index = 0, node = pas_thread_local_cache_node_first; node; node = node->next, ++index) {
        pas_thread_local_cache* cache;
        unsigned num_logged_objects;
        unsigned index_in_deallocation_log;

        cache = node->cache;

        pas_stream_printf(stream, "        %p(%zu): node = %p\n", cache, index, node);

        if (!cache)
            continue;

        num_logged_objects = 0;
        for (index_in_deallocation_log = cache->deallocation_log_index; index_in_deallocation_log--;) {
            if (cache->deallocation_log[index_in_deallocation_log])
                num_logged_objects++;
        }
        pas_stream_printf(stream,
                          "            Deallocation logged objects = %u\n",
                          num_logged_objects);

        for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layout_node)) {
            pas_allocator_index allocator_index;

            allocator_index =
                pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node);

            if (allocator_index >= cache->allocator_index_upper_bound)
                break;

            if (!pas_thread_local_cache_layout_node_is_committed(layout_node, cache))
                continue;
            
            pas_stream_printf(stream, "            %u: ", allocator_index);
            dump_scavenger_data_state(
                stream, pas_thread_local_cache_get_local_allocator_direct(cache, allocator_index));
            pas_stream_printf(stream, "\n");
        }
    }
}

void pas_status_reporter_dump_configuration(pas_stream* stream)
{
    pas_stream_printf(stream, "    Mprotect Decommitted: %s\n",
                      PAS_MPROTECT_DECOMMITTED ? "yes" : "no");
}

void pas_status_reporter_dump_physical_page_sharing_pool(pas_stream* stream)
{
    pas_stream_printf(stream, "    Physical Page Sharing Pool Balance: %ld\n",
                      pas_physical_page_sharing_pool_balance);
}

static void dump_expendable_memory(pas_stream* stream,
                                   pas_expendable_memory* header,
                                   void* payload)
{
    size_t index;
    size_t index_end;

    pas_stream_printf(stream, "Header = %p, Payload = %p...%p, Page States: ",
                      header, payload, (char*)payload + header->size);

    index_end = pas_expendable_memory_num_pages_in_use(header);

    for (index = 0; index < index_end; ++index) {
        pas_expendable_memory_state_kind kind;

        kind = pas_expendable_memory_state_get_kind(header->states[index]);

        switch (kind) {
        case PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED:
            pas_stream_printf(stream, "D");
            break;
        case PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR:
            pas_stream_printf(stream, "I");
            break;
        default:
            PAS_ASSERT(kind >= PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED);
            PAS_ASSERT(kind <= PAS_EXPENDABLE_MEMORY_STATE_KIND_MAX_JUST_USED);
            PAS_ASSERT(kind - PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED < 10);
            pas_stream_printf(stream, "%u", kind - PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED);
            break;
        }
    }
}

void pas_status_reporter_dump_expendable_memories(pas_stream* stream)
{
    pas_large_expendable_memory* large_memory;

    pas_heap_lock_assert_held();

    pas_stream_printf(stream, "    Compact Expendable Memory: ");
    dump_expendable_memory(
        stream, &pas_compact_expendable_memory_header.header, pas_compact_expendable_memory_payload);
    pas_stream_printf(stream, "\n");

    for (large_memory = pas_large_expendable_memory_head; large_memory; large_memory = large_memory->next) {
        pas_stream_printf(stream, "    Large Expendable Memory: ");
        dump_expendable_memory(
            stream, &large_memory->header, pas_large_expendable_memory_payload(large_memory));
        pas_stream_printf(stream, "\n");
    }
}

void pas_status_reporter_dump_everything(pas_stream* stream)
{
    pas_heap_lock_assert_held();
    
    pas_stream_printf(stream, "%d: Heap Status:\n", getpid());
    pas_status_reporter_dump_all_heaps(stream);
    pas_status_reporter_dump_all_shared_page_directories(stream);
    pas_status_reporter_dump_all_heaps_non_utility_summaries(stream);
    
    if (pas_status_reporter_enabled >= 3)
        pas_status_reporter_dump_large_sharing_pool(stream);
    
    pas_status_reporter_dump_utility_heap(stream);

    if (pas_status_reporter_enabled >= 3) {
        pas_status_reporter_dump_large_map(stream);
        pas_status_reporter_dump_baseline_allocators(stream);
        pas_status_reporter_dump_thread_local_caches(stream);
        
        pas_stream_printf(stream, "    Heap Table: ");
        pas_status_reporter_dump_heap_table(stream);
        pas_stream_printf(stream, "\n");
        
        pas_stream_printf(stream, "    Immortal Heap: ");
        pas_status_reporter_dump_immortal_heap(stream);
        pas_stream_printf(stream, "\n");
    }

    pas_stream_printf(stream, "    Compact Large Utility Free Heap: ");
    pas_status_reporter_dump_compact_large_utility_free_heap(stream);
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "    Large Utility Free Heap: ");
    pas_status_reporter_dump_large_utility_free_heap(stream);
    pas_stream_printf(stream, "\n");

    if (pas_status_reporter_enabled >= 3) {
        pas_status_reporter_dump_total_fragmentation(stream);
        pas_status_reporter_dump_view_stats(stream);
        pas_status_reporter_dump_tier_up_rates(stream);
    }

    pas_stream_printf(stream, "    Compact Bootstrap Free Heap: ");
    pas_status_reporter_dump_compact_bootstrap_free_heap(stream);
    pas_stream_printf(stream, "\n");

    pas_stream_printf(stream, "    Bootstrap Free Heap: ");
    pas_status_reporter_dump_bootstrap_free_heap(stream);
    pas_stream_printf(stream, "\n");

    pas_status_reporter_dump_configuration(stream);
    pas_status_reporter_dump_physical_page_sharing_pool(stream);
    pas_status_reporter_dump_expendable_memories(stream);
}

static void* status_reporter_thread_main(void* arg)
{
    pas_fd_stream fd_stream;

    PAS_UNUSED_PARAM(arg);

    pas_fd_stream_construct(&fd_stream, PAS_LOG_DEFAULT_FD);
    
    for (;;) {
        usleep(pas_status_reporter_period_in_microseconds);

        PAS_ASSERT(pas_status_reporter_enabled);

        if (pas_status_reporter_enabled == 1) {
            pas_fd_stream_printf(&fd_stream, "%d: Num Heaps: %zu\n", getpid(), pas_all_heaps_count);
            continue;
        }
        
        pas_heap_lock_lock();
        pas_status_reporter_dump_everything((pas_stream*)&fd_stream);
        pas_heap_lock_unlock();
    }
    
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static void start_reporter(void)
{
    int result;
    pthread_t thread;

    result = pthread_create(&thread, NULL, status_reporter_thread_main, NULL);
    PAS_ASSERT(!result);
    pthread_detach(thread);
}

void pas_status_reporter_start_if_necessary(void)
{
    if (pas_status_reporter_enabled)
        pthread_once(&once_control, start_reporter);
}

#endif /* LIBPAS_ENABLED */
