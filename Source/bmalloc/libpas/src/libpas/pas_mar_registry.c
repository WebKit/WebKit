/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "pas_mar_registry.h"

#include "pas_random.h"

#if PAS_OS(DARWIN)

#include <assert.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool pas_mar_enabled = false;
unsigned pas_mar_qualifying_page_index = 0;

struct pas_mar_registry pas_mar_global_registry = {
    { },
    { },
    0,
    0,
    { },
};

struct pas_mar_registry* pas_mar_registry_for_crash_reporter_enumeration = NULL;

// Backtrace hashing

#if PAS_CPU(ADDRESS64)

static uint32_t hash_backtrace(unsigned num_stack_frames, void** backtrace)
{
    // This implements Murmur hash on the low 32b of each backtrace
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const uint32_t r1 = 15;
    const uint32_t r2 = 13;
    const uint32_t m = 5;
    const uint32_t n = 0xe6546b64;

    uint32_t result = 0;

    for (unsigned i = 0; i < num_stack_frames; ++i) {
        uint32_t k = ((uintptr_t)backtrace[i]) & ((1ull << 32) - 1);
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        result ^= k;
        result = (result << r2) | (result >> (32 - r2));
        result = result * m + n;
    }

    result ^= (num_stack_frames * 4);
    result = result ^ (result >> 16);
    result *= 0x85ebca6b;
    result = result ^ (result >> 13);
    result *= 0xc2b2ae35;
    result = result ^ (result >> 16);

    return result;
}

unsigned pas_mar_insert_backtrace(pas_mar_registry*, unsigned num_stack_frames, void** backtrace, uint32_t hash);
unsigned pas_mar_insert_backtrace(pas_mar_registry* registry, unsigned num_stack_frames, void** backtrace, uint32_t hash)
{
    unsigned index = hash % PAS_MAR_TRACKED_BACKTRACES;
    if (hash == registry->backtrace_registry[index].hash)
        return index;

    registry->backtrace_registry[index].num_frames = num_stack_frames;
    registry->backtrace_registry[index].hash = hash;
    memcpy(registry->backtrace_registry[index].backtrace_buffer, backtrace, num_stack_frames* sizeof(void*));
    return index;
}

#endif /* PAS_CPU(ADDRESS64) */

// MAR Registry

void pas_mar_initialize(void)
{
    if (!pas_mar_registry_for_crash_reporter_enumeration)
        pas_mar_registry_for_crash_reporter_enumeration = &pas_mar_global_registry;

    pas_lock_construct(&pas_mar_global_registry.lock);

    if (PAS_UNLIKELY(getenv("SanitizersAllocationTraces"))) {
        pas_mar_enabled = true;
        pas_mar_qualifying_page_index = pas_get_fast_random(PAS_MAR_PROBABILITY);
        return;
    }
    if (PAS_LIKELY(pas_get_fast_random(1000) >= 1))
        pas_mar_enabled = false;
    else {
        pas_mar_enabled = true;
        pas_mar_qualifying_page_index = pas_get_fast_random(PAS_MAR_PROBABILITY);
    }
}

void* pas_mar_did_allocate(pas_mar_registry* registry, void* address, size_t allocation_size)
{
    void* stacktrace[PAS_MAR_BACKTRACE_MAX_SIZE];
    unsigned num_stack_frames = (unsigned) backtrace(stacktrace, PAS_MAR_BACKTRACE_MAX_SIZE);

    return pas_mar_record_allocation(registry, address, allocation_size, num_stack_frames, stacktrace);
}

void* pas_mar_did_allocate_and_zero(pas_mar_registry* registry, pas_allocation_result result, size_t allocation_size)
{
    void* stacktrace[PAS_MAR_BACKTRACE_MAX_SIZE];
    unsigned num_stack_frames = (unsigned) backtrace(stacktrace, PAS_MAR_BACKTRACE_MAX_SIZE);

    pas_mar_record_allocation(registry, (void*)result.begin, allocation_size, num_stack_frames, stacktrace);
    return (void*)pas_allocation_result_zero(result, allocation_size).begin;
}

void* pas_mar_did_deallocate(pas_mar_registry* registry, void* address)
{
    void* stacktrace[PAS_MAR_BACKTRACE_MAX_SIZE];
    unsigned num_stack_frames = (unsigned) backtrace(stacktrace, PAS_MAR_BACKTRACE_MAX_SIZE);

    return pas_mar_record_deallocation(registry, address, num_stack_frames, stacktrace);
}

