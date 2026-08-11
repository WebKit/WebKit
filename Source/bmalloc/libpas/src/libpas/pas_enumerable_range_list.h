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

#ifndef PAS_ENUMERABLE_RANGE_LIST_H
#define PAS_ENUMERABLE_RANGE_LIST_H

#include "pas_compact_atomic_enumerable_range_list_chunk_ptr.h"
#include "pas_range.h"

PAS_BEGIN_EXTERN_C;

struct pas_enumerable_range_list;
struct pas_enumerable_range_list_chunk;
typedef struct pas_enumerable_range_list pas_enumerable_range_list;
typedef struct pas_enumerable_range_list_chunk pas_enumerable_range_list_chunk;

#define PAS_ENUMERABLE_RANGE_LIST_CHUNK_SIZE 10

struct pas_enumerable_range_list {
    pas_compact_atomic_enumerable_range_list_chunk_ptr head;
};

struct pas_enumerable_range_list_chunk {
    pas_compact_atomic_enumerable_range_list_chunk_ptr next;
    unsigned num_entries;
    pas_range entries[PAS_ENUMERABLE_RANGE_LIST_CHUNK_SIZE];
};

PAS_API void pas_enumerable_range_list_append(pas_enumerable_range_list* list,
                                              pas_range range);

typedef bool (*pas_enumerable_range_list_iterate_callback)(pas_range range,
                                                           void* arg);

PAS_API bool pas_enumerable_range_list_iterate(
    pas_enumerable_range_list* list,
    pas_enumerable_range_list_iterate_callback callback,
    void* arg);

typedef bool (*pas_enumerable_range_list_iterate_remote_callback)(pas_enumerator* enumerator,
                                                                  pas_range range,
                                                                  void* arg);

PAS_API bool pas_enumerable_range_list_iterate_remote(
    pas_enumerable_range_list* remote_list,
    pas_enumerator* enumerator,
    pas_enumerable_range_list_iterate_remote_callback callback,
    void* arg);

PAS_END_EXTERN_C;

#endif /* PAS_ENUMERABLE_RANGE_LIST_H */

