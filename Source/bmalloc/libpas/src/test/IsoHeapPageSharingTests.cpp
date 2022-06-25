/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "TestHarness.h"

#if PAS_ENABLE_ISO && TLC

#include "HeapLocker.h"
#include "LargeSharingPoolDump.h"
#include <condition_variable>
#include <functional>
#include "iso_heap.h"
#include "iso_heap_config.h"
#include "iso_heap_innards.h"
#include <mutex>
#include "pas_all_heaps.h"
#include "pas_baseline_allocator_table.h"
#include "pas_heap.h"
#include "pas_heap_for_config.h"
#include "pas_large_free_heap_helpers.h"
#include "pas_large_sharing_pool.h"
#include "pas_large_utility_free_heap.h"
#include "pas_page_malloc.h"
#include "pas_scavenger.h"
#include "pas_segregated_size_directory.h"
#include "pas_thread_local_cache.h"
#include <set>
#include <vector>

using namespace std;

namespace {

void testTakePages(unsigned firstObjectSize,
                   size_t resultingFirstObjectSize,
                   size_t firstCount,
                   unsigned secondObjectSize,
                   size_t resultingSecondObjectSize,
                   size_t secondCount,
                   size_t thirdCount,
                   size_t expectedNumberOfCommittedPages,
                   size_t expectedNumberOfPageIndices,
                   size_t expectedNumberOfCommittedPagesAfterReusing,
                   size_t expectedNumberOfPageIndicesAfterReusing,
                   size_t expectedNumberOfCommittedPagesAtEnd,
                   size_t expectedNumberOfPageIndicesAtEnd,
                   bool checkHeapLock)
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    pas_large_sharing_pool_validate_each_splat = true;

    set<void*> objects;
    vector<void*> objectList;
    
    pas_heap_ref firstHeapRef = ISO_HEAP_REF_INITIALIZER(firstObjectSize);
    pas_heap_ref secondHeapRef = ISO_HEAP_REF_INITIALIZER(secondObjectSize);
    
    pas_heap* firstHeap = iso_heap_ref_get_heap(&firstHeapRef);
    pas_heap* secondHeap = iso_heap_ref_get_heap(&secondHeapRef);
    
    CHECK_EQUAL(
        firstHeap->segregated_heap.runtime_config->sharing_mode,
        pas_share_pages);
    CHECK_EQUAL(
        secondHeap->segregated_heap.runtime_config->sharing_mode,
        pas_share_pages);

    pas_page_sharing_pool_verify(&pas_physical_page_sharing_pool, pas_lock_is_not_held);
    
    for (size_t index = firstCount; index--;) {
        pas_page_sharing_pool_verify(&pas_physical_page_sharing_pool, pas_lock_is_not_held);
        void* object = iso_try_allocate(&firstHeapRef);
        CHECK(object);
        CHECK(!objects.count(object));
        objects.insert(object);
        objectList.push_back(object);
        CHECK_EQUAL(pas_segregated_size_directory_for_object(
                        reinterpret_cast<uintptr_t>(object),
                        &iso_heap_config)->object_size,
                    resultingFirstObjectSize);
    }
    
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config), pas_lock_is_not_held);
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_page_sharing_pool_verify(&pas_physical_page_sharing_pool, pas_lock_is_not_held);
    
    for (void* object : objectList) {
        iso_deallocate(object);
        objects.erase(object);
    }
    objectList.clear();
    
    auto numCommittedPages = [&] () -> size_t {
        return pas_segregated_heap_num_committed_views(&firstHeap->segregated_heap)
             + pas_segregated_heap_num_committed_views(&secondHeap->segregated_heap);
    };
    
    auto numPageIndices = [&] () -> size_t {
        return pas_segregated_heap_num_views(&firstHeap->segregated_heap)
             + pas_segregated_heap_num_views(&secondHeap->segregated_heap);
    };
    
    CHECK_EQUAL(numCommittedPages(),
                expectedNumberOfCommittedPages);
    CHECK_EQUAL(numPageIndices(),
                expectedNumberOfPageIndices);

    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config), pas_lock_is_not_held);
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_page_sharing_pool_verify(&pas_physical_page_sharing_pool, pas_lock_is_not_held);

    // This test assumes that we will allocate and free enough objects of the first size that even
    // if we don't verifyBeforeReusing (which shrinks the heap), we will end up having free pages
    // in the first size class. That will have to trigger a delta if the use_epoch didn't do that
    // already.
    CHECK(pas_page_sharing_pool_has_delta(&pas_physical_page_sharing_pool));

    if (verbose)
        cout << "Starting loop that takes pages.\n";
    
    for (size_t index = 0; index < secondCount; ++index) {
        if (verbose)
            cout << "index = " << index << "\n";
        
        pas_page_sharing_pool_verify(&pas_physical_page_sharing_pool, pas_lock_is_not_held);
        if (index == 1) {
            // This test assumes that we will allocate and free enough objects of the first size
            // that even if we don't verifyBeforeReusing (which shrinks the heap), we will end up
            // reusing pages from the first size class.
            CHECK(pas_page_sharing_pool_has_current_participant(
                      &pas_physical_page_sharing_pool));
        }

        if (verbose)
            cout << "Allocating.\n";
        
        void* object = iso_try_allocate(&secondHeapRef);

        if (verbose)
            cout << "Did allocate.\n";
    
        CHECK(object);
        CHECK(!objects.count(object));
        objects.insert(object);
        objectList.push_back(object);
        CHECK_EQUAL(pas_segregated_size_directory_for_object(
                        reinterpret_cast<uintptr_t>(object),
                        &iso_heap_config)->object_size,
                    resultingSecondObjectSize);
    }
    
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config), pas_lock_is_not_held);
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_page_sharing_pool_verify(&pas_physical_page_sharing_pool, pas_lock_is_not_held);
    
    CHECK_EQUAL(numCommittedPages(),
                expectedNumberOfCommittedPagesAfterReusing);
    CHECK_EQUAL(numPageIndices(),
                expectedNumberOfPageIndicesAfterReusing);
    
    if (checkHeapLock) {
        for (void* object : objectList) {
            iso_deallocate(object);
            objects.erase(object);
        }
        objectList.clear();
        
        pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config), pas_lock_is_not_held);
        pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    }
    
    for (size_t index = 0; index < thirdCount; ++index) {
        pas_page_sharing_pool_verify(&pas_physical_page_sharing_pool, pas_lock_is_not_held);
        void* object = iso_try_allocate(&firstHeapRef);
        CHECK(object);
        CHECK(!objects.count(object));
        objects.insert(object);
        CHECK_EQUAL(pas_segregated_size_directory_for_object(
                        reinterpret_cast<uintptr_t>(object),
                        &iso_heap_config)->object_size,
                    resultingFirstObjectSize);
    }
    
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config), pas_lock_is_not_held);
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_page_sharing_pool_verify(&pas_physical_page_sharing_pool, pas_lock_is_not_held);
    
    CHECK_EQUAL(numCommittedPages(),
                expectedNumberOfCommittedPagesAtEnd);
    CHECK_EQUAL(numPageIndices(),
                expectedNumberOfPageIndicesAtEnd);
    
    CHECK_EQUAL(pas_scavenger_current_state, pas_scavenger_state_no_thread);
}

