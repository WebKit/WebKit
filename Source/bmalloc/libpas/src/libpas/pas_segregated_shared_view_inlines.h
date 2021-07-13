/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_SHARED_VIEW_INLINES_H
#define PAS_SEGREGATED_SHARED_VIEW_INLINES_H

#include "pas_segregated_shared_view.h"
#include "pas_shared_handle_or_page_boundary_inlines.h"

PAS_BEGIN_EXTERN_C;

/* NOTE: Right now this is responsible for actually doing the page allocation. However, it's not
   obvious that this is the right approach. You could imagine having the shared page directory do
   that. But, that would not reduce the amount of checks that commit_page_if_necessary does. */
PAS_API pas_segregated_shared_handle* pas_segregated_shared_view_commit_page(
    pas_segregated_shared_view* view,
    pas_segregated_heap* heap,
    pas_segregated_shared_page_directory* directory,
    pas_segregated_partial_view* partial_view,
    pas_segregated_page_config* page_config);

/* Must be called holding the commit lock. */
static PAS_ALWAYS_INLINE pas_segregated_shared_handle*
pas_segregated_shared_view_commit_page_if_necessary(
    pas_segregated_shared_view* view,
    pas_segregated_heap* heap,
    pas_segregated_shared_page_directory* directory,
    pas_segregated_partial_view* partial_view,
    pas_segregated_page_config page_config)
{
    pas_shared_handle_or_page_boundary shared_handle_or_page_boundary;
    pas_segregated_shared_handle* result;

    shared_handle_or_page_boundary = view->shared_handle_or_page_boundary;
    
    /* This also checks that we've allocated a page, since we never wrap NULL as a shared handle. */
    if (pas_is_wrapped_shared_handle(shared_handle_or_page_boundary))
        result = pas_unwrap_shared_handle(shared_handle_or_page_boundary, page_config);
    else {
        result = pas_segregated_shared_view_commit_page(
            view, heap, directory, partial_view,
            (pas_segregated_page_config*)page_config.base.page_config_ptr);
    }

    PAS_TESTING_ASSERT(result->directory == directory);

    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_SHARED_VIEW_INLINES_H */

