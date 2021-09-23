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

#ifndef PAS_PAGE_HEADER_TABLE_H
#define PAS_PAGE_HEADER_TABLE_H

#include "pas_lock_free_read_ptr_ptr_hashtable.h"

PAS_BEGIN_EXTERN_C;

/* The page header table is the slow way of getting a page header. We use it for medium pages and
   for pages that need out-of-line headers. It's necessary to have a separate page header table for
   each page size. */

struct pas_page_base;
struct pas_page_header_table;
typedef struct pas_page_base pas_page_base;
typedef struct pas_page_header_table pas_page_header_table;

struct pas_page_header_table {
    size_t page_size;
    pas_lock_free_read_ptr_ptr_hashtable hashtable;
};

#define PAS_PAGE_HEADER_TABLE_INITIALIZER(passed_page_size) \
    ((pas_page_header_table){ \
         .page_size = (passed_page_size), \
         .hashtable = PAS_LOCK_FREE_READ_PTR_PTR_HASHTABLE_INITIALIZER \
     })

static inline unsigned pas_page_header_table_hash(const void* key, void* arg)
{
    size_t page_size;

    page_size = (size_t)arg;

    return pas_hash32((unsigned)((uintptr_t)key / page_size));
}

PAS_API pas_page_base* pas_page_header_table_add(pas_page_header_table* table,
                                                 size_t page_size,
                                                 size_t header_size,
                                                 void* boundary);

PAS_API void pas_page_header_table_remove(pas_page_header_table* table,
                                          size_t page_size,
                                          pas_page_base* page_base);

static PAS_ALWAYS_INLINE void** pas_page_header_table_get_boundary_ptr(pas_page_header_table* table,
                                                                       size_t page_size,
                                                                       pas_page_base* page_base)
{
    PAS_TESTING_ASSERT(page_size == table->page_size);

    return ((void**)page_base) - 1;
}

static PAS_ALWAYS_INLINE void* pas_page_header_table_get_boundary(pas_page_header_table* table,
                                                                  size_t page_size,
                                                                  pas_page_base* page_base)
{
    return *pas_page_header_table_get_boundary_ptr(table, page_size, page_base);
}

static PAS_ALWAYS_INLINE pas_page_base*
pas_page_header_table_get_for_boundary(pas_page_header_table* table,
                                       size_t page_size,
                                       void* boundary)
{
    PAS_TESTING_ASSERT(page_size == table->page_size);
    PAS_TESTING_ASSERT(pas_round_down_to_power_of_2((uintptr_t)boundary, page_size)
                       == (uintptr_t)boundary);

    return (pas_page_base*)pas_lock_free_read_ptr_ptr_hashtable_find(
        &table->hashtable, pas_page_header_table_hash, (void*)page_size, boundary);
}

static PAS_ALWAYS_INLINE pas_page_base*
pas_page_header_table_get_for_address(pas_page_header_table* table,
                                      size_t page_size,
                                      void* address)
{
    void* boundary;

    PAS_TESTING_ASSERT(page_size == table->page_size);

    boundary = (void*)pas_round_down_to_power_of_2((uintptr_t)address, page_size);

    return pas_page_header_table_get_for_boundary(table, page_size, boundary);
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_HEADER_TABLE_H */


