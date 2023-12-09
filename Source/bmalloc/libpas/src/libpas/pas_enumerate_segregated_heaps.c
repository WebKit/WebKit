/*
 * Copyright (c) 2020-2022 Apple Inc. All rights reserved.
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

#include "pas_enumerate_segregated_heaps.h"

#include "pas_baseline_allocator.h"
#include "pas_enumerator_internal.h"
#include "pas_full_alloc_bits.h"
#include "pas_hashtable.h"
#include "pas_ptr_hash_set.h"
#include "pas_redundant_local_allocator_node.h"
#include "pas_root.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_partial_view.h"
#include "pas_segregated_shared_handle.h"
#include "pas_segregated_shared_view.h"
#include "pas_shared_handle_or_page_boundary_inlines.h"
#include "pas_thread_local_cache_layout.h"
#include "pas_thread_local_cache_node.h"

static const bool verbose = false;

struct enumeration_context;
struct local_allocator_node;
struct local_allocators_for_page;
typedef struct enumeration_context enumeration_context;
typedef struct local_allocator_node local_allocator_node;
typedef struct local_allocators_for_page local_allocators_for_page;

struct local_allocator_node {
    local_allocator_node* next;
    pas_local_allocator* allocator;
};

struct local_allocators_for_page {
    uintptr_t page_boundary;
    local_allocator_node* head;
};

typedef uintptr_t local_allocator_map_key;
typedef local_allocators_for_page local_allocator_map_entry;

static inline local_allocator_map_entry local_allocator_map_entry_create_empty(void)
{
    local_allocators_for_page result;
    result.page_boundary = 0;
    result.head = NULL;
    return result;
}

static inline local_allocator_map_entry local_allocator_map_entry_create_deleted(void)
{
    local_allocators_for_page result;
    result.page_boundary = 1;
    result.head = NULL;
    return result;
}

static inline bool local_allocator_map_entry_is_empty_or_deleted(local_allocator_map_entry entry)
{
    if (entry.page_boundary <= 1) {
        PAS_ASSERT_WITH_DETAIL(!entry.head);
        return true;
    }
    return false;
}

static inline bool local_allocator_map_entry_is_empty(local_allocator_map_entry entry)
{
    if (!entry.page_boundary) {
        PAS_ASSERT_WITH_DETAIL(!entry.head);
        return true;
    }
    return false;
}

static inline bool local_allocator_map_entry_is_deleted(local_allocator_map_entry entry)
{
    if (entry.page_boundary == 1) {
        PAS_ASSERT_WITH_DETAIL(!entry.head);
        return true;
    }
    return false;
}

static inline local_allocator_map_key local_allocator_map_entry_get_key(local_allocator_map_entry entry)
{
    return entry.page_boundary;
}

static inline unsigned local_allocator_map_key_get_hash(local_allocator_map_key key)
{
    return pas_hash_ptr((void*)key);
}

static inline bool local_allocator_map_key_is_equal(local_allocator_map_key a,
                                                    local_allocator_map_key b)
{
    return a == b;
}

PAS_CREATE_HASHTABLE(local_allocator_map,
                     local_allocator_map_entry,
                     local_allocator_map_key);

struct enumeration_context {
    local_allocator_map allocators;
    pas_ptr_hash_set shared_page_directories;
    pas_ptr_hash_set objects_in_deallocation_log;
};

static bool for_each_view(pas_enumerator* enumerator,
                          pas_segregated_directory* directory,
                          bool (*callback)(
                              pas_enumerator* enumerator, pas_segregated_view view, void* arg),
                          void* arg)
{
    pas_segregated_view view;
    pas_segregated_directory_data* data;
    size_t index;
    pas_compact_atomic_segregated_view* vector;

    view = pas_compact_atomic_segregated_view_load_remote(enumerator, &directory->first_view);
    if (!view)
        return true;

    if (!callback(enumerator, view, arg))
        return false;

    data = pas_segregated_directory_data_ptr_load_remote(enumerator, &directory->data);
    if (!data)
        return true;

    vector = pas_segregated_directory_view_vector_ptr_load_remote(enumerator, &data->views.array);
    for (index = 0; index < data->views.size; ++index) {
        view = pas_compact_atomic_segregated_view_load_remote(enumerator, vector + index);
        if (!view)
            continue;

        if (!callback(enumerator, view, arg))
            return false;
    }
    
    return true;
}

static bool collect_shared_page_directories_shared_page_directory_callback(
    pas_enumerator* enumerator,
    pas_segregated_shared_page_directory* directory,
    void* arg)
{
    enumeration_context* context;

    context = arg;

    pas_ptr_hash_set_set(
        &context->shared_page_directories, directory, NULL, &enumerator->allocation_config);

    return true;
}

static bool collect_shared_page_directories_heap_callback(
    pas_enumerator* enumerator,
    pas_heap* heap,
    void* arg)
{
    enumeration_context* context;
    const pas_heap_config* config;

    context = arg;

    config = pas_heap_config_kind_get_config(heap->config_kind);
    PAS_ASSERT_WITH_DETAIL(config);

    return config->for_each_shared_page_directory_remote(
        enumerator, &heap->segregated_heap,
        collect_shared_page_directories_shared_page_directory_callback, context);
}

static void record_page_payload_and_meta(pas_enumerator* enumerator,
                                         const pas_segregated_page_config* page_config,
                                         uintptr_t page_boundary,
                                         pas_segregated_page* page,
                                         uintptr_t payload_begin,
                                         uintptr_t payload_end)
{
    pas_page_granule_use_count* use_counts;
    uintptr_t page_size;
    uintptr_t granule_size;

    page_size = page_config->base.page_size;
    granule_size = page_config->base.granule_size;

    if (page_size == granule_size)
        use_counts = NULL;
    else
        use_counts = pas_segregated_page_get_granule_use_counts(page, *page_config);

    pas_enumerator_record_page_payload_and_meta(
        enumerator, page_boundary, page_size, granule_size, use_counts, payload_begin, payload_end);
}

static void record_page_objects(pas_enumerator* enumerator,
                                enumeration_context* context,
                                pas_segregated_size_directory* directory,
                                const pas_segregated_page_config* page_config,
                                uintptr_t page_boundary,
                                pas_segregated_page* page,
                                pas_local_allocator* allocator,
                                pas_full_alloc_bits* full_alloc_bits)
{
    size_t index;
    
    if (!enumerator->record_object)
        return;
    
    for (index = PAS_BITVECTOR_BIT_INDEX(full_alloc_bits->word_index_begin);
         index < PAS_BITVECTOR_BIT_INDEX(full_alloc_bits->word_index_end);
         ++index) {
        uintptr_t offset;
        uintptr_t begin;

        if (!pas_bitvector_get(full_alloc_bits->bits, index))
            continue;
        
        if (!pas_bitvector_get(page->alloc_bits, index))
            continue;

        offset = index << page_config->base.min_align_shift;
        begin = page_boundary + offset;

        if (verbose)
            pas_log("Considering possible object %p\n", (void*)begin);
        
        if (allocator) {
            if (begin < allocator->payload_end
                && begin >= allocator->payload_end - allocator->remaining) {
                if (verbose) {
                    pas_log("The object is within the bump range (payload_end = %p, remaining = %u).\n",
                            (void*)allocator->payload_end, allocator->remaining);
                }
                continue;
            }
            
            if (allocator->end_offset > allocator->current_offset) {
                if (page_config->variant == pas_small_segregated_page_config_variant
                    && PAS_BITVECTOR_WORD64_INDEX(index) == allocator->current_offset
                    && allocator->current_word_is_valid) {
                    uint64_t current_word;

                    current_word = allocator->current_word;
                    if (page_config->use_reversed_current_word)
                        current_word = pas_reverse64(current_word);
                    
                    if (current_word & PAS_BITVECTOR_BIT_MASK64(index)) {
                        if (verbose) {
                            pas_log("The object has an allocator bit set in current_word "
                                    "(current_offset = %u, end_offset = %u).\n",
                                    allocator->current_offset, allocator->end_offset);
                        }
                        continue;
                    }
                } else if (pas_bitvector_get((unsigned*)allocator->bits, index)) {
                    if (verbose) {
                        pas_log("The object has an allocator bit set (current_offset = %u, "
                                "end_offset = %u).\n",
                                allocator->current_offset, allocator->end_offset);
                    }
                    continue;
                }
            }
        }

        if (pas_ptr_hash_set_find(&context->objects_in_deallocation_log, (void*)begin)) {
            if (verbose)
                pas_log("The object is in the deallocation log.\n");
            continue;
        }
        
        pas_enumerator_record(enumerator,
                              (void*)begin,
                              directory->object_size,
                              pas_enumerator_object_record);
    }
}

static bool enumerate_exclusive_view(pas_enumerator* enumerator,
                                     pas_segregated_exclusive_view* view,
                                     enumeration_context* context)
{
    pas_segregated_size_directory* directory;
    const pas_segregated_page_config* page_config;
    pas_segregated_size_directory_data* data;
    uintptr_t page_boundary;
    pas_segregated_page* page;
    local_allocator_node* allocator_node;
    pas_local_allocator* allocator;
    pas_full_alloc_bits alloc_bits;

    directory = pas_compact_segregated_size_directory_ptr_load_remote(enumerator, &view->directory);
    page_config = pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);

    page_boundary = (uintptr_t)view->page_boundary;

    if (verbose)
        pas_log("Enumerating exclusive page %p\n", (void*)page_boundary);

    pas_enumerator_exclude_accounted_pages(
        enumerator, (void*)page_boundary, page_config->base.page_size);
    
    if (!view->is_owned)
        return true;

    data = pas_segregated_size_directory_data_ptr_load_remote(enumerator, &directory->data);

    page = (pas_segregated_page*)page_config->base.page_header_for_boundary_remote(
        enumerator, (void*)page_boundary);
    PAS_ASSERT_WITH_DETAIL(page);

    page = pas_enumerator_read(
        enumerator, page, pas_segregated_page_header_size(*page_config, pas_segregated_page_exclusive_role));
    if (!page)
        return false;

    allocator_node = local_allocator_map_get(&context->allocators, page_boundary).head;
    if (!allocator_node)
        allocator = NULL;
    else {
        /* Exclusives can only have one allocator allocating in them at a time. */
        PAS_ASSERT_WITH_DETAIL(!allocator_node->next);
        allocator = allocator_node->allocator;
        PAS_ASSERT_WITH_DETAIL(allocator);
    }

    if (verbose)
        pas_log("Have allocator = %p\n", allocator);

    record_page_payload_and_meta(
        enumerator,
        page_config,
        page_boundary,
        page,
        data->offset_from_page_boundary_to_first_object,
        data->offset_from_page_boundary_to_end_of_last_object);

    alloc_bits.bits = pas_compact_tagged_unsigned_ptr_load_remote(enumerator, &data->full_alloc_bits);
    alloc_bits.word_index_begin = 0;
    alloc_bits.word_index_end = (unsigned)pas_segregated_page_config_num_alloc_words(*page_config);
    record_page_objects(
        enumerator, context, directory, page_config, page_boundary, page, allocator, &alloc_bits);
    
    return true;
}

