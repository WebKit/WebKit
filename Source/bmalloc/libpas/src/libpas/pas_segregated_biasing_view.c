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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_segregated_biasing_view.h"

#include "pas_epoch.h"
#include "pas_page_sharing_pool.h"
#include "pas_segregated_biasing_directory.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_segregated_page_config.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_segregated_view_inlines.h"

pas_segregated_biasing_view* pas_segregated_biasing_view_create(
    pas_segregated_biasing_directory* directory,
    size_t index)
{
    pas_segregated_biasing_view* result;

    result = pas_immortal_heap_allocate(sizeof(pas_segregated_biasing_view),
                                        "pas_segregated_biasing_view",
                                        pas_object_allocation);

    pas_compact_atomic_segregated_exclusive_view_ptr_store(&result->exclusive, NULL);
    result->index = (unsigned)index;
    PAS_ASSERT(result->index == index);
    pas_compact_segregated_biasing_directory_ptr_store(&result->directory, directory);

    return result;
}

bool pas_segregated_biasing_view_is_owned(pas_segregated_biasing_view* view)
{
    pas_segregated_exclusive_view* exclusive_view;
    pas_segregated_exclusive_view_ownership_kind ownership_kind;
    
    exclusive_view = pas_compact_atomic_segregated_exclusive_view_ptr_load(&view->exclusive);
    
    if (!exclusive_view)
        return false;

    ownership_kind = exclusive_view->ownership_kind;
    
    return pas_segregated_exclusive_view_ownership_kind_is_owned(ownership_kind)
        && pas_segregated_exclusive_view_ownership_kind_is_biased(ownership_kind);
}

bool pas_segregated_biasing_view_lock_ownership_lock(pas_segregated_biasing_view* view)
{
    for (;;) {
        pas_segregated_exclusive_view* exclusive_view;
        
        exclusive_view = pas_compact_atomic_segregated_exclusive_view_ptr_load(&view->exclusive);
        
        if (!exclusive_view)
            return false;

        pas_lock_lock(&exclusive_view->ownership_lock);

        if (exclusive_view == pas_compact_atomic_segregated_exclusive_view_ptr_load(&view->exclusive)
            && pas_segregated_exclusive_view_ownership_kind_is_biased(exclusive_view->ownership_kind))
            return true;

        pas_lock_unlock(&exclusive_view->ownership_lock);
    }
}

bool pas_segregated_biasing_view_should_table(
    pas_segregated_biasing_view* view, pas_segregated_page_config* page_config)
{
    pas_segregated_exclusive_view* exclusive_view;
    
    exclusive_view = pas_compact_atomic_segregated_exclusive_view_ptr_load(&view->exclusive);
    
    if (!exclusive_view)
        return true;

    return pas_segregated_exclusive_view_should_table(exclusive_view, page_config);
}

void pas_segregated_biasing_view_note_eligibility(pas_segregated_biasing_view* view,
                                                  pas_segregated_page* page)
{
    pas_segregated_biasing_directory* biasing_directory;
    pas_segregated_directory* directory;
    unsigned view_index;
    
    biasing_directory = pas_compact_segregated_biasing_directory_ptr_load_non_null(&view->directory);
    directory = &biasing_directory->base;
    view_index = view->index;
    
    pas_segregated_exclusive_ish_view_note_eligibility_impl(
        page, directory, pas_segregated_biasing_view_as_view_non_null(view), view_index);
}

void pas_segregated_biasing_view_note_emptiness(
    pas_segregated_biasing_view* view,
    pas_segregated_page* page)
{
    pas_segregated_exclusive_view_note_emptiness(
        pas_compact_atomic_segregated_exclusive_view_ptr_load_non_null(&view->exclusive),
        page);
}

pas_heap_summary pas_segregated_biasing_view_compute_summary(pas_segregated_biasing_view* view)
{
    pas_segregated_exclusive_view* exclusive;
    pas_heap_summary result;
    
    if (!pas_segregated_biasing_view_lock_ownership_lock(view))
        return pas_heap_summary_create_empty();
    
    exclusive = pas_compact_atomic_segregated_exclusive_view_ptr_load_non_null(&view->exclusive);
    PAS_ASSERT(pas_segregated_exclusive_view_ownership_kind_is_biased(exclusive->ownership_kind));
    result = pas_segregated_exclusive_ish_view_compute_summary_impl(exclusive);

    pas_lock_unlock(&exclusive->ownership_lock);

    return result;
}

bool pas_segregated_biasing_view_is_eligible(pas_segregated_biasing_view* view)
{
    return PAS_SEGREGATED_DIRECTORY_GET_BIT(
        &pas_compact_segregated_biasing_directory_ptr_load_non_null(&view->directory)->base,
        view->index, eligible);
}

bool pas_segregated_biasing_view_is_empty(pas_segregated_biasing_view* view)
{
    pas_segregated_exclusive_view* exclusive;
    exclusive = pas_compact_atomic_segregated_exclusive_view_ptr_load(&view->exclusive);
    if (exclusive)
        return pas_segregated_exclusive_view_is_empty(exclusive);
    return false;
}

#endif /* LIBPAS_ENABLED */
