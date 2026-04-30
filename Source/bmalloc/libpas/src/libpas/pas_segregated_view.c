/*
 * Copyright (c) 2019-2022 Apple Inc. All rights reserved.
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
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_page.h"
#include "pas_segregated_page_and_config.h"
#include "pas_segregated_size_directory.h"

pas_segregated_size_directory*
pas_segregated_view_get_size_directory_slow(pas_segregated_view view)
{
    if (!view)
        return NULL;
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_compact_segregated_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(view)->directory);
    default:
        PAS_ASSERT_NOT_REACHED();
        return NULL;
    }
}

pas_segregated_page_config_kind pas_segregated_view_get_page_config_kind(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_compact_segregated_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(view)->directory)->base.page_config_kind;
    case pas_segregated_size_directory_view_kind:
        return pas_segregated_view_get_size_directory(view)->base.page_config_kind;
    default:
        PAS_ASSERT_NOT_REACHED();
        return pas_segregated_page_config_kind_null;
    }
}

const pas_segregated_page_config* pas_segregated_view_get_page_config(pas_segregated_view view)
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
    default:
        PAS_ASSERT_NOT_REACHED();
        return 0;
    }
}

void* pas_segregated_view_get_page_boundary(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return ((pas_segregated_exclusive_view*)pas_segregated_view_get_ptr(view))->page_boundary;
    default:
        PAS_ASSERT_NOT_REACHED();
        return NULL;
    }
}

pas_segregated_page* pas_segregated_view_get_page(pas_segregated_view view)
{
    return pas_segregated_page_for_boundary_or_null(
        pas_segregated_view_get_page_boundary(view),
        *pas_segregated_view_get_page_config(view));
}

pas_lock* pas_segregated_view_get_commit_lock(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return &pas_segregated_view_get_exclusive(view)->commit_lock;
    default:
        PAS_ASSERT_NOT_REACHED();
        return NULL;
    }
}

pas_lock* pas_segregated_view_get_ownership_lock(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return &pas_segregated_view_get_exclusive(view)->ownership_lock;
    default:
        PAS_ASSERT_NOT_REACHED();
        return NULL;
    }
}

bool pas_segregated_view_is_owned(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_view_get_exclusive(view)->is_owned;
    default:
        PAS_ASSERT_NOT_REACHED();
        return NULL;
    }
}

void pas_segregated_view_lock_ownership_lock(pas_segregated_view view)
{
    pas_lock_lock(pas_segregated_view_get_ownership_lock(view));
}

void pas_segregated_view_lock_ownership_lock_conditionally(pas_segregated_view view,
                                                           pas_lock_hold_mode lock_hold_mode)
{
    pas_compiler_fence();
    if (!lock_hold_mode)
        pas_segregated_view_lock_ownership_lock(view);
    pas_compiler_fence();
}

bool pas_segregated_view_lock_ownership_lock_if_owned(pas_segregated_view view)
{
    return pas_segregated_view_lock_ownership_lock_if_owned_conditionally(view, pas_lock_is_not_held);
}

bool pas_segregated_view_lock_ownership_lock_if_owned_conditionally(pas_segregated_view view,
                                                                    pas_lock_hold_mode lock_hold_mode)
{
    pas_segregated_view_lock_ownership_lock_conditionally(view, lock_hold_mode);

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
    default:
        PAS_ASSERT_NOT_REACHED();
        return;
    }
}

static bool for_each_live_object(
    pas_segregated_view view,
    pas_segregated_view_for_each_live_object_callback callback,
    void *arg)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);

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

    full_alloc_bits = pas_full_alloc_bits_create_for_view(view, page_config);
    page = pas_segregated_view_get_page(view);
    page_boundary = (uintptr_t)pas_segregated_page_boundary(page, page_config);
    object_size = pas_segregated_view_get_size_directory(view)->object_size;

    if (verbose) {
        pas_log("page = %p, got alloc bits range %u...%u.\n",
                page,
                full_alloc_bits.word_index_begin,
                full_alloc_bits.word_index_end);
    }

    for (index = PAS_BITVECTOR_BIT_INDEX(full_alloc_bits.word_index_begin);
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

    /* Holding the ownership lock is superfluous but sufficient for an exclusive view.
     * FIXME: can this be relaxed? */

    pas_segregated_view_lock_ownership_lock_conditionally(view, ownership_lock_hold_mode);
    result = for_each_live_object(view, callback, arg);
    pas_segregated_view_unlock_ownership_lock_conditionally(view, ownership_lock_hold_mode);

    return result;
}

static pas_tri_state should_be_eligible(pas_segregated_view view,
                                        const pas_segregated_page_config* page_config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);

    pas_full_alloc_bits full_alloc_bits;
    pas_segregated_page* page;
    size_t index;

    if (verbose) {
        pas_log("Checking should eligible in owned view %p(%s).\n",
                view,
                pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
    }

    if (!pas_segregated_view_is_owned(view))
        return pas_tri_state_yes;

    page = pas_segregated_view_get_page(view);

    full_alloc_bits = pas_full_alloc_bits_create_for_view(view, *page_config);

    if (verbose) {
        pas_log("page = %p, got alloc bits range %u...%u.\n",
                page,
                full_alloc_bits.word_index_begin,
                full_alloc_bits.word_index_end);
    }

    if (page_config->enable_empty_word_eligibility_optimization_for_exclusive) {
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
                                                     const pas_segregated_page_config* page_config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);

    pas_tri_state result;

    if (verbose) {
        pas_log("Going to lock for should_be_eligible: %p (%s)\n",
                view, pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
    }

    pas_segregated_view_lock_ownership_lock(view);
    result = should_be_eligible(view, page_config);
    pas_segregated_view_unlock_ownership_lock(view);

    return result;
}

pas_segregated_view pas_segregated_view_for_object(
    uintptr_t begin,
    const pas_heap_config* config)
{
    pas_segregated_page* page;
    pas_segregated_view owning_view;
    pas_segregated_page_and_config page_and_config;

    page_and_config = pas_segregated_page_and_config_for_address_and_heap_config(begin, config);
    if (pas_segregated_page_and_config_is_empty(page_and_config))
        return NULL;

    page = page_and_config.page;

    owning_view = page->owner;

    switch (pas_segregated_view_get_kind(owning_view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        /* Return a pointer to the view that claims it's "exclusive" not "ineligible_exclusive".
           This makes equality assertions in the tests easier to write. */
        return pas_segregated_exclusive_view_as_view(pas_segregated_view_get_exclusive(owning_view));

    default:
        PAS_ASSERT_NOT_REACHED();
        return NULL;
    }
}

pas_heap_summary pas_segregated_view_compute_summary(pas_segregated_view view,
                                                     const pas_segregated_page_config* page_config)
{
    PAS_UNUSED_PARAM(page_config);
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_exclusive_view_compute_summary(pas_segregated_view_get_exclusive(view));

    default:
        PAS_ASSERT_NOT_REACHED();
        return pas_heap_summary_create_empty();
    }
}

bool pas_segregated_view_is_eligible(pas_segregated_view view)
{
    switch (pas_segregated_view_get_kind(view)) {
    case pas_segregated_exclusive_view_kind:
    case pas_segregated_ineligible_exclusive_view_kind:
        return pas_segregated_exclusive_view_is_eligible(pas_segregated_view_get_exclusive(view));

    default:
        PAS_ASSERT_NOT_REACHED();
        return false;
    }
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

    default:
        PAS_ASSERT_NOT_REACHED();
        return false;
    }
}

#endif /* LIBPAS_ENABLED */