static bool enumerate_shared_view(pas_enumerator* enumerator,
                                  pas_segregated_shared_view* view,
                                  pas_segregated_shared_page_directory* directory,
                                  enumeration_context* context)
{
    const pas_segregated_page_config* page_config;
    pas_shared_handle_or_page_boundary shared_handle_or_page_boundary;
    pas_segregated_shared_handle* shared_handle;
    uintptr_t page_boundary;
    pas_segregated_page* page;

    PAS_UNUSED_PARAM(context);

    page_config = pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);

    page = NULL;
    shared_handle_or_page_boundary = view->shared_handle_or_page_boundary;
    if (pas_is_wrapped_shared_handle(shared_handle_or_page_boundary)) {
        shared_handle = pas_unwrap_shared_handle_no_liveness_checks(shared_handle_or_page_boundary);
        shared_handle = pas_enumerator_read_compact(enumerator, shared_handle);
        page_boundary = (uintptr_t)shared_handle->page_boundary;
        if (view->is_owned) {
            page = (pas_segregated_page*)page_config->base.page_header_for_boundary_remote(
                enumerator, (void*)page_boundary);
            PAS_ASSERT_WITH_DETAIL(page);
            
            page = pas_enumerator_read(
                enumerator, page,
                pas_segregated_page_header_size(*page_config, pas_segregated_page_shared_role));
            if (!page)
                return false;
        }
    } else {
        shared_handle = NULL;
        page_boundary = (uintptr_t)pas_unwrap_page_boundary(shared_handle_or_page_boundary);
    }

    if (!page_boundary) {
        PAS_ASSERT_WITH_DETAIL(!view->is_owned);
        return true;
    }

    if (verbose)
        pas_log("Enumerating shared view of shared page %p\n", (void*)page_boundary);

    pas_enumerator_exclude_accounted_pages(
        enumerator, (void*)page_boundary, page_config->base.page_size);
    
    if (view->is_owned) {
        uintptr_t payload_begin;
        uintptr_t payload_end;
        
        PAS_ASSERT_WITH_DETAIL(page);
        
        payload_begin = pas_round_up_to_power_of_2(page_config->shared_payload_offset,
                                                   pas_segregated_page_config_min_align(*page_config));
        payload_end = pas_segregated_page_config_payload_end_offset_for_role(
            *page_config, pas_segregated_page_shared_role);
        
        record_page_payload_and_meta(enumerator,
                                     page_config,
                                     page_boundary,
                                     page,
                                     payload_begin,
                                     payload_end);
    }

    return true;
}

