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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_fast_megapage_table.h"

pas_fast_megapage_table_impl pas_fast_megapage_table_impl_null = {
    .index_begin = 0,
    .index_end = 0,
    .last = NULL,
    .bits = { 0 }
};

void pas_fast_megapage_table_initialize_static_by_index(pas_fast_megapage_table* table,
                                                        size_t index,
                                                        uintptr_t begin,
                                                        uintptr_t end,
                                                        pas_lock_hold_mode heap_lock_hold_mode)
{
    size_t size;
    size_t num_fields;

    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    PAS_IGNORE_WARNINGS_BEGIN("type-limits")
    PAS_ASSERT(index < PAS_NUM_STATIC_FAST_MEGAPAGE_TABLES);
    PAS_IGNORE_WARNINGS_END
    PAS_ASSERT(table->instances[index] == &pas_fast_megapage_table_impl_null);

    num_fields = end - begin + 1; /* Make end inclusive to allow end to straddle a fast_megapage boundary. */
    size = PAS_OFFSETOF(pas_fast_megapage_table_impl, bits) +
        sizeof(unsigned) * PAS_BITFIELD_VECTOR_NUM_WORDS(num_fields, PAS_FAST_MEGAPAGE_TABLE_NUM_BITS);
    size = pas_round_up_to_power_of_2(size, PAS_MIN_ALIGN);
    num_fields = PAS_BITFIELD_VECTOR_NUM_FIELDS(
        (size - PAS_OFFSETOF(pas_fast_megapage_table_impl, bits)) /
        sizeof(unsigned),
        PAS_FAST_MEGAPAGE_TABLE_NUM_BITS);
    end = begin + num_fields;

    table->instances[index] = pas_immortal_heap_allocate(
        size, "pas_fast_megapage_table/instances_array", pas_object_allocation);
    pas_zero_memory(table->instances[index], size);
    table->instances[index]->index_begin = begin;
    table->instances[index]->index_end = end;

    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

void pas_fast_megapage_table_initialize_static(pas_fast_megapage_table* table,
                                               size_t index, uintptr_t begin, uintptr_t end,
                                               pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_fast_megapage_table_initialize_static_by_index(table,
                                                       index,
                                                       begin >> PAS_FAST_MEGAPAGE_SHIFT,
                                                       end >> PAS_FAST_MEGAPAGE_SHIFT,
                                                       heap_lock_hold_mode);
}

void pas_fast_megapage_table_set_by_index(pas_fast_megapage_table* table,
                                          size_t index, unsigned value,
                                          pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_fast_megapage_table_impl* instance;
    size_t index_begin;
    size_t index_end;
    size_t table_index;

    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);

    if (index < PAS_NUM_FAST_FAST_MEGAPAGE_BITS
        && value == pas_small_exclusive_segregated_fast_megapage_kind) {
        pas_bitvector_set(table->fast_bits, index, true);
        return;
    }

    PAS_IGNORE_WARNINGS_BEGIN("type-limits")
    for (table_index = 0; table_index < PAS_NUM_STATIC_FAST_MEGAPAGE_TABLES; ++table_index) {
        instance = table->instances[table_index];
        index_begin = instance->index_begin;
        index_end = instance->index_end;

        if (index < index_begin)
            continue;
        if (index >= index_end)
            continue;

        pas_bitfield_vector_set(
            instance->bits, PAS_FAST_MEGAPAGE_TABLE_NUM_BITS, index - index_begin, value);
        pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
        return;
    }
    PAS_IGNORE_WARNINGS_END

    PAS_ASSERT(PAS_USE_DYNAMIC_FAST_MEGAPAGE_TABLE);

    instance = table->instances[PAS_DYNAMIC_FAST_MEGAPAGE_TABLE_INDEX];
    index_begin = instance->index_begin;
    index_end = instance->index_end;
    if (PAS_UNLIKELY(index < index_begin || index >= index_end)) {
        size_t new_index_begin;
        size_t new_index_end;
        size_t num_fields;
        pas_fast_megapage_table_impl* new_instance;
        size_t size;
        size_t initialization_index;

        if (instance == &pas_fast_megapage_table_impl_null) {
            new_index_begin = index;
            new_index_end = index + 1;
        } else if (index < index_begin) {
            PAS_ASSERT(index_begin && index_end);
            new_index_begin = PAS_MIN(index,
                                      index_begin - (index_end - index_begin));
            new_index_end = index_end;
        } else {
            PAS_ASSERT(index_begin && index_end);
            PAS_ASSERT(index >= index_end);
            new_index_begin = index_begin;
            new_index_end = PAS_MAX(index + 1,
                                    index_end + (index_end - index_begin));
        }

        PAS_ASSERT(new_index_end > new_index_begin);

        /* make sure we allocate a page. */
        num_fields = new_index_end - new_index_begin;
        size = PAS_OFFSETOF(pas_fast_megapage_table_impl, bits) +
            sizeof(unsigned) * PAS_BITFIELD_VECTOR_NUM_WORDS(num_fields,
                                                             PAS_FAST_MEGAPAGE_TABLE_NUM_BITS);
        size = pas_round_up_to_power_of_2(size, PAS_INTERNAL_MIN_ALIGN);
        num_fields = PAS_BITFIELD_VECTOR_NUM_FIELDS(
            (size - PAS_OFFSETOF(pas_fast_megapage_table_impl, bits)) /
            sizeof(unsigned),
            PAS_FAST_MEGAPAGE_TABLE_NUM_BITS);
        new_index_end = new_index_begin + num_fields;

        PAS_ASSERT(new_index_end > new_index_begin);

        new_instance = pas_immortal_heap_allocate(
            size, "pas_fast_megapage_table/instance", pas_object_allocation);

        pas_zero_memory(new_instance, size);

        new_instance->index_begin = new_index_begin;
        new_instance->index_end = new_index_end;
        new_instance->last = instance;

        for (initialization_index = index_begin;
             initialization_index < index_end;
             ++initialization_index) {
            pas_bitfield_vector_set(
                new_instance->bits,
                PAS_FAST_MEGAPAGE_TABLE_NUM_BITS,
                initialization_index - new_index_begin,
                pas_bitfield_vector_get(
                    instance->bits,
                    PAS_FAST_MEGAPAGE_TABLE_NUM_BITS,
                    initialization_index - index_begin));
        }

        pas_fence();

        instance = new_instance;
        index_begin = new_index_begin;
        table->instances[PAS_DYNAMIC_FAST_MEGAPAGE_TABLE_INDEX] = instance;
    }

    pas_bitfield_vector_set(instance->bits,
                            PAS_FAST_MEGAPAGE_TABLE_NUM_BITS,
                            index - index_begin, value);

    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

#endif /* LIBPAS_ENABLED */
