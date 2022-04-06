/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"
#include <functional>
#include "pas_all_heaps.h"
#include "pas_baseline_allocator_table.h"
#include "pas_bitvector.h"
#include "pas_committed_pages_vector.h"
#include "pas_get_heap.h"
#include "pas_get_page_base.h"
#include "pas_heap_lock.h"
#include "pas_scavenger.h"
#include "pas_thread_local_cache_layout.h"
#include <thread>

using namespace std;

namespace {

size_t numCommittedPagesInTLC()
{
    pas_thread_local_cache* cache = pas_thread_local_cache_try_get();
    CHECK(cache);
    return pas_count_committed_pages(
        cache,
        pas_thread_local_cache_size_for_allocator_index_capacity(
            cache->allocator_index_capacity),
        &allocationConfig);
}

void testTLCDecommit(unsigned numHeaps,
                     function<bool(unsigned)> shouldStopAllocatorAtIndex,
                     bool commitWithFree,
                     unsigned numCommittedPagesInitially,
                     unsigned numCommittedPagesAfterDecommit,
                     unsigned numCommittedPagesAfterRecommit)
{
    pas_scavenger_suspend();
    pas_local_allocator_should_stop_count_for_suspend = 100; /* request_stop should not actually stop. */

    pas_heap_ref* heaps = new pas_heap_ref[numHeaps];
    for (size_t index = numHeaps; index--;)
        heaps[index] = BMALLOC_HEAP_REF_INITIALIZER(new bmalloc_type(BMALLOC_TYPE_INITIALIZER(42, 2, "test")));

    vector<void*> objects;
    for (size_t index = 0; index < numHeaps; ++index) {
        void* ptr = bmalloc_iso_allocate(heaps + index);
        CHECK(ptr);
        CHECK_EQUAL(pas_get_heap(ptr, BMALLOC_HEAP_CONFIG),
                    bmalloc_heap_ref_get_heap(heaps + index));
        objects.push_back(ptr);
    }

    unsigned* hasViewCache = new unsigned[PAS_BITVECTOR_NUM_WORDS(numHeaps)];
    unsigned* hasAllocator = new unsigned[PAS_BITVECTOR_NUM_WORDS(numHeaps)];

    for (size_t index = numHeaps; index--;) {
        pas_bitvector_set(hasViewCache, index, false);
        pas_bitvector_set(hasAllocator, index, false);
    }

    pas_heap_lock_lock();
    pas_thread_local_cache* cache = pas_thread_local_cache_try_get();
    CHECK(cache);
    pas_thread_local_cache_layout_node layoutNode;
    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layoutNode)) {
        pas_allocator_index allocatorIndex;
        pas_local_allocator_scavenger_data* scavengerData;
        pas_segregated_size_directory* directory;
        pas_heap* heap;

        allocatorIndex = pas_thread_local_cache_layout_node_get_allocator_index_generic(layoutNode);

        if (allocatorIndex >= cache->allocator_index_upper_bound)
            break;

        if (!pas_thread_local_cache_layout_node_is_committed(layoutNode, cache))
            continue;

        scavengerData = static_cast<pas_local_allocator_scavenger_data*>(
            pas_thread_local_cache_get_local_allocator_direct(cache, allocatorIndex));

        directory = pas_thread_local_cache_layout_node_get_directory(layoutNode);
        heap = pas_heap_for_segregated_heap(directory->heap);

        if (!heap->heap_ref)
            continue;

        if (heap->heap_ref >= heaps && heap->heap_ref < heaps + numHeaps) {
            size_t index = heap->heap_ref - heaps;

            if (pas_thread_local_cache_layout_node_represents_allocator(layoutNode))
                pas_bitvector_set(hasAllocator, index, true);
            else {
                PAS_ASSERT(pas_thread_local_cache_layout_node_represents_view_cache(layoutNode));
                pas_bitvector_set(hasViewCache, index, true);
            }

            CHECK(!pas_local_allocator_scavenger_data_is_stopped(scavengerData));
        }
    }
    pas_heap_lock_unlock();

    for (size_t index = numHeaps; index--;) {
        CHECK(pas_bitvector_get(hasViewCache, index));
        CHECK(pas_bitvector_get(hasAllocator, index));
    }

    CHECK_EQUAL(numCommittedPagesInTLC(), numCommittedPagesInitially);

    pas_heap_lock_lock();
    cache = pas_thread_local_cache_try_get();
    CHECK(cache);
    for (PAS_THREAD_LOCAL_CACHE_LAYOUT_EACH_ALLOCATOR(layoutNode)) {
        pas_allocator_index allocatorIndex;
        pas_local_allocator_scavenger_data* scavengerData;

        allocatorIndex = pas_thread_local_cache_layout_node_get_allocator_index_generic(layoutNode);

        if (allocatorIndex >= cache->allocator_index_upper_bound)
            break;

        scavengerData = static_cast<pas_local_allocator_scavenger_data*>(
            pas_thread_local_cache_get_local_allocator_direct(cache, allocatorIndex));

        if (shouldStopAllocatorAtIndex(allocatorIndex))
            pas_local_allocator_scavenger_data_stop(scavengerData, pas_lock_lock_mode_lock, pas_lock_is_held);
    }
    pas_heap_lock_unlock();

    pas_thread_local_cache_for_all(pas_allocator_scavenge_request_stop_action,
                                   pas_deallocator_scavenge_no_action);

    CHECK_EQUAL(numCommittedPagesInTLC(), numCommittedPagesAfterDecommit);

    if (commitWithFree) {
        for (void* object : objects)
            bmalloc_deallocate(object);
    }

    pas_thread_local_cache_shrink(
        pas_thread_local_cache_try_get(),
        pas_lock_is_not_held);

    for (size_t index = 0; index < numHeaps; ++index) {
        void* ptr = bmalloc_iso_allocate(heaps + index);
        CHECK(ptr);
        CHECK_EQUAL(pas_get_heap(ptr, BMALLOC_HEAP_CONFIG),
                    bmalloc_heap_ref_get_heap(heaps + index));
        if (commitWithFree)
            CHECK_EQUAL(ptr, objects[index]);
    }

    CHECK_EQUAL(numCommittedPagesInTLC(), numCommittedPagesAfterRecommit);
}

