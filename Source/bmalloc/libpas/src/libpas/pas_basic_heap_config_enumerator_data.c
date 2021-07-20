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

#include "pas_basic_heap_config_enumerator_data.h"

#include "pas_page_header_table.h"

bool pas_basic_heap_config_enumerator_data_add_page_header_table(
    pas_basic_heap_config_enumerator_data* data,
    pas_enumerator* enumerator,
    pas_page_header_table* page_header_table)
{
    static const bool verbose = false;
    
    pas_lock_free_read_ptr_ptr_hashtable_table* table;
    size_t index;

    if (!page_header_table)
        return false;
    
    if (!page_header_table->hashtable.table)
        return true;
    
    if (verbose)
        pas_log("Have a page header hashtable at %p.\n", page_header_table->hashtable.table);
    
    table = pas_enumerator_read(
        enumerator, page_header_table->hashtable.table,
        PAS_OFFSETOF(pas_lock_free_read_ptr_ptr_hashtable_table, array));
    if (!table)
        return false;
    
    if (verbose)
        pas_log("The table has size %u.\n", table->table_size);
    
    table = pas_enumerator_read(
        enumerator, page_header_table->hashtable.table,
        PAS_OFFSETOF(pas_lock_free_read_ptr_ptr_hashtable_table, array)
        + sizeof(pas_pair) * table->table_size);
    if (!table)
        return false;
    
    for (index = table->table_size; index--;) {
        pas_pair* pair;
        pas_ptr_hash_map_entry entry;
        
        pair = table->array + index;
        
        if (pair->low == UINTPTR_MAX)
            continue;
        
        entry.key = (void*)pair->low;
        entry.value = (void*)pair->high;
        
        pas_ptr_hash_map_add_new(
            &data->page_header_table, entry, NULL, &enumerator->allocation_config);
    }
    
    return true;
}

#endif /* LIBPAS_ENABLED */
