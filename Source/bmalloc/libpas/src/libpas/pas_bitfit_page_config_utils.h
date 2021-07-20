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

#ifndef PAS_BITFIT_PAGE_CONFIG_UTILS_H
#define PAS_BITFIT_PAGE_CONFIG_UTILS_H

#include "pas_config.h"
#include "pas_page_base_config_utils.h"
#include "pas_bitfit_page.h"
#include "pas_bitfit_page_config.h"
#include "pas_bitfit_page_inlines.h"

PAS_BEGIN_EXTERN_C;

#define PAS_BASIC_BITFIT_PAGE_CONFIG_FORWARD_DECLARATIONS(name) \
    PAS_BITFIT_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(name ## _page_config); \
    PAS_BASIC_PAGE_BASE_CONFIG_FORWARD_DECLARATIONS(name)

typedef struct {
    pas_page_header_placement_mode header_placement_mode;
    pas_page_header_table* header_table;
} pas_basic_bitfit_page_config_declarations_arguments;

#define PAS_BASIC_BITFIT_PAGE_CONFIG_DECLARATIONS(name, config_value, ...) \
    PAS_BASIC_PAGE_BASE_CONFIG_DECLARATIONS( \
        name, (config_value).base, \
        .header_placement_mode = \
            ((pas_basic_bitfit_page_config_declarations_arguments){__VA_ARGS__}) \
            .header_placement_mode, \
        .header_table = \
            ((pas_basic_bitfit_page_config_declarations_arguments){__VA_ARGS__}) \
            .header_table)

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_PAGE_CONFIG_UTILS_H */

