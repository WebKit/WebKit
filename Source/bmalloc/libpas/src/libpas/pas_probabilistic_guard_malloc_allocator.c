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

//
// Probabilistic Guard Malloc (PGM) is a new allocator designed to catch use after free attempts and out of bounds accesses.
// It behaves similarly to AddressSanitizer (ASAN), but aims to have minimal runtime overhead.
//
// The design of PGM is quite simple. Each time an allocation is performed an additional guard page is added above and below the newly
// allocated page(s). An allocation may span multiple pages. When a deallocation is performed, the page(s) allocated will be protected
// using mprotect to ensure that any use after frees will trigger a crash. Virtual memory addresses are never reused, so we will never run
// into a case where object 1 is freed, object 2 is allocated over the same address space, and object 1 then accesses the memory address
// space of now object 2.
//
// PGM does add notable memory overhead. Each allocation, no matter the size, adds an additional 2 guard pages (8KB for X86_64 and 32KB
// for ARM64). In addition, there may be free memory left over in the page(s) allocated for the user. This memory may not be used by any
// other allocation.
//
// We added limits on virtual memory and wasted memory to help limit the memory impact on the overall system. Virtual memory for this
// allocator is limited to 1GB. Wasted memory, which is the unused memory in the page(s) allocated by the user, is limited to 1MB.
// These overall limits should ensure that the memory impact on the system is minimal, while helping to tackle the problems of catching
// use after frees and out of bounds accesses.
//

#include "pas_config.h"

#if LIBPAS_ENABLED

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include "pas_utils.h"
#include "pas_heap.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_large_heap.h"
#include "pas_ptr_hash_map.h"
#include "iso_heap_config.h"
#include "pas_large_utility_free_heap.h"

// debug toggle
static const bool pgm_verbose = false;

// max amount of free memory that can be wasted (1MB)
#define MAX_WASTED_MEMORY (1024 * 1024)

// max amount of virtual memory that can be used by PGM (1GB)
// including guard pages and wasted memory
#define MAX_VIRTUAL_MEMORY (1024 * 1024 * 1024)

static bool pgm_enabled = true;

static size_t free_wasted_mem  = MAX_WASTED_MEMORY;
static size_t free_virtual_mem = MAX_VIRTUAL_MEMORY;

// structure for holding metadata of pgm allocations
typedef struct pgm_storage pgm_storage;
struct pgm_storage {
    size_t allocation_size_requested;
    size_t size_of_data_pages;
    size_t mem_to_waste;
    size_t mem_to_alloc;
    uintptr_t start_of_data_pages;
    uintptr_t upper_guard_page;
    uintptr_t lower_guard_page;
};

// the hash map is used to keep track of all pgm allocations
// key   : user's starting memory address
// value : metadata for tracking that allocation (pgm_storage)
pas_ptr_hash_map pgm_hash_map = PAS_HASHTABLE_INITIALIZER;

#if PAS_COMPILER(CLANG)
#pragma mark -
#pragma mark ALLOC/DEALLOC
#endif

void* pas_probabilistic_guard_malloc_allocate(size_t size, pas_heap* heap, pas_heap_config* heap_config,
                                              pas_physical_memory_transaction* transaction) {
    pas_heap_lock_assert_held();

    // input checking
    if (!heap || !size || !heap_config || !transaction)
        return NULL;

    // get page size
    const size_t page_size = pas_page_malloc_alignment();

    // calculate wasted memory
    size_t mem_to_waste = page_size - (size % page_size);
    if (mem_to_waste > free_wasted_mem)
        return NULL;

    // calculate virtual memory
    //
    // *------------------* *------------------* *------------------*
    // | lower guard page | | user alloc pages | | upper guard page |
    // *------------------* *------------------* *------------------*
    size_t mem_to_alloc = (2 * page_size) + size + mem_to_waste;
    if (mem_to_alloc > free_virtual_mem)
        return NULL;

    // allocate memory
    pas_allocation_result result = pas_large_heap_try_allocate_and_forget(&heap->large_heap, mem_to_alloc, page_size,
                                                                          heap_config, transaction);
    if (!result.did_succeed)
        return NULL;

    // protect guard pages from being accessed
    uintptr_t lower_guard_page = result.begin;
    uintptr_t upper_guard_page = result.begin + (mem_to_alloc - page_size);

    int mprotect_res = mprotect( (void *) lower_guard_page, page_size, PROT_NONE);
    PAS_ASSERT(!mprotect_res);

    mprotect_res = mprotect( (void *) upper_guard_page, page_size, PROT_NONE);
    PAS_ASSERT(!mprotect_res);

    // ensure physical addresses are released
    // TODO: investigate using MADV_FREE_REUSABLE instead
    int madvise_res = madvise((void *) upper_guard_page, page_size, MADV_FREE);
    PAS_ASSERT(!madvise_res);

    madvise_res = madvise((void *) lower_guard_page, page_size, MADV_FREE);
    PAS_ASSERT(!madvise_res);

    // the key is the location where the user's starting memory address is located.
    // allocations are right aligned, so the end backs up to the upper guard page.
    void * key = (void*) (result.begin + page_size + mem_to_waste);

    // create struct to hold hash map value
    pgm_storage *value = malloc(sizeof(pgm_storage));
    PAS_ASSERT(value);

    value->mem_to_alloc              = mem_to_alloc;
    value->mem_to_waste              = mem_to_waste;
    value->size_of_data_pages        = size + mem_to_waste;
    value->lower_guard_page          = lower_guard_page;
    value->upper_guard_page          = upper_guard_page;
    value->start_of_data_pages       = result.begin + page_size;
    value->allocation_size_requested = size;

    // add to hash map
    pas_ptr_hash_map_add_result add_result = pas_ptr_hash_map_add(&pgm_hash_map, key, NULL,
                                                                  &pas_large_utility_free_heap_allocation_config);
    PAS_ASSERT(add_result.is_new_entry);

    add_result.entry->key = key;
    add_result.entry->value = value;

    // update global virtual and wasted memory
    free_wasted_mem  -= mem_to_waste;
    free_virtual_mem -= mem_to_alloc;

    if (pgm_verbose) {
        printf("******************************************************\n"
               " Allocating Memory\n\n"
               " Overall System Stats"
               " free_wasted_mem  : %zu\n"
               " free_virtual_mem : %zu\n"
               "\n"
               " Allocation\n"
               " Allocation Size Requested : %zu \n"
               " Memory Allocated          : %zu \n"
               " Memory Wasted             : %zu \n"
               " Size of Data Pages        : %zu \n"
               " Lower Guard Page Address  : %p  \n"
               " Upper Guard Page Address  : %p  \n"
               " Start of Data Pages       : %p  \n"
               " Memory Address for User   : %p  \n"
               "******************************************************\n\n\n",
               free_wasted_mem,
               free_virtual_mem,
               value->allocation_size_requested,
               value->mem_to_alloc,
               value->mem_to_waste,
               value->size_of_data_pages,
               (uintptr_t *) value->lower_guard_page,
               (uintptr_t *) value->upper_guard_page,
               (uintptr_t *) value->start_of_data_pages,
               key);
    }

    return key;
}

