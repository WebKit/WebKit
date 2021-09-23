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

#ifndef PAS_PAGE_SHARING_POOL_SCAVENGE_RESULT_H
#define PAS_PAGE_SHARING_POOL_SCAVENGE_RESULT_H

#include "pas_page_sharing_pool_take_result.h"

PAS_BEGIN_EXTERN_C;

struct pas_page_sharing_pool_scavenge_result;
typedef struct pas_page_sharing_pool_scavenge_result pas_page_sharing_pool_scavenge_result;

struct pas_page_sharing_pool_scavenge_result {
    pas_page_sharing_pool_take_result take_result;
    size_t total_bytes;
};

static inline pas_page_sharing_pool_scavenge_result pas_page_sharing_pool_scavenge_result_create(
    pas_page_sharing_pool_take_result take_result,
    size_t total_bytes)
{
    pas_page_sharing_pool_scavenge_result result;
    result.take_result = take_result;
    result.total_bytes = total_bytes;
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_SHARING_POOL_SCAVENGE_RESULT_H */