void testChaosThenDecommit(unsigned numHeaps, unsigned typeSize, unsigned maxObjectsAtATime,
                           unsigned maxFromSameHeap, size_t maxSize, size_t count)
{
    static constexpr bool verbose = false;
    
    pas_heap_ref* heapRefs = new pas_heap_ref[numHeaps];
    for (unsigned index = numHeaps; index--;) {
        heapRefs[index] = BMALLOC_HEAP_REF_INITIALIZER(
            new bmalloc_type(BMALLOC_TYPE_INITIALIZER(typeSize, 1, "test")));
    }
    
    vector<vector<void*>> objects;
    size_t totalSize = 0;

    pas_heap_ref* heapRef = nullptr;

    auto selectNextHeap = [&] () {
        heapRef = heapRefs + deterministicRandomNumber(numHeaps);
    };

    selectNextHeap();

    while (count--) {
        if (!objects.empty()
            && (totalSize >= maxSize
                || deterministicRandomNumber(2))) {
            unsigned index = deterministicRandomNumber(static_cast<unsigned>(objects.size()));
            vector<void*> objectsToFree = std::move(objects[index]);
            objects[index] = std::move(objects.back());
            objects.pop_back();
            for (void* object : objectsToFree) {
                size_t size = bmalloc_get_allocation_size(object);
                CHECK_GREATER_EQUAL(totalSize, size);
                totalSize -= size;
                bmalloc_deallocate(object);
            }
        } else {
            unsigned numObjectsNow = deterministicRandomNumber(maxObjectsAtATime) + 1;
            vector<void*> objectsNow;
            while (numObjectsNow--) {
                if (!deterministicRandomNumber(maxFromSameHeap))
                    selectNextHeap();
                void* ptr = bmalloc_iso_allocate(heapRef);
                CHECK(ptr);
                CHECK_EQUAL(pas_get_heap(ptr, BMALLOC_HEAP_CONFIG),
                            bmalloc_heap_ref_get_heap(heapRef));
                objectsNow.push_back(ptr);
                totalSize += bmalloc_get_allocation_size(ptr);
            }
            objects.push_back(std::move(objectsNow));
        }
    }

    pas_scavenger_suspend();

    if (verbose)
        cout << "Running scavenger.\n";
    
    pas_scavenger_run_synchronously_now();

    if (verbose)
        cout << "Done running scavenger.\n";

    CHECK_EQUAL(numCommittedPagesInTLC(),
                pas_round_up_to_power_of_2(
                    pas_thread_local_cache_offset_of_allocator(PAS_LOCAL_ALLOCATOR_UNSELECTED_NUM_INDICES),
                    pas_page_malloc_alignment()) >> pas_page_malloc_alignment_shift());

    pas_heap_lock_lock();
    pas_all_heaps_verify_in_steady_state();
    pas_heap_lock_unlock();
}

