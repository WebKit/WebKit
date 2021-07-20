/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#include "pas_segregated_view.h"

#include "pas_full_alloc_bits_inlines.h"
#include "pas_segregated_biasing_directory.h"
#include "pas_segregated_biasing_view.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_partial_view.h"
#include "pas_segregated_shared_handle.h"
#include "pas_segregated_shared_page_directory.h"
#include "pas_segregated_shared_view.h"
#include "pas_segregated_size_directory.h"
#include "pas_shared_handle_or_page_boundary_inlines.h"

pas_segregated_directory* pas_segregated_view_get_size_directory_slow(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return &pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(view)->directory)->base;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return &pas_compact_segregated_biasing_directory_ptr_load_non_null(
            &pas_segregated_view_get_biasing(view)->directory)->base;
    case pas_segregated_partial_view_kind:
        return &pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_partial(view)->directory)->base;
    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

pas_segregated_global_size_directory*
pas_segregated_view_get_global_size_directory_slow(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(view)->directory);
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &pas_compact_segregated_biasing_directory_ptr_load_non_null(
                &pas_segregated_view_get_biasing(view)->directory)->parent_global);
    case pas_segregated_partial_view_kind:
        return pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_partial(view)->directory);
    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

pas_segregated_page_config_kind pas_segregated_view_get_page_config_kind(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(view)->directory)->base.page_config_kind;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return pas_compact_segregated_biasing_directory_ptr_load_non_null(
            &pas_segregated_view_get_biasing(view)->directory)->base.page_config_kind;
    case pas_segregated_shared_handle_kind:
        return pas_segregated_view_get_shared_handle(view)->directory->base.page_config_kind;
    case pas_segregated_shared_view_kind:
        return pas_unwrap_shared_handle_no_liveness_checks(
            pas_segregated_view_get_shared(view)->shared_handle_or_page_boundary)
            ->directory->base.page_config_kind;
    case pas_segregated_partial_view_kind:
        return pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_partial(view)->directory)->base.page_config_kind;
    case pas_segregated_global_size_directory_view_kind:
        return pas_segregated_view_get_global_size_directory(view)->base.page_config_kind;
    default:
        PAS_ASSERT(!"Should not be reached");
        return pas_segregated_page_config_kind_null;
    }
}

pas_segregated_page_config* pas_segregated_view_get_page_config(pas_segregated_view view)
{
    return pas_segregated_page_config_kind_get_config(
        pas_segregated_view_get_page_config_kind(view));
}

size_t pas_segregated_view_get_index(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return ((pas_segregated_exclusive_view*)pas_segregated_view_get_ptr(view))->index;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return ((pas_segregated_biasing_view*)pas_segregated_view_get_ptr(view))->index;
    case pas_segregated_shared_view_kind:
        return ((pas_segregated_shared_view*)pas_segregated_view_get_ptr(view))->index;
    case pas_segregated_shared_handle_kind:
        return pas_compact_segregated_shared_view_ptr_load_non_null(
            &((pas_segregated_shared_handle*)pas_segregated_view_get_ptr(view))->shared_view)->index;
    case pas_segregated_partial_view_kind:
        return ((pas_segregated_partial_view*)pas_segregated_view_get_ptr(view))->index;
    default:
        PAS_ASSERT(!"Should not be reached");
        return 0;
    }
}

