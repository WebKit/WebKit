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

#ifndef PAS_BASIC_HEAP_CONFIG_ENUMERATOR_DATA_H
#define PAS_BASIC_HEAP_CONFIG_ENUMERATOR_DATA_H

#include "pas_ptr_hash_map.h"

PAS_BEGIN_EXTERN_C;

struct pas_basic_heap_config_enumerator_data;
struct pas_enumerator;
struct pas_page_header_table;
typedef struct pas_basic_heap_config_enumerator_data pas_basic_heap_config_enumerator_data;
typedef struct pas_enumerator pas_enumerator;
typedef struct pas_page_header_table pas_page_header_table;

struct pas_basic_heap_config_enumerator_data {
    pas_ptr_hash_map page_header_table;
};

PAS_API bool pas_basic_heap_config_enumerator_data_add_page_header_table(
    pas_basic_heap_config_enumerator_data* data,
    pas_enumerator* enumerator,
    pas_page_header_table* page_header_table);

PAS_END_EXTERN_C;

#endif /* PAS_BASIC_HEAP_CONFIG_ENUMERATOR_DATA_H */