static bool enumerate_partial_view(pas_enumerator* enumerator,
                                   pas_segregated_partial_view* view,
                                   enumeration_context* context)
{
    pas_segregated_size_directory* directory;
    const pas_segregated_page_config* page_config;
    pas_segregated_shared_view* shared_view;
    pas_shared_handle_or_page_boundary shared_handle_or_page_boundary;
    pas_segregated_shared_handle* shared_handle;
    uintptr_t page_boundary;
    pas_segregated_page* page;
    pas_full_alloc_bits full_alloc_bits;
    local_allocator_node* allocator_node;
    pas_local_allocator* allocator;

    if (!view->is_attached_to_shared_handle)
        return true;

    directory = pas_compact_segregated_size_directory_ptr_load_remote(enumerator, &view->directory);
    page_config = pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);

    shared_view = pas_compact_segregated_shared_view_ptr_load_remote(enumerator, &view->shared_view);
    PAS_ASSERT_WITH_DETAIL(shared_view);
    if (!shared_view->is_owned)
        return true;

    page = NULL;
    shared_handle_or_page_boundary = shared_view->shared_handle_or_page_boundary;
    shared_handle = pas_unwrap_shared_handle_no_liveness_checks(shared_handle_or_page_boundary);
    shared_handle = pas_enumerator_read_compact(enumerator, shared_handle);
    page_boundary = (uintptr_t)shared_handle->page_boundary;

    page = (pas_segregated_page*)page_config->base.page_header_for_boundary_remote(
        enumerator, (void*)page_boundary);
    PAS_ASSERT_WITH_DETAIL(page);
    
    page = pas_enumerator_read(
        enumerator, page, pas_segregated_page_header_size(*page_config, pas_segregated_page_shared_role));
    if (!page)
        return false;

    if (verbose)
        pas_log("Enumerating partial view of shared page %p\n", (void*)page_boundary);

    allocator = NULL;
    for (allocator_node = local_allocator_map_get(&context->allocators, page_boundary).head;
         allocator_node;
         allocator_node = allocator_node->next) {
        pas_segregated_view allocator_view;

        allocator_view = pas_enumerator_read_compact(enumerator, allocator_node->allocator->view);

        if (verbose) {
            pas_log("Considering allocator %p (allocator_view = %p, view = %p)\n",
                    allocator_node->allocator, allocator_view, view);
        }
        
        if (pas_segregated_view_get_ptr(allocator_view) == view) {
            allocator = allocator_node->allocator;
            break;
        }
    }

    if (verbose)
        pas_log("Found allocator = %p\n", allocator);

    /* This is so weird: the size we pass is only valid when view->alloc_bits is pointing at the
       local_allocator's bits. But that's the only time that load_remote will go down the path where it needs
       to know the size. So, it's fine, I guess. */
    full_alloc_bits.bits = pas_lenient_compact_unsigned_ptr_load_remote(
        enumerator, &view->alloc_bits, pas_segregated_page_config_num_alloc_bytes(*page_config));
    full_alloc_bits.word_index_begin = view->alloc_bits_offset;
    full_alloc_bits.word_index_end = view->alloc_bits_offset + view->alloc_bits_size;
    record_page_objects(
        enumerator, context, directory, page_config, page_boundary, page, allocator, &full_alloc_bits);
    
    return true;
}

