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

#ifndef PAS_SEGREGATED_PARTIAL_VIEW_H
#define PAS_SEGREGATED_PARTIAL_VIEW_H

#include "pas_compact_segregated_shared_view_ptr.h"
#include "pas_compact_segregated_size_directory_ptr.h"
#include "pas_lenient_compact_unsigned_ptr.h"
#include "pas_page_granule_use_count.h"
#include "pas_segregated_page_config.h"
#include "pas_segregated_view.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_partial_view;
struct pas_segregated_shared_view;
struct pas_segregated_size_directory;
typedef struct pas_segregated_partial_view pas_segregated_partial_view;
typedef struct pas_segregated_shared_view pas_segregated_shared_view;
typedef struct pas_segregated_size_directory pas_segregated_size_directory;

PAS_API extern size_t pas_segregated_partial_view_count;

struct pas_segregated_partial_view {
    unsigned inline_alloc_bits;
    
    pas_compact_segregated_shared_view_ptr shared_view;
    pas_compact_segregated_size_directory_ptr directory;
    pas_lenient_compact_unsigned_ptr alloc_bits;

    /* The index can be small since we would never create a high-indexed partial view. That would not
       have a meaningful effect on the footprint of the directory.
    
       In fact, it's likely that this will never be bigger than 100, so 8-bit is plenty.
    
       We assert that the index fits in 8-bit. */
    uint8_t index;

    /* These things can be null/zero while the partial view is being built. In that case, the view
       must be owned, committed, non-empty, and ineligible. Some local allocator must be using it. */
    uint8_t alloc_bits_size;
    uint8_t alloc_bits_offset;

    bool is_in_use_for_allocation : 1;
    bool eligibility_notification_has_been_deferred : 1;

    /* This bit is super special. It needs to be set before we mark the page eligible. */
    bool eligibility_has_been_noted : 1;

    /* This is useful for scanning through the partial views of a handle. It's used with both the
       ownership and commit lock held. If you don't hold the commit lock then you can't touch this.
       Basically you need to hold the commit lock around the whole scan so that when the commit
       lock is dropped, this is false. Also because this is in a bitfield protected only by the
       ownership lock, you need to hold the ownership lock at the instants when you mess with
       this. */
    bool noted_in_scan : 1;

    bool is_attached_to_shared_handle : 1;
};

static inline pas_segregated_view
pas_segregated_partial_view_as_view(pas_segregated_partial_view* view)
{
    return pas_segregated_view_create(view, pas_segregated_partial_view_kind);
}

static inline pas_segregated_view
pas_segregated_partial_view_as_view_non_null(pas_segregated_partial_view* view)
{
    return pas_segregated_view_create_non_null(view, pas_segregated_partial_view_kind);
}

PAS_API pas_segregated_partial_view*
pas_segregated_partial_view_create(
    pas_segregated_size_directory* directory,
    size_t index);

PAS_API void pas_segregated_partial_view_note_eligibility(
    pas_segregated_partial_view* view,
    pas_segregated_page* page);

/* Call with page lock held. This handles all of the is_in_use_for_allocation/eligibility stuff
   for start-of-allocation that is common to partial primordials and normal primordials. */
PAS_API void pas_segregated_partial_view_set_is_in_use_for_allocation(
    pas_segregated_partial_view* view,
    pas_segregated_shared_view* shared_view,
    pas_segregated_shared_handle* shared_handle);

PAS_API bool pas_segregated_partial_view_should_table(
    pas_segregated_partial_view* view,
    const pas_segregated_page_config* page_config);

PAS_API bool pas_segregated_partial_view_for_each_live_object(
    pas_segregated_partial_view* view,
    pas_segregated_view_for_each_live_object_callback callback,
    void *arg);

PAS_API pas_heap_summary pas_segregated_partial_view_compute_summary(
    pas_segregated_partial_view* view);

PAS_API bool pas_segregated_partial_view_is_eligible(pas_segregated_partial_view* view);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PARTIAL_VIEW_H */

