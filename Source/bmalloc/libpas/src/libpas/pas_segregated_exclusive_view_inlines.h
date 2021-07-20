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

#ifndef PAS_SEGREGATED_EXCLUSIVE_VIEW_INLINES_H
#define PAS_SEGREGATED_EXCLUSIVE_VIEW_INLINES_H

#include "pas_log.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_segregated_page_inlines.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE void pas_segregated_exclusive_ish_view_did_start_allocating(
    pas_segregated_exclusive_view* view,
    pas_segregated_view owner_view, /* Could be the biasing view, or the exclusive view. */
    pas_segregated_global_size_directory* global_directory,
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_segregated_page_config page_config)
{
    /* This is called with the page lock held. */

    static const bool verbose = false;

    unsigned view_index;

    PAS_UNUSED_PARAM(global_directory);

    view_index = view->index;

    if (verbose)
        pas_log("Did start allocating in %p.\n", page);

    PAS_TESTING_ASSERT(!page->is_in_use_for_allocation);

    /* We want to defer notifying anyone about any newly freed objects. Do this here since
       the code below may release (and then reacquire) the page lock. That could cause some
       additional objects to be freed. We don't want that to cause eligibility/emptiness
       notifications to happen! */
    page->is_in_use_for_allocation = true;

    if (page_config.base.page_size != page_config.base.granule_size)
        pas_segregated_page_commit_fully(page, held_lock, pas_commit_fully_holding_page_lock);

    page->owner = pas_segregated_view_as_ineligible(owner_view);
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_EXCLUSIVE_VIEW_INLINES_H */