typedef struct {
    enumeration_context* context;
    pas_segregated_shared_page_directory* directory;
} shared_page_directory_view_data;

static bool shared_page_directory_view_callback(pas_enumerator* enumerator,
                                                pas_segregated_view view,
                                                void* arg)
{
    shared_page_directory_view_data* data;

    data = arg;

    PAS_ASSERT_WITH_DETAIL(pas_segregated_view_get_kind(view) == pas_segregated_shared_view_kind);

    return enumerate_shared_view(
        enumerator, pas_segregated_view_get_ptr(view), data->directory, data->context);
}

static bool size_directory_view_callback(pas_enumerator* enumerator,
                                         pas_segregated_view view,
                                         void* arg)
{
    enumeration_context* context;

    context = arg;

    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return enumerate_exclusive_view(enumerator, pas_segregated_view_get_ptr(view), context);

    case pas_segregated_partial_view_kind:
        return enumerate_partial_view(enumerator, pas_segregated_view_get_ptr(view), context);

    default:
        pas_log("Invalid view kind in size directory: %s\n",
                pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
        PAS_ASSERT_WITH_DETAIL(!"Should not be reached");
        return true;
    }
}

static bool enumerate_segregated_heap_callback(pas_enumerator* enumerator,
                                               pas_heap* heap,
                                               void* arg)
{
    enumeration_context* context;
    pas_segregated_size_directory* directory;

    context = arg;

    for (directory = pas_compact_atomic_segregated_size_directory_ptr_load_remote(
             enumerator, &heap->segregated_heap.basic_size_directory_and_head);
         directory;
         directory = pas_compact_atomic_segregated_size_directory_ptr_load_remote(
             enumerator, &directory->next_for_heap))
        for_each_view(enumerator, &directory->base, size_directory_view_callback, context);
    
    return true;
}

