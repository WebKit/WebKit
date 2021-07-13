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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_local_allocator.h"

#include "pas_heap_for_config.h"
#include "pas_local_allocator_inlines.h"
#include "pas_race_test_hooks.h"
#include "pas_segregated_page.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_segregated_view_inlines.h"
#include "pas_utility_heap.h"

uint8_t pas_local_allocator_should_stop_count_for_suspend = 5;

void pas_local_allocator_construct(pas_local_allocator* allocator,
                                   pas_segregated_global_size_directory* directory)
{
    static const bool verbose = false;
    
    allocator->payload_end = 0;
    allocator->remaining = 0;
    
    allocator->object_size = directory->object_size;
    PAS_ASSERT(allocator->object_size);
    allocator->alignment_shift = directory->alignment_shift;
    PAS_ASSERT(pas_is_aligned(allocator->object_size, pas_local_allocator_alignment(allocator)));
    
    allocator->page_ish = 0;
    allocator->current_offset = 0;
    allocator->end_offset = 0;
    
    allocator->view = pas_segregated_global_size_directory_as_view(directory);
    allocator->config_kind = pas_local_allocator_config_kind_create_normal(
        directory->base.page_config_kind);
    if (verbose) {
        pas_log("allocator %p has kind %s\n",
                allocator, pas_local_allocator_config_kind_get_string(allocator->config_kind), "\n");
    }
    
    allocator->is_in_use = false;
    allocator->should_stop_count = 0;
    allocator->dirty = false;
    allocator->current_word_is_valid = false;
}

void pas_local_allocator_destruct(pas_local_allocator* allocator)
{
    PAS_UNUSED_PARAM(allocator);
}

void pas_local_allocator_reset(pas_local_allocator* allocator)
{
    pas_segregated_global_size_directory* directory;

    directory = pas_segregated_view_get_global_size_directory(allocator->view);

    pas_local_allocator_reset_impl(allocator, directory, directory->base.page_config_kind);
}

void pas_local_allocator_move(pas_local_allocator* dst,
                              pas_local_allocator* src)
{
    size_t size;
    pas_segregated_global_size_directory* directory;
    pas_segregated_partial_view* partial_view;
    pas_segregated_shared_view* shared_view;

    directory = pas_segregated_view_get_global_size_directory(src->view);
    
    size = pas_segregated_global_size_directory_local_allocator_size(directory);
    
    memcpy(dst, src, size);

    if (!pas_local_allocator_config_kind_is_primordial_partial(dst->config_kind))
        return;

    partial_view = pas_segregated_view_get_partial(dst->view);
    shared_view = pas_compact_segregated_shared_view_ptr_load(&partial_view->shared_view);

    /* Holding the ownership lock during this time ensures that
       pas_segregated_view_for_each_live_object works. We grab that lock here because that function
       is the only client of partial_view->alloc_bits being right during primordial mode, and it
       happens to hold the ownership lock. */
    pas_lock_lock(&shared_view->ownership_lock);
    if (pas_compact_tagged_unsigned_ptr_load(&partial_view->alloc_bits) == (unsigned*)src->bits)
        pas_compact_tagged_unsigned_ptr_store(&partial_view->alloc_bits, (unsigned*)dst->bits);
    pas_lock_unlock(&shared_view->ownership_lock);
}

bool pas_local_allocator_refill_with_bitfit(
    pas_local_allocator* allocator,
    pas_allocator_counts* counts)
{
    pas_segregated_page_config fake_null_config;
    bool skip_bitfit;

    fake_null_config.base.is_enabled = false;
    fake_null_config.kind = pas_segregated_page_config_kind_null;
    skip_bitfit = false;

    return pas_local_allocator_refill_with_known_config(
        allocator, counts, fake_null_config, skip_bitfit);
}

