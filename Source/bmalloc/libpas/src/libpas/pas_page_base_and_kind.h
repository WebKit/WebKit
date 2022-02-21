/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_PAGE_BASE_AND_KIND_H
#define PAS_PAGE_BASE_AND_KIND_H

#include "pas_page_base.h"

PAS_BEGIN_EXTERN_C;

struct pas_page_base;
struct pas_page_base_and_kind;
typedef struct pas_page_base pas_page_base;
typedef struct pas_page_base_and_kind pas_page_base_and_kind;

struct pas_page_base_and_kind {
    pas_page_base* page_base;
    pas_page_kind page_kind;
};

static inline pas_page_base_and_kind pas_page_base_and_kind_create(pas_page_base* page_base,
                                                                   pas_page_kind page_kind)
{
    pas_page_base_and_kind result;
    PAS_TESTING_ASSERT(page_base);
    PAS_TESTING_ASSERT(pas_page_base_get_kind(page_base) == page_kind);
    result.page_base = page_base;
    result.page_kind = page_kind;
    return result;
}

static inline pas_page_base_and_kind pas_page_base_and_kind_create_empty(void)
{
    pas_page_base_and_kind result;
    pas_zero_memory(&result, sizeof(result));
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_BASE_AND_KIND_H */

