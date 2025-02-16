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

#include <bit>
#include <mach/arm/kern_return.h>
#include <stdlib.h>
#include <unistd.h>

#include "TestHarness.h"

#include "bmalloc_heap.h"
#include "iso_heap.h"
#include "iso_heap_config.h"
#include "pas_heap.h"
#include "pas_heap_ref_kind.h"
#include "pas_large_utility_free_heap.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_ptr_hash_map.h"
#include "pas_report_crash.h"
#include "pas_root.h"

using namespace std;

namespace {

// checkMalloc verifies an allocation returned an aligned non-null pointer.
void checkMalloc(void* ptr)
{
    static const size_t expectedAlignment = 16;
    CHECK(ptr);
    CHECK(!(std::bit_cast<uintptr_t>(ptr) % expectedAlignment));
}

/* Test single PGM Allocation to ensure basic functionality is working. */
void testPGMSingleAlloc()
{
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, 1);
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    size_t init_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t init_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    size_t alloc_size = 1024;
    pas_allocation_result result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    CHECK(result.begin);

    size_t updated_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t updated_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    CHECK_EQUAL(init_free_virtual_mem - (3 * getpagesize()), updated_free_virtual_mem);
    CHECK_EQUAL(init_free_wasted_mem - (getpagesize() - alloc_size), updated_free_wasted_mem);

    pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(result.begin));

    updated_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    updated_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    CHECK_EQUAL(init_free_virtual_mem, updated_free_virtual_mem);
    CHECK_EQUAL(init_free_wasted_mem, updated_free_wasted_mem);

    pas_heap_lock_unlock();
}

/* Testing multiple allocations to ensure numerous allocations are correctly handled. */
void testPGMMultipleAlloc()
{
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, 1);
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    size_t init_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t init_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    size_t num_allocations = 100;
    pas_allocation_result mem_storage[num_allocations];

    for (size_t i = 0; i < num_allocations; ++i) {
        size_t alloc_size = random() % 100000;
        mem_storage[i] = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
        pas_allocation_result mem = mem_storage[i];
        memset(reinterpret_cast<void *>(mem.begin), 0x42, alloc_size);
    }

    for (size_t i = 0; i < num_allocations; ++i)
        pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(mem_storage[i].begin));

    size_t updated_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t updated_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    CHECK_EQUAL(init_free_virtual_mem, updated_free_virtual_mem);
    CHECK_EQUAL(init_free_wasted_mem, updated_free_wasted_mem);

    pas_heap_lock_unlock();
}

void testPGMAlignedAlloc()
{
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, 1);
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    const size_t num_allocations = 100;
    pas_allocation_result mem_storage[num_allocations];

    for (size_t shift = 0; shift < 20; ++shift) {
        size_t init_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
        size_t init_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

        size_t alignment = 1 << shift;
        for (size_t i = 0; i < num_allocations; ++i) {
            size_t alloc_size = random() % 100000;
            pas_allocation_result mem = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, alignment, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
            mem_storage[i] = mem;
            memset(reinterpret_cast<void *>(mem.begin), 0x42, alloc_size);
            CHECK(pas_is_aligned(mem.begin, alignment));
        }
        for (size_t i = 0; i < num_allocations; ++i)
            pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(mem_storage[i].begin));

        size_t updated_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
        size_t updated_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

        CHECK_EQUAL(init_free_virtual_mem, updated_free_virtual_mem);
        CHECK_EQUAL(init_free_wasted_mem, updated_free_wasted_mem);
    }

    pas_heap_lock_unlock();
}

/* Ensure reallocating PGM allocations works correctly. */
void testPGMRealloc()
{

    /* setup code */
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, 1);
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    PAS_UNUSED_PARAM(heap);

    /* Realloc the same size */
    pas_heap_lock_lock();
    pas_allocation_result alloc_memory = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 10000000, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    pas_heap_lock_unlock();

    void* new_realloc_memory = bmalloc_try_reallocate((void *) alloc_memory.begin, 10000000, pas_non_compact_allocation_mode, pas_reallocate_free_always);

    /* Realloc bigger size */
    pas_heap_lock_lock();
    alloc_memory = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 10000000, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    pas_heap_lock_unlock();

    new_realloc_memory = bmalloc_try_reallocate((void *) alloc_memory.begin, 20000000, pas_non_compact_allocation_mode, pas_reallocate_free_always);

    /* Realloc smaller size */
    pas_heap_lock_lock();
    alloc_memory = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 10000000, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    pas_heap_lock_unlock();

    new_realloc_memory = bmalloc_try_reallocate((void *) alloc_memory.begin, 05000000, pas_non_compact_allocation_mode, pas_reallocate_free_always);

    /* Realloc size of 0 */
    pas_heap_lock_lock();
    alloc_memory = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 10000000, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    pas_heap_lock_unlock();

    new_realloc_memory = bmalloc_try_reallocate((void *) alloc_memory.begin, 0, pas_non_compact_allocation_mode, pas_reallocate_free_always);
}

