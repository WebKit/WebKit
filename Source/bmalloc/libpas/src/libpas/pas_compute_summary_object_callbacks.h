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

#ifndef PAS_COMPUTE_SUMMARY_OBJECT_CALLBACKS_H
#define PAS_COMPUTE_SUMMARY_OBJECT_CALLBACKS_H

#include "pas_heap_config.h"
#include "pas_large_free.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

PAS_API bool pas_compute_summary_live_object_callback(
    uintptr_t begin,
    uintptr_t end,
    void* arg);

PAS_API bool pas_compute_summary_live_object_callback_without_physical_sharing(
    uintptr_t begin,
    uintptr_t end,
    void* arg);

PAS_API bool (*pas_compute_summary_live_object_callback_for_config(const pas_heap_config* config))(
    uintptr_t begin,
    uintptr_t end,
    void* arg);

PAS_API bool pas_compute_summary_dead_object_callback(
    pas_large_free free,
    void* arg);

PAS_API bool pas_compute_summary_dead_object_callback_without_physical_sharing(
    pas_large_free free,
    void* arg);

PAS_API bool (*pas_compute_summary_dead_object_callback_for_config(const pas_heap_config* config))(
    pas_large_free free,
    void* arg);

PAS_END_EXTERN_C;

#endif /* PAS_COMPUTE_SUMMARY_OBJECT_CALLBACKS_H */