void testTakePagesFromCorrectHeap(unsigned numHeaps,
                                  std::function<size_t(unsigned)> sizeFunc,
                                  unsigned numHeapsInFirstPhase)
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();

    pas_heap_for_config_force_bootstrap = true; // Otherwise we take things from utility and that throws this test off.
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    pas_heap_ref* heapRefs = new pas_heap_ref[numHeaps];
    void** objects = new void*[numHeaps];
    pas_segregated_size_directory** directories = new pas_segregated_size_directory*[numHeaps];
    
    unsigned numHeapsInSecondPhase = numHeaps - numHeapsInFirstPhase;
    
    for (unsigned i = 0; i < numHeaps; ++i)
        heapRefs[i] = ISO_HEAP_REF_INITIALIZER(sizeFunc(i));
    
    auto allocate = [&] (unsigned i) {
        if (verbose)
            cout << "Allocating i = " << i << ", size = " << sizeFunc(i) << "\n";
        objects[i] = iso_try_allocate(heapRefs + i);
        if (verbose)
            cout << "    Allocated object at " << objects[i] << "\n";
        directories[i] = pas_segregated_size_directory_for_object(
            reinterpret_cast<uintptr_t>(objects[i]), &iso_heap_config);
        if (directories[i])
            CHECK_EQUAL(directories[i]->object_size, sizeFunc(i));
        else {
            pas_heap_summary summary =
                pas_large_sharing_pool_compute_summary(
                    pas_range_create(
                        reinterpret_cast<uintptr_t>(objects[i]),
                        reinterpret_cast<uintptr_t>(objects[i]) + sizeFunc(i)),
                    pas_large_sharing_pool_compute_summary_known_allocated,
                    pas_lock_is_not_held);
            CHECK(!summary.free);
            CHECK(summary.allocated);
            CHECK(summary.committed);
            CHECK(!summary.decommitted);
        }
        
        if (i < 25 && verbose)
            dumpLargeSharingPool();
    };
    
    for (unsigned i = 0; i < numHeapsInFirstPhase; ++i)
        allocate(i);
    
    for (unsigned i = 0; i < numHeapsInFirstPhase; ++i)
        iso_deallocate(objects[i]);
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);
    
    if (verbose)
        dumpLargeSharingPool();
    
    auto verify = [&] (unsigned i) {
        if (verbose)
            cout << "Verifying i = " << i << "\n";
        unsigned cutoff = std::min(i, numHeapsInFirstPhase);
        for (unsigned j = 0; j < cutoff; ++j) {
            if (directories[j]) {
                CHECK_EQUAL(pas_segregated_directory_size(&directories[j]->base), 1);
                CHECK(pas_segregated_directory_is_eligible(&directories[j]->base, 0));
                CHECK(!pas_segregated_directory_is_empty(&directories[j]->base, 0));
                CHECK(!pas_segregated_directory_is_committed(&directories[j]->base, 0));
            } else {
                pas_heap_summary summary =
                    pas_large_sharing_pool_compute_summary(
                        pas_range_create(
                            reinterpret_cast<uintptr_t>(objects[j]),
                            reinterpret_cast<uintptr_t>(objects[j]) + sizeFunc(j)),
                        pas_large_sharing_pool_compute_summary_known_free,
                        pas_lock_is_not_held);
                CHECK(summary.free);
                CHECK(!summary.allocated);
                CHECK(summary.decommitted);
            }
        }
        for (unsigned j = cutoff; j < numHeapsInFirstPhase; ++j) {
            if (directories[j]) {
                CHECK_EQUAL(pas_segregated_directory_size(&directories[j]->base), 1);
                CHECK(pas_segregated_directory_is_eligible(&directories[j]->base, 0));
                CHECK(pas_segregated_directory_is_committed(&directories[j]->base, 0));
                CHECK(pas_segregated_directory_is_empty(&directories[j]->base, 0));
                CHECK(pas_segregated_view_get_page(
                          pas_segregated_directory_get(&directories[j]->base, 0)));
            } else {
                pas_heap_summary summary =
                    pas_large_sharing_pool_compute_summary(
                        pas_range_create(
                            reinterpret_cast<uintptr_t>(objects[j]),
                            reinterpret_cast<uintptr_t>(objects[j]) + sizeFunc(j)),
                        pas_large_sharing_pool_compute_summary_known_free,
                        pas_lock_is_not_held);
                CHECK(summary.free);
                CHECK(!summary.allocated);
                if (!cutoff) {
                    CHECK(!summary.decommitted);
                    CHECK(summary.committed);
                } // Else we may have decommitted all of this.
            }
        }
    };
    
    for (unsigned i = 0; i < numHeapsInSecondPhase; ++i) {
        verify(i);
        allocate(i + numHeapsInFirstPhase);
    }
    
    verify(numHeapsInSecondPhase);
}

void testLargeHeapTakesPagesFromCorrectSmallHeap()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto addObject = [&] (void* object) {
        CHECK(object);
        objects.push_back(object);
    };
    
    for (size_t size = 0; size < 10000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating big object.\n";
    
    addObject(iso_try_allocate(&heapRefFour));
    
    if (verbose)
        cout << "Did allocate big object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.decommitted, 10000000);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
}

void testLargeHeapTakesPagesFromCorrectSmallHeapAllocateAfterFree()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto checkObject = [&] (void* object) -> void* {
        CHECK(object);
        return object;
    };
    
    auto addObject = [&] (void* object) {
        objects.push_back(checkObject(object));
    };
    
    for (size_t size = 0; size < 10000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    iso_deallocate(checkObject(iso_try_allocate(&heapRefOne)));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating big object.\n";
    
    checkObject(iso_try_allocate(&heapRefFour));
    
    if (verbose)
        cout << "Did allocate big object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.decommitted, 10000384);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
}

void testLargeHeapTakesPagesFromCorrectSmallHeapWithFancyOrder()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto addObject = [&] (void* object) {
        CHECK(object);
        objects.push_back(object);
    };
    
    for (size_t size = 0; size < 5000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    for (size_t size = 0; size < 5000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating big object.\n";
    
    addObject(iso_try_allocate(&heapRefFour));
    
    if (verbose)
        cout << "Did allocate big object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.decommitted, 10000384);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
}

void testLargeHeapTakesPagesFromCorrectLargeHeap()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto addObject = [&] (void* object) {
        CHECK(object);
        objects.push_back(object);
    };
    
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    for (size_t size = 0; size < 10000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating big object.\n";
    
    addObject(iso_try_allocate(&heapRefFour));
    
    if (verbose)
        cout << "Did allocate big object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_LESS_EQUAL(summaryOne.decommitted, PAS_SMALL_PAGE_DEFAULT_SIZE * 2);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
}

void testLargeHeapTakesPagesFromCorrectLargeHeapAllocateAfterFreeOnSmallHeap()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto checkObject = [&] (void* object) -> void* {
        CHECK(object);
        return object;
    };
    
    auto addObject = [&] (void* object) {
        objects.push_back(checkObject(object));
    };

    auto deleteAllObjects = [&] () {
        for (void* object : objects) {
            iso_deallocate(object);
            pas_thread_local_cache_flush_deallocation_log(
                pas_thread_local_cache_get(&iso_heap_config),
                pas_lock_is_not_held);
        }
        objects.clear();
    };
    
    for (size_t size = 0; size < 10000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    for (size_t size = 0; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    if (verbose)
        cout << "summaryOne before = " << dumpToString(summaryOne, pas_heap_summary_dump) << "\n";
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);

    deleteAllObjects();

    // Use the first heap a decent amount.
    for (size_t size = 0; size < 5000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));

    deleteAllObjects();

    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&iso_heap_config),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating big object.\n";
    
    checkObject(iso_try_allocate(&heapRefFour));
    
    if (verbose)
        cout << "Did allocate big object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    if (verbose)
        cout << "summaryOne = " << dumpToString(summaryOne, pas_heap_summary_dump) << "\n";

    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    if (verbose)
        cout << "summaryTwo = " << dumpToString(summaryTwo, pas_heap_summary_dump) << "\n";

    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    if (verbose)
        cout << "summaryThree = " << dumpToString(summaryThree, pas_heap_summary_dump) << "\n";

    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    if (verbose)
        cout << "summaryFour = " << dumpToString(summaryFour, pas_heap_summary_dump) << "\n";

    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER(summaryOne.committed, 0);
    CHECK_GREATER(summaryOne.decommitted, 0);
    
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_LESS_EQUAL(summaryTwo.decommitted, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER(summaryThree.committed, 0);
    CHECK_GREATER(summaryThree.decommitted, 0);
    
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
}

