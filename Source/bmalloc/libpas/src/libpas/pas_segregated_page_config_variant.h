/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_PAGE_CONFIG_VARIANT_H
#define PAS_SEGREGATED_PAGE_CONFIG_VARIANT_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* Every segregated heap has two page variants - small and medium. This tells us which. */
enum pas_segregated_page_config_variant {
    pas_small_segregated_page_config_variant,
    pas_medium_segregated_page_config_variant
};

typedef enum pas_segregated_page_config_variant pas_segregated_page_config_variant;

#define PAS_NUM_SEGREGATED_PAGE_CONFIG_VARIANTS 2

#define PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_ASCENDING(variable) \
    variable = pas_small_segregated_page_config_variant; \
    variable <= pas_medium_segregated_page_config_variant; \
    variable = (pas_segregated_page_config_variant)((unsigned)variable + 1)

#define PAS_EACH_SEGREGATED_PAGE_CONFIG_VARIANT_DESCENDING(variable) \
    variable = pas_medium_segregated_page_config_variant; \
    (int)variable >= (int)pas_small_segregated_page_config_variant; \
    variable = (pas_segregated_page_config_variant)((unsigned)variable - 1)

static inline const char*
pas_segregated_page_config_variant_get_string(pas_segregated_page_config_variant variant)
{
    switch (variant) {
    case pas_small_segregated_page_config_variant:
        return "small";
    case pas_medium_segregated_page_config_variant:
        return "medium";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_CONFIG_VARIANT_H */

