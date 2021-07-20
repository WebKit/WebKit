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

#ifndef PAS_FAST_MEGAPAGE_CACHE_H
#define PAS_FAST_MEGAPAGE_CACHE_H

#include "pas_config.h"
#include "pas_fast_megapage_kind.h"
#include "pas_page_header_placement_mode.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_fast_megapage_table;
struct pas_megapage_cache;
struct pas_page_base_config;
typedef struct pas_fast_megapage_table pas_fast_megapage_table;
typedef struct pas_megapage_cache pas_megapage_cache;
typedef struct pas_page_base_config pas_page_base_config;

/* Allocates a small page aligned the way that the type-safe heap demands and returns the
   base on the allocation. This is guaranteed to returned zeroed memory. */
PAS_API void* pas_fast_megapage_cache_try_allocate(
    pas_megapage_cache* cache,
    pas_fast_megapage_table* table,
    pas_page_base_config* config,
    pas_fast_megapage_kind kind,
    bool should_zero);

PAS_END_EXTERN_C;

#endif /* PAS_FAST_MEGAPAGE_CACHE_H */

