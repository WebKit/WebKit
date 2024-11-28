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

#include "pas_segregated_shared_page_directory.h"

#include "pas_all_shared_page_directories.h"
#include "pas_deferred_decommit_log.h"
#include "pas_heap_lock.h"
#include "pas_page_malloc.h"
#include "pas_random.h"
#include "pas_segregated_directory_inlines.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_page_config.h"
#include "pas_segregated_partial_view.h"
#include "pas_segregated_shared_handle.h"
#include "pas_segregated_shared_view.h"
#include "pas_segregated_size_directory.h"
#include "pas_shared_handle_or_page_boundary_inlines.h"
#include "pas_stream.h"

/* Probability of an eligible page that can't satisfy an allocation being made ineligible.
   
   When we fail to allocate in a page, we get a random number in [0, UINT_MAX]. If that number
   is less equal this, the page is made ineligible. So, it's not possible to have a zero
   probability (which is fine since that would be absurd).
   
   It's probably best to have this be a smallish fraction like 1/8, which is represented as
   0x1fffffff. */
unsigned pas_segregated_shared_page_directory_probability_of_ineligibility = 0x1fffffff;

typedef struct {
    unsigned size;
    unsigned alignment;
    pas_segregated_shared_view* view;
    pas_segregated_page_config page_config;
} find_first_eligible_data;

static PAS_ALWAYS_INLINE unsigned
find_first_eligible_should_consider_view_parallel(
    pas_segregated_directory_bitvector_segment segment,
    pas_segregated_directory_iterate_config* config)
{
    PAS_UNUSED_PARAM(config);
    return segment.eligible_bits;
}

static PAS_ALWAYS_INLINE bool
find_first_eligible_consider_view(
    pas_segregated_directory_iterate_config* config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    find_first_eligible_data* data;
    pas_segregated_shared_view* view;

    data = config->arg;

    if (verbose)
        pas_log("%p: considering shared view at %zu.\n", config->directory, config->index);

    view = pas_segregated_view_get_shared(
        pas_segregated_directory_get(config->directory, config->index));

    if (pas_segregated_shared_view_can_bump(
            view, data->size, data->alignment, data->page_config)) {
        if (verbose)
            pas_log("can bump at %zu!\n", config->index);
        data->view = view;
        return true;
    }

    if (pas_get_fast_random(0)
        <= pas_segregated_shared_page_directory_probability_of_ineligibility) {
        if (verbose)
            pas_log("cannot bump at %zu, clearing eligibility.\n", config->index);
        bool result = PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(
            config->directory, config->bit_reference, eligible, false);
        PAS_UNUSED_PARAM(result);
    }

    if (verbose)
        pas_log("cannot bump at %zu, moving on.\n", config->index);
    return false;
}

pas_segregated_shared_view* pas_segregated_shared_page_directory_find_first_eligible(
    pas_segregated_shared_page_directory* shared_page_directory,
    unsigned size,
    unsigned alignment,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    pas_segregated_directory* directory;
    const pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    pas_segregated_directory_iterate_config config;
    bool did_find_something;
    find_first_eligible_data data;

    directory = &shared_page_directory->base;
    page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);
    page_config = *page_config_ptr;

    if (verbose)
        pas_log("trying to allocate size = %u, alignment = %u.\n", size, alignment);

    data.size = size;
    data.alignment = alignment;
    data.page_config = page_config;

    for (;;) {
        size_t new_size;
        pas_segregated_shared_view* view;
        
        config.directory = directory;
        config.should_consider_view_parallel = find_first_eligible_should_consider_view_parallel;
        config.consider_view = find_first_eligible_consider_view;
        config.arg = &data;
        data.view = NULL;

        did_find_something = pas_segregated_directory_iterate_forward_to_take_first_eligible(&config);

        if (did_find_something) {
            if (!data.view) {
                pas_log("Erroneously found a null view at index = %zu, directory = %p.\n",
                        config.index, directory);
            }
            PAS_ASSERT(data.view);
            return data.view;
        }
        
        pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
        
        new_size = pas_segregated_directory_size(directory);
        PAS_ASSERT(new_size >= config.index);
        if (new_size > config.index) {
            pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
            continue;
        }
        
        PAS_ASSERT(new_size == config.index);
        
        view = pas_segregated_shared_view_create(config.index);
        PAS_ASSERT(view);

        /* If we don't have anything in the directory right now then this is the "real"
           initialization of the shared page directory. Make sure we advertise ourselves. */
        if (!pas_segregated_directory_size(directory))
            pas_all_shared_page_directories_add(shared_page_directory);
        
        pas_segregated_directory_append(
            directory, config.index, pas_segregated_shared_view_as_view_non_null(view));

        pas_segregated_directory_view_did_become_eligible_at_index(directory, config.index);
        
        pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
        
        return view;
    }
}

