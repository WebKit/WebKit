/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_TRY_REALLOCATE_H
#define PAS_TRY_REALLOCATE_H

#include "pas_bitfit_global_directory.h"
#include "pas_deallocate.h"
#include "pas_large_map.h"
#include "pas_reallocate_free_mode.h"
#include "pas_reallocate_heap_teleport_rule.h"
#include "pas_try_allocate.h"
#include "pas_try_allocate_array.h"
#include "pas_try_allocate_intrinsic_primitive.h"
#include "pas_try_allocate_primitive.h"

PAS_BEGIN_EXTERN_C;

typedef pas_typed_allocation_result
(*pas_try_reallocate_allocate_callback)(pas_heap* heap,
                                        size_t new_count,
                                        void* arg);

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_allocate_for_reallocate_and_copy(
    pas_heap* source_heap,
    pas_heap* target_heap,
    void* old_ptr,
    size_t old_size,
    size_t new_count,
    pas_reallocate_heap_teleport_rule teleport_rule,
    pas_try_reallocate_allocate_callback allocate_callback,
    void* allocate_callback_arg)
{
    pas_typed_allocation_result result;

    /* If heaps are based on some rigorous notion of type, then we should disallow heap
       teleporting since that's just type confusion.
       
       But if heaps are based on something sloppy like allocation site then we have no choice
       but to allow heap teleporting.
    
       We could have made this part of the heap_config, but making it a separate parameter means
       we could hypothetically have a heap_config that allows both. */
    switch (teleport_rule) {
    case pas_reallocate_allow_heap_teleport:
        /* The only danger here is that new_count and old_count were in
           different units. But that should not matter:
        
           - We only care about the old size, not the old count. We get a
             size from the allocator.
        
           - We end up computing the new size using the new count and the
             new heap.
        
           Then we take the min of those two. No big deal if the old count
           and new count had different units. */
        break;
        
    case pas_reallocate_disallow_heap_teleport: {
        if (source_heap != target_heap) {
            pas_reallocation_did_fail(
                "Attempting to teleport heaps",
                source_heap, target_heap, old_ptr, old_size, new_count);
        }
        break;
    } }
    
    result = allocate_callback(target_heap, new_count, allocate_callback_arg);
    
    if (result.ptr) {
        /* FIXME: We need to actually be rounding down the old size to our
           type size. To do that we'd want the typed allocation result to
           give us the type size. For primitive allocations, it would
           statically give us 1.
        
           Also: this only matters for thingy because of fat pointers,
           and there it might not even matter that much since fat pointers
           are aligned (I think). */
        
        size_t copy_size = PAS_MIN(result.size, old_size);
        memcpy(result.ptr, old_ptr, copy_size);
    }
    
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate_table_segregated_case(pas_page_base* page_base,
                                         uintptr_t begin,
                                         pas_heap* heap,
                                         size_t new_count,
                                         pas_segregated_page_config segregated_config,
                                         pas_reallocate_heap_teleport_rule teleport_rule,
                                         pas_reallocate_free_mode free_mode,
                                         pas_try_reallocate_allocate_callback allocate_callback,
                                         void* allocate_callback_arg)
{
    size_t old_size;
    pas_typed_allocation_result result;
    pas_heap* old_heap;
    pas_segregated_page* page;
    
    page = pas_page_base_get_segregated(page_base);
    
    switch (teleport_rule) {
    case pas_reallocate_allow_heap_teleport:
        old_size = pas_segregated_page_get_object_size_for_address_in_page(
            page, begin, segregated_config);
        old_heap = NULL;
        break;
        
    case pas_reallocate_disallow_heap_teleport: {
        pas_segregated_global_size_directory* directory;
        directory = pas_segregated_page_get_directory_for_address_in_page(
            page, begin, segregated_config);
        old_size = directory->object_size;
        old_heap = pas_heap_for_segregated_heap(directory->heap);
        break;
    } }
    
    result = pas_try_allocate_for_reallocate_and_copy(
        old_heap, heap, (void*)begin, old_size, new_count, teleport_rule,
        allocate_callback, allocate_callback_arg);
    if (result.ptr || free_mode == pas_reallocate_free_always)
        pas_deallocate_known_segregated((void*)begin, segregated_config);
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate_table_bitfit_case(pas_page_base* page_base,
                                     uintptr_t begin,
                                     pas_heap* heap,
                                     size_t new_count,
                                     pas_bitfit_page_config bitfit_config,
                                     pas_reallocate_heap_teleport_rule teleport_rule,
                                     pas_reallocate_free_mode free_mode,
                                     pas_try_reallocate_allocate_callback allocate_callback,
                                     void* allocate_callback_arg)
{
    size_t old_size;
    pas_typed_allocation_result result;
    pas_bitfit_page* page;
    pas_heap* old_heap;
    
    page = pas_page_base_get_bitfit(page_base);
    old_size = bitfit_config.specialized_page_get_allocation_size_with_page(page, begin);
    
    switch (teleport_rule) {
    case pas_reallocate_allow_heap_teleport:
        old_heap = NULL;
        break;
        
    case pas_reallocate_disallow_heap_teleport:
        old_heap = pas_heap_for_segregated_heap(
            pas_compact_bitfit_global_directory_ptr_load_non_null(
                &pas_compact_atomic_bitfit_view_ptr_load_non_null(
                    &page->owner)->global_directory)->heap);
        break;
    }
    
    result = pas_try_allocate_for_reallocate_and_copy(
        old_heap, heap, (void*)begin, old_size, new_count, teleport_rule,
        allocate_callback, allocate_callback_arg);
    if (result.ptr || free_mode == pas_reallocate_free_always)
        bitfit_config.specialized_page_deallocate_with_page(page, begin);
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate(void* old_ptr,
                   pas_heap* heap,
                   size_t new_count,
                   pas_heap_config config,
                   pas_reallocate_heap_teleport_rule teleport_rule,
                   pas_reallocate_free_mode free_mode,
                   pas_try_reallocate_allocate_callback allocate_callback,
                   void* allocate_callback_arg)
{
    uintptr_t begin;
    
    begin = (uintptr_t)old_ptr;

    switch (config.fast_megapage_kind_func(begin)) {
    case pas_small_segregated_fast_megapage_kind: {
        size_t old_size;
        pas_typed_allocation_result result;
        pas_heap* old_heap;

        switch (teleport_rule) {
        case pas_reallocate_allow_heap_teleport:
            old_size = pas_segregated_page_get_object_size_for_address_and_page_config(
                begin, config.small_segregated_config);
            old_heap = NULL;
            break;

        case pas_reallocate_disallow_heap_teleport: {
            pas_segregated_global_size_directory* directory;
            directory = pas_segregated_page_get_directory_for_address_and_page_config(
                begin, config.small_segregated_config);
            old_size = directory->object_size;
            old_heap = pas_heap_for_segregated_heap(directory->heap);
            break;
        } }

        result = pas_try_allocate_for_reallocate_and_copy(
            old_heap, heap, old_ptr, old_size, new_count, teleport_rule,
            allocate_callback, allocate_callback_arg);
        if (result.ptr || free_mode == pas_reallocate_free_always)
            pas_deallocate_known_segregated(old_ptr, config.small_segregated_config);
        return result;
    }
    case pas_small_bitfit_fast_megapage_kind: {
        size_t old_size;
        pas_typed_allocation_result result;
        pas_bitfit_page* page;
        pas_heap* old_heap;

        page = pas_bitfit_page_for_address_and_page_config(begin, config.small_bitfit_config);
        old_size = config.small_bitfit_config.specialized_page_get_allocation_size_with_page(
            page, begin);

        switch (teleport_rule) {
        case pas_reallocate_allow_heap_teleport:
            old_heap = NULL;
            break;

        case pas_reallocate_disallow_heap_teleport:
            old_heap = pas_heap_for_segregated_heap(
                pas_compact_bitfit_global_directory_ptr_load_non_null(
                    &pas_compact_atomic_bitfit_view_ptr_load_non_null(
                        &page->owner)->global_directory)->heap);
            break;
        }
        
        result = pas_try_allocate_for_reallocate_and_copy(
            old_heap, heap, old_ptr, old_size, new_count, teleport_rule,
            allocate_callback, allocate_callback_arg);
        if (result.ptr || free_mode == pas_reallocate_free_always)
            config.small_bitfit_config.specialized_page_deallocate_with_page(page, begin);
        return result;
    }
    case pas_not_a_fast_megapage_kind: {
        pas_heap* source_heap;
        size_t old_size;
        pas_large_map_entry entry;
        pas_typed_allocation_result result;
        pas_page_base* page_base;

        page_base = config.page_header_func(begin);
        if (page_base) {
            switch (pas_page_base_get_kind(page_base)) {
            case pas_small_segregated_page_kind:
                PAS_ASSERT(!config.small_segregated_is_in_megapage);
                return pas_try_reallocate_table_segregated_case(
                    page_base, begin, heap, new_count, config.small_segregated_config,
                    teleport_rule, free_mode, allocate_callback, allocate_callback_arg);

            case pas_small_bitfit_page_kind:
                PAS_ASSERT(!config.small_bitfit_is_in_megapage);
                return pas_try_reallocate_table_bitfit_case(
                    page_base, begin, heap, new_count, config.small_bitfit_config,
                    teleport_rule, free_mode, allocate_callback, allocate_callback_arg);

            case pas_medium_segregated_page_kind:
                return pas_try_reallocate_table_segregated_case(
                    page_base, begin, heap, new_count, config.medium_segregated_config,
                    teleport_rule, free_mode, allocate_callback, allocate_callback_arg);

            case pas_medium_bitfit_page_kind:
                return pas_try_reallocate_table_bitfit_case(
                    page_base, begin, heap, new_count, config.medium_bitfit_config,
                    teleport_rule, free_mode, allocate_callback, allocate_callback_arg);

            case pas_marge_bitfit_page_kind:
                return pas_try_reallocate_table_bitfit_case(
                    page_base, begin, heap, new_count, config.marge_bitfit_config,
                    teleport_rule, free_mode, allocate_callback, allocate_callback_arg);
            }
            
            PAS_ASSERT(!"Wrong page kind");
            return pas_typed_allocation_result_create_empty();
        }

        if (!begin)
            return allocate_callback(heap, new_count, allocate_callback_arg);

        if (PAS_UNLIKELY(pas_debug_heap_is_enabled())) {
            void* raw_result;
            size_t size;
            bool did_overflow;
            
            PAS_ASSERT(free_mode == pas_reallocate_free_if_successful);

            did_overflow = __builtin_mul_overflow(new_count, config.get_type_size(heap->type), &size);
            PAS_ASSERT(!did_overflow);
            
            raw_result = pas_debug_heap_realloc(old_ptr, size);

            result = pas_typed_allocation_result_create_empty();

            if (raw_result) {
                result.type = heap->type;
                result.ptr = raw_result;
                result.size = size;
                result.did_succeed = true;
            }

            return result;
        }

        pas_heap_lock_lock();
        
        entry = pas_large_map_find(begin);
        
        if (pas_large_map_entry_is_empty(entry)) {
            pas_reallocation_did_fail(
                "Source object not allocated",
                NULL, heap, old_ptr, 0, new_count);
        }
        
        PAS_ASSERT(entry.begin == begin);
        PAS_ASSERT(entry.end > begin);
        PAS_ASSERT(entry.heap);
        
        old_size = entry.end - begin;
        source_heap = pas_heap_for_large_heap(entry.heap);
        pas_heap_lock_unlock();
        
        result = pas_try_allocate_for_reallocate_and_copy(
            source_heap, heap, old_ptr, old_size, new_count, teleport_rule,
            allocate_callback, allocate_callback_arg);
        
        if (result.ptr || free_mode == pas_reallocate_free_always)
            pas_deallocate_known_large(old_ptr, config.config_ptr);
        
        return result;
    } }
    
    PAS_ASSERT(!"Should never be reached");
}

typedef struct {
    pas_try_allocate_intrinsic_primitive_for_realloc try_allocate_intrinsic_primitive;
} pas_try_reallocate_intrinsic_primitive_allocate_data;

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate_intrinsic_primitive_allocate_callback(
    pas_heap* heap,
    size_t new_count,
    void* arg)
{
    pas_try_reallocate_intrinsic_primitive_allocate_data* data;

    PAS_UNUSED_PARAM(heap);
    
    data = (pas_try_reallocate_intrinsic_primitive_allocate_data*)arg;
    
    return pas_typed_allocation_result_create_with_intrinsic_allocation_result(
        data->try_allocate_intrinsic_primitive(new_count),
        NULL,
        new_count);
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_try_reallocate_intrinsic_primitive(
    void* old_ptr,
    pas_heap* heap,
    size_t new_size,
    pas_heap_config config,
    pas_try_allocate_intrinsic_primitive_for_realloc try_allocate_intrinsic_primitive,
    pas_reallocate_heap_teleport_rule teleport_rule,
    pas_reallocate_free_mode free_mode)
{
    pas_try_reallocate_intrinsic_primitive_allocate_data data;
    
    data.try_allocate_intrinsic_primitive = try_allocate_intrinsic_primitive;
    
    return pas_typed_allocation_result_as_intrinsic_allocation_result(
        pas_try_reallocate(
            old_ptr,
            heap,
            new_size,
            config,
            teleport_rule,
            free_mode,
            pas_try_reallocate_intrinsic_primitive_allocate_callback,
            &data));
}

typedef struct {
    pas_try_allocate try_allocate;
    pas_heap_ref* heap_ref;
} pas_try_reallocate_single_allocate_data;

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate_single_allocate_callback(
    pas_heap* heap,
    size_t new_count,
    void* arg)
{
    pas_try_reallocate_single_allocate_data* data;

    PAS_UNUSED_PARAM(heap);
    
    data = (pas_try_reallocate_single_allocate_data*)arg;
    
    PAS_ASSERT(new_count == 1);
    
    return data->try_allocate(data->heap_ref);
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate_single(
    void* old_ptr,
    pas_heap_ref* heap_ref,
    pas_heap_config config,
    pas_try_allocate try_allocate,
    pas_heap_runtime_config* runtime_config,
    pas_reallocate_heap_teleport_rule teleport_rule,
    pas_reallocate_free_mode free_mode)
{
    pas_try_reallocate_single_allocate_data data;
    
    data.heap_ref = heap_ref;
    data.try_allocate = try_allocate;
    
    return pas_try_reallocate(
        old_ptr,
        pas_ensure_heap(heap_ref,
                        pas_normal_heap_ref_kind,
                        config.config_ptr,
                        runtime_config),
        1,
        config,
        teleport_rule,
        free_mode,
        pas_try_reallocate_single_allocate_callback,
        &data);
}

typedef struct {
    pas_try_allocate_array_for_realloc try_allocate_array;
    pas_heap_ref* heap_ref;
} pas_try_reallocate_array_allocate_data;

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate_array_allocate_callback(
    pas_heap* heap,
    size_t new_count,
    void* arg)
{
    pas_try_reallocate_array_allocate_data* data;
    pas_typed_allocation_result result;
    
    data = (pas_try_reallocate_array_allocate_data*)arg;
    
    result = data->try_allocate_array(data->heap_ref, heap, new_count);
    
    return result;
}

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate_array(
    void* old_ptr,
    pas_heap_ref* heap_ref,
    size_t new_count,
    pas_heap_config config,
    pas_try_allocate_array_for_realloc try_allocate_array,
    pas_heap_runtime_config* runtime_config,
    pas_reallocate_heap_teleport_rule teleport_rule,
    pas_reallocate_free_mode free_mode)
{
    pas_try_reallocate_array_allocate_data data;
    
    data.try_allocate_array = try_allocate_array;
    data.heap_ref = heap_ref;
    
    return pas_try_reallocate(
        old_ptr,
        pas_ensure_heap(heap_ref,
                        pas_normal_heap_ref_kind,
                        config.config_ptr,
                        runtime_config),
        new_count,
        config,
        teleport_rule,
        free_mode,
        pas_try_reallocate_array_allocate_callback,
        &data);
}

typedef struct {
    pas_primitive_heap_ref* heap_ref;
    pas_try_allocate_primitive_for_realloc try_allocate_primitive;
} pas_try_reallocate_primitive_allocate_data;

static PAS_ALWAYS_INLINE pas_typed_allocation_result
pas_try_reallocate_primitive_allocate_callback(
    pas_heap* heap,
    size_t new_count,
    void* arg)
{
    pas_try_reallocate_primitive_allocate_data* data;

    PAS_UNUSED_PARAM(heap);
    
    data = (pas_try_reallocate_primitive_allocate_data*)arg;
    
    return pas_typed_allocation_result_create_with_intrinsic_allocation_result(
        data->try_allocate_primitive(data->heap_ref, new_count),
        NULL,
        new_count);
}

static PAS_ALWAYS_INLINE pas_intrinsic_allocation_result
pas_try_reallocate_primitive(
    void* old_ptr,
    pas_primitive_heap_ref* heap_ref,
    size_t new_size,
    pas_heap_config config,
    pas_try_allocate_primitive_for_realloc try_allocate_primitive,
    pas_heap_runtime_config* runtime_config,
    pas_reallocate_heap_teleport_rule teleport_rule,
    pas_reallocate_free_mode free_mode)
{
    pas_try_reallocate_primitive_allocate_data data;
    
    data.heap_ref = heap_ref;
    data.try_allocate_primitive = try_allocate_primitive;
    
    return pas_typed_allocation_result_as_intrinsic_allocation_result(
        pas_try_reallocate(
            old_ptr,
            pas_ensure_heap(&heap_ref->base,
                            pas_primitive_heap_ref_kind,
                            config.config_ptr,
                            runtime_config),
            new_size,
            config,
            teleport_rule,
            free_mode,
            pas_try_reallocate_primitive_allocate_callback,
            &data));
}

PAS_END_EXTERN_C;

#endif /* PAS_TRY_REALLOCATE_H */

