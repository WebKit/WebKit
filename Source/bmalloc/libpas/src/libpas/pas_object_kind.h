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

#ifndef PAS_OBJECT_KIND_H
#define PAS_OBJECT_KIND_H

#include "pas_bitfit_page_config_variant.h"
#include "pas_page_kind.h"
#include "pas_segregated_page_config_variant.h"

PAS_BEGIN_EXTERN_C;

enum pas_object_kind {
    pas_not_an_object_kind,
    pas_small_segregated_object_kind,
    pas_medium_segregated_object_kind,
    pas_small_bitfit_object_kind,
    pas_medium_bitfit_object_kind,
    pas_marge_bitfit_object_kind,
    pas_large_object_kind
};

typedef enum pas_object_kind pas_object_kind;

static inline const char* pas_object_kind_get_string(pas_object_kind object_kind)
{
    switch (object_kind) {
    case pas_small_segregated_object_kind:
        return "small_segregated";
    case pas_medium_segregated_object_kind:
        return "medium_segregated";
    case pas_small_bitfit_object_kind:
        return "small_bitfit";
    case pas_medium_bitfit_object_kind:
        return "medium_bitfit";
    case pas_marge_bitfit_object_kind:
        return "marge_bitfit";
    case pas_large_object_kind:
        return "large";
    case pas_not_an_object_kind:
        return "not_an_object";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static inline pas_object_kind
pas_object_kind_for_segregated_variant(pas_segregated_page_config_variant variant)
{
    switch (variant) {
    case pas_small_segregated_page_config_variant:
        return pas_small_segregated_object_kind;
    case pas_medium_segregated_page_config_variant:
        return pas_medium_segregated_object_kind;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_not_an_object_kind;
}

static inline pas_object_kind
pas_object_kind_for_bitfit_variant(pas_bitfit_page_config_variant variant)
{
    switch (variant) {
    case pas_small_bitfit_page_config_variant:
        return pas_small_bitfit_object_kind;
    case pas_medium_bitfit_page_config_variant:
        return pas_medium_bitfit_object_kind;
    case pas_marge_bitfit_page_config_variant:
        return pas_marge_bitfit_object_kind;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_not_an_object_kind;
}

static inline pas_object_kind pas_object_kind_for_page_kind(pas_page_kind page_kind)
{
    switch (page_kind) {
    case pas_small_exclusive_segregated_page_kind:
    case pas_small_shared_segregated_page_kind:
        return pas_small_segregated_object_kind;
    case pas_medium_exclusive_segregated_page_kind:
    case pas_medium_shared_segregated_page_kind:
        return pas_medium_segregated_object_kind;
    case pas_small_bitfit_page_kind:
        return pas_small_bitfit_object_kind;
    case pas_medium_bitfit_page_kind:
        return pas_medium_bitfit_object_kind;
    case pas_marge_bitfit_page_kind:
        return pas_marge_bitfit_object_kind;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_not_an_object_kind;
}

PAS_END_EXTERN_C;

#endif /* PAS_OBJECT_KIND_H */