vector<void*> prepareToTestDLCDecommitThenStuff(unsigned numHeaps)
{
    pas_heap_ref* heaps = new pas_heap_ref[numHeaps];
    for (size_t index = numHeaps; index--;)
        heaps[index] = BMALLOC_HEAP_REF_INITIALIZER(
            new bmalloc_type(BMALLOC_TYPE_INITIALIZER(512, 512, "test")));

    vector<void*> objects;
    for (size_t index = 0; index < numHeaps; ++index) {
        pas_page_base* page = nullptr;
        for (size_t objectIndex = pas_segregated_page_number_of_objects(
                 512, BMALLOC_HEAP_CONFIG.small_segregated_config, pas_segregated_page_exclusive_role);
             objectIndex--;) {
            void* ptr = bmalloc_iso_allocate(heaps + index);
            CHECK(ptr);
            CHECK_EQUAL(pas_get_heap(ptr, BMALLOC_HEAP_CONFIG),
                        bmalloc_heap_ref_get_heap(heaps + index));
            pas_page_base* myPage = pas_get_page_base(ptr, BMALLOC_HEAP_CONFIG);
            if (!page) {
                objects.push_back(ptr);
                page = myPage;
            } else
                CHECK_EQUAL(myPage, page);
            CHECK_EQUAL(pas_page_base_get_kind(myPage), pas_small_exclusive_segregated_page_kind);
        }
    }

    return objects;
}

void testTLCDecommitThenDestroyImpl(unsigned numHeaps)
{
    vector<void*> objects = prepareToTestDLCDecommitThenStuff(numHeaps);

    for (void* object : objects)
        bmalloc_deallocate(object);

    CHECK_GREATER(numCommittedPagesInTLC(), 1);
    CHECK(pas_thread_local_cache_try_get()->deallocation_log_index);

    pas_thread_local_cache_for_all(pas_allocator_scavenge_force_stop_action,
                                   pas_deallocator_scavenge_no_action);

    CHECK_EQUAL(numCommittedPagesInTLC(), 1);
    CHECK(pas_thread_local_cache_try_get()->deallocation_log_index);
}

void testTLCDecommitThenDestroy(unsigned numHeaps)
{
    pas_scavenger_suspend();

    testTLCDecommitThenDestroyImpl(numHeaps);

    pas_thread_local_cache_destroy(pas_lock_is_not_held);
}

void testTLCDecommitThenDestroyInThread(unsigned numHeaps)
{
    pas_scavenger_suspend();

    thread myThread = thread([&] () {
        testTLCDecommitThenDestroyImpl(numHeaps);
    });

    myThread.join();
}

void testTLCDecommitThenFlush(unsigned numHeaps)
{
    pas_scavenger_suspend();

    testTLCDecommitThenDestroyImpl(numHeaps);
    pas_thread_local_cache_flush_deallocation_log(pas_thread_local_cache_try_get(), pas_lock_is_not_held);
}

void testTLCDecommitThenDeallocate(unsigned numHeaps)
{
    pas_scavenger_suspend();

    vector<void*> objects = prepareToTestDLCDecommitThenStuff(numHeaps);

    CHECK_GREATER(numCommittedPagesInTLC(), 1);
    CHECK(!pas_thread_local_cache_try_get()->deallocation_log_index);

    pas_thread_local_cache_for_all(pas_allocator_scavenge_force_stop_action,
                                   pas_deallocator_scavenge_no_action);

    for (void* object : objects)
        bmalloc_deallocate(object);
    CHECK(pas_thread_local_cache_try_get()->deallocation_log_index);
    pas_thread_local_cache_flush_deallocation_log(pas_thread_local_cache_try_get(), pas_lock_is_not_held);
}

void testAllocateFromStoppedBaselineImpl()
{
    static constexpr bool verbose = false;

    if (verbose) {
        cout << "TLC = " << pas_thread_local_cache_try_get() << "\n";
        cout << "can set TLC = " << pas_thread_local_cache_can_set() << "\n";
    }

    pas_heap_ref heapRef = BMALLOC_HEAP_REF_INITIALIZER(
        new bmalloc_type(BMALLOC_TYPE_INITIALIZER(32, 1, "test")));

    void* ptr = bmalloc_iso_allocate(&heapRef);
    CHECK(ptr);
    pas_segregated_view view = pas_segregated_view_for_object(
        reinterpret_cast<uintptr_t>(ptr), &bmalloc_heap_config);
    pas_segregated_size_directory* directory = pas_segregated_view_get_size_directory(view);

    bool foundInBaselineTable = false;
    unsigned foundIndex = UINT_MAX;
    for (unsigned i = pas_baseline_allocator_table_bound; i--;) {
        if (pas_segregated_view_get_size_directory(pas_baseline_allocator_table[i].u.allocator.view)
            == directory) {
            foundInBaselineTable = true;
            foundIndex = i;
            break;
        }
    }
    CHECK(foundInBaselineTable);

    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);

    CHECK_EQUAL(pas_baseline_allocator_table[foundIndex].u.allocator.scavenger_data.kind,
                pas_local_allocator_stopped_allocator_kind);

    if (verbose)
        cout << "TLC = " << pas_thread_local_cache_try_get() << "\n";

    ptr = bmalloc_iso_allocate(&heapRef);
    CHECK(ptr);
}

