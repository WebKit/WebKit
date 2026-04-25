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

#ifndef PAS_SHARED_HANDLE_OR_PAGE_BOUNDARY_INLINES_H
#define PAS_SHARED_HANDLE_OR_PAGE_BOUNDARY_INLINES_H

#include "pas_shared_handle_or_page_boundary.h"

#include "pas_segregated_page.h"
#include "pas_segregated_shared_handle.h"
#include "pas_utility_heap_config.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE pas_shared_handle_or_page_boundary
pas_wrap_shared_handle(pas_segregated_shared_handle* handle,
                       pas_segregated_page_config page_config)
{
    if (!pas_segregated_page_config_is_utility(page_config)) {
        PAS_TESTING_ASSERT(pas_segregated_page_is_allocated(
                               (uintptr_t)handle, PAS_UTILITY_HEAP_CONFIG.small_segregated_config));
    }
    return (pas_shared_handle_or_page_boundary)((uintptr_t)handle | PAS_IS_SHARED_HANDLE_BIT);
}

static inline pas_segregated_shared_handle*
pas_unwrap_shared_handle_no_liveness_checks(pas_shared_handle_or_page_boundary shared_handle_or_page)
{
    PAS_ASSERT(pas_is_wrapped_shared_handle(shared_handle_or_page));
    return (pas_segregated_shared_handle*)(
        (uintptr_t)shared_handle_or_page & ~PAS_IS_SHARED_HANDLE_BIT);
}

static PAS_ALWAYS_INLINE pas_segregated_shared_handle*
pas_unwrap_shared_handle(pas_shared_handle_or_page_boundary shared_handle_or_page,
                         pas_segregated_page_config page_config)
{
    pas_segregated_shared_handle* result;
    result = pas_unwrap_shared_handle_no_liveness_checks(shared_handle_or_page);
    if (!pas_segregated_page_config_is_utility(page_config)) {
        PAS_TESTING_ASSERT(pas_segregated_page_is_allocated(
                               (uintptr_t)result, PAS_UTILITY_HEAP_CONFIG.small_segregated_config));
    }
    return result;
}

static PAS_ALWAYS_INLINE void*
pas_shared_handle_or_page_boundary_get_page_boundary_no_liveness_checks(
    pas_shared_handle_or_page_boundary shared_handle_or_page)
{
    if (pas_is_wrapped_shared_handle(shared_handle_or_page))
        return pas_unwrap_shared_handle_no_liveness_checks(shared_handle_or_page)->page_boundary;

    return pas_unwrap_page_boundary(shared_handle_or_page);
}

static PAS_ALWAYS_INLINE void*
pas_shared_handle_or_page_boundary_get_page_boundary(
    pas_shared_handle_or_page_boundary shared_handle_or_page,
    pas_segregated_page_config page_config)
{
    if (pas_is_wrapped_shared_handle(shared_handle_or_page))
        return pas_unwrap_shared_handle(shared_handle_or_page, page_config)->page_boundary;

    return pas_unwrap_page_boundary(shared_handle_or_page);
}

PAS_END_EXTERN_C;

#endif /* PAS_SHARED_HANDLE_OR_PAGE_BOUNDARY_INLINES_H */

