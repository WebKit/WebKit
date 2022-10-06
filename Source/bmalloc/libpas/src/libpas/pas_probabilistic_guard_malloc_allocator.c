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
#include "pas_utility_heap.h"
#include "pas_large_utility_free_heap.h"

static size_t free_wasted_mem  = PAS_PGM_MAX_WASTED_MEMORY;
static size_t free_virtual_mem = PAS_PGM_MAX_VIRTUAL_MEMORY;

bool pas_pgm_can_use = true;

// the hash map is used to keep track of all pgm allocations
// key   : user's starting memory address
// value : metadata for tracking that allocation (pas_pgm_storage)
pas_ptr_hash_map pas_pgm_hash_map = PAS_HASHTABLE_INITIALIZER;

static void pas_probabilistic_guard_malloc_debug_info(const void* key, const pas_pgm_storage* value, const char* operation);

#if PAS_COMPILER(CLANG)
#pragma mark -
#pragma mark ALLOC/DEALLOC
#endif

pas_allocation_result pas_probabilistic_guard_malloc_allocate(pas_large_heap* large_heap, size_t size, const pas_heap_config* heap_config,
                                                              pas_physical_memory_transaction* transaction)
{
    pas_heap_lock_assert_held();
    static const bool verbose = false;

    pas_allocation_result result = pas_allocation_result_create_failure();

    if (verbose)
        printf("Memory requested to allocate %zu\n", size);

    if (!large_heap || !size || !heap_config || !transaction)
        return result;

    const size_t page_size = pas_page_malloc_alignment();

    size_t mem_to_waste = (page_size - (size % page_size)) % page_size;
    if (mem_to_waste > free_wasted_mem)
        return result;

    // calculate virtual memory
    //
    // *------------------* *------------------* *------------------*
    // | lower guard page | | user alloc pages | | upper guard page |
    // *------------------* *------------------* *------------------*
    size_t mem_to_alloc = (2 * page_size) + size + mem_to_waste;
    if (mem_to_alloc > free_virtual_mem)
        return result;

    result = pas_large_heap_try_allocate_and_forget(large_heap, mem_to_alloc, page_size,heap_config, transaction);
    if (!result.did_succeed)
        return result;

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
    pas_pgm_storage *value = pas_utility_heap_try_allocate(sizeof(pas_pgm_storage), "pas_pgm_hash_map_VALUE");
    PAS_ASSERT(value);

    value->mem_to_alloc              = mem_to_alloc;
    value->mem_to_waste              = mem_to_waste;
    value->size_of_data_pages        = size + mem_to_waste;
    value->lower_guard_page          = lower_guard_page;
    value->upper_guard_page          = upper_guard_page;
    value->start_of_data_pages       = result.begin + page_size;
    value->allocation_size_requested = size;

    pas_ptr_hash_map_add_result add_result = pas_ptr_hash_map_add(&pas_pgm_hash_map, key, NULL,&pas_large_utility_free_heap_allocation_config);
    PAS_ASSERT(add_result.is_new_entry);

    add_result.entry->key = key;
    add_result.entry->value = value;

    free_wasted_mem  -= mem_to_waste;
    free_virtual_mem -= mem_to_alloc;

    if (verbose)
        pas_probabilistic_guard_malloc_debug_info(key, value, "Allocating memory");

    result.begin = (uintptr_t)key;

    // 3 pages are the minimum required for PGM
    if (free_virtual_mem < 3 * page_size)
        pas_pgm_can_use = false;

    return result;
}

void pas_probabilistic_guard_malloc_deallocate(void* mem)
{
    pas_heap_lock_assert_held();
    static const bool verbose = false;

    if (verbose)
        printf("Memory Address Requested to Deallocate %p\n", mem);

    uintptr_t * key = (uintptr_t *) mem;

    pas_ptr_hash_map_entry * entry = pas_ptr_hash_map_find(&pas_pgm_hash_map, key);
    if (!entry || !entry->value)
        return;

    pas_pgm_storage * value = (pas_pgm_storage *) entry->value;
    int mprotect_res = mprotect( (void *) value->start_of_data_pages, value->size_of_data_pages, PROT_NONE);
    PAS_ASSERT(!mprotect_res);

    // ensure physical addresses are released
    // TODO: investigate using MADV_FREE_REUSABLE instead
    int madvise_res = madvise((void *) value->start_of_data_pages, value->size_of_data_pages, MADV_FREE);
    PAS_ASSERT(!madvise_res);

    bool removed = pas_ptr_hash_map_remove(&pas_pgm_hash_map, key, NULL, &pas_large_utility_free_heap_allocation_config);
    PAS_ASSERT(removed);

    free_wasted_mem  += value->mem_to_waste;
    free_virtual_mem += value->mem_to_alloc;

    if (verbose)
        pas_probabilistic_guard_malloc_debug_info(key, value, "Deallocating Memory");

    pas_pgm_can_use = true;

    pas_utility_heap_deallocate(value);
}

bool pas_probabilistic_guard_malloc_check_exists(uintptr_t mem)
{
    pas_heap_lock_assert_held();
    static const bool verbose = false;

    if (verbose)
        printf("Checking if is PGM entry\n");

    pas_ptr_hash_map_entry * entry = pas_ptr_hash_map_find(&pas_pgm_hash_map, (void *) mem);
    return (entry && entry->value);
}


#if PAS_COMPILER(CLANG)
#pragma mark -
#pragma mark Helper Functions
#endif

size_t pas_probabilistic_guard_malloc_get_free_virtual_memory(void)
{
    pas_heap_lock_assert_held();
    return free_virtual_mem;
}

size_t pas_probabilistic_guard_malloc_get_free_wasted_memory(void)
{
    pas_heap_lock_assert_held();
    return free_wasted_mem;
}

void pas_probabilistic_guard_malloc_debug_info(const void* key, const pas_pgm_storage* value, const char* operation)
{
    printf("******************************************************\n"
        " %s\n\n"
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
        operation,
        free_wasted_mem,
        free_virtual_mem,
        value->allocation_size_requested,
        value->mem_to_alloc,
        value->mem_to_waste,
        value->size_of_data_pages,
        (uintptr_t*) value->lower_guard_page,
        (uintptr_t*) value->upper_guard_page,
        (uintptr_t*) value->start_of_data_pages,
        key);
}

#endif /* LIBPAS_ENABLED */
