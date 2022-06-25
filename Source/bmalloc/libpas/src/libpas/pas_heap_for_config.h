/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef PAS_HEAP_FOR_CONFIG_H
#define PAS_HEAP_FOR_CONFIG_H

#include "pas_heap_config.h"
#include "pas_segregated_page_config.h"

PAS_BEGIN_EXTERN_C;

struct pas_allocation_config;
typedef struct pas_allocation_config pas_allocation_config;

PAS_API extern bool pas_heap_for_config_force_bootstrap;

PAS_API void* pas_heap_for_config_allocate(
    pas_heap_config* config,
    size_t size,
    const char* name);

PAS_API void* pas_heap_for_page_config_kind_allocate(
    pas_segregated_page_config_kind page_config_kind,
    size_t size,
    const char* name);

PAS_API void* pas_heap_for_page_config_allocate(
    pas_segregated_page_config* page_config,
    size_t size,
    const char* name);

PAS_API void* pas_heap_for_config_allocate_with_alignment(
    pas_heap_config* config,
    size_t size,
    size_t alignment,
    const char* name);

PAS_API void* pas_heap_for_page_config_allocate_with_alignment(
    pas_segregated_page_config* page_config,
    size_t size,
    size_t alignment,
    const char* name);

PAS_API void* pas_heap_for_config_allocate_with_manual_alignment(
    pas_heap_config* config,
    size_t size,
    size_t alignment,
    const char* name);

PAS_API void* pas_heap_for_page_config_kind_allocate_with_manual_alignment(
    pas_segregated_page_config_kind page_config_kind,
    size_t size,
    size_t alignment,
    const char* name);

PAS_API void* pas_heap_for_page_config_allocate_with_manual_alignment(
    pas_segregated_page_config* page_config,
    size_t size,
    size_t alignment,
    const char* name);

PAS_API void pas_heap_for_config_deallocate(
    pas_heap_config* config,
    void* ptr,
    size_t size);

PAS_API void pas_heap_for_page_config_kind_deallocate(
    pas_segregated_page_config_kind config_kind,
    void* ptr,
    size_t size);

PAS_API void pas_heap_for_page_config_deallocate(
    pas_segregated_page_config* config,
    void* ptr,
    size_t size);

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_FOR_CONFIG_H */

