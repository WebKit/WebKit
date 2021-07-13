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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_ensure_heap_forced_into_reserved_memory.h"

#include "pas_create_basic_heap_page_caches_with_reserved_memory.h"
#include "pas_ensure_heap_with_page_caches.h"

pas_heap* pas_ensure_heap_forced_into_reserved_memory(
    pas_heap_ref* heap_ref,
    pas_heap_ref_kind heap_ref_kind,
    pas_heap_config* config,
    pas_heap_runtime_config* template_runtime_config,
    uintptr_t begin,
    uintptr_t end)
{
    return pas_ensure_heap_with_page_caches(
        heap_ref, heap_ref_kind, config, template_runtime_config,
        pas_create_basic_heap_page_caches_with_reserved_memory(
            (pas_basic_heap_runtime_config*)template_runtime_config, begin, end));
}

#endif /* LIBPAS_ENABLED */