void testPGMMetaData()
{
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, 1);
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    size_t init_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t init_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();
    pas_ptr_hash_map_entry* metadata_array = pas_probabilistic_guard_malloc_get_metadata_array();

    constexpr size_t num_allocations = 100;
    size_t num_success_allocs = 0, num_de_allocs = 0;
    pas_allocation_result mem_storage[num_allocations];

    for (size_t i = 0; i < num_allocations; ++i) {
        size_t alloc_size = 20;
        pas_allocation_result mem = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
        mem_storage[i] = mem;
        if (mem_storage[i].did_succeed)
            num_success_allocs++;
    }

    for (size_t i = 0; i < num_allocations; ++i) {
        pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(mem_storage[i].begin));
        if (mem_storage[i].did_succeed) {
            /* MetaData entry should be preserved during above deallocation for respective object memory */
            if (reinterpret_cast<uintptr_t>(metadata_array[i % MAX_PGM_DEALLOCATED_METADATA_ENTRIES].key) == mem_storage[i].begin)
                num_de_allocs++;
        }
    }

    size_t updated_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t updated_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    CHECK_EQUAL(init_free_virtual_mem, updated_free_virtual_mem);
    CHECK_EQUAL(init_free_wasted_mem, updated_free_wasted_mem);

    CHECK_EQUAL(num_success_allocs, num_de_allocs);
    pas_heap_lock_unlock();
}

/* Ensure all PGM errors cases are handled. */
void testPGMErrors()
{
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, 1);
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);

    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    pas_allocation_result result;

    /* Test invalid alloc size */
    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 0, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    CHECK(!result.begin);
    CHECK(!result.did_succeed);

    /* Test NULL heap */
    result = pas_probabilistic_guard_malloc_allocate(nullptr, 1024, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    CHECK(!result.begin);
    CHECK(!result.did_succeed);

    /* Test allocating more than virtual memory available */
    result = pas_probabilistic_guard_malloc_allocate(nullptr, 1024 * 1024 * 1024 + 1, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    CHECK(!result.begin);
    CHECK(!result.did_succeed);

    /* Test allocating when wasted memory is full */
    size_t num_allocations = 1000;
    pas_allocation_result mem_storage[num_allocations];
    for (size_t i = 0; i < num_allocations; ++i) {
        size_t alloc_size = 1; /* A small alloc size wastes more memory */
        mem_storage[i] = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    }

    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 1, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    CHECK(!result.begin);
    CHECK(!result.did_succeed);

    for (size_t i = 0; i < num_allocations; ++i)
        pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(mem_storage[i].begin));

    /* Test deallocating invalid memory locations */
    pas_probabilistic_guard_malloc_deallocate(nullptr);
    pas_probabilistic_guard_malloc_deallocate((void *) -1);
    pas_probabilistic_guard_malloc_deallocate((void *) 0x42);

    /* Test deallocating same memory location multiple times */
    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 1, 1, pas_non_compact_allocation_mode, &iso_heap_config, &transaction);
    CHECK(result.begin);
    CHECK(result.did_succeed);

    pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(result.begin));
    pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(result.begin));

    pas_heap_lock_unlock();
}

