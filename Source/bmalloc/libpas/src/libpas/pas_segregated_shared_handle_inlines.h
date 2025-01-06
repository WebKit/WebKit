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

#ifndef PAS_SEGREGATED_SHARED_HANDLE_INLINES_H
#define PAS_SEGREGATED_SHARED_HANDLE_INLINES_H

#include "pas_segregated_shared_handle.h"

#include "pas_segregated_shared_view.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_compact_atomic_segregated_partial_view_ptr*
pas_segregated_shared_handle_partial_view_ptr_for_index(
    pas_segregated_shared_handle* handle,
    size_t index,
    pas_segregated_page_config page_config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    index >>= page_config.sharing_shift;
    if (verbose)
        pas_log("sharing index = %zu\n", index);
    PAS_ASSERT(index < pas_segregated_shared_handle_num_views(page_config));
    return handle->partial_views + index;
}

static PAS_ALWAYS_INLINE pas_segregated_partial_view*
pas_segregated_shared_handle_partial_view_for_index(
    pas_segregated_shared_handle* handle,
    size_t index,
    pas_segregated_page_config page_config)
{
    return pas_compact_atomic_segregated_partial_view_ptr_load(
        pas_segregated_shared_handle_partial_view_ptr_for_index(handle, index, page_config));
}

static PAS_ALWAYS_INLINE pas_segregated_partial_view*
pas_segregated_shared_handle_partial_view_for_object(
    pas_segregated_shared_handle* handle,
    uintptr_t begin,
    pas_segregated_page_config page_config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    uintptr_t offset;
    uintptr_t index;

    if (verbose)
        pas_log("Looking up partial view for begin = %p.\n", (void*)begin);

    offset = pas_modulo_power_of_2(begin, page_config.base.page_size);
    index = pas_page_base_index_of_object_at_offset_from_page_boundary(offset, page_config.base);

    if (verbose)
        pas_log("offset = %zu, index = %zu\n", offset, index);

    return pas_segregated_shared_handle_partial_view_for_index(handle, index, page_config);
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_SHARED_HANDLE_INLINES_H */