void* pas_mar_record_allocation(pas_mar_registry* registry, void* address, size_t allocation_size_bytes, unsigned num_stack_frames, void** backtrace)
{
    pas_lock_lock(&registry->lock);
    PAS_ASSERT(num_stack_frames <= PAS_MAR_BACKTRACE_MAX_SIZE);

    if (pas_mar_is_allocation_table_full(registry))
        pas_mar_increment_allocation_record_table_head(registry);

    unsigned allocation_table_index = pas_mar_allocation_table_tail_index(registry);
    pas_mar_increment_allocation_record_table_tail(registry);

    uint32_t backtrace_hash = hash_backtrace(num_stack_frames, backtrace);
    unsigned backtrace_registry_index = pas_mar_insert_backtrace(registry, num_stack_frames, backtrace, backtrace_hash);
    struct pas_mar_memory_action_record new_record = {
        address,
        allocation_size_bytes,
        backtrace_registry_index,
        backtrace_hash,
        true
    };
    registry->allocation_record_table[allocation_table_index] = new_record;
    pas_lock_unlock(&registry->lock);
    return address;
}

void* pas_mar_record_deallocation(pas_mar_registry* registry, void* address, unsigned num_stack_frames, void** backtrace)
{
    pas_lock_lock(&registry->lock);
    PAS_ASSERT(num_stack_frames <= PAS_MAR_BACKTRACE_MAX_SIZE);

    if (pas_mar_is_allocation_table_full(registry))
        pas_mar_increment_allocation_record_table_head(registry);

    unsigned allocation_table_index = pas_mar_allocation_table_tail_index(registry);
    pas_mar_increment_allocation_record_table_tail(registry);

    uint32_t backtrace_hash = hash_backtrace(num_stack_frames, backtrace);
    unsigned backtrace_registry_index = pas_mar_insert_backtrace(registry, num_stack_frames, backtrace, backtrace_hash);
    struct pas_mar_memory_action_record new_record = {
        address,
        0,
        backtrace_registry_index,
        backtrace_hash,
        false
    };
    registry->allocation_record_table[allocation_table_index] = new_record;
    pas_lock_unlock(&registry->lock);
    return address;
}

struct pas_mar_exported_allocation_record pas_mar_get_allocation_record(pas_mar_registry* registry, void* address)
{
    struct pas_mar_exported_allocation_record result;
    result.is_valid = false;

    address = pas_mar_canonicalize_address(address);

    void* base_object_address = NULL;
    for (unsigned i = 0; i < PAS_MAR_TRACKED_ALLOCATIONS; ++i) {
        unsigned index = (pas_mar_allocation_table_head_index(registry) + i) % PAS_MAR_TRACKED_ALLOCATIONS;
        if ((registry->allocation_record_table_head + i) % MAR_ALLOCATION_RECORD_TABLE_FIFO_MODULUS == registry->allocation_record_table_tail)
            break;
        struct pas_mar_memory_action_record* art_entry = &registry->allocation_record_table[index];
        if (art_entry->is_allocation) {
            // Check if the allocation was within the range
            if ((uintptr_t) address >= (uintptr_t) pas_mar_canonicalize_address(art_entry->address) && (uintptr_t) address < ((uintptr_t) pas_mar_canonicalize_address(art_entry->address)) + art_entry->allocation_size_bytes) {
                // Check that we have a valid backtrace
                unsigned registry_index = art_entry->backtrace_registry_index;
                if (registry->backtrace_registry[registry_index].hash != art_entry->backtrace_hash)
                    continue;

                pas_mar_backtrace_record* backtrace = &registry->backtrace_registry[registry_index];

                result.allocation_size_bytes = art_entry->allocation_size_bytes;
                result.is_valid = true;
                result.allocation_trace.num_frames= backtrace->num_frames;
                memcpy(result.allocation_trace.backtrace_buffer, backtrace->backtrace_buffer, backtrace->num_frames * sizeof(void*));

                base_object_address = art_entry->address;
            }
        }
        if (result.is_valid && !art_entry->is_allocation && art_entry->address == base_object_address) {
            // Check that we have a valid backtrace
            unsigned registry_index = art_entry->backtrace_registry_index;
            if (registry->backtrace_registry[registry_index].hash != art_entry->backtrace_hash)
                continue;

            pas_mar_backtrace_record* backtrace = &registry->backtrace_registry[registry_index];

            result.deallocation_trace.num_frames= backtrace->num_frames;
            memcpy(result.deallocation_trace.backtrace_buffer, backtrace->backtrace_buffer, backtrace->num_frames * sizeof(void*));

            base_object_address = NULL;
        }
    }
    return result;
}

#endif /* PAS_OS(DARWIN) */

#endif /* LIBPAS_ENABLED */