static PAS_NEVER_INLINE void consider_allocator(pas_enumerator* enumerator, enumeration_context* context, pas_local_allocator* allocator)
{
    local_allocator_map_add_result add_result;
    const pas_segregated_page_config* page_config;
    uintptr_t page_boundary;
    local_allocator_node* allocator_node;
    
    if (!allocator->page_ish)
        return;

    if (verbose)
        pas_log("Have allocator %p in page_ish = %p\n", allocator, (void*)allocator->page_ish);
    
    PAS_ASSERT_WITH_EXTRA_DETAIL(!pas_local_allocator_config_kind_is_bitfit(allocator->config_kind), allocator->config_kind);
    
    page_config = pas_segregated_page_config_kind_get_config(
        pas_local_allocator_config_kind_get_segregated_page_config_kind(allocator->config_kind));
    
    page_boundary = pas_round_down_to_power_of_2(allocator->page_ish, page_config->base.page_size);
    
    add_result = local_allocator_map_add(
        &context->allocators, page_boundary, NULL, &enumerator->allocation_config);
    if (add_result.is_new_entry) {
        add_result.entry->page_boundary = page_boundary;
        add_result.entry->head = NULL;
    }
    
    allocator_node = pas_enumerator_allocate(enumerator, sizeof(local_allocator_node));
    allocator_node->next = add_result.entry->head;
    allocator_node->allocator = allocator;
    add_result.entry->head = allocator_node;
}