void pas_probabilistic_guard_malloc_deallocate(void* mem) {
    pas_heap_lock_assert_held();

    if (pgm_verbose)
        printf("Memory Address Requested to Deallocate %p\n", mem);

    uintptr_t * key = (uintptr_t *) mem;

    // get value from hash map
    pas_ptr_hash_map_entry * entry = pas_ptr_hash_map_find(&pgm_hash_map, key);
    if (!entry || !entry->value)
        return;

    // protect the pages
    pgm_storage * value = (pgm_storage *) entry->value;
    int mprotect_res = mprotect( (void *) value->start_of_data_pages, value->size_of_data_pages, PROT_NONE);
    PAS_ASSERT(!mprotect_res);

    // ensure physical addresses are released
    // TODO: investigate using MADV_FREE_REUSABLE instead
    int madvise_res = madvise((void *) value->start_of_data_pages, value->size_of_data_pages, MADV_FREE);
    PAS_ASSERT(!madvise_res);

    // remove value from hash map
    bool removed = pas_ptr_hash_map_remove(&pgm_hash_map, key, NULL, &pas_large_utility_free_heap_allocation_config);
    PAS_ASSERT(removed);

    // update global virtual and wasted memory
    free_wasted_mem  += value->mem_to_waste;
    free_virtual_mem += value->mem_to_alloc;

    if (pgm_verbose) {
        printf("******************************************************\n"
               " Deallocating Memory\n\n"
               " Overall System Stats"
               " free_wasted_mem  : %zu\n"
               " free_virtual_mem : %zu\n"
               "\n"
               " Deallocation\n"
               " Allocation Size Requested : %zu \n"
               " Memory Allocated          : %zu \n"
               " Memory Wasted             : %zu \n"
               " Size of Data Pages        : %zu \n"
               " Lower Guard Page Address  : %p  \n"
               " Upper Guard Page Address  : %p  \n"
               " Start of Data Pages       : %p  \n"
               " Memory Address for User   : %p  \n"
               "******************************************************\n\n\n",
               free_wasted_mem,
               free_virtual_mem,
               value->allocation_size_requested,
               value->mem_to_alloc,
               value->mem_to_waste,
               value->size_of_data_pages,
               (uintptr_t *) value->lower_guard_page,
               (uintptr_t *) value->upper_guard_page,
               (uintptr_t *) value->start_of_data_pages,
               key);
    }

    // free memory
    free(value);
}


#if PAS_COMPILER(CLANG)
#pragma mark -
#pragma mark Determine whether to use PGM
#endif

void pas_probabilistic_guard_malloc_trigger(void) {
    // ???
}

bool pas_probabilistic_guard_malloc_can_use(void) {
    pas_heap_lock_assert_held();

    if (!pgm_enabled)
        return false;

    if (!free_wasted_mem || !free_virtual_mem)
        return false;

    return true;
}

bool pas_probabilistic_guard_malloc_should_use(void) {
    pas_heap_lock_assert_held();

    if (!pgm_enabled)
        return false;

    return true;
}

#if PAS_COMPILER(CLANG)
#pragma mark -
#pragma mark Helper Functions
#endif

size_t pas_probabilistic_guard_malloc_get_free_virtual_memory() {
    pas_heap_lock_assert_held();
    return free_virtual_mem;
}

size_t pas_probabilistic_guard_malloc_get_free_wasted_memory() {
    pas_heap_lock_assert_held();
    return free_wasted_mem;
}

#endif /* LIBPAS_ENABLED */
