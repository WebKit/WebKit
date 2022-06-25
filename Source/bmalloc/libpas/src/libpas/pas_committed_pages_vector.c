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

#include "pas_committed_pages_vector.h"

#include "pas_allocation_config.h"
#include "pas_page_malloc.h"

PAS_BEGIN_EXTERN_C;

void pas_committed_pages_vector_construct(pas_committed_pages_vector* vector,
                                          void* object,
                                          size_t size,
                                          const pas_allocation_config* allocation_config)
{
    size_t page_size;
    size_t page_size_shift;
    size_t num_pages;

    page_size = pas_page_malloc_alignment();
    page_size_shift = pas_page_malloc_alignment_shift();

    PAS_ASSERT(pas_is_aligned((uintptr_t)object, page_size));
    PAS_ASSERT(pas_is_aligned(size, page_size));

    num_pages = size >> page_size_shift;

    vector->raw_data = allocation_config->allocate(
        num_pages, "pas_committed_pages_vector/raw_data", pas_object_allocation, allocation_config->arg);
    vector->size = num_pages;

#if PAS_OS(LINUX)
    PAS_SYSCALL(mincore(object, size, (unsigned char*)vector->raw_data));
#else
    PAS_SYSCALL(mincore(object, size, vector->raw_data));
#endif
}

void pas_committed_pages_vector_destruct(pas_committed_pages_vector* vector,
                                         const pas_allocation_config* allocation_config)
{
    allocation_config->deallocate(
        vector->raw_data, vector->size, pas_object_allocation, allocation_config->arg);
}

size_t pas_committed_pages_vector_count_committed(pas_committed_pages_vector* vector)
{
    size_t result;
    size_t index;
    result = 0;
    for (index = vector->size; index--;)
        result += (size_t)pas_committed_pages_vector_is_committed(vector, index);
    return result;
}

size_t pas_count_committed_pages(void* object,
                                 size_t size,
                                 const pas_allocation_config* allocation_config)
{
    size_t result;
    pas_committed_pages_vector vector;
    pas_committed_pages_vector_construct(&vector, object, size, allocation_config);
    result = pas_committed_pages_vector_count_committed(&vector);
    pas_committed_pages_vector_destruct(&vector, allocation_config);
    return result;
}

PAS_END_EXTERN_C;

#endif /* LIBPAS_ENABLED */