void testLargeHeapTakesPagesFromCorrectLargeHeapAllocateAfterFreeOnAnotherLargeHeap()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(2000000);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto checkObject = [&] (void* object) -> void* {
        CHECK(object);
        return object;
    };
    
    auto addObject = [&] (void* object) {
        objects.push_back(checkObject(object));
    };

    if (verbose)
        cout << "Filling up heaps 1-3.\n";
    
    for (size_t size = 0; size < 10000000; size += 2000000)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    for (size_t size = 0; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_EQUAL(summaryOne.free, 0);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);

    if (verbose)
        cout << "Freeing all objects.\n";
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();

    if (verbose)
        cout << "Shrinking caches.\n";
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Filling up heap 1 and emptying it again.\n";
    
    for (size_t size = 0; size < 10000000; size += 2000000)
        addObject(iso_try_allocate(&heapRefOne));
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating big object.\n";
    
    checkObject(iso_try_allocate(&heapRefFour));
    
    if (verbose)
        cout << "Did allocate big object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_LESS_EQUAL(summaryTwo.decommitted, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2); // A weird memory layout could cause us to have to decommit two medium pages. It's usually one small page though.
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, pas_page_malloc_alignment() * 3); // Weird layouts could cause us to decommit up to almost three pages.
    CHECK_GREATER(summaryThree.decommitted, 10000000 - pas_page_malloc_alignment() * 3);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
}

void testLargeHeapTakesPagesFromCorrectLargeHeapWithFancyOrder()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto addObject = [&] (void* object) {
        CHECK(object);
        objects.push_back(object);
    };
    
    for (size_t size = 0; size < 5000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    for (size_t size = 0; size < 5000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 0);
    CHECK_EQUAL(summaryFour.decommitted, 0);

    if (verbose)
        printStatusReport();
    
    if (verbose)
        cout << "Allocating big object.\n";
    
    addObject(iso_try_allocate(&heapRefFour));
    
    if (verbose)
        cout << "Did allocate big object.\n";

    if (verbose)
        printStatusReport();
    
    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_LESS_EQUAL(summaryOne.decommitted, PAS_SMALL_PAGE_DEFAULT_SIZE * 3);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_GREATER_EQUAL(summaryTwo.free, 10000384);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
}

