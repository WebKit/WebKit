/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_SIZE_DIRECTORY_H
#define PAS_SEGREGATED_SIZE_DIRECTORY_H

#include "pas_allocator_index.h"
#include "pas_config.h"
#include "pas_heap_config.h"
#include "pas_segregated_directory.h"

PAS_BEGIN_EXTERN_C;

PAS_API extern bool pas_segregated_size_directory_use_tabling;

PAS_API void pas_segregated_size_directory_construct(
    pas_segregated_directory* directory,
    pas_segregated_page_config_kind page_config_kind,
    pas_page_sharing_mode page_sharing_mode,
    pas_segregated_directory_kind kind);

typedef bool (*pas_segregated_size_directory_for_each_live_object_callback)(
    pas_segregated_directory* directory,
    pas_segregated_view view,
    uintptr_t begin,
    void* arg);

PAS_API bool pas_segregated_size_directory_for_each_live_object(
    pas_segregated_directory* directory,
    pas_segregated_size_directory_for_each_live_object_callback callback,
    void* arg);

PAS_API pas_segregated_directory* pas_segregated_size_directory_for_object(
    uintptr_t begin,
    pas_heap_config* config);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_SIZE_DIRECTORY_H */

