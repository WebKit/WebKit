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

#ifndef PAS_SIMPLE_TYPE_H
#define PAS_SIMPLE_TYPE_H

#include "pas_config.h"

#include "pas_heap_ref.h"
#include "pas_utils.h"
#include <stdio.h>

PAS_BEGIN_EXTERN_C;

#define PAS_SIMPLE_TYPE_DATA_BIT (PAS_NUM_PTR_BITS - 1)
#define PAS_SIMPLE_TYPE_DATA_PTR_MASK (((uintptr_t)1 << PAS_SIMPLE_TYPE_DATA_BIT) - 1)
#define PAS_SIMPLE_TYPE_NUM_ALIGNMENT_BITS 5
#define PAS_SIMPLE_TYPE_ALIGNMENT_SHIFT (PAS_SIMPLE_TYPE_DATA_BIT - \
                                         PAS_SIMPLE_TYPE_NUM_ALIGNMENT_BITS)
#define PAS_SIMPLE_TYPE_ALIGNMENT_MASK ((((uintptr_t)1 << PAS_SIMPLE_TYPE_NUM_ALIGNMENT_BITS) \
                                         - 1) \
                                        << PAS_SIMPLE_TYPE_ALIGNMENT_SHIFT)
#define PAS_SIMPLE_TYPE_SIZE_MASK (((uintptr_t)1 << PAS_SIMPLE_TYPE_ALIGNMENT_SHIFT) - 1)

typedef uintptr_t pas_simple_type;

struct pas_simple_type_with_key_data;
struct pas_stream;
typedef struct pas_simple_type_with_key_data pas_simple_type_with_key_data;
typedef struct pas_stream pas_stream;

struct pas_simple_type_with_key_data {
    uintptr_t simple_type;
    const void* key;
};

/* NOTE: to get default alignment, use alignment=1. This means to defer the alignment decision
   to the the minalign settings of the heap you're allocating in. */
#define PAS_SIMPLE_TYPE_CREATE(size, alignment) \
    ((size) | ((pas_simple_type)__builtin_ctzll(alignment) \
               << PAS_SIMPLE_TYPE_ALIGNMENT_SHIFT))

#define PAS_SIMPLE_TYPE_SIZE(type) \
    ((type) << PAS_SIMPLE_TYPE_NUM_ALIGNMENT_BITS >> PAS_SIMPLE_TYPE_NUM_ALIGNMENT_BITS)

#define PAS_SIMPLE_TYPE_ALIGNMENT(type) \
    ((size_t)1 << ((type) >> PAS_SIMPLE_TYPE_ALIGNMENT_SHIFT))

static inline bool pas_simple_type_has_key(pas_simple_type type)
{
    return type >> PAS_SIMPLE_TYPE_DATA_BIT;
}

static inline const pas_simple_type_with_key_data* pas_simple_type_get_key_data(pas_simple_type type)
{
    PAS_ASSERT(pas_simple_type_has_key(type));
    return (const pas_simple_type_with_key_data*)(type & PAS_SIMPLE_TYPE_DATA_PTR_MASK);
}

static inline pas_simple_type pas_simple_type_unwrap(pas_simple_type type)
{
    if (pas_simple_type_has_key(type))
        return pas_simple_type_get_key_data(type)->simple_type;
    return type;
}

/* It's important that this function can be DCE'd. */
static inline size_t pas_simple_type_size(pas_simple_type type)
{
    return pas_simple_type_unwrap(type) & PAS_SIMPLE_TYPE_SIZE_MASK;
}

/* It's important that this function can be DCE'd. */
static inline size_t pas_simple_type_alignment(pas_simple_type type)
{
    return (size_t)1 << ((pas_simple_type_unwrap(type) & PAS_SIMPLE_TYPE_ALIGNMENT_MASK)
                         >> PAS_SIMPLE_TYPE_ALIGNMENT_SHIFT);
}

static inline const void* pas_simple_type_key(pas_simple_type type)
{
    return pas_simple_type_get_key_data(type)->key;
}

static inline pas_simple_type pas_simple_type_create(size_t size, size_t alignment)
{
    pas_simple_type result;
    
    result = PAS_SIMPLE_TYPE_CREATE(size, alignment);
    
    PAS_ASSERT(pas_simple_type_size(result) == size);
    PAS_ASSERT(pas_simple_type_alignment(result) == alignment);
    PAS_ASSERT(!pas_simple_type_has_key(result));
    
    return result;
}

static inline pas_simple_type pas_simple_type_create_with_key_data(
    const pas_simple_type_with_key_data* data)
{
    pas_simple_type result;
    
    result = ((uintptr_t)1 << PAS_SIMPLE_TYPE_DATA_BIT) | (uintptr_t)data;

    PAS_ASSERT(pas_simple_type_has_key(result));
    PAS_ASSERT(pas_simple_type_get_key_data(result) == data);

    return result;
}

PAS_API void pas_simple_type_dump(pas_simple_type type, pas_stream* stream);

static inline size_t pas_simple_type_as_heap_type_get_type_size(const pas_heap_type* type)
{
    return pas_simple_type_size((pas_simple_type)type);
}

static inline size_t pas_simple_type_as_heap_type_get_type_alignment(const pas_heap_type* type)
{
    return pas_simple_type_alignment((pas_simple_type)type);
}

PAS_API void pas_simple_type_as_heap_type_dump(const pas_heap_type* type, pas_stream* stream);

PAS_END_EXTERN_C;

#endif /* PAS_SIMPLE_TYPE_H */

