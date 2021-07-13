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

#ifndef PAS_BITFIT_VIEW_INLINES_H
#define PAS_BITFIT_VIEW_INLINES_H

#include "pas_bitfit_biasing_directory.h"
#include "pas_bitfit_directory_and_index.h"
#include "pas_bitfit_global_directory.h"
#include "pas_bitfit_page.h"
#include "pas_bitfit_view.h"

PAS_BEGIN_EXTERN_C;

static inline pas_bitfit_directory_and_index
pas_bitfit_view_current_directory_and_index(pas_bitfit_view* view)
{
    pas_bitfit_biasing_directory* directory;
    directory = pas_compact_atomic_bitfit_biasing_directory_ptr_load(&view->biasing_directory);
    if (directory) {
        return pas_bitfit_directory_and_index_create(
            &directory->base, view->index_in_biasing, pas_bitfit_biasing_directory_kind);
    }
    return pas_bitfit_directory_and_index_create(
        &pas_compact_bitfit_global_directory_ptr_load_non_null(&view->global_directory)->base,
        view->index_in_global, pas_bitfit_global_directory_kind);
}

static inline pas_bitfit_directory* pas_bitfit_view_current_directory(pas_bitfit_view* view)
{
    return pas_bitfit_view_current_directory_and_index(view).directory;
}

static inline unsigned pas_bitfit_view_index_in_current(pas_bitfit_view* view)
{
    return pas_bitfit_view_current_directory_and_index(view).index;
}


static PAS_ALWAYS_INLINE bool pas_bitfit_view_is_empty(pas_bitfit_view* view,
                                                       pas_bitfit_page_config page_config)
{
    if (!view->is_owned)
        return true;

    return !pas_bitfit_page_for_boundary(view->page_boundary, page_config)->num_live_bits;
}

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_VIEW_INLINES_H */

