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

#ifndef BMALLOC_TYPE_H
#define BMALLOC_TYPE_H

#include "pas_simple_type.h"

PAS_BEGIN_EXTERN_C;

struct bmalloc_type;
typedef struct bmalloc_type bmalloc_type;

struct bmalloc_type {
    unsigned size;
    unsigned alignment;
    const char* name;
};

#define BMALLOC_TYPE_INITIALIZER(passed_size, passed_alignment, passed_name) \
    ((bmalloc_type){ \
         .size = (passed_size), \
         .alignment = (passed_alignment), \
         .name = (passed_name) \
     })

/* It's a bit better to use these getters instead of accessing the type struct directly because we want to be
   able to change the shape of the struct. */
static inline size_t bmalloc_type_size(const bmalloc_type* type)
{
    return type->size;
}

static inline size_t bmalloc_type_alignment(const bmalloc_type* type)
{
    return type->alignment;
}

static inline const char* bmalloc_type_name(const bmalloc_type* type)
{
    return type->name;
}

PAS_API bmalloc_type* bmalloc_type_create(size_t size, size_t alignment, const char* name);

PAS_API bool bmalloc_type_try_name_dump(pas_stream* stream, const char* name);
PAS_API void bmalloc_type_name_dump(pas_stream* stream, const char* name);

PAS_API void bmalloc_type_dump(const bmalloc_type* type, pas_stream* stream);

static inline size_t bmalloc_type_as_heap_type_get_type_size(const pas_heap_type* type)
{
    return bmalloc_type_size((const bmalloc_type*)type);
}

static inline size_t bmalloc_type_as_heap_type_get_type_alignment(const pas_heap_type* type)
{
    return bmalloc_type_alignment((const bmalloc_type*)type);
}

PAS_API void bmalloc_type_as_heap_type_dump(const pas_heap_type* type, pas_stream* stream);

PAS_END_EXTERN_C;

#endif /* BMALLOC_TYPE_H */

