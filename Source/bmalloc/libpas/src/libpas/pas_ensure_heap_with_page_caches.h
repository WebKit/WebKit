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

#ifndef PAS_ENSURE_HEAP_WITH_PAGE_CACHES_H
#define PAS_ENSURE_HEAP_WITH_PAGE_CACHES_H

#include "pas_heap_ref.h"

PAS_BEGIN_EXTERN_C;

struct pas_basic_heap_page_caches;
typedef struct pas_basic_heap_page_caches pas_basic_heap_page_caches;

/* To call this function, the heap_ref must still not be initialized. Also, the heap must be
   one of the "basic" ones - created with pas_heap_config_utils or something that broadly uses
   the same defaults. In particular, it must be the kind of heap that expects the runtime_config
   to be a pas_basic_heap_runtime_config. This will copy the runtime_config you pass and combine
   it with the basic_heap_page_caches to create a new pas_basic_heap_runtime_config. */
PAS_API pas_heap* pas_ensure_heap_with_page_caches(
    pas_heap_ref* heap_ref,
    pas_heap_ref_kind heap_ref_kind,
    pas_heap_config* config,
    pas_heap_runtime_config* template_runtime_config,
    pas_basic_heap_page_caches* page_caches);

PAS_END_EXTERN_C;

#endif /* PAS_ENSURE_HEAP_WITH_PAGE_CACHES_H */