void testSmallHeapTakesPagesFromCorrectLargeHeap()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto addObject = [&] (void* object) {
        CHECK(object);
        objects.push_back(object);
    };
    
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    addObject(iso_try_allocate(&heapRefFour));
    for (size_t size = 0; size < 10000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_EQUAL(summaryTwo.free, 0);
    CHECK_EQUAL(summaryTwo.committed, 0);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_EQUAL(summaryTwo.free, 0);
    CHECK_EQUAL(summaryTwo.committed, 0);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating small object.\n";
    
    addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Did allocate small object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 512);
    CHECK_LESS_EQUAL(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE);
    CHECK_GREATER_EQUAL(summaryTwo.committed, PAS_SMALL_PAGE_DEFAULT_SIZE);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_LESS_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, 9000000 + pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 1000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    // It's possible that heap four has also been decommitted, if it was adjacent to heap three.
    
    for (size_t size = 512; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Did allocate the rest of the small objects.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_LESS_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_LESS(summaryFour.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryFour.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
}

void testSmallHeapTakesPagesFromCorrectLargeHeapWithFancyOrder()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto addObject = [&] (void* object) {
        CHECK(object);
        objects.push_back(object);
    };
    
    for (size_t size = 0; size < 5000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    addObject(iso_try_allocate(&heapRefFour));
    for (size_t size = 0; size < 5000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_EQUAL(summaryTwo.free, 0);
    CHECK_EQUAL(summaryTwo.committed, 0);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_EQUAL(summaryTwo.free, 0);
    CHECK_EQUAL(summaryTwo.committed, 0);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating small object.\n";
    
    addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Did allocate small object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 512);
    CHECK_LESS_EQUAL(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE);
    CHECK_GREATER_EQUAL(summaryTwo.committed, PAS_SMALL_PAGE_DEFAULT_SIZE);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_LESS_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, 9000000 + pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 1000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    // It's possible that heap four has also been decommitted, if it was adjacent to heap three.
    
    for (size_t size = 512; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Did allocate the rest of the small objects.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_LESS_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_LESS(summaryFour.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryFour.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
}

void testSmallHeapTakesPagesFromCorrectLargeHeapAllocateAfterFreeOnSmallHeap()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto checkObject = [&] (void* object) -> void* {
        CHECK(object);
        return object;
    };
    
    auto addObject = [&] (void* object) {
        objects.push_back(checkObject(object));
    };
    
    for (size_t size = 0; size < 10000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    addObject(iso_try_allocate(&heapRefFour));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_EQUAL(summaryTwo.free, 0);
    CHECK_EQUAL(summaryTwo.committed, 0);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    for (size_t size = 0; size < 10000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_EQUAL(summaryTwo.free, 0);
    CHECK_EQUAL(summaryTwo.committed, 0);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating small object.\n";
    
    checkObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Did allocate small object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 512);
    CHECK_LESS_EQUAL(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE);
    CHECK_GREATER_EQUAL(summaryTwo.committed, PAS_SMALL_PAGE_DEFAULT_SIZE);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_LESS_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, 9000000 + pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 1000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    // It's possible that heap four has also been decommitted, if it was adjacent to heap three.
    
    for (size_t size = 512; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Did allocate the rest of the small objects.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_LESS_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_LESS(summaryFour.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryFour.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
}

void testSmallHeapTakesPagesFromCorrectLargeHeapAllocateAfterFreeOnAnotherLargeHeap()
{
    static constexpr bool verbose = false;
    
    pas_scavenger_suspend();
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    pas_heap_ref heapRefOne = ISO_HEAP_REF_INITIALIZER(64);
    pas_heap_ref heapRefTwo = ISO_HEAP_REF_INITIALIZER(512);
    pas_heap_ref heapRefThree = ISO_HEAP_REF_INITIALIZER(1000000);
    pas_heap_ref heapRefFour = ISO_HEAP_REF_INITIALIZER(10000000);
    
    pas_heap* heapOne = iso_heap_ref_get_heap(&heapRefOne);
    pas_heap* heapTwo = iso_heap_ref_get_heap(&heapRefTwo);
    pas_heap* heapThree = iso_heap_ref_get_heap(&heapRefThree);
    pas_heap* heapFour = iso_heap_ref_get_heap(&heapRefFour);

    vector<void*> objects;
    
    auto checkObject = [&] (void* object) -> void* {
        CHECK(object);
        return object;
    };
    
    auto addObject = [&] (void* object) {
        objects.push_back(checkObject(object));
    };
    
    addObject(iso_try_allocate(&heapRefFour));
    for (size_t size = 0; size < 10000000; size += 1000000)
        addObject(iso_try_allocate(&heapRefThree));
    for (size_t size = 0; size < 10000000; size += 64)
        addObject(iso_try_allocate(&heapRefOne));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    pas_heap_summary summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 10000000);
    CHECK_LESS(summaryOne.free, PAS_SMALL_PAGE_DEFAULT_SIZE * 2); // * 2 because baseline.
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    pas_heap_summary summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_EQUAL(summaryTwo.free, 0);
    CHECK_EQUAL(summaryTwo.committed, 0);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    pas_heap_summary summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    pas_heap_summary summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 10000000);
    CHECK_EQUAL(summaryFour.free, 0);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (void* object : objects)
        iso_deallocate(object);
    objects.clear();
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);
    
    iso_deallocate(checkObject(iso_try_allocate(&heapRefFour)));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 0);
    CHECK_EQUAL(summaryTwo.free, 0);
    CHECK_EQUAL(summaryTwo.committed, 0);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_GREATER_EQUAL(summaryThree.free, 10000000);
    CHECK_GREATER_EQUAL(summaryThree.committed, 10000000);
    CHECK_EQUAL(summaryThree.decommitted, 0);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    if (verbose)
        cout << "Allocating small object.\n";
    
    checkObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Did allocate small object.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER_EQUAL(summaryOne.committed, 10000000);
    CHECK_EQUAL(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 512);
    CHECK_LESS_EQUAL(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE);
    CHECK_GREATER_EQUAL(summaryTwo.committed, PAS_SMALL_PAGE_DEFAULT_SIZE);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_LESS_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, 9000000 + pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 1000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
    
    for (size_t size = 512; size < 10000000; size += 512)
        addObject(iso_try_allocate(&heapRefTwo));
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    if (verbose)
        cout << "Did allocate the rest of the small objects.\n";

    summaryOne = pas_heap_compute_summary(heapOne, pas_lock_is_not_held);
    CHECK_EQUAL(summaryOne.allocated, 0);
    CHECK_GREATER_EQUAL(summaryOne.free, 10000000);
    CHECK_GREATER(summaryOne.decommitted, 0);
    
    summaryTwo = pas_heap_compute_summary(heapTwo, pas_lock_is_not_held);
    CHECK_EQUAL(summaryTwo.allocated, 10000384);
    CHECK_LESS(summaryTwo.free, PAS_MEDIUM_PAGE_DEFAULT_SIZE * 2);
    CHECK_GREATER_EQUAL(summaryTwo.committed, 10000384);
    CHECK_EQUAL(summaryTwo.decommitted, 0);
    
    summaryThree = pas_heap_compute_summary(heapThree, pas_lock_is_not_held);
    CHECK_EQUAL(summaryThree.allocated, 0);
    CHECK_LESS_EQUAL(summaryThree.free, 10000000);
    CHECK_LESS(summaryThree.committed, pas_page_malloc_alignment() * 2);
    CHECK_GREATER(summaryThree.decommitted, 10000000 - pas_page_malloc_alignment() * 2);
    
    summaryFour = pas_heap_compute_summary(heapFour, pas_lock_is_not_held);
    CHECK_EQUAL(summaryFour.allocated, 0);
    CHECK_EQUAL(summaryFour.free, 10000000);
    CHECK_GREATER_EQUAL(summaryFour.committed, 10000000);
    CHECK_EQUAL(summaryFour.decommitted, 0);
}

enum ThingyKind {
    SmallHeapOne,
    SmallHeapTwo,
    SmallHeapThree,
    LargeHeapOne,
    LargeHeapTwo,
    PrimitiveSmallOne,
    PrimitiveSmallTwo,
    PrimitiveSmallThree,
    PrimitiveLarge
};

const char* thingyName(ThingyKind kind)
{
    switch (kind) {
    case SmallHeapOne:
        return "SmallHeapOne";
    case SmallHeapTwo:
        return "SmallHeapTwo";
    case SmallHeapThree:
        return "SmallHeapThree";
    case LargeHeapOne:
        return "LargeHeapOne";
    case LargeHeapTwo:
        return "LargeHeapTwo";
    case PrimitiveSmallOne:
        return "PrimitiveSmallOne";
    case PrimitiveSmallTwo:
        return "PrimitiveSmallTwo";
    case PrimitiveSmallThree:
        return "PrimitiveSmallThree";
    case PrimitiveLarge:
        return "PrimitiveLarge";
    }
    PAS_ASSERT(!"Should not be reached");
    return nullptr;
}

template<typename Func>
void forEachThingyKind(const Func& func)
{
    func(SmallHeapOne);
    func(SmallHeapTwo);
    func(SmallHeapThree);
    func(LargeHeapOne);
    func(LargeHeapTwo);
    func(PrimitiveSmallOne);
    func(PrimitiveSmallTwo);
    func(PrimitiveSmallThree);
    func(PrimitiveLarge);
}

pas_heap_ref smallHeapRefOne = ISO_HEAP_REF_INITIALIZER(32);
pas_heap_ref smallHeapRefTwo = ISO_HEAP_REF_INITIALIZER(128);
pas_heap_ref smallHeapRefThree = ISO_HEAP_REF_INITIALIZER(151);
pas_heap_ref largeHeapRefOne = ISO_HEAP_REF_INITIALIZER(1000000);
pas_heap_ref largeHeapRefTwo = ISO_HEAP_REF_INITIALIZER(254384);
vector<void*> objectsAllocated;

pas_heap* smallHeapOne;
pas_heap* smallHeapTwo;
pas_heap* smallHeapThree;
pas_heap* largeHeapOne;
pas_heap* largeHeapTwo;
pas_segregated_size_directory* primitiveSmallOneDirectory;
pas_segregated_size_directory* primitiveSmallTwoDirectory;
pas_segregated_size_directory* primitiveSmallThreeDirectory;

void setupThingy()
{
    pas_scavenger_suspend();
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    smallHeapOne = iso_heap_ref_get_heap(&smallHeapRefOne);
    smallHeapTwo = iso_heap_ref_get_heap(&smallHeapRefTwo);
    smallHeapThree = iso_heap_ref_get_heap(&smallHeapRefThree);
    largeHeapOne = iso_heap_ref_get_heap(&largeHeapRefOne);
    largeHeapTwo = iso_heap_ref_get_heap(&largeHeapRefTwo);
}

void cleanThingy()
{
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);
}

void* checkThingy(void* object)
{
    CHECK(object);
    return object;
}

void* addObject(void* object)
{
    static constexpr bool verbose = false;

    if (verbose) {
        cout << "Adding allocated object: " << object << "\n";
        cout.flush();
    }
    
    objectsAllocated.push_back(checkThingy(object));
    return object;
}

// This deletes objects in exactly the same order that they were allocated in.
void deleteThingy()
{
    for (void* object : objectsAllocated) {
        CHECK(object);
        iso_deallocate(object);
        pas_thread_local_cache_flush_deallocation_log(pas_thread_local_cache_get(&iso_heap_config),
                                                      pas_lock_is_not_held);
    }
    objectsAllocated.clear();
}

enum AllocationKind {
    AllocateOne,
    AllocateMany
};

const char* allocationKindName(AllocationKind allocationKind)
{
    switch (allocationKind) {
    case AllocateOne:
        return "AllocateOne";
    case AllocateMany:
        return "AllocateMany";
    }
    PAS_ASSERT(!"Should not be reached");
    return nullptr;
}

