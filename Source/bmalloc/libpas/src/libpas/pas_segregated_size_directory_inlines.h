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

#ifndef PAS_SEGREGATED_SIZE_DIRECTORY_INLINES_H
#define PAS_SEGREGATED_SIZE_DIRECTORY_INLINES_H

#include "pas_baseline_allocator_result.h"
#include "pas_baseline_allocator_table.h"
#include "pas_count_lookup_mode.h"
#include "pas_segregated_biasing_directory.h"
#include "pas_segregated_directory_inlines.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_size_directory.h"
#include "pas_thread_local_cache.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE unsigned
pas_segregated_size_directory_take_first_eligible_impl_should_consider_view_not_tabled_parallel(
    pas_segregated_directory_bitvector_segment segment,
    pas_segregated_directory_iterate_config* config)
{
    PAS_UNUSED_PARAM(config);
    
    return segment.eligible_bits & ~segment.tabled_bits;
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_size_directory_take_first_eligible_impl_should_consider_view_tabled_parallel(
    pas_segregated_directory_bitvector_segment segment,
    pas_segregated_directory_iterate_config* config)
{
    PAS_UNUSED_PARAM(config);
    
    return segment.eligible_bits & segment.tabled_bits;
}

static PAS_ALWAYS_INLINE pas_segregated_view
pas_segregated_size_directory_take_first_eligible_impl(
    pas_segregated_directory* directory,
    pas_segregated_directory_iterate_config* config,
    void (*loop_head_callback)(pas_segregated_directory* directory),
    pas_segregated_view (*create_new_view_callback)(
        pas_segregated_directory_iterate_config* config))
{
    static const bool verbose = false;
    
    bool did_find_something;
    pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    pas_segregated_view view;

    page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);
    page_config = *page_config_ptr;

    view = NULL;
    
    if (verbose)
        pas_log("%p: At start of take_first_eligible_impl.\n", directory);
    
    for (;;) {
        if (verbose)
            pas_log("%p: At top of take_first_eligible_impl loop.\n", directory);
        
        loop_head_callback(directory);
        
        config->directory = directory;
        config->should_consider_view_parallel =
            pas_segregated_size_directory_take_first_eligible_impl_should_consider_view_not_tabled_parallel;
        config->consider_view = NULL;
        config->arg = NULL;

        did_find_something = pas_segregated_directory_iterate_forward_to_take_first_eligible(
            config, pas_segregated_directory_first_eligible_but_not_tabled_kind);

        if (did_find_something) {
            view = pas_segregated_directory_get(directory, config->index);
            
            if (verbose) {
                pas_log("At index %zu: Taking existing view %p (not tabled).\n",
                        (size_t)config->index,
                        pas_segregated_view_get_ptr(view));
            }
            
            PAS_TESTING_ASSERT(config->segment.eligible_bits & config->bit_reference.mask);
            PAS_TESTING_ASSERT(!(config->segment.tabled_bits & config->bit_reference.mask));

            if (PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(
                    directory, config->bit_reference, eligible, false)) {
                if (verbose)
                    pas_log("Did take first eligible.\n");
                
                if (!pas_segregated_size_directory_use_tabling)
                    break;
                
                if (!pas_segregated_view_should_table(view, page_config_ptr))
                    break;

                if (verbose)
                    pas_log("Tabling.\n");
                
                PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(
                    directory, config->bit_reference, tabled, true);
                PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(
                    directory, config->bit_reference, eligible, true);
                pas_segregated_directory_minimize_first_eligible(
                    directory, pas_segregated_directory_first_eligible_and_tabled_kind, config->index);
            }
        } else {
            size_t not_tabled_index;

            if (verbose)
                pas_log("Didn't find anything, searching tabled.\n");

            not_tabled_index = config->index;

            config->directory = directory;
            config->should_consider_view_parallel =
                pas_segregated_size_directory_take_first_eligible_impl_should_consider_view_tabled_parallel;
            config->consider_view = NULL;
            config->arg = NULL;
            did_find_something = pas_segregated_directory_iterate_forward_to_take_first_eligible(
                config, pas_segregated_directory_first_eligible_and_tabled_kind);

            if (did_find_something) {
                view = pas_segregated_directory_get(directory, config->index);
                
                if (verbose) {
                    pas_log("At index %zu: Taking existing view %p (tabled).\n",
                            config->index,
                            pas_segregated_view_get_ptr(view));
                }
                
                PAS_TESTING_ASSERT(config->segment.eligible_bits & config->bit_reference.mask);
                PAS_TESTING_ASSERT(config->segment.tabled_bits & config->bit_reference.mask);
                
                if (PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(
                        directory, config->bit_reference, eligible, false)) {
                    if (verbose)
                        pas_log("Did take first eligible.\n");

                    /* FIXME: This is weird for partials. Ideally, we'd want to untable all of the partials
                       that share the same shared view as this one. But tabling is most likely to happen
                       for decommitted pages, in which case we cannot find the other partials - we would
                       have forgotten about them. So, we let the other partials stay tabled. */
                    PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(
                        directory, config->bit_reference, tabled, false);
                    break;
                }
            } else {
                size_t new_size;

                pas_heap_lock_lock_conditionally(
                    pas_segregated_page_config_heap_lock_hold_mode(page_config));

                new_size = pas_segregated_directory_size(directory);
                PAS_ASSERT(new_size >= not_tabled_index);
                PAS_ASSERT(new_size >= config->index);
                if (new_size == not_tabled_index && new_size == config->index) {
                    view = create_new_view_callback(config);
                    pas_segregated_directory_append(directory, config->index, view);
                    pas_heap_lock_unlock_conditionally(
                        pas_segregated_page_config_heap_lock_hold_mode(page_config));
                    break;
                }
                
                pas_heap_lock_unlock_conditionally(
                    pas_segregated_page_config_heap_lock_hold_mode(page_config));
            }
        }
    }
    
    if (verbose) {
        pas_log("Taking eligible view %p (%s).\n",
                pas_segregated_view_get_ptr(view),
                pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
    }

    PAS_TESTING_ASSERT(!PAS_SEGREGATED_DIRECTORY_GET_BIT(
                           directory, pas_segregated_view_get_index(view), eligible));
    
    return view;
}

static PAS_ALWAYS_INLINE pas_segregated_global_size_directory*
pas_segregated_size_directory_get_global(pas_segregated_directory* directory)
{
    if (!directory)
        return NULL;
    switch (directory->directory_kind) {
    case pas_segregated_global_size_directory_kind:
        return (pas_segregated_global_size_directory*)directory;
    case pas_segregated_biasing_directory_kind:
        return pas_compact_segregated_global_size_directory_ptr_load_non_null(
            &((pas_segregated_biasing_directory*)directory)->parent_global);
    default:
        break;
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_SIZE_DIRECTORY_INLINES_H */

