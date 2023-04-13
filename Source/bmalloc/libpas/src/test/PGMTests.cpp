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

#include <unistd.h>
#include <stdlib.h>

#include "TestHarness.h"

#include "bmalloc_heap.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_heap.h"
#include "iso_heap.h"
#include "iso_heap_config.h"
#include "pas_heap_ref_kind.h"

using namespace std;

namespace {

/* Test single PGM Allocation to ensure basic functionality is working. */
void testPGMSingleAlloc() {
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, getpagesize());
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    size_t init_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t init_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    size_t alloc_size = 1024;
    pas_allocation_result result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, &iso_heap_config, &transaction);
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
    return;
}

/* Testing multiple allocations to ensure numerous allocations are correctly handled. */
void testPGMMultipleAlloc() {
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, getpagesize());
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    size_t init_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t init_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    size_t num_allocations = 100;
    pas_allocation_result mem_storage[num_allocations];

    for (size_t i = 0; i < num_allocations; i++ ) {
        size_t alloc_size = random() % 100000;
        mem_storage[i] = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, &iso_heap_config, &transaction);
        pas_allocation_result mem = mem_storage[i];
        memset(reinterpret_cast<void *>(mem.begin), 0x42, alloc_size);
    }

    for (size_t i = 0; i < num_allocations; i++ ) {
        pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(mem_storage[i].begin));
    }

    size_t updated_free_virtual_mem = pas_probabilistic_guard_malloc_get_free_virtual_memory();
    size_t updated_free_wasted_mem = pas_probabilistic_guard_malloc_get_free_wasted_memory();

    CHECK_EQUAL(init_free_virtual_mem, updated_free_virtual_mem);
    CHECK_EQUAL(init_free_wasted_mem, updated_free_wasted_mem);

    pas_heap_lock_unlock();
}

/* Ensure reallocating PGM allocations works correctly. */
void testPGMRealloc()
{

    /* setup code */
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, getpagesize());
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    PAS_UNUSED_PARAM(heap);

    /* Realloc the same size */
    pas_heap_lock_lock();
    pas_allocation_result alloc_memory = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 10000000, &iso_heap_config, &transaction);
    pas_heap_lock_unlock();

    void* new_realloc_memory = bmalloc_try_reallocate((void *) alloc_memory.begin, 10000000, pas_reallocate_free_always);

    /* Realloc bigger size */
    pas_heap_lock_lock();
    alloc_memory = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 10000000, &iso_heap_config, &transaction);
    pas_heap_lock_unlock();

    new_realloc_memory = bmalloc_try_reallocate((void *) alloc_memory.begin, 20000000, pas_reallocate_free_always);

    /* Realloc smaller size */
    pas_heap_lock_lock();
    alloc_memory = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 10000000, &iso_heap_config, &transaction);
    pas_heap_lock_unlock();

    new_realloc_memory = bmalloc_try_reallocate((void *) alloc_memory.begin, 05000000, pas_reallocate_free_always);

    /* Realloc size of 0 */
    pas_heap_lock_lock();
    alloc_memory = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 10000000, &iso_heap_config, &transaction);
    pas_heap_lock_unlock();

    new_realloc_memory = bmalloc_try_reallocate((void *) alloc_memory.begin, 0, pas_reallocate_free_always);
}

/* Ensure all PGM errors cases are handled. */
void testPGMErrors() {
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, getpagesize());
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);

    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    pas_allocation_result result;

    /* Test invalid alloc size */
    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 0, &iso_heap_config, &transaction);
    CHECK(!result.begin);
    CHECK(!result.did_succeed);

    /* Test NULL heap */
    result = pas_probabilistic_guard_malloc_allocate(nullptr, 1024, &iso_heap_config, &transaction);
    CHECK(!result.begin);
    CHECK(!result.did_succeed);

    /* Test allocating more than virtual memory available */
    result = pas_probabilistic_guard_malloc_allocate(nullptr, 1024 * 1024 * 1024 + 1, &iso_heap_config, &transaction);
    CHECK(!result.begin);
    CHECK(!result.did_succeed);

    /* Test allocating when wasted memory is full */
    size_t num_allocations = 1000;
    pas_allocation_result mem_storage[num_allocations];
    for (size_t i = 0; i < num_allocations; i++ ) {
        size_t alloc_size = 1; /* A small alloc size wastes more memory */
        mem_storage[i] = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, &iso_heap_config, &transaction);
    }

    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 1, &iso_heap_config, &transaction);
    CHECK(!result.begin);
    CHECK(!result.did_succeed);

    for (size_t i = 0; i < num_allocations; i++ ) {
        pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(mem_storage[i].begin));
    }

    /* Test deallocating invalid memory locations */
    pas_probabilistic_guard_malloc_deallocate(nullptr);
    pas_probabilistic_guard_malloc_deallocate((void *) -1);
    pas_probabilistic_guard_malloc_deallocate((void *) 0x42);

    /* Test deallocating same memory location multiple times */
    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, 1, &iso_heap_config, &transaction);
    CHECK(result.begin);
    CHECK(result.did_succeed);

    pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(result.begin));
    pas_probabilistic_guard_malloc_deallocate(reinterpret_cast<void *>(result.begin));

    pas_heap_lock_unlock();
}

} // anonymous namespace

void addPGMTests() {
    ADD_TEST(testPGMSingleAlloc());
    ADD_TEST(testPGMMultipleAlloc());
    ADD_TEST(testPGMRealloc());
    ADD_TEST(testPGMErrors());
}
