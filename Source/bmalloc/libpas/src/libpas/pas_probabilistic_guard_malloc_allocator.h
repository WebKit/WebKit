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

#ifndef PAS_PROBABILISTIC_GUARD_MALLOC_ALLOCATOR
#define PAS_PROBABILISTIC_GUARD_MALLOC_ALLOCATOR

#include "pas_utils.h"
#include "pas_large_heap.h"
#include <stdbool.h>
#include <stdint.h>

PAS_BEGIN_EXTERN_C;

/*
 * structure for holding metadata of pgm allocations
 * FIXME : Reduce size of structure
 */
typedef struct pas_pgm_storage pas_pgm_storage;
struct pas_pgm_storage {
    size_t allocation_size_requested;
    size_t size_of_data_pages;
    size_t mem_to_waste;
    size_t mem_to_alloc;
    uintptr_t start_of_data_pages;
    uintptr_t upper_guard_page;
    uintptr_t lower_guard_page;
    pas_large_heap* large_heap;
};

/* max amount of free memory that can be wasted (1MB) */
#define PAS_PGM_MAX_WASTED_MEMORY (1024 * 1024)

/*
 * max amount of virtual memory that can be used by PGM (1GB)
 * including guard pages and wasted memory
 */
#define PAS_PGM_MAX_VIRTUAL_MEMORY (1024 * 1024 * 1024)

/*
 * We want a fast way to determine if we can call PGM or not.
 * It would be really wasteful to recompute this answer each time we try to allocate,
 * so just update this variable each time we allocate or deallocate.
 */
extern PAS_API bool pas_probabilistic_guard_malloc_can_use;

extern PAS_API bool pas_probabilistic_guard_malloc_is_initialized;

extern PAS_API uint16_t pas_probabilistic_guard_malloc_random;
extern PAS_API uint16_t pas_probabilistic_guard_malloc_counter;

pas_allocation_result pas_probabilistic_guard_malloc_allocate(pas_large_heap* large_heap, size_t size, const pas_heap_config* heap_config, pas_physical_memory_transaction* transaction);
void pas_probabilistic_guard_malloc_deallocate(void* memory);

size_t pas_probabilistic_guard_malloc_get_free_virtual_memory(void);
size_t pas_probabilistic_guard_malloc_get_free_wasted_memory(void);

bool pas_probabilistic_guard_malloc_check_exists(uintptr_t mem);

/*
 * Determine whether PGM can be called at runtime.
 * PGM will be called between every 4,000 to 5,000 times an allocation is tried.
 */
static PAS_ALWAYS_INLINE bool pas_probabilistic_guard_malloc_should_call_pgm(void)
{
    if (++pas_probabilistic_guard_malloc_counter == pas_probabilistic_guard_malloc_random) {
        pas_probabilistic_guard_malloc_counter = 0;
        return true;
    }

    return false;
}

extern PAS_API void pas_probabilistic_guard_malloc_initialize_pgm(void);
pas_large_map_entry pas_probabilistic_guard_malloc_return_as_large_map_entry(uintptr_t mem);

PAS_END_EXTERN_C;

#endif