void testPGMMetadataVectorManagement()
{
    pas_probabilistic_guard_malloc_initialize_pgm_as_enabled(1);

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    size_t total_allocations = 12;
    int** int_arr = static_cast<int**>(bmalloc_allocate(total_allocations * sizeof(int*), pas_non_compact_allocation_mode));
    // Allocate arrays of ints, of random size between [1, 30000].
    for (size_t i = 0; i < total_allocations; ++i) {
        int_arr[i] = static_cast<int*>(bmalloc_allocate(((rand() % 30000) + 1) * sizeof(int), pas_non_compact_allocation_mode));
        checkMalloc(int_arr[i]);
    }

    pas_heap_lock_lock();
    // Deallocate all previous allocations, except holder `int_arr`.
    for (size_t i = 0; i < total_allocations; ++i)
        pas_probabilistic_guard_malloc_deallocate(int_arr[i]);

    pas_ptr_hash_map* hash_map = root->pas_pgm_hash_map_instance;
    CHECK(hash_map);

    // Make sure we only hold MAX_PGM_DEALLOCATED_METADATA_ENTRIES + 1 entries in our hash map. +1 for the still allocated `int** int_arr` holder array.
    CHECK_EQUAL(hash_map->key_count, MAX_PGM_DEALLOCATED_METADATA_ENTRIES + 1u);
    pas_ptr_hash_map_entry* metadata_array = pas_probabilistic_guard_malloc_get_metadata_array();

    size_t count = 0;
    for (; count < MAX_PGM_DEALLOCATED_METADATA_ENTRIES; ++count) {
        if (!metadata_array[count].key)
            break;
    }
    CHECK_EQUAL(count, (size_t)MAX_PGM_DEALLOCATED_METADATA_ENTRIES);
    pas_heap_lock_unlock();
}

void testPGMMetadataVectorManagementFewDeallocations()
{
    pas_probabilistic_guard_malloc_initialize_pgm_as_enabled(1);

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    size_t total_allocations = 12;
    int** int_arr = static_cast<int**>(bmalloc_allocate(total_allocations * sizeof(int*), pas_non_compact_allocation_mode));
    // Allocate arrays of ints, of random size between [1, 30000].
    for (size_t i = 0; i < total_allocations; ++i) {
        int_arr[i] = static_cast<int*>(bmalloc_allocate(((rand() % 30000) + 1) * sizeof(int), pas_non_compact_allocation_mode));
        checkMalloc(int_arr[i]);
    }

    pas_heap_lock_lock();
    // Deallocate 4 int arrays.
    size_t num_deallocations = 4;
    for (size_t i = 0; i < num_deallocations; ++i)
        pas_probabilistic_guard_malloc_deallocate(int_arr[i]);

    pas_ptr_hash_map* hash_map = root->pas_pgm_hash_map_instance;
    CHECK(hash_map);

    CHECK_EQUAL(hash_map->key_count, 13u);
    pas_ptr_hash_map_entry* metadata_array = pas_probabilistic_guard_malloc_get_metadata_array();

    size_t count = 0;
    for (; count < MAX_PGM_DEALLOCATED_METADATA_ENTRIES; ++count) {
        if (!metadata_array[count].key)
            break;
    }
    CHECK_EQUAL(count, (size_t)std::min(MAX_PGM_DEALLOCATED_METADATA_ENTRIES, (int)num_deallocations));
    pas_heap_lock_unlock();
}

void testPGMMetadataDoubleFreeBehavior()
{
    pas_probabilistic_guard_malloc_initialize_pgm_as_enabled(1);

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    size_t total_allocations = 20;
    int** int_arr = static_cast<int**>(bmalloc_allocate(total_allocations * sizeof(int*), pas_non_compact_allocation_mode));
    // Allocate arrays of ints, of random size between [1, 30000].
    for (size_t i = 0; i < total_allocations; ++i) {
        int_arr[i] = static_cast<int*>(bmalloc_allocate(((rand() % 30000) + 1) * sizeof(int), pas_non_compact_allocation_mode));
        checkMalloc(int_arr[i]);
    }

    pas_heap_lock_lock();
    // Deallocate all previous allocations, except holder `int_arr`.
    for (size_t i = 0; i < total_allocations; ++i)
        pas_probabilistic_guard_malloc_deallocate(int_arr[i]);

    // Deallocate a previously deallocated chunk of memory (double free)
    pas_probabilistic_guard_malloc_deallocate(int_arr[0]);

    pas_ptr_hash_map* hash_map = root->pas_pgm_hash_map_instance;
    CHECK(hash_map);

    // Make sure we only hold MAX_PGM_DEALLOCATED_METADATA_ENTRIES + 1 entries in our hash map. +1 for the still allocated `int** int_arr` holder array.
    CHECK_EQUAL(hash_map->key_count, MAX_PGM_DEALLOCATED_METADATA_ENTRIES + 1u);
    pas_ptr_hash_map_entry* metadata_array = pas_probabilistic_guard_malloc_get_metadata_array();

    size_t count = 0;
    for (; count < MAX_PGM_DEALLOCATED_METADATA_ENTRIES; ++count) {
        if (!metadata_array[count].key)
            break;
    }
    CHECK_EQUAL(count, (size_t)MAX_PGM_DEALLOCATED_METADATA_ENTRIES);
    pas_heap_lock_unlock();
}