void allocateThingiesImpl(ThingyKind kind, AllocationKind allocateMany)
{
    static constexpr bool verbose = false;

    if (verbose) {
        cout << "Allocating " << thingyName(kind) << ", " << allocationKindName(allocateMany) << "\n";
        cout.flush();
    }
    
    switch (kind) {
    case SmallHeapOne:
        for (unsigned i = allocateMany ? 156250 : 1; i--;)
            addObject(iso_try_allocate(&smallHeapRefOne));
        return;
    case SmallHeapTwo:
        for (unsigned i = allocateMany ? 39062 : 1; i--;)
            addObject(iso_try_allocate(&smallHeapRefTwo));
        return;
    case SmallHeapThree:
        for (unsigned i = allocateMany ? 33112 : 1; i--;)
            addObject(iso_try_allocate(&smallHeapRefThree));
        return;
    case LargeHeapOne:
        for (unsigned i = allocateMany ? 5 : 1; i--;)
            addObject(iso_try_allocate(&largeHeapRefOne));
        return;
    case LargeHeapTwo:
        for (unsigned i = allocateMany ? 19 : 1; i--;)
            addObject(iso_try_allocate(&largeHeapRefTwo));
        return;
    case PrimitiveSmallOne:
        for (unsigned i = allocateMany ? 156250 : 1; i--;) {
            void* object = addObject(iso_try_allocate_common_primitive(32));
            
            pas_segregated_size_directory* directory;
            
            directory = pas_segregated_size_directory_for_object(
                reinterpret_cast<uintptr_t>(object),
                &iso_heap_config);
            
            if (primitiveSmallOneDirectory)
                CHECK_EQUAL(directory, primitiveSmallOneDirectory);
            else
                primitiveSmallOneDirectory = directory;
        }
        return;
    case PrimitiveSmallTwo:
        for (unsigned i = allocateMany ? 39062 : 1; i--;) {
            void* object = addObject(iso_try_allocate_common_primitive(128));
            
            pas_segregated_size_directory* directory;
            
            directory = pas_segregated_size_directory_for_object(
                reinterpret_cast<uintptr_t>(object),
                &iso_heap_config);
            
            if (primitiveSmallTwoDirectory)
                CHECK_EQUAL(directory, primitiveSmallTwoDirectory);
            else
                primitiveSmallTwoDirectory = directory;
        }
        return;
    case PrimitiveSmallThree:
        for (unsigned i = allocateMany ? 104166 : 1; i--;) {
            void* object = addObject(iso_try_allocate_common_primitive(48));
            
            pas_segregated_size_directory* directory;
            
            directory = pas_segregated_size_directory_for_object(
                reinterpret_cast<uintptr_t>(object),
                &iso_heap_config);
            
            if (primitiveSmallThreeDirectory)
                CHECK_EQUAL(directory, primitiveSmallThreeDirectory);
            else
                primitiveSmallThreeDirectory = directory;
        }
        return;
    case PrimitiveLarge:
        for (unsigned i = allocateMany ? 19 : 1; i--;)
            addObject(iso_try_allocate_common_primitive(254384));
        return;
    }
    PAS_ASSERT(!"Should not be reached");
}

void allocateThingies(ThingyKind kind, AllocationKind allocateMany)
{
    allocateThingiesImpl(kind, allocateMany);
    pas_thread_local_cache_stop_local_allocators(pas_thread_local_cache_get(&iso_heap_config),
                                                 pas_lock_is_not_held);
}

pas_heap_summary heapSummaryFor(ThingyKind kind)
{
    HeapLocker heapLocker;
    switch (kind) {
    case SmallHeapOne:
        return pas_heap_compute_summary(smallHeapOne, pas_lock_is_held);
    case SmallHeapTwo:
        return pas_heap_compute_summary(smallHeapTwo, pas_lock_is_held);
    case SmallHeapThree:
        return pas_heap_compute_summary(smallHeapThree, pas_lock_is_held);
    case LargeHeapOne:
        return pas_heap_compute_summary(largeHeapOne, pas_lock_is_held);
    case LargeHeapTwo:
        return pas_heap_compute_summary(largeHeapTwo, pas_lock_is_held);
    case PrimitiveSmallOne:
        if (!primitiveSmallOneDirectory)
            return pas_heap_summary_create_empty();
        return pas_segregated_directory_compute_summary(&primitiveSmallOneDirectory->base);
    case PrimitiveSmallTwo:
        if (!primitiveSmallTwoDirectory)
            return pas_heap_summary_create_empty();
        return pas_segregated_directory_compute_summary(&primitiveSmallTwoDirectory->base);
    case PrimitiveSmallThree:
        if (!primitiveSmallThreeDirectory)
            return pas_heap_summary_create_empty();
        return pas_segregated_directory_compute_summary(&primitiveSmallThreeDirectory->base);
    case PrimitiveLarge:
        return pas_large_heap_compute_summary(&iso_common_primitive_heap.large_heap);
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_heap_summary_create_empty();
}

void assertOnlyDecommitted(const set<ThingyKind>& thingies)
{
    forEachThingyKind(
        [&] (ThingyKind kind) {
            pas_heap_summary summary = heapSummaryFor(kind);
            if (thingies.count(kind)) {
                if (!summary.decommitted) {
                    cout << "Expected " << thingyName(kind) << " to be decommitted but it wasn't.\n";
                    CHECK(!"Test failed");
                }
            } else {
                if (summary.decommitted) {
                    cout << "Expected " << thingyName(kind) << " to NOT be decommitted but it "
                         << "decommitted " << summary.decommitted << " bytes.\n";
                    CHECK(!"Test failed");
                }
            }
        });
}

void testFullVdirToVdirObvious()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallTwo });
}

void testFullVdirToVdirObviousBackwards()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallTwo, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToVdirOpportunistic()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullVdirToVdirOpportunisticBackwards()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullVdirToVdirNewAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallThree, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallThree });
}

void testFullVdirToVdirNewLateAllocation()
{
    pas_heap_for_config_force_bootstrap = true; // Otherwise we take things from utility and that throws this test off.
    
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallThree, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallThree });
}

void testFullVdirToVdirNewDirAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallThree, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallThree });
}

void testFullVdirToVdirNewLateDirAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallThree, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallThree });
}

void testFullVdirToVdirNewLargeAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallThree, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallThree });
}

void testFullVdirToVdirNewLateLargeAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallThree, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();

    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallThree });
}

void testFullVdirToDir()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToDirBackwardsTarget()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToDirBackwardsSource()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallTwo });
}

void testFullVdirToDirNewAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToDirNewLateAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToDirNewDirAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToDirNewLateDirAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToDirNewLargeAllocation()
{
    static constexpr bool verbose = false;
    
    setupThingy();

    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    if (verbose) {
        cout << "About to allocate small.\n";
        
        dumpLargeSharingPool();
    }
    
    allocateThingies(SmallHeapOne, AllocateOne);

    if (verbose)
        cout << "Did allocate small.\n";

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullNotVdirButLargeToDirNewLargeAllocation()
{
    static constexpr bool verbose = false;
    
    setupThingy();

    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    if (verbose) {
        cout << "About to allocate small.\n";
        
        dumpLargeSharingPool();
    }
    
    allocateThingies(SmallHeapOne, AllocateOne);

    if (verbose)
        cout << "Did allocate small.\n";

    assertOnlyDecommitted({ LargeHeapTwo });
}

void testFullVdirToDirNewLateLargeAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToDirNewAllocationAlsoPhysical()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToDirNewLateAllocationAlsoPhysical()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLarge()
{
    static constexpr bool verbose = false;
    
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    if (verbose)
        cout << "Allocating the large thing!\n";
    
    allocateThingies(LargeHeapOne, AllocateOne);

    if (verbose)
        cout << "Did allocate the large thing!\n";
    
    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLargeBackwardsTarget()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapTwo, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLargeBackwardsSource()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallTwo });
}

void testFullVdirToLargeNewAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLargeNewLateAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLargeNewDirAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLargeNewLateDirAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLargeNewLargeAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullNotVdirButLargeToLargeNewLargeAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapTwo });
}

void testFullVdirToLargeNewLateLargeAllocation()
{
    static constexpr bool verbose = false;
    
    // Maybe we will need to do this for other tests in this file.
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;
    
    setupThingy();

    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);

    if (verbose)
        dumpLargeSharingPool();
    assertOnlyDecommitted({ });
    
    deleteThingy();
    cleanThingy();

    if (verbose)
        dumpLargeSharingPool();
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapTwo, AllocateMany);

    if (verbose)
        dumpLargeSharingPool();
    assertOnlyDecommitted({ });

    deleteThingy();
    cleanThingy();
    
    if (verbose)
        dumpLargeSharingPool();
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLargeNewAllocationAlsoPhysical()
{
    setupThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullVdirToLargeNewLateAllocationAlsoPhysical()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveSmallOne });
}

void testFullDirToVdir()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullDirToVdirBackwards()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ SmallHeapTwo });
}

void testFullDirToVdirNewAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ SmallHeapTwo });
}

void testFullDirToVdirNewLateAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ SmallHeapTwo });
}

void testFullDirToDir()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullDirToDirBackwards()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ SmallHeapTwo });
}

void testFullDirToDirWithThree()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullDirToDirWithThreeBackwards()
{
    setupThingy();

    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapThree });
}

void testFullDirToDirWithThreeNewAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapThree });
}

void testFullDirToDirWithThreeNewLateAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapThree });
}

void testFullDirToDirWithThreeNewVdirAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapThree });
}

void testFullDirToDirWithThreeNewLateVdirAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapThree });
}

void testFullDirToLarge()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullDirToLargeNewAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullDirToLargeNewLateAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullDirToLargeNewVdirAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullDirToLargeNewLateVdirAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapTwo, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullLargeToVdirForwardMinEpoch()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullNotLargeButDirToVdirCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ SmallHeapOne });
}

void testFullLargeToVdirCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToVdirBackwards()
{
    setupThingy();

    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapTwo });
}

void testFullLargeToVdirNewAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToVdirNewLateAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapTwo });
}

void testFullLargeToVdirNewDirAllocationForwardMinEpoch()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToVdirNewDirAllocationCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToVdirNewLateDirAllocationForwardMinEpoch()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToVdirNewLateDirAllocationCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveSmallOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirForwardMinEpoch()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirBackwardsSource()
{
    setupThingy();

    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapTwo });
}

void testFullLargeToDirBackwardsTarget()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirBackwardsSourceAndTarget()
{
    setupThingy();

    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapOne, AllocateOne);

    assertOnlyDecommitted({ LargeHeapTwo });
}

void testFullLargeToDirNewAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirNewLateAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapTwo });
}

