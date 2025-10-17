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

/*
MAR: Malloc Audit Records

MAR provides a new way to audit bmalloc/libpas memory allocations
without resorting to PGM's guard pages. MAR maintains the address
of each allocation, but instead tracks what allocations were made
within pages of interest through the stack trace when `malloc` is
invoked.
*/

#ifndef MAR_REGISTRY_H
#define MAR_REGISTRY_H

#include "pas_platform.h"

#if PAS_OS(DARWIN)

#include "pas_allocation_result.h"
#include "pas_lock.h"
#include "pas_mar_crash_reporter_report.h"
#include "pas_utils.h"

#include <assert.h>
#include <execinfo.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PAS_MAR_PROBABILITY 8192
#define PAS_MAR_TRACKED_BACKTRACES 16384
#define PAS_MAR_TRACKED_ALLOCATIONS 16384

/*
We'll use an approach similar to hardware FIFO; the queue is empty if head == tail
and full if head ^ tail == size
*/

#define MAR_ALLOCATION_RECORD_TABLE_FIFO_MODULUS (2 * PAS_MAR_TRACKED_ALLOCATIONS)

#define PAS_MAR_PAGE_SHIFT 14

typedef struct pas_mar_backtrace_record pas_mar_backtrace_record;
struct pas_mar_backtrace_record {
    unsigned num_frames;
    uint32_t hash;
    void* backtrace_buffer[PAS_MAR_BACKTRACE_MAX_SIZE];
};

typedef struct pas_mar_memory_action_record pas_mar_memory_action_record;
struct pas_mar_memory_action_record {
    void* address;
    size_t allocation_size_bytes;
    unsigned backtrace_registry_index;
    uint32_t backtrace_hash;
    bool is_allocation;
};

typedef struct pas_mar_registry pas_mar_registry;
struct pas_mar_registry {
    struct pas_mar_backtrace_record backtrace_registry[PAS_MAR_TRACKED_BACKTRACES];
    struct pas_mar_memory_action_record allocation_record_table[PAS_MAR_TRACKED_ALLOCATIONS];
    /* push to the tail of the FIFO, evict from head */
    unsigned allocation_record_table_head;
    unsigned allocation_record_table_tail;
    pas_lock lock;
};

struct pas_mar_exported_allocation_record {
    struct pas_mar_backtrace allocation_trace;
    struct pas_mar_backtrace deallocation_trace;
    size_t allocation_size_bytes;
    bool is_valid;
};

/* Paging helpers */

PAS_ALWAYS_INLINE static void* pas_mar_canonicalize_address(void* address)
{
    return (void*)((uintptr_t)(address) & ((1ull << 48) - 1));
}

PAS_ALWAYS_INLINE static uintptr_t pas_mar_address_to_virtual_page_number(void* address)
{
    return (uintptr_t)(pas_mar_canonicalize_address(address)) >> PAS_MAR_PAGE_SHIFT;
}

// FIFO helpers

PAS_ALWAYS_INLINE static unsigned pas_mar_allocation_table_head_index(pas_mar_registry* registry)
{
    return registry->allocation_record_table_head % PAS_MAR_TRACKED_ALLOCATIONS;
}

PAS_ALWAYS_INLINE static unsigned pas_mar_allocation_table_tail_index(pas_mar_registry* registry)
{
    return registry->allocation_record_table_tail % PAS_MAR_TRACKED_ALLOCATIONS;
}

PAS_ALWAYS_INLINE static bool pas_mar_is_allocation_table_full(pas_mar_registry* registry)
{
    return (registry->allocation_record_table_head ^ registry->allocation_record_table_tail) == PAS_MAR_TRACKED_ALLOCATIONS;
}

PAS_ALWAYS_INLINE static void pas_mar_increment_allocation_record_table_head(pas_mar_registry* registry)
{
    registry->allocation_record_table_head = registry->allocation_record_table_head + 1;
    registry->allocation_record_table_head %= MAR_ALLOCATION_RECORD_TABLE_FIFO_MODULUS;
}

PAS_ALWAYS_INLINE static void pas_mar_increment_allocation_record_table_tail(pas_mar_registry* registry)
{
    registry->allocation_record_table_tail = registry->allocation_record_table_tail + 1;
    registry->allocation_record_table_tail %= MAR_ALLOCATION_RECORD_TABLE_FIFO_MODULUS;
}

PAS_BEGIN_EXTERN_C;

void* pas_mar_record_allocation(pas_mar_registry*, void* address, size_t allocation_size_bytes, unsigned num_stack_frames, void** backtrace);
void* pas_mar_record_deallocation(pas_mar_registry*, void* address, unsigned num_stack_frames, void** backtrace);

struct pas_mar_exported_allocation_record pas_mar_get_allocation_record(pas_mar_registry*, void* address);

void pas_mar_initialize(void);
void* pas_mar_did_allocate(pas_mar_registry*, void* address, size_t allocation_size);
void* pas_mar_did_allocate_and_zero(pas_mar_registry*, pas_allocation_result, size_t allocation_size);
void* pas_mar_did_deallocate(pas_mar_registry*, void* address);

extern bool pas_mar_enabled;
extern unsigned pas_mar_qualifying_page_index;
extern struct pas_mar_registry pas_mar_global_registry;
extern struct pas_mar_registry* pas_mar_registry_for_crash_reporter_enumeration;

PAS_END_EXTERN_C;

PAS_ALWAYS_INLINE bool pas_mar_is_address_in_qualifying_page(void* address)
{
    return pas_mar_address_to_virtual_page_number(address) % PAS_MAR_PROBABILITY == pas_mar_qualifying_page_index;
}

#endif /* PAS_OS(DARWIN) */

#endif