typedef struct {
    pas_deferred_decommit_log* decommit_log;
    pas_lock_hold_mode heap_lock_hold_mode;
    const pas_segregated_page_config* page_config; /* Needs to be a pointer since this activation is
                                                passed to out-of-line code. */
    pas_page_sharing_pool_take_result result;
} take_last_empty_data;

static PAS_ALWAYS_INLINE unsigned
take_last_empty_should_consider_view_parallel(
    pas_segregated_directory_bitvector_segment segment,
    pas_segregated_directory_iterate_config* config)
{
    PAS_UNUSED_PARAM(config);
    return segment.empty_bits;
}

static PAS_ALWAYS_INLINE void switch_to_ownership(pas_segregated_shared_view* shared_view,
                                                  pas_lock** held_lock,
                                                  pas_segregated_page_config page_config)
{
    if (pas_segregated_page_config_is_utility(page_config))
        pas_compiler_fence();
    else
        pas_lock_switch(held_lock, &shared_view->ownership_lock);
}

/* It would be dumb to inline this since it's huge. */
static bool
take_last_empty_consider_view(
    pas_segregated_directory_iterate_config* config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    /* We put our take_last_empty logic in consider_view because should_consider_view_parallel
       cannot really tell if a page can be taken. */

    pas_segregated_directory* directory;
    pas_segregated_directory_bit_reference bit_reference;
    size_t shared_view_index;
    take_last_empty_data* data;
    pas_deferred_decommit_log* decommit_log;
    pas_segregated_page_config page_config;
    pas_lock_hold_mode heap_lock_hold_mode;
    pas_lock_hold_mode inner_heap_lock_hold_mode;
    pas_segregated_shared_view* shared_view;
    pas_shared_handle_or_page_boundary shared_handle_or_page_boundary;
    pas_segregated_shared_handle* shared_handle;
    size_t partial_index;
    bool did_take_empty_bit;
    pas_segregated_page* page;
    pas_lock* held_lock;
    bool did_add_to_log;
    bool result;
    bool decommit_result;
    unsigned is_in_use_for_allocation_count;

    directory = config->directory;
    bit_reference = config->bit_reference;
    shared_view_index = config->index;

    data = config->arg;

    decommit_log = data->decommit_log;
    heap_lock_hold_mode = data->heap_lock_hold_mode;
    page_config = *data->page_config;

    result = false;
    did_add_to_log = false;
    held_lock = NULL;

    if (verbose)
        pas_log("Considering shared page directory %p index %zu.\n", directory, shared_view_index);
    
    did_take_empty_bit =
        PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(directory, bit_reference, empty, false);
    if (!did_take_empty_bit) {
        if (verbose)
            pas_log("Didn't take the empty bit.\n");
        return false;
    }

    shared_view = pas_segregated_view_get_shared(
        pas_segregated_directory_get(directory, shared_view_index));
    
    if (verbose)
        pas_log("Considering shared %p.\n", shared_view);
    
    /* If our heap is the utility heap, then we'd kinda like to use the heap lock to protect
       everything. That's easy because we can always grab that. Otherwise, we need the commit
       lock, which we can only contend for if we aren't holding either the heap lock or any
       other locks. The deferred decommit log knows how to do that for us.
       
       It's sort of fortuitous that:
       
       - Except for the utility heap, we never run allocation or deallocation code with the
         heap lock held. Hence, segregated heap guts for non-utility heaps can try to acquire
         the commit lock in most of their paths. And on this path, even though we may have lock
         inversion from acquiring the commit lock, we have a path to dealing with that because
         the deferred decommit log has the right magic (including optional hooks into the
         physical memory transaction).
       
       - The utility heap is only used with the heap lock held. Hence, the utility heap can
         use the implicitly-held heap lock to protect most of their state. The utility heap
         basically avoids holding any other locks, ever.
       
       Because of this nice arrangement, we have non-utility heaps use the commit_lock to
       protect the process of adding partial views to shared views. This nicely guarantees that
       if someone tried to add a partial view to this shared view right now, they'd succeed
       either before we grabbed the lock (thus causing us to move onto another page) or they'd
       succeed after we released it (thus causing them to recommit the page). */
    
    if (pas_segregated_page_config_is_utility(page_config)) {
        pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
        inner_heap_lock_hold_mode = pas_lock_is_held;
    } else {
        inner_heap_lock_hold_mode = heap_lock_hold_mode;
        if (!pas_deferred_decommit_log_lock_for_adding(
                decommit_log, &shared_view->commit_lock, heap_lock_hold_mode)) {
            if (verbose)
                pas_log("Couldn't take locks.\n");
            
            /* NOTE: It's possible for the empty bit to have been set because we haven't taken
               eligibility. So, someone else could have taken eligibility and then given it back
               and set the empty bit again. No biggie. */
            pas_segregated_directory_view_did_become_empty_at_index(directory, shared_view_index);
            
            data->result = pas_page_sharing_pool_take_locks_unavailable;
            return true;
        }
    }

    /* Note that right now it's still possible for the shared view to become newly empty. That'll be
       true so long as there are eligible partial views: someone could take an eligible view and use
       it to put more objects into the page, making it non-empty, and then free all of those objects
       and make the page empty again. */

    switch_to_ownership(shared_view, &held_lock, page_config);
    
    /* It's possible to now find the shared view decommitted. No big deal. */
    if (!shared_view->is_owned) {
        if (verbose)
            pas_log("It's decommitted already.\n");

        goto unlock_this_view_and_return_result;
    }

    /* It's possible that this view is being used for allocation. Note that looping over all partial views
       won't necessarily discover this fact, since there could be a primordial allocation, and that one won't
       be registered as a partial view in the handle. */
    if (shared_view->is_in_use_for_allocation_count) {
        if (verbose)
            pas_log("It's in use for allocation.\n");

        goto unlock_this_view_and_return_result;
    }
    
    shared_handle_or_page_boundary = shared_view->shared_handle_or_page_boundary;
    shared_handle = pas_unwrap_shared_handle(shared_handle_or_page_boundary, page_config);

    /* We can take this shared page if either:
       
       - There are no partial views. That's actually totally possible since after decommit the
         shared view thinks that it has no partial views.

            -or-
       
       - All of the partial views are eligible and we took their eligibility.
    
       Note that we need to hold the ownership lock to touch noted_in_scan. */

    for (partial_index = pas_segregated_shared_handle_num_views(page_config); partial_index--;) {
        pas_segregated_partial_view* partial_view;
        pas_segregated_size_directory* size_directory_of_partial;
        pas_segregated_directory* directory_of_partial;

        partial_view = pas_compact_atomic_segregated_partial_view_ptr_load(
            shared_handle->partial_views + partial_index);
        if (!partial_view || partial_view->noted_in_scan)
            continue;

        size_directory_of_partial =
            pas_compact_segregated_size_directory_ptr_load(&partial_view->directory);
        directory_of_partial = &size_directory_of_partial->base;
        
        if (!PAS_SEGREGATED_DIRECTORY_SET_BIT(
                directory_of_partial, partial_view->index, eligible, false)) {
            if (verbose)
                pas_log("Couldn't take eligibility for partial %p.\n", partial_view);

            goto return_taken_partial_views_unlock_this_view_and_return_result;
        }

        if (verbose)
            pas_log("Made partial %p ineligible to take shared %p.\n", partial_view, shared_view);

        PAS_ASSERT(partial_view->eligibility_has_been_noted);
        PAS_ASSERT(!partial_view->is_in_use_for_allocation);

        partial_view->noted_in_scan = true;
    }

    /* It's possible that the empty bit had gotten set again; clear it. */
    bool set_bit_result = PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(directory, bit_reference, empty, false);
    PAS_UNUSED_PARAM(set_bit_result);

    page = pas_segregated_page_for_boundary(shared_handle->page_boundary, page_config);

    /* It's possible that the page isn't actually empty. Between when we took the empty bit and here,
       someone could have allocated some objects in this page. */
    if (!pas_segregated_page_qualifies_for_decommit(page, page_config))
        goto return_taken_partial_views_unlock_this_view_and_return_result;

    if (verbose)
        pas_log("Took shared %p because it was empty.\n", shared_view);
    
    PAS_ASSERT(!PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_GET(directory, bit_reference, empty));
    PAS_ASSERT(shared_view->is_owned);
    is_in_use_for_allocation_count = shared_view->is_in_use_for_allocation_count;
    if (is_in_use_for_allocation_count) {
        pas_log("Error: shared view %p (%s) has is_in_use_for_allocation_count = %u\n",
                shared_view, pas_segregated_page_config_kind_get_string(page_config.kind),
                is_in_use_for_allocation_count);
        for (partial_index = 0;
             partial_index < pas_segregated_shared_handle_num_views(page_config);
             partial_index++) {
            pas_segregated_partial_view* partial_view;
            partial_view = pas_compact_atomic_segregated_partial_view_ptr_load(
                shared_handle->partial_views + partial_index);
            if (!partial_view)
                continue;
            pas_log("partial_index = %zu, partial_view = %p, is_in_use_for_allocation = %s\n",
                    partial_index, partial_view, partial_view->is_in_use_for_allocation ? "yes" : "no");
        }
        PAS_ASSERT(!is_in_use_for_allocation_count);
    }

    if (pas_segregated_page_config_is_utility(page_config)) {
        PAS_ASSERT(page_config.base.page_size == page_config.base.granule_size);
        shared_view->is_owned = false;
        pas_page_malloc_decommit(
            pas_segregated_page_boundary(page, page_config),
            page_config.base.page_size,
            page_config.base.heap_config_ptr->mmap_capability);
        page_config.base.destroy_page_header(&page->base, pas_lock_is_held);
        decommit_log->total += page_config.base.page_size;
        goto return_taken_partial_views_after_decommit;
    }
    
    if (page->emptiness.num_non_empty_words) {
        size_t num_committed_granules;

        PAS_ASSERT(page_config.base.page_size > page_config.base.granule_size);

        /* Ideally this function would not bother releasing the held_lock if it knows that the
           range is locked. But that doesn't seem like a necessary optimization. */
        decommit_result = pas_segregated_page_take_empty_granules(
            page, decommit_log, &held_lock, pas_range_is_locked, heap_lock_hold_mode);
        PAS_ASSERT(decommit_result); /* We already held the lock so it has to succeed. */
        
        num_committed_granules = pas_segregated_page_get_num_committed_granules(page);
        PAS_ASSERT(num_committed_granules);

        did_add_to_log = true;
        result = true;
        data->result = pas_page_sharing_pool_take_success;

        /* NOTE: It might be more efficient if we didn't make the partial views ineligible during
           this time, but instead just checked that they still were eligible, since partial views
           can't go from eligible to allocating without grabbing the commit lock in between.
           
           But this code path is really tuned to be best for small pages and full decommit, which
           is OK. Also, it's not clear that optimizing CASes is meaningful when we're doing
           decommits. */
        goto return_taken_partial_views_unlock_this_view_and_return_result;
    }

    shared_view->is_owned = false;

    pas_lock_switch(&held_lock, NULL);

    /* Ideally this function would not bother releasing the held_lock if it knows that the
       range is locked. But that doesn't seem like a necessary optimization. */
    decommit_result = pas_segregated_page_take_physically(
        page, decommit_log, pas_range_is_locked, heap_lock_hold_mode);
    PAS_ASSERT(decommit_result);
    page_config.base.destroy_page_header(&page->base, heap_lock_hold_mode);

return_taken_partial_views_after_decommit:
    /* Need to hold the ownership lock to fiddle with is_attached_to_shared_handle and
       noted_in_scan. */
    switch_to_ownership(shared_view, &held_lock, page_config);
    for (partial_index = pas_segregated_shared_handle_num_views(page_config); partial_index--;) {
        pas_segregated_partial_view* partial_view;
        pas_segregated_size_directory* size_directory_of_partial;
        pas_segregated_directory* directory_of_partial;
        bool set_result;

        partial_view = pas_compact_atomic_segregated_partial_view_ptr_load(
            shared_handle->partial_views + partial_index);
        if (!partial_view || !partial_view->noted_in_scan)
            continue;

        partial_view->noted_in_scan = false;

        size_directory_of_partial =
            pas_compact_segregated_size_directory_ptr_load(&partial_view->directory);
        directory_of_partial = &size_directory_of_partial->base;

        PAS_ASSERT(partial_view->is_attached_to_shared_handle);
        partial_view->is_attached_to_shared_handle = false;

        set_result = pas_segregated_directory_view_did_become_eligible_at_index(
            directory_of_partial, partial_view->index);
        PAS_ASSERT(set_result);

        if (verbose) {
            pas_log("Make partial %p eligible after decommitting shared %p.\n",
                    partial_view, shared_view);
        }
    }
    pas_lock_switch(&held_lock, NULL);

    pas_heap_lock_lock_conditionally(inner_heap_lock_hold_mode);
    pas_segregated_shared_handle_destroy(shared_handle);
    pas_heap_lock_unlock_conditionally(inner_heap_lock_hold_mode);
    did_add_to_log = true;
    result = true;
    data->result = pas_page_sharing_pool_take_success;
    goto unlock_this_view_and_return_result;
    
return_taken_partial_views_unlock_this_view_and_return_result:
    switch_to_ownership(shared_view, &held_lock, page_config);
    for (partial_index = pas_segregated_shared_handle_num_views(page_config); partial_index--;) {
        pas_segregated_partial_view* partial_view;
        pas_segregated_size_directory* size_directory_of_partial;
        pas_segregated_directory* directory_of_partial;
        bool set_result;

        partial_view = pas_compact_atomic_segregated_partial_view_ptr_load(
            shared_handle->partial_views + partial_index);
        if (!partial_view || !partial_view->noted_in_scan)
            continue;
        
        size_directory_of_partial =
            pas_compact_segregated_size_directory_ptr_load(&partial_view->directory);
        directory_of_partial = &size_directory_of_partial->base;

        set_result = pas_segregated_directory_view_did_become_eligible_at_index(
            directory_of_partial, partial_view->index);
        PAS_ASSERT(set_result);

        if (verbose) {
            pas_log("Make partial %p eligible after attempting to decommit or partly decommitting "
                    "shared %p.\n",
                    partial_view, shared_view);
        }
        
        partial_view->noted_in_scan = false;
    }
    
unlock_this_view_and_return_result:
    pas_lock_switch(&held_lock, NULL);
    if (pas_segregated_page_config_is_utility(page_config))
        pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    else if (!did_add_to_log)
        pas_deferred_decommit_log_unlock_after_aborted_add(decommit_log, &shared_view->commit_lock);
    return result;
}

