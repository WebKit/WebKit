/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_PAGE_KIND_H
#define PAS_PAGE_KIND_H

#include "pas_bitfit_page_config_variant.h"
#include "pas_page_config_kind.h"
#include "pas_segregated_page_config_variant.h"

PAS_BEGIN_EXTERN_C;

enum pas_page_kind {
    pas_small_segregated_page_kind = 1, /* We don't want zero-initialized memory to look like it has
                                           a kind. */
    pas_medium_segregated_page_kind,
    pas_small_bitfit_page_kind,
    pas_medium_bitfit_page_kind,
    pas_marge_bitfit_page_kind
};

typedef enum pas_page_kind pas_page_kind;

static inline const char* pas_page_kind_get_string(pas_page_kind page_kind)
{
    switch (page_kind) {
    case pas_small_segregated_page_kind:
        return "small_segregated";
    case pas_medium_segregated_page_kind:
        return "medium_segregated";
    case pas_small_bitfit_page_kind:
        return "small_bitfit";
    case pas_medium_bitfit_page_kind:
        return "medium_bitfit";
    case pas_marge_bitfit_page_kind:
        return "marge_bitfit";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static inline pas_page_config_kind pas_page_kind_get_config_kind(pas_page_kind page_kind)
{
    switch (page_kind) {
    case pas_small_segregated_page_kind:
    case pas_medium_segregated_page_kind:
        return pas_page_config_kind_segregated;
    case pas_small_bitfit_page_kind:
    case pas_medium_bitfit_page_kind:
    case pas_marge_bitfit_page_kind:
        return pas_page_config_kind_bitfit;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_page_config_kind_segregated;
}

static inline pas_segregated_page_config_variant
pas_page_kind_get_segregated_variant(pas_page_kind page_kind)
{
    switch (page_kind) {
    case pas_small_segregated_page_kind:
        return pas_small_segregated_page_config_variant;
    case pas_medium_segregated_page_kind:
        return pas_medium_segregated_page_config_variant;
    default:
        PAS_ASSERT(!"Should not be reached");
        return pas_small_segregated_page_config_variant;
    }
}

static inline pas_bitfit_page_config_variant
pas_page_kind_get_bitfit_variant(pas_page_kind page_kind)
{
    switch (page_kind) {
    case pas_small_bitfit_page_kind:
        return pas_small_bitfit_page_config_variant;
    case pas_medium_bitfit_page_kind:
        return pas_medium_bitfit_page_config_variant;
    case pas_marge_bitfit_page_kind:
        return pas_marge_bitfit_page_config_variant;
    default:
        PAS_ASSERT(!"Should not be reached");
        return pas_small_bitfit_page_config_variant;
    }
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_KIND_H */

