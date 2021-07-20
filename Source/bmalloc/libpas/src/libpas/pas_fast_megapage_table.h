/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_FAST_MEGAPAGE_TABLE_H
#define PAS_FAST_MEGAPAGE_TABLE_H

#include "pas_bitfield_vector.h"
#include "pas_bitvector.h"
#include "pas_fast_megapage_kind.h"
#include "pas_internal_config.h"
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_lock.h"
#include <stdio.h>

PAS_BEGIN_EXTERN_C;

#define PAS_NUM_FAST_MEGAPAGE_TABLES \
    (PAS_NUM_STATIC_FAST_MEGAPAGE_TABLES + PAS_USE_DYNAMIC_FAST_MEGAPAGE_TABLE)
#define PAS_DYNAMIC_FAST_MEGAPAGE_TABLE_INDEX PAS_NUM_STATIC_FAST_MEGAPAGE_TABLES

#define PAS_FAST_MEGAPAGE_TABLE_NUM_BITS 2

struct pas_fast_megapage_table;
struct pas_fast_megapage_table_impl;
typedef struct pas_fast_megapage_table pas_fast_megapage_table;
typedef struct pas_fast_megapage_table_impl pas_fast_megapage_table_impl;

struct pas_fast_megapage_table_impl {
    size_t index_begin; /* inclusive */
    size_t index_end; /* exclusive */
    pas_fast_megapage_table_impl* last; /* this is just to prevent `leaks` from complaining */
    unsigned bits[1];
};

struct pas_fast_megapage_table {
    unsigned fast_bits[PAS_BITVECTOR_NUM_WORDS(PAS_NUM_FAST_FAST_MEGAPAGE_BITS)];
    pas_fast_megapage_table_impl* instances[PAS_NUM_FAST_MEGAPAGE_TABLES];
};

PAS_API extern pas_fast_megapage_table_impl pas_fast_megapage_table_impl_null;

#define PAS_FAST_MEGAPAGE_TABLE_INITIALIZER \
    { \
        .fast_bits = {[0 ... PAS_BITVECTOR_NUM_WORDS(PAS_NUM_FAST_FAST_MEGAPAGE_BITS) - 1] = 0}, \
        .instances = {[0 ... PAS_NUM_FAST_MEGAPAGE_TABLES - 1] = &pas_fast_megapage_table_impl_null} \
    }

PAS_API void pas_fast_megapage_table_initialize_static_by_index(
    pas_fast_megapage_table* table,
    size_t index, uintptr_t begin, uintptr_t end,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_fast_megapage_table_initialize_static(
    pas_fast_megapage_table* table,
    size_t index, uintptr_t begin, uintptr_t end,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_fast_megapage_table_set_by_index(
    pas_fast_megapage_table* table,
    size_t index, pas_fast_megapage_kind value,
    pas_lock_hold_mode heap_lock_hold_mode);

static inline pas_fast_megapage_kind pas_fast_megapage_table_get_by_index(
    pas_fast_megapage_table* table,
    size_t index)
{
    pas_fast_megapage_table_impl* instance;
    size_t index_begin;
    size_t table_index;
    
    if (PAS_LIKELY(index < PAS_NUM_FAST_FAST_MEGAPAGE_BITS)
        && PAS_LIKELY(pas_bitvector_get(table->fast_bits, index)))
        return pas_small_segregated_fast_megapage_kind;
    
    for (table_index = 0; table_index < PAS_NUM_FAST_MEGAPAGE_TABLES; ++table_index) {
        instance = table->instances[table_index];
        
        index_begin = instance->index_begin;
        if (PAS_UNLIKELY(index < index_begin))
            continue;
        
        if (PAS_UNLIKELY(index >= instance->index_end))
            continue;
        
        return (pas_fast_megapage_kind)pas_bitfield_vector_get(
            instance->bits, PAS_FAST_MEGAPAGE_TABLE_NUM_BITS, index - index_begin);
    }
    
    return pas_not_a_fast_megapage_kind;
}

static inline pas_fast_megapage_kind pas_fast_megapage_table_get(
    pas_fast_megapage_table* table,
    uintptr_t begin)
{
    return pas_fast_megapage_table_get_by_index(table, begin >> PAS_FAST_MEGAPAGE_SHIFT);
}

PAS_END_EXTERN_C;

#endif /* PAS_FAST_MEGAPAGE_TABLE_H */