void testFullLargeToDirNewVdirAllocationForwardMinEpoch()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirNewVdirAllocationCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirNewLateVdirAllocationForwardMinEpoch()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirNewLateVdirAllocationCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirNewDirAllocationForwardMinEpoch()
{
    setupThingy();

    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(SmallHeapThree, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirNewDirAllocationCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapThree, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirNewLateDirAllocationForwardMinEpoch()
{
    setupThingy();

    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(PrimitiveLarge, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapThree, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToDirNewLateDirAllocationCombinedUseEpoch()
{
    setupThingy();

    allocateThingies(SmallHeapThree, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapThree, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(SmallHeapTwo, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToLargeForwardMinEpoch()
{
    static constexpr bool verbose = false;
    
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    if (verbose)
        cout << "About to allocate primitive large.\n";
    
    allocateThingies(PrimitiveLarge, AllocateOne);

    if (verbose)
        cout << "Did allocate primitive large.\n";

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToLargeCombinedUseEpoch()
{
    static constexpr bool verbose = false;
    
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    if (verbose)
        cout << "About to allocate primitive large.\n";
    
    allocateThingies(PrimitiveLarge, AllocateOne);

    if (verbose)
        cout << "Did allocate primitive large.\n";

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToLargeReverse()
{
    setupThingy();

    allocateThingies(PrimitiveLarge, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(LargeHeapOne, AllocateOne);

    assertOnlyDecommitted({ PrimitiveLarge });
}

void testFullLargeToLargeNewAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveLarge, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToLargeNewLateAllocation()
{
    setupThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(LargeHeapOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveLarge, AllocateOne);

    assertOnlyDecommitted({ LargeHeapTwo });
}

void testFullLargeToLargeNewVdirAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveLarge, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToLargeNewLateVdirAllocation()
{
    setupThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(PrimitiveSmallOne, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveLarge, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToLargeNewDirAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveLarge, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

void testFullLargeToLargeNewLateDirAllocation()
{
    setupThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    allocateThingies(LargeHeapOne, AllocateMany);
    allocateThingies(LargeHeapTwo, AllocateMany);
    allocateThingies(PrimitiveSmallTwo, AllocateMany);
    allocateThingies(PrimitiveSmallOne, AllocateMany);
    allocateThingies(SmallHeapOne, AllocateMany);
    
    deleteThingy();
    cleanThingy();

    allocateThingies(SmallHeapTwo, AllocateMany);
    deleteThingy();
    cleanThingy();
    
    assertOnlyDecommitted({ });
    
    allocateThingies(PrimitiveLarge, AllocateOne);

    assertOnlyDecommitted({ LargeHeapOne });
}

// FIXME: This test is badly named. The name reflects behavior it used to have before we switched to
// the use_epoch being time of deletion.
void testNewEligibleHasOlderEpoch()
{
    static constexpr bool verbose = false;
    
    vector<void*> stashedObjects;
    
    setupThingy();
    
    swap(stashedObjects, objectsAllocated);
    allocateThingies(SmallHeapOne, AllocateMany);
    swap(stashedObjects, objectsAllocated);
    
    allocateThingies(SmallHeapTwo, AllocateMany);
    
    if (verbose)
        cout << "Deallocating from SmallHeapTwo.\n";
    
    deleteThingy();
    cleanThingy();
    
    if (verbose)
        cout << "Allocating in SmallHeapThree.\n";
    
    allocateThingies(SmallHeapThree, AllocateOne); // This should decommit from SmallHeapTwo

    assertOnlyDecommitted({ SmallHeapTwo });
    
    if (verbose)
        cout << "Deallocating from SmallHeapOne.\n";
    
    swap(stashedObjects, objectsAllocated);
    deleteThingy();
    cleanThingy();
    
    if (verbose)
        cout << "Allocating in PrimitiveSmallOne.\n";
    
    allocateThingies(PrimitiveSmallOne, AllocateOne); // This should keep decommitting from SmallHeapTwo
    
    assertOnlyDecommitted({ SmallHeapTwo });
}

pas_heap* scavengerCompletionHeap;
bool scavengerCompletionExpectDone;
bool scavengerCompletionIsDone;
mutex scavengerCompletionMutex;
condition_variable scavengerCompletionCond;

void scavengerCompletionCallback()
{
    bool hasCommittedPages;
    
    pas_heap_lock_lock();
    hasCommittedPages = !!
        pas_segregated_heap_num_committed_views(&scavengerCompletionHeap->segregated_heap);
    pas_heap_lock_unlock();
    
    lock_guard<mutex> locker(scavengerCompletionMutex);
    if (!scavengerCompletionExpectDone)
        return;
    if (!hasCommittedPages) {
        scavengerCompletionIsDone = true;
        scavengerCompletionCond.notify_all();
        return;
    }
}

template<typename Func>
void testScavengerEventuallyReturnsMemory(unsigned objectSize,
                                          size_t count,
                                          const Func& scavengerSetupCallback)
{
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER(objectSize);
    
    vector<void*> objectList;

    for (size_t index = count; index--;) {
        void* object = iso_try_allocate(&heapRef);
        CHECK(object);
        objectList.push_back(object);
    }
    
    scavengerCompletionHeap = iso_heap_ref_get_heap(&heapRef);
    pas_scavenger_completion_callback = scavengerCompletionCallback;

    scavengerSetupCallback();
    
    for (void* object : objectList)
        iso_deallocate(object);
    
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);

    {
        unique_lock<mutex> locker(scavengerCompletionMutex);
        scavengerCompletionExpectDone = true;
        scavengerCompletionCond.wait(
            locker,
            [&] () -> bool {
                return scavengerCompletionIsDone;
            });
    }
    
    CHECK_EQUAL(pas_segregated_heap_num_committed_views(
                    &scavengerCompletionHeap->segregated_heap),
                0);
}

void testScavengerEventuallyReturnsMemory(unsigned objectSize,
                                          size_t count)
{
    testScavengerEventuallyReturnsMemory(objectSize, count, [] { });
}

void testScavengerEventuallyReturnsMemoryEvenWithoutManualShrink(unsigned objectSize,
                                                                 size_t count)
{
    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER(objectSize);
    
    vector<void*> objectList;

    for (size_t index = count; index--;) {
        void* object = iso_try_allocate(&heapRef);
        CHECK(object);
        objectList.push_back(object);
    }
    
    scavengerCompletionHeap = iso_heap_ref_get_heap(&heapRef);
    pas_scavenger_completion_callback = scavengerCompletionCallback;
    
    for (void* object : objectList)
        iso_deallocate(object);
    
    {
        unique_lock<mutex> locker(scavengerCompletionMutex);
        scavengerCompletionExpectDone = true;
        scavengerCompletionCond.wait(
            locker,
            [&] () -> bool {
                return scavengerCompletionIsDone;
            });
    }
    
    CHECK_EQUAL(pas_segregated_heap_num_committed_views(
                    &scavengerCompletionHeap->segregated_heap),
                0);
}

bool scavengerShutDownIsDone;
mutex scavengerShutDownMutex;
condition_variable scavengerShutDownCond;

void scavengerShutDownCallback()
{
    lock_guard<mutex> locker(scavengerShutDownMutex);
    scavengerShutDownIsDone = true;
    scavengerShutDownCond.notify_all();
}

void testScavengerShutsDownEventually(unsigned objectSize,
                                      size_t count,
                                      double deepSleepTimeoutInMilliseconds,
                                      double periodInMilliseconds)
{
    pas_scavenger_deep_sleep_timeout_in_milliseconds = deepSleepTimeoutInMilliseconds;
    pas_scavenger_period_in_milliseconds = periodInMilliseconds;
    
    testScavengerEventuallyReturnsMemory(
        objectSize, count,
        [] () {
            pas_scavenger_will_shut_down_callback = scavengerShutDownCallback;
        });
    
    unique_lock<mutex> locker(scavengerShutDownMutex);
    scavengerShutDownCond.wait(
        locker,
        [&] () -> bool {
            return scavengerShutDownIsDone;
        });
}

void addAllTests()
{
    {
        EpochIsCounter epochIsCounter;
        ADD_TEST(testTakePages(32, 32, 10000, 64, 64, 10000, 10000, 20, 20, 40, 60, 60, 60, false));
        ADD_TEST(testTakePages(32, 32, 10000, 64, 64, 5000, 10000, 20, 20, 20, 40, 20, 40, true));

        ADD_TEST(testTakePagesFromCorrectHeap(100, [] (unsigned) { return 48; }, 50));
        ADD_TEST(testTakePagesFromCorrectHeap(100, [] (unsigned) { return 48; }, 25));
        ADD_TEST(testTakePagesFromCorrectHeap(100, [] (unsigned) { return 48; }, 75));
        ADD_TEST(testTakePagesFromCorrectHeap(100, [] (unsigned index) { return index < 25 ? 1000000 : 48; }, 99));

        SKIP_TEST(testLargeHeapTakesPagesFromCorrectSmallHeap());
        SKIP_TEST(testLargeHeapTakesPagesFromCorrectSmallHeapAllocateAfterFree());
        SKIP_TEST(testLargeHeapTakesPagesFromCorrectSmallHeapWithFancyOrder());
        if (hasScope("forward_min_epoch")) {
            SKIP_TEST(testLargeHeapTakesPagesFromCorrectLargeHeap());
            ADD_TEST(testLargeHeapTakesPagesFromCorrectLargeHeapAllocateAfterFreeOnSmallHeap());
            ADD_TEST(testLargeHeapTakesPagesFromCorrectLargeHeapAllocateAfterFreeOnAnotherLargeHeap());

            // Skip this test because some large allocation leaves behind memory in the large sharing
            // cache, then gets decommitted, and then that decommitted allocation gets reused for a later
            // allocation. This then creates a situation where allocating a 1MB object causes allocation
            // of 1MB of physical memory (a 1MB take) and also a commit of some of that previously
            // decommitted memory (a handful of KB take). That causes us to take more memory than the
            // test thinks we should take. I guess we could make the test have the right numerical limits
            // but since this test has never caught a real issue, it's probably better to skip.
            SKIP_TEST(testLargeHeapTakesPagesFromCorrectLargeHeapWithFancyOrder());
        }
        ADD_TEST(testSmallHeapTakesPagesFromCorrectLargeHeap());
        ADD_TEST(testSmallHeapTakesPagesFromCorrectLargeHeapWithFancyOrder());
        ADD_TEST(testSmallHeapTakesPagesFromCorrectLargeHeapAllocateAfterFreeOnSmallHeap());
        if (hasScope("forward_min_epoch"))
            ADD_TEST(testSmallHeapTakesPagesFromCorrectLargeHeapAllocateAfterFreeOnAnotherLargeHeap());

        ADD_TEST(testFullVdirToVdirObvious());
        ADD_TEST(testFullVdirToVdirObviousBackwards());
        ADD_TEST(testFullVdirToVdirOpportunistic());
        ADD_TEST(testFullVdirToVdirOpportunisticBackwards());
        ADD_TEST(testFullVdirToVdirNewAllocation());
        ADD_TEST(testFullVdirToVdirNewLateAllocation());
        ADD_TEST(testFullVdirToVdirNewDirAllocation());
        ADD_TEST(testFullVdirToVdirNewLateDirAllocation());
        if (hasScope("combined_use_epoch"))
            ADD_TEST(testFullVdirToVdirNewLargeAllocation());
        ADD_TEST(testFullVdirToVdirNewLateLargeAllocation());
        ADD_TEST(testFullVdirToDir());
        ADD_TEST(testFullVdirToDirBackwardsTarget());
        ADD_TEST(testFullVdirToDirBackwardsSource());
        ADD_TEST(testFullVdirToDirNewAllocation());
        ADD_TEST(testFullVdirToDirNewLateAllocation());
        ADD_TEST(testFullVdirToDirNewDirAllocation());
        ADD_TEST(testFullVdirToDirNewLateDirAllocation());
        if (hasScope("combined_use_epoch"))
            ADD_TEST(testFullVdirToDirNewLargeAllocation());
        else
            ADD_TEST(testFullNotVdirButLargeToDirNewLargeAllocation());
        ADD_TEST(testFullVdirToDirNewLateLargeAllocation());
        ADD_TEST(testFullVdirToDirNewAllocationAlsoPhysical());
        ADD_TEST(testFullVdirToDirNewLateAllocationAlsoPhysical());
        ADD_TEST(testFullVdirToLarge());
        ADD_TEST(testFullVdirToLargeBackwardsTarget());
        ADD_TEST(testFullVdirToLargeBackwardsSource());
        ADD_TEST(testFullVdirToLargeNewAllocation());
        ADD_TEST(testFullVdirToLargeNewLateAllocation());
        ADD_TEST(testFullVdirToLargeNewDirAllocation());
        ADD_TEST(testFullVdirToLargeNewLateDirAllocation());
        if (hasScope("combined_use_epoch"))
            ADD_TEST(testFullVdirToLargeNewLargeAllocation());
        else
            ADD_TEST(testFullNotVdirButLargeToLargeNewLargeAllocation());
        ADD_TEST(testFullVdirToLargeNewLateLargeAllocation());
        ADD_TEST(testFullVdirToLargeNewAllocationAlsoPhysical());
        ADD_TEST(testFullVdirToLargeNewLateAllocationAlsoPhysical());

        ADD_TEST(testFullDirToVdir());
        ADD_TEST(testFullDirToVdirBackwards());
        ADD_TEST(testFullDirToVdirNewAllocation());
        ADD_TEST(testFullDirToVdirNewLateAllocation());
        ADD_TEST(testFullDirToDir());
        ADD_TEST(testFullDirToDirBackwards());
        ADD_TEST(testFullDirToDirWithThree());
        ADD_TEST(testFullDirToDirWithThreeBackwards());
        ADD_TEST(testFullDirToDirWithThreeNewAllocation());
        ADD_TEST(testFullDirToDirWithThreeNewLateAllocation());
        ADD_TEST(testFullDirToDirWithThreeNewVdirAllocation());
        ADD_TEST(testFullDirToDirWithThreeNewLateVdirAllocation());
        ADD_TEST(testFullDirToLarge());
        ADD_TEST(testFullDirToLargeNewAllocation());
        ADD_TEST(testFullDirToLargeNewLateAllocation());
        ADD_TEST(testFullDirToLargeNewVdirAllocation());
        ADD_TEST(testFullDirToLargeNewLateVdirAllocation());

        if (hasScope("combined_use_epoch")) {
            ADD_TEST(testFullLargeToVdirCombinedUseEpoch());
            ADD_TEST(testFullNotLargeButDirToVdirCombinedUseEpoch());
            ADD_TEST(testFullLargeToVdirNewDirAllocationCombinedUseEpoch());
            ADD_TEST(testFullLargeToVdirNewLateDirAllocationCombinedUseEpoch());
            ADD_TEST(testFullLargeToDirCombinedUseEpoch());
            ADD_TEST(testFullLargeToDirNewDirAllocationCombinedUseEpoch());
            ADD_TEST(testFullLargeToDirNewLateDirAllocationCombinedUseEpoch());
            ADD_TEST(testFullLargeToDirNewVdirAllocationCombinedUseEpoch());
            ADD_TEST(testFullLargeToDirNewLateVdirAllocationCombinedUseEpoch());
            ADD_TEST(testFullLargeToLargeCombinedUseEpoch());
        } else {
            ADD_TEST(testFullLargeToVdirForwardMinEpoch());
            ADD_TEST(testFullLargeToVdirBackwards());
            ADD_TEST(testFullLargeToVdirNewAllocation());
            ADD_TEST(testFullLargeToVdirNewLateAllocation());
            ADD_TEST(testFullLargeToVdirNewDirAllocationForwardMinEpoch());
            ADD_TEST(testFullLargeToVdirNewLateDirAllocationForwardMinEpoch());
            ADD_TEST(testFullLargeToDirForwardMinEpoch());
            ADD_TEST(testFullLargeToDirBackwardsSource());
            ADD_TEST(testFullLargeToDirBackwardsTarget());
            ADD_TEST(testFullLargeToDirBackwardsSourceAndTarget());
            ADD_TEST(testFullLargeToDirNewAllocation());
            ADD_TEST(testFullLargeToDirNewLateAllocation());
            ADD_TEST(testFullLargeToDirNewDirAllocationForwardMinEpoch());
            ADD_TEST(testFullLargeToDirNewLateDirAllocationForwardMinEpoch());
            ADD_TEST(testFullLargeToDirNewVdirAllocationForwardMinEpoch());
            ADD_TEST(testFullLargeToDirNewLateVdirAllocationForwardMinEpoch());
            ADD_TEST(testFullLargeToLargeForwardMinEpoch());
            ADD_TEST(testFullLargeToLargeReverse());
            ADD_TEST(testFullLargeToLargeNewAllocation());
            ADD_TEST(testFullLargeToLargeNewLateAllocation());
            ADD_TEST(testFullLargeToLargeNewVdirAllocation());
            ADD_TEST(testFullLargeToLargeNewLateVdirAllocation());
            ADD_TEST(testFullLargeToLargeNewDirAllocation());
            ADD_TEST(testFullLargeToLargeNewLateDirAllocation());
        }
    
        ADD_TEST(testNewEligibleHasOlderEpoch());
    
        ADD_TEST(testScavengerEventuallyReturnsMemory(128, 1));
        ADD_TEST(testScavengerEventuallyReturnsMemory(128, 10000));
        ADD_TEST(testScavengerEventuallyReturnsMemory(8, 10000));
        ADD_TEST(testScavengerEventuallyReturnsMemoryEvenWithoutManualShrink(128, 1));
        ADD_TEST(testScavengerEventuallyReturnsMemoryEvenWithoutManualShrink(128, 10000));
        ADD_TEST(testScavengerEventuallyReturnsMemoryEvenWithoutManualShrink(8, 10000));
        ADD_TEST(testScavengerShutsDownEventually(64, 10000, 1, 1));
    }
    
    ADD_TEST(testScavengerEventuallyReturnsMemory(128, 1));
    ADD_TEST(testScavengerEventuallyReturnsMemory(128, 10000));
    ADD_TEST(testScavengerEventuallyReturnsMemory(8, 10000));
    ADD_TEST(testScavengerEventuallyReturnsMemoryEvenWithoutManualShrink(128, 1));
    ADD_TEST(testScavengerEventuallyReturnsMemoryEvenWithoutManualShrink(128, 10000));
    ADD_TEST(testScavengerEventuallyReturnsMemoryEvenWithoutManualShrink(8, 10000));
    ADD_TEST(testScavengerShutsDownEventually(64, 10000, 1, 1));
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO && TLC

void addIsoHeapPageSharingTests()
{
#if PAS_ENABLE_ISO && TLC
    // FIXME: The fact that we have to enable this feature for tests due to the fact that it's enabled
    // by default is super weird. Maybe we should just kill the feature and adapt everything to that.
    TestScope enableBalancing(
        "enable-balancing",
        [] () {
            pas_physical_page_sharing_pool_balancing_enabled = true;
            pas_physical_page_sharing_pool_balancing_enabled_for_utility = true;
        });
    
    InstallVerifier installVerifier;
    ForceExclusives forceExclusives;
    ForceTLAs forceTLAs;
    DisableBitfit disableBitfit;
    
    {
        TestScope testScope(
            "forward_min_epoch",
            [] () {
                pas_large_sharing_pool_epoch_update_mode_setting =
                    pas_large_sharing_pool_forward_min_epoch;
            });
        addAllTests();
    }

    {
        TestScope testScope(
            "combined_use_epoch",
            [] () {
                pas_large_sharing_pool_epoch_update_mode_setting =
                    pas_large_sharing_pool_combined_use_epoch;
            });
        addAllTests();
    }
#endif // PAS_ENABLE_ISO && TLC
}
