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

/*
 * Probabilistic Guard Malloc (PGM) is an allocator designed to catch use after free attempts
 * and out of bounds accesses. It behaves similarly to AddressSanitizer (ASAN), but aims to have
 * minimal runtime overhead.
 *
 * The design of PGM is quite simple. Each time an allocation is performed an additional guard page
 * is added above and below the newly allocated page(s). An allocation may span multiple pages.
 * When a deallocation is performed, the page(s) allocated will be protected using mprotect to ensure
 * that any use after frees will trigger a crash. Virtual memory addresses are never reused, so we will
 * never run into a case where object 1 is freed, object 2 is allocated over the same address space, and
 * object 1 then accesses the memory address space of now object 2.
 *
 * PGM does add notable virtual memory overhead. Each allocation, no matter the size, adds an additional 2 guard pages
 * (8KB for X86_64 and 32KB for ARM64). In addition, there may be free memory left over in the page(s) allocated
 * for the user. This memory may not be used by any other allocation.
 *
 * We added limits on virtual memory and wasted memory to help limit the memory impact on the overall system.
 * Virtual memory for this allocator is limited to 1GB. Wasted memory, which is the unused memory in the page(s)
 * allocated by the user, is limited to 1MB. These overall limits should ensure that the memory impact on the system
 * is minimal, while helping to tackle the problems of catching use after frees and out of bounds accesses.
 */

#ifndef PAS_PROBABILISTIC_GUARD_MALLOC_ALLOCATOR
#define PAS_PROBABILISTIC_GUARD_MALLOC_ALLOCATOR

#include "pas_utils.h"
#include "pas_large_heap.h"
#include "pas_ptr_hash_map.h"
#include <stdbool.h>
#include <stdint.h>

PAS_BEGIN_EXTERN_C;

/* structure for holding pgm metadata allocations */
typedef struct pas_pgm_storage pas_pgm_storage;
struct pas_pgm_storage {
    size_t allocation_size_requested;
    size_t size_of_data_pages;
    uintptr_t start_of_data_pages;

    /*
     * These parameter below rely on page sizes being less than 65536.
     * I am not aware of any platforms using more than this at the moment.
     */
    uint16_t mem_to_waste;
    uint16_t page_size;

    pas_large_heap* large_heap;
};

/* max amount of free memory that can be wasted (1MB) */
#define PAS_PGM_MAX_WASTED_MEMORY (1024 * 1024)

/*
 * max amount of virtual memory that can be used by PGM (1GB)
 * including guard pages and wasted memory
 */
#define PAS_PGM_MAX_VIRTUAL_MEMORY (1024 * 1024 * 1024)

extern PAS_API pas_ptr_hash_map pas_pgm_hash_map;
extern PAS_API pas_ptr_hash_map_in_flux_stash pas_pgm_hash_map_in_flux_stash;

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
