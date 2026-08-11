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

#ifndef PAS_PAGE_SHARING_POOL_TAKE_RESULT_H
#define PAS_PAGE_SHARING_POOL_TAKE_RESULT_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_page_sharing_pool_take_result {
    pas_page_sharing_pool_take_none_available,
    pas_page_sharing_pool_take_none_within_max_epoch,
    pas_page_sharing_pool_take_locks_unavailable,
    pas_page_sharing_pool_take_success,
};

typedef enum pas_page_sharing_pool_take_result pas_page_sharing_pool_take_result;

static inline const char*
pas_page_sharing_pool_take_result_get_string(pas_page_sharing_pool_take_result result)
{
    switch (result) {
    case pas_page_sharing_pool_take_none_available:
        return "none_available";
    case pas_page_sharing_pool_take_none_within_max_epoch:
        return "none_within_max_epoch";
    case pas_page_sharing_pool_take_locks_unavailable:
        return "locks_unavailable";
    case pas_page_sharing_pool_take_success:
        return "success";
    }
    PAS_ASSERT(!"Bad take result");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_SHARING_POOL_TAKE_RESULT_H */