void* pas_segregated_view_get_page_boundary(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return ((pas_segregated_exclusive_view*)pas_segregated_view_get_ptr(view))->page_boundary;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind: {
        pas_segregated_exclusive_view* exclusive;
        exclusive = pas_compact_atomic_segregated_exclusive_view_ptr_load(
            &((pas_segregated_biasing_view*)pas_segregated_view_get_ptr(view))->exclusive);
        return exclusive ? exclusive->page_boundary : NULL;
    }
    case pas_segregated_shared_view_kind:
        return pas_shared_handle_or_page_boundary_get_page_boundary_no_liveness_checks(
            ((pas_segregated_shared_view*)
             pas_segregated_view_get_ptr(view))->shared_handle_or_page_boundary);
    case pas_segregated_shared_handle_kind:
        return ((pas_segregated_shared_handle*)pas_segregated_view_get_ptr(view))->page_boundary;
    case pas_segregated_partial_view_kind: {
        pas_segregated_partial_view* partial_view;
        pas_segregated_shared_view* shared_view;
        partial_view = pas_segregated_view_get_partial(view);
        shared_view = pas_compact_segregated_shared_view_ptr_load(&partial_view->shared_view);
        if (!shared_view)
            return NULL;
        return pas_shared_handle_or_page_boundary_get_page_boundary(
            shared_view->shared_handle_or_page_boundary,
            *pas_segregated_page_config_kind_get_config(
                pas_compact_segregated_global_size_directory_ptr_load_non_null(
                    &partial_view->directory)->base.page_config_kind));
    }
    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

pas_segregated_page* pas_segregated_view_get_page(pas_segregated_view view)
{
    return pas_segregated_page_for_boundary_or_null(
        pas_segregated_view_get_page_boundary(view),
        *pas_segregated_view_get_page_config(view));
}

bool pas_segregated_view_should_restart(pas_segregated_view owner_view,
                                        pas_magazine* magazine)
{
    pas_segregated_view_kind kind;
    pas_segregated_directory* directory;
    unsigned index;

    kind = pas_segregated_view_get_kind(owner_view);
    switch (kind) {
    case pas_segregated_exclusive_view_kind: {
        pas_segregated_exclusive_view* exclusive;
        exclusive = pas_segregated_view_get_exclusive(owner_view);
        directory = &pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &exclusive->directory)->base;
        index = exclusive->index;
        break;
    }
    case pas_segregated_ineligible_exclusive_view_kind:
        return false;
    case pas_segregated_biasing_view_kind: {
        pas_segregated_biasing_view* biasing;
        pas_segregated_biasing_directory* biasing_directory;
        biasing = pas_segregated_view_get_biasing(owner_view);
        biasing_directory = pas_compact_segregated_biasing_directory_ptr_load_non_null(
            &biasing->directory);
        if (!magazine ||
            pas_segregated_biasing_directory_magazine_index(
                biasing_directory) != magazine->magazine_index)
            return false;
        directory = &biasing_directory->base;
        index = biasing->index;
        break;
    }
    case pas_segregated_ineligible_biasing_view_kind:
        return false;
    case pas_segregated_partial_view_kind: {
        pas_segregated_partial_view* partial;
        partial = pas_segregated_view_get_partial(owner_view);
        if (!partial->eligibility_has_been_noted)
            return false;
        directory = &pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &partial->directory)->base;
        index = partial->index;
        break;
    }
    default:
        PAS_ASSERT(!"Should not be reached");
        return false;
    }

    /* There's something interesting here: the <= comparison of view indices. It really ought to
       be <, since the owner_view isn't actually eligible.
       
       But the first_eligible property has an off-by-one stickiness to it. That just sort of falls
       out of the generic take_first_eligible algorithm. That necessitates the <=.
       
       This doesn't have to be super correct. It's important for the algorithm to tend towards using
       the lowest-indexed view. But if it sometimes fails to do that, then it's not the end of the
       world.
    
       However, it might be nice to revisit whether this is really the best that we can do. I think
       that under complex races, we might have first_eligible pointing too low, making it seem like
       owner_view isn't the first_eligible one. One way to fix this is to just have
       take_first_eligible take our index and have it not return anything unless that something has
       a lower index than us. That seems better and more efficient in lots of ways. */
    return (index <= pas_segregated_directory_get_first_eligible_torn(
                directory, pas_segregated_directory_first_eligible_but_not_tabled_kind).value)
        && (index <= pas_segregated_directory_get_first_eligible_torn(
                directory, pas_segregated_directory_first_eligible_and_tabled_kind).value);
}