bool pas_enumerate_segregated_heaps(pas_enumerator* enumerator)
{
    pas_thread_local_cache_node** tlc_node_first_ptr;
    pas_thread_local_cache_node* tlc_node;
    pas_thread_local_cache_layout_segment** tlc_layout_first_segment_ptr;
    enumeration_context context;
    pas_baseline_allocator** baseline_allocator_table_ptr;
    size_t index;

    local_allocator_map_construct(&context.allocators);
    pas_ptr_hash_set_construct(&context.shared_page_directories);
    pas_ptr_hash_set_construct(&context.objects_in_deallocation_log);

    tlc_node_first_ptr = pas_enumerator_read(enumerator,
                                             enumerator->root->thread_local_cache_node_first,
                                             sizeof(pas_thread_local_cache_node*));
    if (!tlc_node_first_ptr)
        return false;

    tlc_layout_first_segment_ptr = pas_enumerator_read(enumerator, enumerator->root->thread_local_cache_layout_first_segment, sizeof(pas_thread_local_cache_layout_segment*));
    if (!tlc_layout_first_segment_ptr)
        return false;

    for (tlc_node = *tlc_node_first_ptr; tlc_node; tlc_node = tlc_node->next) {
        pas_thread_local_cache* tlc;
        unsigned index;
        
        tlc_node = pas_enumerator_read(enumerator,
                                       tlc_node,
                                       sizeof(pas_thread_local_cache_node));
        if (!tlc_node)
            return false;

        if (!tlc_node->cache)
            continue;

        tlc = pas_enumerator_read(enumerator,
                                  tlc_node->cache,
                                  pas_thread_local_cache_size_for_allocator_index_capacity(0));
        if (!tlc)
            return false;

        tlc = pas_enumerator_read(enumerator,
                                  tlc_node->cache,
                                  pas_thread_local_cache_size_for_allocator_index_capacity(
                                      tlc->allocator_index_capacity));
        if (!tlc)
            return false;

        for (index = PAS_DEALLOCATION_LOG_SIZE; index--;) {
            uintptr_t object =
                tlc->deallocation_log[index] & ~PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_MASK;
            if (object) {
                pas_ptr_hash_set_set(
                    &context.objects_in_deallocation_log, (void*)object,
                    NULL, &enumerator->allocation_config);
            }
        }

        if (*tlc_layout_first_segment_ptr) {
            pas_thread_local_cache_layout_segment** tlc_layout_segment_ptr;
            uintptr_t node_index = 0;
            pas_thread_local_cache_layout_node layout_node;
            pas_compact_atomic_thread_local_cache_layout_node* layout_node_ptr;

            tlc_layout_segment_ptr = tlc_layout_first_segment_ptr;

            layout_node_ptr = pas_enumerator_read(enumerator, &((*tlc_layout_segment_ptr)->nodes[node_index]), sizeof(pas_compact_atomic_thread_local_cache_layout_node));
            if (!layout_node_ptr)
                return false;
            layout_node = pas_compact_atomic_thread_local_cache_layout_node_load_remote(enumerator, layout_node_ptr);
            while (1) {
                bool has_allocator;
                unsigned allocator_index;

                if (!layout_node) {
                    tlc_layout_segment_ptr = pas_enumerator_read(enumerator, &((*tlc_layout_segment_ptr)->next), sizeof(pas_thread_local_cache_layout_segment*));
                    if (!tlc_layout_segment_ptr)
                        return false;
                    if (!*tlc_layout_segment_ptr)
                        break;
                    node_index = 0;
                    layout_node_ptr = pas_enumerator_read(enumerator, &((*tlc_layout_segment_ptr)->nodes[node_index]), sizeof(pas_compact_atomic_thread_local_cache_layout_node));
                    if (!layout_node_ptr)
                        return false;
                    layout_node = pas_compact_atomic_thread_local_cache_layout_node_load_remote(enumerator, layout_node_ptr);
                    if (!layout_node)
                        break;
                }
                
                if (pas_is_wrapped_segregated_size_directory(layout_node)) {
                    pas_segregated_size_directory* directory;

                    directory = pas_unwrap_segregated_size_directory(layout_node);

                    allocator_index = directory->allocator_index;
                    has_allocator = true;
                } else if (pas_is_wrapped_redundant_local_allocator_node(layout_node)) {
                    pas_redundant_local_allocator_node* redundant_node;

                    redundant_node = pas_unwrap_redundant_local_allocator_node(layout_node);

                    allocator_index = redundant_node->allocator_index;
                    has_allocator = true;
                } else {
                    PAS_ASSERT_WITH_DETAIL(pas_is_wrapped_local_view_cache_node(layout_node));
                    allocator_index = 0;
                    has_allocator = false;
                }

                if (has_allocator) {
                    pas_local_allocator* allocator;
                    
                    if (!allocator_index || allocator_index >= tlc->allocator_index_upper_bound)
                        break;

                    allocator = pas_thread_local_cache_get_local_allocator_direct(tlc, allocator_index);
                    
                    consider_allocator(enumerator, &context, allocator);
                }

                ++node_index;
                layout_node_ptr = pas_enumerator_read(enumerator, &((*tlc_layout_segment_ptr)->nodes[node_index]), sizeof(pas_compact_atomic_thread_local_cache_layout_node));
                if (!layout_node_ptr)
                    return false;
                layout_node = pas_compact_atomic_thread_local_cache_layout_node_load_remote(enumerator, layout_node_ptr);
            }
        }
    }

    baseline_allocator_table_ptr = pas_enumerator_read(enumerator,
                                                       enumerator->root->baseline_allocator_table,
                                                       sizeof(pas_baseline_allocator*));
    if (!baseline_allocator_table_ptr)
        return false;

    if (*baseline_allocator_table_ptr) {
        pas_baseline_allocator* baseline_allocator_table;
        size_t index;
        
        baseline_allocator_table = pas_enumerator_read(
            enumerator,
            *baseline_allocator_table_ptr,
            sizeof(pas_baseline_allocator) * enumerator->root->num_baseline_allocators);
        if (!baseline_allocator_table)
            return false;
        
        for (index = enumerator->root->num_baseline_allocators; index--;)
            consider_allocator(enumerator, &context, &baseline_allocator_table[index].u.allocator);
    }

    if (!pas_enumerator_for_each_heap(enumerator, collect_shared_page_directories_heap_callback, &context))
        return false;

    for (index = pas_ptr_hash_set_entry_index_end(&context.shared_page_directories); index--;) {
        pas_segregated_shared_page_directory* directory;
        shared_page_directory_view_data data;

        directory = *pas_ptr_hash_set_entry_at_index(&context.shared_page_directories, index);
        if (pas_ptr_hash_set_entry_is_empty_or_deleted(directory))
            continue;

        data.context = &context;
        data.directory = directory;
        if (!for_each_view(enumerator, &directory->base, shared_page_directory_view_callback, &data))
            return false;
    }
    
    if (!pas_enumerator_for_each_heap(enumerator, enumerate_segregated_heap_callback, &context))
        return false;

    return true;
}

#endif /* LIBPAS_ENABLED */