void pas_local_allocator_finish_refill_with_bitfit(
    pas_local_allocator* allocator,
    pas_segregated_global_size_directory* size_directory,
    pas_magazine* magazine)
{
    static const bool verbose = false;
    
    pas_bitfit_allocator* bitfit;
    pas_bitfit_global_size_class* global_size_class;
    pas_bitfit_size_class* size_class;
    pas_bitfit_view* bitfit_view;
    pas_bitfit_page_config_kind bitfit_kind;
    pas_bitfit_page_config* bitfit_config;

    if (verbose) {
        pas_log("local allocator %p for size directory %p doing bitfit allocation.\n",
                allocator, size_directory);
    }
    
    global_size_class = pas_compact_atomic_bitfit_global_size_class_ptr_load_non_null(
        &size_directory->bitfit_size_class);
    
    bitfit_kind = pas_compact_bitfit_directory_ptr_load_non_null(
        &global_size_class->base.directory)->config_kind;
    
    allocator->config_kind = pas_local_allocator_config_kind_create_bitfit(bitfit_kind);
    
    bitfit = pas_local_allocator_get_bitfit(allocator);
    
    size_class = pas_bitfit_global_size_class_select_for_magazine(global_size_class, magazine);
    
    PAS_ASSERT(size_class);
    
    bitfit_config = pas_bitfit_page_config_kind_get_config(bitfit_kind);
    
    bitfit_view = pas_bitfit_size_class_get_first_free_view(
        size_class, global_size_class, bitfit_config);
    
    PAS_ASSERT(bitfit_view);
    
    bitfit->global_size_class = global_size_class;
    bitfit->size_class = size_class;
    bitfit->view = bitfit_view;
}

static bool stop_impl(
    pas_local_allocator* allocator,
    pas_lock_lock_mode page_lock_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    pas_segregated_view view;
    pas_segregated_page* page;
    pas_segregated_global_size_directory* directory;
    pas_segregated_page_config page_config;
    pas_lock* held_lock;

    if (verbose)
        pas_log("stopping allocator %p\n", allocator);

    if (!allocator->page_ish)
        return true;

    view = allocator->view;
    directory = pas_segregated_view_get_global_size_directory(view);

    page_config = *pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);

    page = pas_segregated_page_for_boundary((void*)pas_local_allocator_page_boundary(
                                                allocator, page_config),
                                            page_config);
    
    if (pas_segregated_view_is_global_size_directory(view)) {
        view = page->owner;
        PAS_ASSERT(pas_segregated_view_is_exclusive_ish(view));
    } else
        PAS_ASSERT(pas_segregated_view_is_partial(view));

    held_lock = NULL;
    if (!pas_segregated_page_switch_lock_with_mode(page, &held_lock, page_lock_mode, page_config))
        return false;
    
    page_config.specialized_local_allocator_return_memory_to_page(
        allocator, view, page, directory, heap_lock_hold_mode);

    pas_race_test_hook(pas_race_test_hook_local_allocator_stop_before_did_stop_allocating);
    
    pas_segregated_view_did_stop_allocating(view, page, page_config);
    
    pas_race_test_hook(pas_race_test_hook_local_allocator_stop_before_unlock);

    pas_lock_switch(&held_lock, NULL);
    
    pas_local_allocator_reset(allocator);
    
    return true;
}

/* FIXME: This thing is meaningless with heap_lock_hold_mode = pas_lock_is_not_held unless you
   also set is_in_use to true. Seems like maybe this should set it for you? */
bool pas_local_allocator_stop(
    pas_local_allocator* allocator,
    pas_lock_lock_mode page_lock_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = false;
    
    bool result;

    PAS_ASSERT(!allocator->is_in_use);

    allocator->is_in_use = true;
    pas_compiler_fence();

    result = stop_impl(allocator, page_lock_mode, heap_lock_hold_mode);

    if (result) {
        if (verbose)
            pas_log("Did stop allocator %p\n", allocator);
        allocator->should_stop_count = 0;
    }
    
    pas_compiler_fence();
    allocator->is_in_use = false;

    return result;
}

bool pas_local_allocator_scavenge(pas_local_allocator* allocator,
                                  pas_allocator_scavenge_action action)
{
    PAS_ASSERT(action != pas_allocator_scavenge_no_action);
    
    if (pas_local_allocator_is_null(allocator))
        return false;
    
    if (action == pas_allocator_scavenge_request_stop_action) {
        if (allocator->dirty) {
            allocator->dirty = false;
            return true;
        }
    }

    pas_local_allocator_stop(allocator, pas_lock_lock_mode_lock, pas_lock_is_not_held);
    return false;
}

#endif /* LIBPAS_ENABLED */
