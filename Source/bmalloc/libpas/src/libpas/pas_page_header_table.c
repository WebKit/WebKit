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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_page_header_table.h"

#include "pas_log.h"
#include "pas_utility_heap.h"

static const bool verbose = false;

pas_page_base* pas_page_header_table_add(pas_page_header_table* table,
                                         size_t page_size,
                                         size_t header_size,
                                         void* boundary)
{
    pas_page_base* page_base;

    uintptr_t boundary_int = (uintptr_t)boundary;
    PAS_PROFILE(PAGE_HEADER_TABLE_ADD, boundary_int);
    boundary = (void*)boundary_int;

    if (verbose)
        pas_log("Adding page header for boundary = %p.\n", boundary);
    
    PAS_ASSERT(pas_round_down_to_power_of_2((uintptr_t)boundary, page_size)
               == (uintptr_t)boundary);

    PAS_ASSERT(page_size == table->page_size);

    /* This protects against leaks. */
    PAS_ASSERT(!pas_page_header_table_get_for_boundary(table, page_size, boundary));

    /* We allocate two slots before the pas_page_base. The one is used for storing boundary.
       Another is not used, just allocated to align the page with 16byte alignment, which is
       required since it includes 16byte aligned data structures. */
    page_base = (pas_page_base*)(
        (void**)pas_utility_heap_allocate_with_alignment(
            sizeof(void*) * 2 + header_size, 16, "pas_page_header_table/header") + 2);

    if (verbose)
        pas_log("created page header at %p\n", page_base);

    *pas_page_header_table_get_boundary_ptr(table, page_size, page_base) = boundary;

    pas_lock_free_read_ptr_ptr_hashtable_set(
        &table->hashtable,
        pas_page_header_table_hash,
        (void*)page_size,
        boundary,
        page_base,
        pas_lock_free_read_ptr_ptr_hashtable_set_maybe_existing);

    return page_base;
}

void pas_page_header_table_remove(pas_page_header_table* table,
                                  size_t page_size,
                                  pas_page_base* page_base)
{
    void* boundary;
    
    PAS_ASSERT(page_size == table->page_size);

    if (verbose)
        pas_log("destroying page header at %p\n", page_base);

    boundary = pas_page_header_table_get_boundary(table, page_size, page_base);

    if (verbose)
        pas_log("Removing page header for boundary = %p.\n", boundary);

    PAS_ASSERT(pas_round_down_to_power_of_2((uintptr_t)boundary, page_size)
               == (uintptr_t)boundary);

    pas_lock_free_read_ptr_ptr_hashtable_set(
        &table->hashtable,
        pas_page_header_table_hash,
        (void*)page_size,
        boundary,
        NULL,
        pas_lock_free_read_ptr_ptr_hashtable_set_maybe_existing);

    pas_utility_heap_deallocate(
        pas_page_header_table_get_boundary_ptr(table, page_size, page_base));
}

#endif /* LIBPAS_ENABLED */
