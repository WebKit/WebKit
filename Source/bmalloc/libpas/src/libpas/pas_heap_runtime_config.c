/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "pas_heap_runtime_config.h"

#include "pas_designated_intrinsic_heap_inlines.h"

uint8_t pas_heap_runtime_config_view_cache_capacity_for_object_size(
    pas_heap_runtime_config* config,
    size_t object_size,
    pas_segregated_page_config* page_config)
{
    size_t result;

    result = config->view_cache_capacity_for_object_size(object_size, page_config);

    PAS_ASSERT((uint8_t)result == result);
    return (uint8_t)result;
}

size_t pas_heap_runtime_config_zero_view_cache_capacity(
    size_t object_size, pas_segregated_page_config* page_config)
{
    PAS_UNUSED_PARAM(object_size);
    PAS_UNUSED_PARAM(page_config);
    return 0;
}

size_t pas_heap_runtime_config_aggressive_view_cache_capacity(
    size_t object_size, pas_segregated_page_config* page_config)
{
    static const size_t cache_size = 1638400;

    PAS_UNUSED_PARAM(object_size);

    PAS_ASSERT(page_config->base.page_size < cache_size);

    return cache_size / page_config->base.page_size;
}

#endif /* LIBPAS_ENABLED */