void testPGMMetadataVectorManagementRehash()
{
    pas_probabilistic_guard_malloc_initialize_pgm_as_enabled(1);

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    int** int_arr = static_cast<int**>(bmalloc_allocate(MAX_PGM_DEALLOCATED_METADATA_ENTRIES * sizeof(int*), pas_non_compact_allocation_mode));
    // Allocate arrays of ints, of random size between [1, 30000].
    for (size_t i = 0; i < MAX_PGM_DEALLOCATED_METADATA_ENTRIES; ++i) {
        int_arr[i] = static_cast<int*>(bmalloc_allocate(((rand() % 30000) + 1) * sizeof(int), pas_non_compact_allocation_mode));
        checkMalloc(int_arr[i]);
    }

    pas_heap_lock_lock();
    // Fill the pas_probabilistic_guard_malloc_get_metadata_array() metadata array.
    for (size_t i = 0; i < MAX_PGM_DEALLOCATED_METADATA_ENTRIES; ++i)
        pas_probabilistic_guard_malloc_deallocate(int_arr[i]);

    pas_ptr_hash_map* hash_map = root->pas_pgm_hash_map_instance;
    CHECK(hash_map);

    unsigned old_size = hash_map->table_size;
    // Force expand and rehash of the hash map.
    pas_ptr_hash_map_expand(hash_map, NULL, &pas_large_utility_free_heap_allocation_config);

    // Check that the expand actually changed the size of the table, forcing a realloc of the table and rehash.
    CHECK_NOT_EQUAL(old_size, hash_map->table_size);
    pas_heap_lock_unlock();

    for (size_t i = 0; i < MAX_PGM_DEALLOCATED_METADATA_ENTRIES; ++i) {
        int_arr[i] = static_cast<int*>(bmalloc_allocate(((rand() % 30000) + 1) * sizeof(int), pas_non_compact_allocation_mode));
        checkMalloc(int_arr[i]);
    }
    pas_heap_lock_lock();

    // Shrink back to the original size to encourage elements to collide and rehash to different locations.
    pas_ptr_hash_map_shrink(hash_map, NULL, &pas_large_utility_free_heap_allocation_config);

    // Flush the metadata array, ensuring this works across hashtable resizes and rehashes.
    for (size_t i = 0; i < MAX_PGM_DEALLOCATED_METADATA_ENTRIES; ++i)
        pas_probabilistic_guard_malloc_deallocate(int_arr[i]);

    // Make sure we only hold MAX_PGM_DEALLOCATED_METADATA_ENTRIES + 1 entries in our hash map. +1 for the still allocated `int** int_arr` holder array.
    CHECK_EQUAL(hash_map->key_count, MAX_PGM_DEALLOCATED_METADATA_ENTRIES + 1u);

    pas_ptr_hash_map_entry* metadata_array = pas_probabilistic_guard_malloc_get_metadata_array();
    size_t count = 0;
    for (; count < MAX_PGM_DEALLOCATED_METADATA_ENTRIES; ++count) {
        if (!metadata_array[count].key)
            break;
    }
    CHECK_EQUAL(count, (size_t)MAX_PGM_DEALLOCATED_METADATA_ENTRIES);
    pas_heap_lock_unlock();
}

} // anonymous namespace

void addPGMTests()
{
    ADD_TEST(testPGMSingleAlloc());
    ADD_TEST(testPGMMultipleAlloc());
    ADD_TEST(testPGMAlignedAlloc());
    ADD_TEST(testPGMRealloc());
    ADD_TEST(testPGMErrors());
    ADD_TEST(testPGMMetaData());
    ADD_TEST(testPGMMetadataVectorManagement());
    ADD_TEST(testPGMMetadataVectorManagementFewDeallocations());
    ADD_TEST(testPGMMetadataDoubleFreeBehavior());
    ADD_TEST(testPGMMetadataVectorManagementRehash());
}
