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

#ifndef PAS_COMMIT_SPAN_H
#define PAS_COMMIT_SPAN_H

#include "pas_lock.h"

PAS_BEGIN_EXTERN_C;

struct pas_commit_span;
struct pas_deferred_decommit_log;
struct pas_page_base;
struct pas_page_base_config;
typedef struct pas_commit_span pas_commit_span;
typedef struct pas_deferred_decommit_log pas_deferred_decommit_log;
typedef struct pas_page_base pas_page_base;
typedef struct pas_page_base_config pas_page_base_config;

struct pas_commit_span {
    uintptr_t index_of_start_of_span;
    bool did_add_first;
    size_t total_bytes;
};

PAS_API void pas_commit_span_construct(pas_commit_span* span);
PAS_API void pas_commit_span_add_to_change(pas_commit_span* span, uintptr_t granule_index);
PAS_API void pas_commit_span_add_unchanged(pas_commit_span* span,
                                           pas_page_base* page,
                                           uintptr_t granule_index,
                                           pas_page_base_config* config,
                                           void (*commit_or_decommit)(
                                               void* base, size_t size, void* arg),
                                           void* arg);
PAS_API void pas_commit_span_add_unchanged_and_commit(pas_commit_span* span,
                                                      pas_page_base* page,
                                                      uintptr_t granule_index,
                                                      pas_page_base_config* config);
PAS_API void pas_commit_span_add_unchanged_and_decommit(pas_commit_span* span,
                                                        pas_page_base* page,
                                                        uintptr_t granule_index,
                                                        pas_deferred_decommit_log* log,
                                                        pas_lock* commit_lock,
                                                        pas_page_base_config* config,
                                                        pas_lock_hold_mode heap_lock_hold_mode);

PAS_END_EXTERN_C;

#endif /* PAS_COMMIT_SPAN_H */

