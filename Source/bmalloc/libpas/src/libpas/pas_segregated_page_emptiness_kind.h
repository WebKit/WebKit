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

#ifndef PAS_SEGREGATED_PAGE_EMPTINESS_KIND_H
#define PAS_SEGREGATED_PAGE_EMPTINESS_KIND_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_page_emptiness_kind {
    pas_page_partial_emptiness,
    pas_page_full_emptiness
};

typedef enum pas_page_emptiness_kind pas_page_emptiness_kind;

static inline pas_page_emptiness_kind
pas_page_emptiness_kind_get_inverted(pas_page_emptiness_kind kind)
{
    switch (kind) {
    case pas_page_partial_emptiness:
        return pas_page_full_emptiness;
    case pas_page_full_emptiness:
        return pas_page_partial_emptiness;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_page_partial_emptiness;
}

static inline const char*
pas_page_emptiness_kind_get_string(pas_page_emptiness_kind kind)
{
    switch (kind) {
    case pas_page_partial_emptiness:
        return "partial";
    case pas_page_full_emptiness:
        return "full";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_EMPTINESS_KIND_H */