void testAllocateFromStoppedBaseline()
{
    pas_scavenger_suspend();
    testAllocateFromStoppedBaselineImpl();
}

pthread_key_t testAllocateFromStoppedBaselineDuringThreadDestructionKey;

void testAllocateFromStoppedBaselineDuringThreadDestructionDestruct(void*)
{
    testAllocateFromStoppedBaselineImpl();
    int result = pthread_setspecific(testAllocateFromStoppedBaselineDuringThreadDestructionKey, "hello");
    CHECK_EQUAL(result, 0);
}

void testAllocateFromStoppedBaselineDuringThreadDestruction()
{
    pas_scavenger_suspend();

    thread myThread([&] () {
        int result = pthread_key_create(&testAllocateFromStoppedBaselineDuringThreadDestructionKey,
                                        testAllocateFromStoppedBaselineDuringThreadDestructionDestruct);
        CHECK_EQUAL(result, 0);
        result = pthread_setspecific(testAllocateFromStoppedBaselineDuringThreadDestructionKey, "hello");
        CHECK_EQUAL(result, 0);
    });

    myThread.join();
}

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addTLCDecommitTests()
{
#if PAS_ENABLE_BMALLOC
    {
        DecommitZeroFill decommitZeroFill;
    
        TestScope testScope(
            "rage-tlcs",
            [] () {
                bmalloc_typed_runtime_config.base.directory_size_bound_for_baseline_allocators = 0;
                bmalloc_typed_runtime_config.base.directory_size_bound_for_no_view_cache = 0;
                bmalloc_typed_runtime_config.base.view_cache_capacity_for_object_size =
                    pas_heap_runtime_config_aggressive_view_cache_capacity;
            });
    
        ADD_TEST(testTLCDecommit(10000, [] (unsigned index) { return index < 10000; }, false,
                                 588, 300, 304));
        ADD_TEST(testTLCDecommit(10000, [] (unsigned index) { return index < 10000; }, true,
                                 588, 300, 304));
        ADD_TEST(testTLCDecommit(10000, [] (unsigned index) { return index > 10000; }, false,
                                 588, 6, 304));
        ADD_TEST(testTLCDecommit(10000, [] (unsigned index) { return index > 10000; }, true,
                                 588, 6, 304));
        ADD_TEST(testTLCDecommit(10000, [] (unsigned index) { return index < 100000; }, false,
                                 588, 256, 304));
        ADD_TEST(testTLCDecommit(10000, [] (unsigned index) { return index < 100000; }, true,
                                 588, 256, 304));
        ADD_TEST(testTLCDecommit(10000, [] (unsigned index) { return index > 100000; }, false,
                                 588, 50, 304));
        ADD_TEST(testTLCDecommit(10000, [] (unsigned index) { return index > 100000; }, true,
                                 588, 50, 304));
        ADD_TEST(testChaosThenDecommit(10000, 16, 1000, 100, 400000000, 500000));
        ADD_TEST(testChaosThenDecommit(10000, 16, 1000, 10000, 400000000, 1000000));
        ADD_TEST(testChaosThenDecommit(10000, 512, 10, 50, 500000000, 3000000));
        ADD_TEST(testChaosThenDecommit(10000, 48, 100, 1000, 600000000, 3000000));
        ADD_TEST(testChaosThenDecommit(10000, 48, 100, 10000, 600000000, 3000000));

        {
            ForceExclusives forceExclusives;
            ADD_TEST(testTLCDecommitThenDeallocate(6666));
            ADD_TEST(testTLCDecommitThenFlush(6666));
            ADD_TEST(testTLCDecommitThenDestroy(6666));
            ADD_TEST(testTLCDecommitThenDestroyInThread(6666));
        }
    }

    {
        ForceBaselines forceBaselines;
        ADD_TEST(testAllocateFromStoppedBaseline());
        ADD_TEST(testAllocateFromStoppedBaselineDuringThreadDestruction());
    }
#endif // PAS_ENABLE_BMALLOC
}
