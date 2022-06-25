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

#ifndef PAS_FULL_ALLOC_BITS_INLINES_H
#define PAS_FULL_ALLOC_BITS_INLINES_H

#include "pas_full_alloc_bits.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_page_config.h"
#include "pas_segregated_partial_view.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_view.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_full_alloc_bits
pas_full_alloc_bits_create_for_exclusive(
    pas_segregated_size_directory* directory,
    pas_segregated_page_config page_config)
{
    return pas_full_alloc_bits_create(
        pas_compact_tagged_unsigned_ptr_load_non_null(
            &pas_segregated_size_directory_data_ptr_load_non_null(
                &directory->data)->full_alloc_bits),
        0,
        (unsigned)pas_segregated_page_config_num_alloc_words(page_config));
}

static PAS_ALWAYS_INLINE pas_full_alloc_bits
pas_full_alloc_bits_create_for_partial_but_not_primordial(pas_segregated_view view)
{
    pas_segregated_partial_view* partial_view;
    
    partial_view = pas_segregated_view_get_partial(view);

    return pas_full_alloc_bits_create(
        pas_lenient_compact_unsigned_ptr_load_compact_non_null(&partial_view->alloc_bits),
        partial_view->alloc_bits_offset,
        partial_view->alloc_bits_offset + partial_view->alloc_bits_size);
}

static PAS_ALWAYS_INLINE pas_full_alloc_bits
pas_full_alloc_bits_create_for_partial(pas_segregated_view view)
{
    pas_segregated_partial_view* partial_view;
    
    partial_view = pas_segregated_view_get_partial(view);

    return pas_full_alloc_bits_create(
        pas_lenient_compact_unsigned_ptr_load(&partial_view->alloc_bits),
        partial_view->alloc_bits_offset,
        partial_view->alloc_bits_offset + partial_view->alloc_bits_size);
}

static PAS_ALWAYS_INLINE pas_full_alloc_bits
pas_full_alloc_bits_create_for_view_and_directory(
    pas_segregated_view view,
    pas_segregated_size_directory* directory,
    pas_segregated_page_config page_config)
{
    if (pas_segregated_view_is_some_exclusive(view))
        return pas_full_alloc_bits_create_for_exclusive(directory, page_config);

    return pas_full_alloc_bits_create_for_partial(view);
}

static PAS_ALWAYS_INLINE pas_full_alloc_bits
pas_full_alloc_bits_create_for_view(
    pas_segregated_view view,
    pas_segregated_page_config page_config)
{
    if (pas_segregated_view_is_some_exclusive(view)) {
        pas_segregated_size_directory* size_directory;
        size_directory = pas_compact_segregated_size_directory_ptr_load_non_null(
            &pas_segregated_view_get_exclusive(view)->directory);
        return pas_full_alloc_bits_create_for_exclusive(size_directory, page_config);
    }

    return pas_full_alloc_bits_create_for_partial(view);
}

PAS_END_EXTERN_C;

#endif /* PAS_FULL_ALLOC_BITS_INLINES_H */

