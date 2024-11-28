/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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
#include "pas_segregated_size_directory.h"
#include "pas_segregated_view.h"
#include "pas_utility_heap.h"

#if PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY
double pas_local_allocator_refill_efficiency_sum = 0.;
double pas_local_allocator_refill_efficiency_n = 0.;
PAS_DEFINE_LOCK(pas_local_allocator_refill_efficiency);
#endif /* PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY */

void pas_local_allocator_construct(pas_local_allocator* allocator,
                                   pas_segregated_size_directory* directory)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_OTHER);

    pas_local_allocator_scavenger_data_construct(
        &allocator->scavenger_data, pas_local_allocator_allocator_kind);
    
    allocator->payload_end = 0;
    allocator->remaining = 0;
    
    allocator->object_size = directory->object_size;
    PAS_ASSERT(allocator->object_size);
    allocator->alignment_shift = directory->alignment_shift;

    if (!pas_segregated_size_directory_is_bitfit(directory))
        PAS_ASSERT(pas_is_aligned(allocator->object_size, pas_local_allocator_alignment(allocator)));
    
    allocator->page_ish = 0;
    allocator->current_offset = 0;
    allocator->end_offset = 0;
    
    allocator->view = pas_segregated_size_directory_as_view(directory);

    if (pas_segregated_size_directory_is_bitfit(directory)) {
        pas_bitfit_size_class* size_class;
        pas_bitfit_allocator* bitfit_allocator;

        size_class = pas_segregated_size_directory_get_bitfit_size_class(directory);
        
        allocator->config_kind = pas_local_allocator_config_kind_create_bitfit(
            pas_compact_bitfit_directory_ptr_load_non_null(&size_class->directory)->config_kind);

        bitfit_allocator = pas_local_allocator_get_bitfit(allocator);
        pas_bitfit_allocator_construct(bitfit_allocator, size_class);
    } else
        allocator->config_kind = pas_local_allocator_config_kind_create_normal(directory->base.page_config_kind);
    
    if (verbose) {
        pas_log("allocator %p has kind %s, ends at %p\n",
                allocator, pas_local_allocator_config_kind_get_string(allocator->config_kind),
                (char*)allocator + pas_segregated_size_directory_local_allocator_size(directory));
    }
    
    allocator->current_word_is_valid = false;
}

void pas_local_allocator_construct_unselected(pas_local_allocator* allocator)
{
    pas_zero_memory(allocator, PAS_LOCAL_ALLOCATOR_UNSELECTED_SIZE);

    pas_local_allocator_scavenger_data_construct(
        &allocator->scavenger_data, pas_local_allocator_allocator_kind);

    allocator->config_kind = pas_local_allocator_config_kind_unselected;
}

void pas_local_allocator_reset(pas_local_allocator* allocator)
{
    pas_segregated_size_directory* directory;

    directory = pas_segregated_view_get_size_directory(allocator->view);

    pas_local_allocator_reset_impl(allocator, directory, directory->base.page_config_kind);
}

void pas_local_allocator_move(pas_local_allocator* dst,
                              pas_local_allocator* src)
{
    size_t size;
    pas_segregated_size_directory* directory;
    pas_segregated_partial_view* partial_view;
    pas_segregated_shared_view* shared_view;

    pas_heap_lock_assert_held(); /* Needed to modify a lenient_compact_ptr. */

    PAS_ASSERT(!dst->scavenger_data.is_in_use);
    PAS_ASSERT(!src->scavenger_data.is_in_use);

    directory = pas_segregated_view_get_size_directory(src->view);
    
    size = pas_segregated_size_directory_local_allocator_size(directory);
    
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
    if (pas_lenient_compact_unsigned_ptr_load(&partial_view->alloc_bits) == (unsigned*)src->bits)
        pas_lenient_compact_unsigned_ptr_store(&partial_view->alloc_bits, (unsigned*)dst->bits);
    pas_lock_unlock(&shared_view->ownership_lock);
}

static bool stop_impl(
    pas_local_allocator* allocator,
    pas_lock_lock_mode page_lock_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_OTHER);
    
    pas_segregated_view view;
    pas_segregated_page* page;
    pas_segregated_size_directory* directory;
    pas_segregated_page_config page_config;
    pas_lock* held_lock;

    if (verbose)
        pas_log("stopping allocator %p\n", allocator);

    if (pas_local_allocator_has_bitfit(allocator)) {
        PAS_ASSERT(!allocator->page_ish);
        pas_bitfit_allocator_stop(pas_local_allocator_get_bitfit(allocator));
        return true;
    }

    /* This also stops us from doing anything when we've been decommitted. */
    if (!allocator->page_ish)
        return true;

    held_lock = NULL;

    view = allocator->view;
    directory = pas_segregated_view_get_size_directory(view);

    page_config = *pas_segregated_page_config_kind_get_config(directory->base.page_config_kind);

    page = pas_segregated_page_for_boundary((void*)pas_local_allocator_page_boundary(
                                                allocator, page_config),
                                            page_config);
    
    if (pas_segregated_view_is_size_directory(view)) {
        view = page->owner;
        PAS_ASSERT(pas_segregated_view_is_some_exclusive(view));
    } else
        PAS_ASSERT(pas_segregated_view_is_partial(view));
    
    if (!pas_segregated_page_switch_lock_with_mode(page, &held_lock, page_lock_mode, page_config))
        return false;

    if (!pas_segregated_page_config_is_utility(page_config))
        PAS_ASSERT(held_lock);
    
    page_config.specialized_local_allocator_return_memory_to_page(
        allocator, view, page, directory, heap_lock_hold_mode);
    
    pas_race_test_hook(pas_race_test_hook_local_allocator_stop_before_did_stop_allocating);
    
    pas_segregated_view_did_stop_allocating(view, page, page_config);
    
    pas_race_test_hook(pas_race_test_hook_local_allocator_stop_before_unlock);
    
    pas_local_allocator_reset(allocator);
    
    pas_lock_switch(&held_lock, NULL);
    
    return true;
}