pas_page_sharing_pool_take_result
pas_segregated_shared_page_directory_take_last_empty(
    pas_segregated_shared_page_directory* shared_page_directory,
    pas_deferred_decommit_log* decommit_log,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    /* Be careful: heap_lock_hold_mode here can be unrelated to what page_config says, since
       we may be asked by the physical page sharing pool to decommit on behalf of any other heap,
       including ones that hold the heap lock while doing business. */

    pas_segregated_directory* directory;
    const pas_segregated_page_config* page_config_ptr;
    bool did_find_something;
    pas_segregated_directory_iterate_config config;
    take_last_empty_data data;

    directory = &shared_page_directory->base;
    page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);

    data.decommit_log = decommit_log;
    data.heap_lock_hold_mode = heap_lock_hold_mode;
    data.page_config = page_config_ptr;
    data.result = pas_page_sharing_pool_take_none_available; /* Tell the compiler to cool its
                                                                jets. */
    config.directory = directory;
    config.should_consider_view_parallel = take_last_empty_should_consider_view_parallel;
    config.consider_view = take_last_empty_consider_view;
    config.arg = &data;
    
    /* Why take last?
       
       The idea here is that this is roughly last-fit. The downside is that if we have a
       size directory that has its first uses late in the program's execution, it will become
       the last-fit solution.
       
       Size directories have the same pathology, but on a smaller scale.
       
       As a mitigation, the shared page directory has its epoch bumped anytime any partial
       view that uses its pages gets allocated out of. So, the shared page directory as a whole
       is likely to be a low priority target for the scavenger. */
    did_find_something = pas_segregated_directory_iterate_backward_to_take_last_empty(&config);
    
    if (!did_find_something)
        return pas_page_sharing_pool_take_none_available;

    return data.result;
}

void pas_segregated_shared_page_directory_dump_reference(
    pas_segregated_shared_page_directory* directory,
    pas_stream* stream)
{
    pas_stream_printf(stream, "%p(shared_page_directory)", directory);
}

void pas_segregated_shared_page_directory_dump_for_spectrum(
    pas_stream* stream, void* directory)
{
    pas_segregated_shared_page_directory_dump_reference(directory, stream);
}

#endif /* LIBPAS_ENABLED */
