/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LARGE_FREE_HEAP_DEFERRED_COMMIT_LOG_H
#define PAS_LARGE_FREE_HEAP_DEFERRED_COMMIT_LOG_H

#include "pas_large_virtual_range.h"
#include "pas_large_virtual_range_min_heap.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_free_heap_deferred_commit_log;
struct pas_physical_memory_transaction;
typedef struct pas_large_free_heap_deferred_commit_log pas_large_free_heap_deferred_commit_log;
typedef struct pas_physical_memory_transaction pas_physical_memory_transaction;

struct pas_large_free_heap_deferred_commit_log {
    pas_large_virtual_range_min_heap impl;
    size_t total; /* This is accurate so long as the ranges are non-overlapping. */
};

PAS_API void pas_large_free_heap_deferred_commit_log_construct(
    pas_large_free_heap_deferred_commit_log* log);

PAS_API void pas_large_free_heap_deferred_commit_log_destruct(
    pas_large_free_heap_deferred_commit_log* log);

PAS_API bool pas_large_free_heap_deferred_commit_log_add(
    pas_large_free_heap_deferred_commit_log* log,
    pas_large_virtual_range range,
    pas_physical_memory_transaction* transaction);

PAS_API void pas_large_free_heap_deferred_commit_log_commit_all(
    pas_large_free_heap_deferred_commit_log* log,
    pas_physical_memory_transaction* transaction);

/* Useful for writing tests. */
PAS_API void pas_large_free_heap_deferred_commit_log_pretend_to_commit_all(
    pas_large_free_heap_deferred_commit_log* log,
    pas_physical_memory_transaction* transaction);

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_FREE_HEAP_DEFERRED_COMMIT_LOG_H */

