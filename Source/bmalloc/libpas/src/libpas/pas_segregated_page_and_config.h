/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_PAGE_AND_CONFIG_H
#define PAS_SEGREGATED_PAGE_AND_CONFIG_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_page;
struct pas_segregated_page_and_config;
struct pas_segregated_page_config;
typedef struct pas_segregated_page pas_segregated_page;
typedef struct pas_segregated_page_and_config pas_segregated_page_and_config;
typedef struct pas_segregated_page_config pas_segregated_page_config;

struct pas_segregated_page_and_config {
    pas_segregated_page* page;
    pas_segregated_page_config* config;
};

static inline pas_segregated_page_and_config
pas_segregated_page_and_config_create(pas_segregated_page* page,
                                      pas_segregated_page_config* config)
{
    pas_segregated_page_and_config result;
    PAS_ASSERT(!!page == !!config);
    result.page = page;
    result.config = config;
    return result;
}

static inline pas_segregated_page_and_config
pas_segregated_page_and_config_create_empty(void)
{
    return pas_segregated_page_and_config_create(NULL, NULL);
}

static inline bool pas_segregated_page_and_config_is_empty(
    pas_segregated_page_and_config page_and_config)
{
    PAS_ASSERT(!!page_and_config.page == !!page_and_config.config);
    return !page_and_config.page;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_AND_CONFIG_H */


