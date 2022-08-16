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

#ifndef PAS_BITFIT_PAGE_CONFIG_KIND_H
#define PAS_BITFIT_PAGE_CONFIG_KIND_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_page_base_config;
struct pas_bitfit_page_config;
typedef struct pas_page_base_config pas_page_base_config;
typedef struct pas_bitfit_page_config pas_bitfit_page_config;

enum pas_bitfit_page_config_kind {
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) \
    pas_bitfit_page_config_kind_ ## name,
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
};

typedef enum pas_bitfit_page_config_kind pas_bitfit_page_config_kind;

PAS_API const char* pas_bitfit_page_config_kind_get_string(pas_bitfit_page_config_kind kind);

typedef bool (*pas_bitfit_page_config_kind_callback)(pas_bitfit_page_config_kind kind,
                                                         const pas_bitfit_page_config* config,
                                                         void* arg);

PAS_API bool pas_bitfit_page_config_kind_for_each(
    pas_bitfit_page_config_kind_callback callback,
    void *arg);

PAS_API extern const pas_page_base_config* pas_bitfit_page_config_kind_for_config_table[];

static inline const pas_bitfit_page_config* pas_bitfit_page_config_kind_get_config(
    pas_bitfit_page_config_kind kind)
{
    return (const pas_bitfit_page_config*)pas_bitfit_page_config_kind_for_config_table[kind];
}

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_PAGE_CONFIG_KIND_H */