bool pas_segregated_view_could_bump(pas_segregated_view view)
{
    pas_segregated_page_config* page_config;
    page_config = pas_segregated_view_get_page_config(view);
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return !pas_segregated_page_for_boundary(
            ((pas_segregated_exclusive_view*)pas_segregated_view_get_ptr(view))->page_boundary,
            *page_config)->num_non_empty_words;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return !pas_segregated_page_for_boundary(
            pas_compact_atomic_segregated_exclusive_view_ptr_load_non_null(
                &((pas_segregated_biasing_view*)pas_segregated_view_get_ptr(
                      view))->exclusive)->page_boundary,
            *page_config)->num_non_empty_words;
    case pas_segregated_partial_view_kind:
        return false;
    default:
        PAS_ASSERT(!"Should not be reached");
        return false;
    }
}

pas_lock* pas_segregated_view_get_commit_lock(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return &pas_segregated_view_get_exclusive(view)->commit_lock;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return &pas_compact_atomic_segregated_exclusive_view_ptr_load_non_null(
            &pas_segregated_view_get_biasing(view)->exclusive)->commit_lock;
    case pas_segregated_shared_view_kind:
        return &pas_segregated_view_get_shared(view)->commit_lock;
    case pas_segregated_shared_handle_kind:
        return &pas_compact_segregated_shared_view_ptr_load_non_null(
            &pas_segregated_view_get_shared_handle(view)->shared_view)->commit_lock;
    case pas_segregated_partial_view_kind:
        return &pas_compact_segregated_shared_view_ptr_load_non_null(
            &pas_segregated_view_get_partial(view)->shared_view)->commit_lock;
    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

pas_lock* pas_segregated_view_get_ownership_lock(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return &pas_segregated_view_get_exclusive(view)->ownership_lock;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return &pas_compact_atomic_segregated_exclusive_view_ptr_load_non_null(
            &pas_segregated_view_get_biasing(view)->exclusive)->ownership_lock;
    case pas_segregated_shared_view_kind:
        return &pas_segregated_view_get_shared(view)->ownership_lock;
    case pas_segregated_shared_handle_kind:
        return &pas_compact_segregated_shared_view_ptr_load_non_null(
            &pas_segregated_view_get_shared_handle(view)->shared_view)->ownership_lock;
    case pas_segregated_partial_view_kind:
        return &pas_compact_segregated_shared_view_ptr_load_non_null(
            &pas_segregated_view_get_partial(view)->shared_view)->ownership_lock;
    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

bool pas_segregated_view_is_owned(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_exclusive_view_ownership_kind_is_owned(
            pas_segregated_view_get_exclusive(view)->ownership_kind);
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return pas_segregated_biasing_view_is_owned(pas_segregated_view_get_biasing(view));
    case pas_segregated_shared_view_kind:
        return pas_segregated_view_get_shared(view)->is_owned;
    case pas_segregated_shared_handle_kind:
        /* If you have a shared handle in hand then it _must_ be owned. Those get deleted once
           it's not owned. */
        PAS_ASSERT(pas_compact_segregated_shared_view_ptr_load_non_null(
                       &pas_segregated_view_get_shared_handle(view)->shared_view)->is_owned);
        return true;
    case pas_segregated_partial_view_kind:
        return pas_compact_segregated_shared_view_ptr_load_non_null(
            &pas_segregated_view_get_partial(view)->shared_view)->is_owned;
    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

bool pas_segregated_view_should_table(
    pas_segregated_view view, pas_segregated_page_config* page_config)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_exclusive_view_should_table(
            pas_segregated_view_get_exclusive(view), page_config);
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return pas_segregated_biasing_view_should_table(
            pas_segregated_view_get_biasing(view), page_config);
    case pas_segregated_partial_view_kind:
        return pas_segregated_partial_view_should_table(
            pas_segregated_view_get_partial(view), page_config);
    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

bool pas_segregated_view_is_biased_exclusive(pas_segregated_view view)
{
    pas_segregated_exclusive_view* exclusive;
    
    if (!pas_segregated_view_is_exclusive(view))
        return false;
    
    exclusive = pas_segregated_view_get_exclusive(view);
    
    return pas_segregated_exclusive_view_ownership_kind_is_biased(exclusive->ownership_kind);
}

bool pas_segregated_view_lock_ownership_lock(pas_segregated_view view)
{
    if (pas_segregated_view_is_some_biasing(view))
        return pas_segregated_biasing_view_lock_ownership_lock(pas_segregated_view_get_biasing(view));

    pas_lock_lock(pas_segregated_view_get_ownership_lock(view));
    return true;
}

bool pas_segregated_view_lock_ownership_lock_conditionally(pas_segregated_view view,
                                                           pas_lock_hold_mode lock_hold_mode)
{
    bool result;
    pas_compiler_fence();
    if (lock_hold_mode)
        result = true;
    else
        result = pas_segregated_view_lock_ownership_lock(view);
    pas_compiler_fence();
    return result;
}

bool pas_segregated_view_lock_ownership_lock_if_owned(pas_segregated_view view)
{
    return pas_segregated_view_lock_ownership_lock_if_owned_conditionally(view, pas_lock_is_not_held);
}

bool pas_segregated_view_lock_ownership_lock_if_owned_conditionally(pas_segregated_view view,
                                                                    pas_lock_hold_mode lock_hold_mode)
{
    bool result;

    result = pas_segregated_view_lock_ownership_lock_conditionally(view, lock_hold_mode);

    if (!result)
        return false;

    if (pas_segregated_view_is_owned(view))
        return true;

    pas_segregated_view_unlock_ownership_lock_conditionally(view, lock_hold_mode);
    return false;
}

void pas_segregated_view_unlock_ownership_lock(pas_segregated_view view)
{
    pas_lock_unlock(pas_segregated_view_get_ownership_lock(view));
}

void pas_segregated_view_unlock_ownership_lock_conditionally(pas_segregated_view view,
                                                             pas_lock_hold_mode lock_hold_mode)
{
    pas_lock_unlock_conditionally(pas_segregated_view_get_ownership_lock(view), lock_hold_mode);
}

bool pas_segregated_view_is_primordial_partial(pas_segregated_view view)
{
    if (!pas_segregated_view_is_partial(view))
        return false;

    /* This requires no further locking because we assume that this is only ever called on partial
       views that were just made ineligible.
    
       If this is true then the local_allocator takes a very delicate path for allocating, which
       doesn't involve things like did_start_allocating. */
    return !pas_compact_segregated_shared_view_ptr_load(
        &pas_segregated_view_get_partial(view)->shared_view);
}

void pas_segregated_view_note_eligibility(
    pas_segregated_view view,
    pas_segregated_page* page)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("Noting eligibility for view %p, page %p.\n", view, page);
    
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        pas_segregated_exclusive_view_note_eligibility(pas_segregated_view_get_exclusive(view), page);
        return;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        pas_segregated_biasing_view_note_eligibility(pas_segregated_view_get_biasing(view), page);
        return;
    case pas_segregated_partial_view_kind:
        pas_segregated_partial_view_note_eligibility(pas_segregated_view_get_partial(view), page);
        return;
    default:
        PAS_ASSERT(!"Should not be reached");
        return;
    }
}

void pas_segregated_view_note_emptiness(
    pas_segregated_view view,
    pas_segregated_page* page)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        pas_segregated_exclusive_view_note_emptiness(
            pas_segregated_view_get_exclusive(view), page);
        return;
    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        pas_segregated_biasing_view_note_emptiness(
            pas_segregated_view_get_biasing(view), page);
        return;
    case pas_segregated_shared_handle_kind:
        pas_segregated_shared_handle_note_emptiness(
            pas_segregated_view_get_shared_handle(view));
        return;
    default:
        PAS_ASSERT(!"Should not be reached");
        return;
    }
}