bool pas_local_allocator_stop(
    pas_local_allocator* allocator,
    pas_lock_lock_mode page_lock_mode,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_OTHER);
    
    bool result;
    bool is_in_use;

    is_in_use = allocator->scavenger_data.is_in_use;
    if (is_in_use) {
        pas_log("allocator = %p\n", allocator);
        pas_log("allocator->scavenger_data.kind = %s\n",
                pas_local_allocator_kind_get_string(allocator->scavenger_data.kind));
        pas_log("allocator->scavenger_data.is_in_use = %s\n",
                allocator->scavenger_data.is_in_use ? "yes" : "no");
        pas_log("at time of assert: allocator->scavenger_data.is_in_use = %s\n",
                is_in_use ? "yes" : "no");
        PAS_ASSERT(!"Should not be reached");
    }

    /* Doing this check before setting is_in_use guards against situations where calling stop would
       recommit a decommitted allocator.
    
       This is a pretty weak optimization since in my experiments, reads cause commits. Web Template
       Framework! */
    if (pas_local_allocator_scavenger_data_is_stopped(&allocator->scavenger_data))
        return true;

    allocator->scavenger_data.is_in_use = true;
    pas_compiler_fence();

    /* The scavenger thread may race with the client thread on calling pas_local_allocator_stop.
       The scavenger will first suspend the client thread before checking the is_in_use flag.
       If is_in_use is set, the scavenger will not call pas_local_allocator_scavenger_data_stop, and move on.
       If is_in_use is not set, the scavenger will call pas_local_allocator_scavenger_data_stop,
       which in turn calls pas_local_allocator_stop to stop the local_allocator.

       Normally, if the scavenger has already completed its call to pas_local_allocator_stop
       before the client calls it, pas_local_allocator_stop will just return early for the client.
       This is because pas_local_allocator_stop first check pas_local_allocator_scavenger_data_is_stopped
       before doing the work to stop the local_allocator.

       However, if the client thread is already in the process of executing pas_local_allocator_stop,
       gets past the pas_local_allocator_scavenger_data_is_stopped check, and then, gets suspended by
       the scavenger before setting the is_in_use flag, the scavenger can stop the local_allocator
       after the client already checked and thinks it is not stopped yet. When the client thread
       resumes from suspension, it will be unhappy to find that the local_allocator is already
       stopped, and corrupt data structures.

       The fix is to re-check pas_local_allocator_scavenger_data_is_stopped after setting the
       is_in_use flag.

       If the re-check shows that the local_allocator is already stopped, then pas_local_allocator_stop
       can return early like the first pas_local_allocator_scavenger_data_is_stopped check. This is
       fine to do because the caller expects this as a possible outcome.

       Before returning early due to the re-check, we also need to clear the is_in_use flag.
       The is_in_use flag is meant to be set like with an RAII scope. Hence, it is correct and
       required that we also clear it here before we return.
    */
    if (pas_local_allocator_scavenger_data_is_stopped(&allocator->scavenger_data)) {
        allocator->scavenger_data.is_in_use = false;
        return true;
    }

    result = stop_impl(allocator, page_lock_mode, heap_lock_hold_mode);

    if (result) {
        if (verbose)
            pas_log("Did stop allocator %p\n", allocator);
        allocator->scavenger_data.should_stop_count = 0;
        allocator->scavenger_data.kind = pas_local_allocator_stopped_allocator_kind;
    }
    
    pas_compiler_fence();
    allocator->scavenger_data.is_in_use = false;

    return result;
}

bool pas_local_allocator_scavenge(pas_local_allocator* allocator,
                                  pas_allocator_scavenge_action action)
{
    PAS_ASSERT(action != pas_allocator_scavenge_no_action);
    
    if (pas_local_allocator_is_null(allocator))
        return false;
    
    if (action == pas_allocator_scavenge_request_stop_action) {
        if (allocator->scavenger_data.dirty) {
            allocator->scavenger_data.dirty = false;
            return true;
        }
    }

    pas_local_allocator_stop(allocator, pas_lock_lock_mode_lock, pas_lock_is_not_held);
    return false;
}

#endif /* LIBPAS_ENABLED */
