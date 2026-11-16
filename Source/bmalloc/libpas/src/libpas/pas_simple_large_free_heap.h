/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#ifndef PAS_SIMPLE_LARGE_FREE_HEAP_H
#define PAS_SIMPLE_LARGE_FREE_HEAP_H

#include "pas_alignment.h"
#include "pas_allocation_result.h"
#include "pas_large_free_visitor.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_free;
struct pas_simple_large_free_heap;
struct pas_large_free_heap_config;
typedef struct pas_large_free pas_large_free;
typedef struct pas_simple_large_free_heap pas_simple_large_free_heap;
typedef struct pas_large_free_heap_config pas_large_free_heap_config;

struct pas_simple_large_free_heap {
    pas_large_free* free_list;
    size_t free_list_size; /* in units of pas_large_free, not byte size. */
    size_t free_list_capacity; /* in units of pas_large_free, not byte size. */
    size_t num_mapped_bytes;
};

/* Note that all of these functions have to be called with the heap lock held. */

#define PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER { \
        .free_list = NULL, \
        .free_list_size = 0, \
        .free_list_capacity = 0, \
        .num_mapped_bytes = 0 \
    }

PAS_API void pas_simple_large_free_heap_construct(pas_simple_large_free_heap* heap);

PAS_API pas_allocation_result
pas_simple_large_free_heap_try_allocate(pas_simple_large_free_heap* heap,
                                        size_t size,
                                        pas_alignment alignment,
                                        pas_large_free_heap_config* config);

PAS_API void pas_simple_large_free_heap_deallocate(pas_simple_large_free_heap* heap,
                                                   uintptr_t begin,
                                                   uintptr_t end,
                                                   pas_zero_mode zero_mode,
                                                   pas_large_free_heap_config* config);

PAS_API void pas_simple_large_free_heap_for_each_free(pas_simple_large_free_heap* heap,
                                                      pas_large_free_visitor visitor,
                                                      void* arg);

PAS_API size_t pas_simple_large_free_heap_get_num_free_bytes(pas_simple_large_free_heap* heap);

/* This is a hilarious function that only works if pasmalloc is not the system malloc. */
PAS_API void pas_simple_large_free_heap_dump_to_printf(pas_simple_large_free_heap* heap);

PAS_END_EXTERN_C;

#endif /* PAS_SIMPLE_LARGE_FREE_HEAP_H */

