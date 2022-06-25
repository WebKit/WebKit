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

#ifndef PAS_SEGREGATED_EXCLUSIVE_VIEW_H
#define PAS_SEGREGATED_EXCLUSIVE_VIEW_H

#include "pas_compact_segregated_size_directory_ptr.h"
#include "pas_lock.h"
#include "pas_segregated_deallocation_mode.h"
#include "pas_segregated_view.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_exclusive_view;
struct pas_segregated_page;
struct pas_thread_local_cache;
typedef struct pas_segregated_exclusive_view pas_segregated_exclusive_view;
typedef struct pas_segregated_page pas_segregated_page;
typedef struct pas_thread_local_cache pas_thread_local_cache;

PAS_API extern size_t pas_segregated_exclusive_view_count;

struct pas_segregated_exclusive_view {
    void* page_boundary;
    
    pas_compact_segregated_size_directory_ptr directory;

    bool is_owned;

    unsigned index;

    /* This lock needs to be held before the heap lock is acquired. */
    pas_lock commit_lock;

    /* This lock can be acquired after the heap lock is held.
     
       This lock doubles as the page fallback lock.
    
       This lock's other purpose is to allow heap iteration to happen with the heap lock held.
       It's not obvious that we need that capability, but it seems like a useful escape hatch,
       if even just when we find that we need it in a debugging session. The alternative is to
       say that this is just the fallback lock and say that heap iteration grabs the commit
       lock. That would mean also having to make virtual page taking grab the commit lock.
    
       I don't think we have a story for the ordering between the page lock and the ownership
       lock, if they are different. */
    pas_lock ownership_lock;
};

static inline pas_segregated_view
pas_segregated_exclusive_view_as_view(pas_segregated_exclusive_view* view)
{
    return pas_segregated_view_create(view, pas_segregated_exclusive_view_kind);
}

static inline pas_segregated_view
pas_segregated_exclusive_view_as_ineligible_view(pas_segregated_exclusive_view* view)
{
    return pas_segregated_view_create(view, pas_segregated_ineligible_exclusive_view_kind);
}

static inline pas_segregated_view
pas_segregated_exclusive_view_as_view_non_null(pas_segregated_exclusive_view* view)
{
    return pas_segregated_view_create_non_null(view, pas_segregated_exclusive_view_kind);
}

static inline pas_segregated_view
pas_segregated_exclusive_view_as_ineligible_view_non_null(pas_segregated_exclusive_view* view)
{
    return pas_segregated_view_create_non_null(view, pas_segregated_ineligible_exclusive_view_kind);
}

PAS_API pas_segregated_exclusive_view*
pas_segregated_exclusive_view_create(
    pas_segregated_size_directory* directory,
    size_t index);

PAS_API void pas_segregated_exclusive_view_note_emptiness(
    pas_segregated_exclusive_view* view,
    pas_segregated_page* page);

PAS_API pas_heap_summary pas_segregated_exclusive_view_compute_summary(
    pas_segregated_exclusive_view* view);

PAS_API void pas_segregated_exclusive_view_install_full_use_counts(
    pas_segregated_exclusive_view* view);

PAS_API bool pas_segregated_exclusive_view_is_eligible(pas_segregated_exclusive_view* view);
PAS_API bool pas_segregated_exclusive_view_is_empty(pas_segregated_exclusive_view* view);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_EXCLUSIVE_VIEW_H */

