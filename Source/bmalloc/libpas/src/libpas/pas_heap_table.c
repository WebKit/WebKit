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

#include "pas_heap_table.h"

#include "pas_heap_lock.h"
#include "pas_bootstrap_free_heap.h"

pas_large_heap** pas_heap_table = NULL;
unsigned pas_heap_table_bump_index = 0;

void pas_heap_table_try_allocate_index(pas_large_heap* heap)
{
    pas_heap_lock_assert_held();

    if (!pas_heap_table) {
        PAS_ASSERT(!pas_heap_table_bump_index);
        
        pas_heap_table = pas_bootstrap_free_heap_allocate_simple(
            sizeof(pas_large_heap*) * PAS_HEAP_TABLE_SIZE,
            "pas_heap_table",
            pas_delegate_allocation);
    }

    if (pas_heap_table_bump_index >= PAS_HEAP_TABLE_SIZE) {
        PAS_ASSERT(pas_heap_table_bump_index == PAS_HEAP_TABLE_SIZE);
        heap->table_state = pas_heap_table_state_failed;
        return;
    }

    heap->index = pas_heap_table_bump_index++;
    pas_heap_table[heap->index] = heap;
    heap->table_state = pas_heap_table_state_has_index;
}

#endif /* LIBPAS_ENABLED */
