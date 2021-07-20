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

#ifndef PAS_SEGREGATED_BIASING_VIEW_H
#define PAS_SEGREGATED_BIASING_VIEW_H

#include "pas_compact_atomic_segregated_exclusive_view_ptr.h"
#include "pas_compact_segregated_biasing_directory_ptr.h"
#include "pas_segregated_view.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_biasing_view;
typedef struct pas_segregated_biasing_view pas_segregated_biasing_view;

struct pas_segregated_biasing_view {
    pas_compact_atomic_segregated_exclusive_view_ptr exclusive;
    unsigned index; /* If we wanted to be hardcore, we could make this one byte. */
    pas_compact_segregated_biasing_directory_ptr directory;
};

static inline pas_segregated_view
pas_segregated_biasing_view_as_view(pas_segregated_biasing_view* view)
{
    return pas_segregated_view_create(view, pas_segregated_biasing_view_kind);
}

static inline pas_segregated_view
pas_segregated_biasing_view_as_ineligible_view(pas_segregated_biasing_view* view)
{
    return pas_segregated_view_create(view, pas_segregated_ineligible_biasing_view_kind);
}

static inline pas_segregated_view
pas_segregated_biasing_view_as_view_non_null(pas_segregated_biasing_view* view)
{
    return pas_segregated_view_create_non_null(view, pas_segregated_biasing_view_kind);
}

static inline pas_segregated_view
pas_segregated_biasing_view_as_ineligible_view_non_null(pas_segregated_biasing_view* view)
{
    return pas_segregated_view_create_non_null(view, pas_segregated_ineligible_biasing_view_kind);
}

PAS_API pas_segregated_biasing_view* pas_segregated_biasing_view_create(
    pas_segregated_biasing_directory* directory,
    size_t index);

PAS_API bool pas_segregated_biasing_view_is_owned(pas_segregated_biasing_view* view);
PAS_API bool pas_segregated_biasing_view_should_table(
    pas_segregated_biasing_view* view, pas_segregated_page_config* page_config);
PAS_API bool pas_segregated_biasing_view_lock_ownership_lock(pas_segregated_biasing_view* view);

PAS_API void pas_segregated_biasing_view_note_eligibility(pas_segregated_biasing_view* view,
                                                          pas_segregated_page* page);
PAS_API void pas_segregated_biasing_view_note_emptiness(pas_segregated_biasing_view* view,
                                                        pas_segregated_page* page);
PAS_API pas_heap_summary pas_segregated_biasing_view_compute_summary(pas_segregated_biasing_view* view);
PAS_API bool pas_segregated_biasing_view_is_eligible(pas_segregated_biasing_view* view);
PAS_API bool pas_segregated_biasing_view_is_empty(pas_segregated_biasing_view* view);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_BIASING_VIEW_H */

