/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef PAS_SHARED_HANDLE_OR_PAGE_H
#define PAS_SHARED_HANDLE_OR_PAGE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_page;
struct pas_segregated_shared_handle;
struct pas_shared_handle_or_page_opaque;
typedef struct pas_segregated_page pas_segregated_page;
typedef struct pas_segregated_shared_handle pas_segregated_shared_handle;
typedef struct pas_shared_handle_or_page_opaque* pas_shared_handle_or_page;

#define PAS_IS_SHARED_HANDLE_BIT ((uintptr_t)1)

static inline pas_shared_handle_or_page
pas_wrap_page(pas_segregated_page* page)
{
    return (pas_shared_handle_or_page)page;
}

static inline bool
pas_is_wrapped_shared_handle(pas_shared_handle_or_page shared_handle_or_page)
{
    return (uintptr_t)shared_handle_or_page & PAS_IS_SHARED_HANDLE_BIT;
}

static inline bool
pas_is_wrapped_page(pas_shared_handle_or_page shared_handle_or_page)
{
    return !pas_is_wrapped_shared_handle(shared_handle_or_page);
}

static inline pas_segregated_page*
pas_unwrap_page(pas_shared_handle_or_page shared_handle_or_page)
{
    PAS_ASSERT(pas_is_wrapped_page(shared_handle_or_page));
    return (pas_segregated_page*)shared_handle_or_page;
}

PAS_END_EXTERN_C;

#endif /* PAS_SHARED_HANDLE_OR_PAGE_H */