static bool for_each_live_object(
    pas_segregated_view view,
    pas_segregated_view_for_each_live_object_callback callback,
    void *arg)
{
    static const bool verbose = false;
    
    pas_segregated_page_config page_config;
    pas_full_alloc_bits full_alloc_bits;
    pas_segregated_page* page;
    uintptr_t page_boundary;
    size_t index;
    size_t object_size;

    if (!pas_segregated_view_is_owned(view))
        return true;

    if (verbose) {
        pas_log("Iterating live objects in owned view %p(%s).\n",
                view,
                pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
    }

    page_config = *pas_segregated_view_get_page_config(view);

    if (pas_segregated_view_is_shared(view) || pas_segregated_view_is_shared_handle(view)) {
        pas_segregated_shared_handle* handle;
        size_t partial_index;

        if (pas_segregated_view_is_shared(view)) {
            pas_shared_handle_or_page_boundary shared_handle_or_page_boundary;

            shared_handle_or_page_boundary =
                pas_segregated_view_get_shared(view)->shared_handle_or_page_boundary;
            if (pas_is_wrapped_page_boundary(shared_handle_or_page_boundary)) {
                /* No live objects! */
                return true;
            }

            handle = pas_unwrap_shared_handle(shared_handle_or_page_boundary, page_config);
        } else
            handle = pas_segregated_view_get_shared_handle(view);

        /* This code is going to scan the list of partial views while holding the ownership lock
           but not the commit lock, so the only way for it to tell if it has seen a view more than
           once is to do O(n^2). That's fine. This is a verification code path that isn't meant to
           be on in production, so it's fine for it to be hella slow. */

        for (partial_index = 0;
             partial_index < pas_segregated_shared_handle_num_views(page_config);
             ++partial_index) {
            pas_segregated_partial_view* partial_view;
            size_t other_partial_index;
            bool already_seen;

            partial_view = pas_compact_atomic_segregated_partial_view_ptr_load(
                handle->partial_views + partial_index);
            if (!partial_view)
                continue;

            already_seen = false;
            for (other_partial_index = 0;
                 other_partial_index < partial_index;
                 ++other_partial_index) {
                if (pas_compact_atomic_segregated_partial_view_ptr_load(
                        handle->partial_views + other_partial_index) == partial_view) {
                    already_seen = true;
                    break;
                }
            }

            if (already_seen)
                continue;

            if (!for_each_live_object(pas_segregated_partial_view_as_view_non_null(partial_view),
                                      callback, arg))
                return false;
        }

        return true;
    }

    full_alloc_bits = pas_full_alloc_bits_create_for_view(view, page_config);
    page = pas_segregated_view_get_page(view);
    page_boundary = (uintptr_t)pas_segregated_page_boundary(page, page_config);
    object_size = pas_segregated_view_get_global_size_directory(view)->object_size;

    if (verbose) {
        pas_log("page = %p, got alloc bits range %zu...%zu.\n",
                page,
                full_alloc_bits.word_index_begin,
                full_alloc_bits.word_index_end);
    }

    /* FIXME: Pretty sure this can just start at word_index_begin. */
    for (index = PAS_MAX(
             PAS_BITVECTOR_BIT_INDEX(full_alloc_bits.word_index_begin),
             pas_page_base_index_of_object_at_offset_from_page_boundary(
                 pas_segregated_page_offset_from_page_boundary_to_first_object(
                     page, NULL, page_config),
                 page_config.base));
         index < PAS_BITVECTOR_BIT_INDEX(full_alloc_bits.word_index_end);
         ++index) {
        uintptr_t object;

        if (verbose)
            pas_log("Considering object index %zu.\n", index);
        
        if (!pas_bitvector_get(full_alloc_bits.bits, index)) {
            if (verbose)
                pas_log("Doesn't belong in view.\n");
            continue;
        }

        if (!pas_bitvector_get(page->alloc_bits, index)) {
            if (verbose)
                pas_log("Isn't allocated.\n");
            continue;
        }
        
        if (verbose)
            pas_log("Got one!\n");
        
        object = page_boundary +
            pas_page_base_object_offset_from_page_boundary_at_index(
                (unsigned)index, page_config.base);

        if (!callback(view, pas_range_create(object, object + object_size), arg))
            return false;
    }

    return true;
}

bool pas_segregated_view_for_each_live_object(
    pas_segregated_view view,
    pas_segregated_view_for_each_live_object_callback callback,
    void *arg,
    pas_lock_hold_mode ownership_lock_hold_mode)
{
    bool result;

    /* For partial and shared views, the page lock is the ownership lock. For exclusive views,
       holding the ownership lock is adequate anyway. */

    if (pas_segregated_view_lock_ownership_lock_conditionally(view, ownership_lock_hold_mode)) {
        result = for_each_live_object(view, callback, arg);
        pas_segregated_view_unlock_ownership_lock_conditionally(view, ownership_lock_hold_mode);
    } else
        result = true;

    return result;
}

static pas_tri_state should_be_eligible(pas_segregated_view view,
                                        pas_segregated_page_config* page_config)
{
    static const bool verbose = false;
    
    pas_full_alloc_bits full_alloc_bits;
    pas_segregated_page* page;
    size_t index;

    if (verbose) {
        pas_log("Checking should eligible in owned view %p(%s).\n",
                view,
                pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
    }

    if (pas_segregated_view_is_shared(view) || pas_segregated_view_is_shared_handle(view)) {
        pas_segregated_shared_view* shared_view;

        if (pas_segregated_view_is_shared(view))
            shared_view = pas_segregated_view_get_shared(view);
        else {
            shared_view = pas_compact_segregated_shared_view_ptr_load_non_null(
                &pas_segregated_view_get_shared_handle(view)->shared_view);
        }

        if (verbose) {
            pas_log("Checking if can bump for shared view %p, bump %u, max object size %zu, "
                    "bump limit %u\n",
                    shared_view, shared_view->bump_offset, page_config->base.max_object_size,
                    pas_segregated_page_config_object_payload_end_offset_from_boundary(*page_config));
        }
        
        PAS_ASSERT((unsigned)page_config->base.max_object_size == page_config->base.max_object_size);

        if (pas_segregated_shared_view_can_bump(
                shared_view, (unsigned)page_config->base.max_object_size, 1, *page_config))
            return pas_tri_state_yes;

        return pas_tri_state_maybe;
    }
    
    if (pas_segregated_view_is_biased_exclusive(view))
        return pas_tri_state_no;

    if (!pas_segregated_view_is_owned(view))
        return pas_tri_state_yes;
    
    page = pas_segregated_view_get_page(view);

    full_alloc_bits = pas_full_alloc_bits_create_for_view(view, *page_config);

    if (verbose) {
        pas_log("page = %p, got alloc bits range %zu...%zu.\n",
                page,
                full_alloc_bits.word_index_begin,
                full_alloc_bits.word_index_end);
    }

    if (page_config->enable_empty_word_eligibility_optimization) {
        if (verbose)
            pas_log("Doing empty word eligibility for view %p\n", view);
        for (index = full_alloc_bits.word_index_begin;
             index < full_alloc_bits.word_index_end;
             ++index) {
            if (verbose) {
                pas_log("index = %zu, full alloc bits word = %u, alloc bits word = %u\n",
                        index, full_alloc_bits.bits[index], page->alloc_bits[index]);
            }
            if (full_alloc_bits.bits[index] && !page->alloc_bits[index])
                return pas_tri_state_yes;
        }
        return pas_tri_state_no;
    }

    for (index = PAS_BITVECTOR_BIT_INDEX(full_alloc_bits.word_index_begin);
         index < PAS_BITVECTOR_BIT_INDEX(full_alloc_bits.word_index_end);
         ++index) {
        if (verbose)
            pas_log("Considering object index %zu.\n", index);
        
        if (!pas_bitvector_get(full_alloc_bits.bits, index)) {
            if (verbose)
                pas_log("Doesn't belong in view.\n");
            continue;
        }

        if (!pas_bitvector_get(page->alloc_bits, index)) {
            if (verbose)
                pas_log("Isn't allocated.\n");
            return pas_tri_state_yes;
        }
    }

    return pas_tri_state_no;
}

pas_tri_state pas_segregated_view_should_be_eligible(pas_segregated_view view,
                                                     pas_segregated_page_config* page_config)
{
    static const bool verbose = false;
    
    pas_tri_state result;

    if (verbose) {
        pas_log("Going to lock for should_be_eligible: %p (%s)\n",
                view, pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
    }

    if (pas_segregated_view_lock_ownership_lock(view)) {
        result = should_be_eligible(view, page_config);
        pas_segregated_view_unlock_ownership_lock(view);
    } else {
        /* This happens when a biasing view is unowned. So it should be eligible! */
        result = pas_tri_state_yes;
    }

    return result;
}

pas_segregated_view pas_segregated_view_for_object(
    uintptr_t begin,
    pas_heap_config* config)
{
    pas_segregated_page* page;
    pas_segregated_view owning_view;
    pas_segregated_page_config* page_config;
    pas_segregated_page_and_config page_and_config;

    page_and_config = pas_segregated_page_and_config_for_address_and_heap_config(begin, config);
    if (pas_segregated_page_and_config_is_empty(page_and_config))
        return NULL;

    page = page_and_config.page;
    page_config = page_and_config.config;

    owning_view = page->owner;

    switch (pas_segregated_view_get_kind(owning_view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        /* Return a pointer to the view that claims it's "exclusive" not "ineligible_exclusive".
           This makes equality assertions in the tests easier to write. */
        return pas_segregated_exclusive_view_as_view(pas_segregated_view_get_exclusive(owning_view));

    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        /* See above. */
        return pas_segregated_biasing_view_as_view(pas_segregated_view_get_biasing(owning_view));

    case pas_segregated_shared_handle_kind:
        return pas_segregated_partial_view_as_view(
            pas_segregated_shared_handle_partial_view_for_object(
                pas_segregated_view_get_ptr(owning_view), begin, *page_config));

    default:
        PAS_ASSERT(!"Should not be reached");
        return NULL;
    }
}

pas_heap_summary pas_segregated_view_compute_summary(pas_segregated_view view,
                                                     pas_segregated_page_config* page_config)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_exclusive_view_compute_summary(pas_segregated_view_get_exclusive(view));

    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return pas_segregated_biasing_view_compute_summary(pas_segregated_view_get_biasing(view));

    case pas_segregated_shared_view_kind:
        return pas_segregated_shared_view_compute_summary(pas_segregated_view_get_shared(view),
                                                          page_config);

    case pas_segregated_partial_view_kind:
        return pas_segregated_partial_view_compute_summary(pas_segregated_view_get_partial(view));

    default:
        PAS_ASSERT(!"Should not be reached");
        return pas_heap_summary_create_empty();
    }
}

bool pas_segregated_view_is_eligible(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_exclusive_view_is_eligible(pas_segregated_view_get_exclusive(view));

    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return pas_segregated_biasing_view_is_eligible(pas_segregated_view_get_biasing(view));

    case pas_segregated_partial_view_kind:
        return pas_segregated_partial_view_is_eligible(pas_segregated_view_get_partial(view));

    default:
        PAS_ASSERT(!"Should not be reached");
        return false;
    }
}

bool pas_segregated_view_is_eligible_or_biased(pas_segregated_view view)
{
    return pas_segregated_view_is_eligible(view)
        || pas_segregated_view_is_biased_exclusive(view);
}

static bool is_payload_empty_callback(pas_segregated_view view,
                                      pas_range range,
                                      void *arg)
{
    PAS_UNUSED_PARAM(view);
    PAS_UNUSED_PARAM(range);
    PAS_UNUSED_PARAM(arg);
    return false;
}

bool pas_segregated_view_is_payload_empty(pas_segregated_view view)
{
    return pas_segregated_view_for_each_live_object(
        view, is_payload_empty_callback, NULL, pas_lock_is_not_held);
}

bool pas_segregated_view_is_empty(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_exclusive_view_is_empty(pas_segregated_view_get_exclusive(view));

    case pas_segregated_biasing_view_kind:
    case pas_segregated_ineligible_biasing_view_kind:
        return pas_segregated_biasing_view_is_empty(pas_segregated_view_get_biasing(view));

    case pas_segregated_shared_view_kind:
        return pas_segregated_shared_view_is_empty(pas_segregated_view_get_shared(view));

    case pas_segregated_shared_handle_kind:
        return pas_segregated_shared_view_is_empty(
            pas_compact_segregated_shared_view_ptr_load_non_null(
                &pas_segregated_view_get_shared_handle(view)->shared_view));

    case pas_segregated_partial_view_kind:
        return false;

    default:
        PAS_ASSERT(!"Should not be reached");
        return false;
    }
}

bool pas_segregated_view_is_empty_or_biased(pas_segregated_view view)
{
    return pas_segregated_view_is_empty(view)
        || pas_segregated_view_is_biased_exclusive(view);
}

#endif /* LIBPAS_ENABLED */
