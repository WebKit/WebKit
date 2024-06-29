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
#include <condition_variable>
#include <mutex>
#include "pas_compact_expendable_memory.h"
#include "pas_large_expendable_memory.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_size_directory_inlines.h"
#include <thread>

using namespace std;

namespace {

static const bmalloc_type theType = BMALLOC_TYPE_INITIALIZER(42, 2, "foo");
pas_heap_ref theHeap = BMALLOC_HEAP_REF_INITIALIZER(&theType);

void testPayloadImpl(pas_heap_ref& heap, bool firstRun)
{
    void* object = bmalloc_iso_allocate(&heap, pas_non_compact_allocation_mode);
    CHECK(object);
    CHECK_EQUAL(bmalloc_get_allocation_size(object), 48);

    if (firstRun) {
        // Check that so far, we don't need any expendable memories.
        CHECK(!pas_compact_expendable_memory_header.header.size);
        CHECK(!pas_compact_expendable_memory_header.header.bump);
        CHECK(!pas_compact_expendable_memory_payload);
        CHECK(!pas_large_expendable_memory_head);
    }

    void* smallArray = bmalloc_iso_allocate_array_by_size(&heap, 100, pas_non_compact_allocation_mode);
    CHECK(smallArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(smallArray), 112);

    void* mediumArray = bmalloc_iso_allocate_array_by_size(&heap, 400, pas_non_compact_allocation_mode);
    CHECK(mediumArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(mediumArray), 400);

    void* largeArray = bmalloc_iso_allocate_array_by_size(&heap, 2000, pas_non_compact_allocation_mode);
    CHECK(largeArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(largeArray), 2016);

    void* largerArray = bmalloc_iso_allocate_array_by_size(&heap, 10000, pas_non_compact_allocation_mode);
    CHECK(largerArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(largerArray), 10752);

    void* largestArray = bmalloc_iso_allocate_array_by_size(&heap, 100000, pas_non_compact_allocation_mode);
    CHECK(largestArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(largestArray), 100000);

    // Check that the test indeed created expendable memories.
    CHECK(pas_compact_expendable_memory_header.header.size);
    CHECK(pas_compact_expendable_memory_header.header.bump);
    CHECK(pas_compact_expendable_memory_payload);
    CHECK(pas_large_expendable_memory_head);
    CHECK(pas_large_expendable_memory_head->header.size);
    CHECK(pas_large_expendable_memory_head->header.bump);
}

void testPayloadImpl(bool firstRun = true)
{
    testPayloadImpl(theHeap, firstRun);
}

template<typename Func>
void forEachExpendableMemory(const Func& func)
{
    pas_heap_lock_lock();
    func(pas_compact_expendable_memory_header.header, pas_compact_expendable_memory_payload);
    for (pas_large_expendable_memory* largeMemory = pas_large_expendable_memory_head;
         largeMemory;
         largeMemory = largeMemory->next)
        func(largeMemory->header, pas_large_expendable_memory_payload(largeMemory));
    pas_heap_lock_unlock();
}

void checkAllDecommitted()
{
    forEachExpendableMemory([&] (pas_expendable_memory& header, void* payload) {
        CHECK(pas_expendable_memory_num_pages_in_use(&header));
        for (unsigned index = 0; index < pas_expendable_memory_num_pages(&header); ++index) {
            CHECK_EQUAL(pas_expendable_memory_state_get_kind(header.states[index]),
                        PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED);
        }
    });
}

void checkAllInUseCommitted()
{
    forEachExpendableMemory([&] (pas_expendable_memory& header, void* payload) {
        CHECK(pas_expendable_memory_num_pages_in_use(&header));
        CHECK_EQUAL(pas_expendable_memory_state_get_kind(header.states[0]),
                    PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED);
        CHECK_EQUAL(pas_expendable_memory_state_get_kind(
                        header.states[pas_expendable_memory_num_pages_in_use(&header) - 1]),
                    PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED);
        for (unsigned index = 0; index < pas_expendable_memory_num_pages_in_use(&header); ++index) {
            CHECK_NOT_EQUAL(pas_expendable_memory_state_get_kind(header.states[index]),
                            PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED);
            CHECK_LESS_EQUAL(pas_expendable_memory_state_get_kind(header.states[index]),
                             PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED);
            CHECK((pas_expendable_memory_state_get_kind(header.states[index])
                   == PAS_EXPENDABLE_MEMORY_STATE_KIND_INTERIOR) ||
                  (pas_expendable_memory_state_get_kind(header.states[index])
                   == PAS_EXPENDABLE_MEMORY_STATE_KIND_JUST_USED));
        }
        for (unsigned index = pas_expendable_memory_num_pages_in_use(&header);
             index < pas_expendable_memory_num_pages(&header);
             ++index) {
            CHECK_EQUAL(pas_expendable_memory_state_get_kind(header.states[index]),
                        PAS_EXPENDABLE_MEMORY_STATE_KIND_DECOMMITTED);
        }
    });
}

void testSynchronousScavengingExpendsExpendableMemory()
{
    pas_scavenger_suspend();
    testPayloadImpl();
    checkAllInUseCommitted();
    CHECK(!pas_segregated_heap_num_size_lookup_rematerializations);
    pas_scavenger_run_synchronously_now();
    checkAllDecommitted();
    testPayloadImpl(false);
    checkAllInUseCommitted();
    CHECK(pas_segregated_heap_num_size_lookup_rematerializations);
}

bool scavengerIsDone;
mutex scavengerMutex;
condition_variable scavengerCondition;

void scavengerWillShutDown()
{
    lock_guard<mutex> locker(scavengerMutex);
    scavengerIsDone = true;
    scavengerCondition.notify_all();
}

void testScavengerExpendsExpendableMemory()
{
    // Make the scavenger very fast so that this test does not run for too long.
    pas_scavenger_deep_sleep_timeout_in_milliseconds = 1;
    pas_scavenger_period_in_milliseconds = 1;

    scavengerIsDone = false;
    pas_scavenger_will_shut_down_callback = scavengerWillShutDown;

    testPayloadImpl();

    {
        unique_lock<mutex> locker(scavengerMutex);
        scavengerCondition.wait(locker, [&] () -> bool { return scavengerIsDone; });
    }

    checkAllDecommitted();
    testPayloadImpl(false);
    CHECK(pas_segregated_heap_num_size_lookup_rematerializations);
}

void testSoManyHeaps()
{
    static constexpr unsigned numHeaps = 10000;
    
    pas_heap_ref* heaps = new pas_heap_ref[numHeaps];

    pas_scavenger_suspend();
    
    for (unsigned i = numHeaps; i--;)
        heaps[i] = BMALLOC_HEAP_REF_INITIALIZER(&theType);

    for (unsigned i = 0; i < numHeaps; ++i)
        testPayloadImpl(heaps[i], !i);

    checkAllInUseCommitted();
    CHECK(!pas_segregated_heap_num_size_lookup_rematerializations);

    pas_scavenger_run_synchronously_now();
    checkAllDecommitted();

    for (unsigned i = 0; i < numHeaps; ++i)
        testPayloadImpl(heaps[i], false);

    checkAllInUseCommitted();
    CHECK(pas_segregated_heap_num_size_lookup_rematerializations);
}

void testRage(unsigned numHeaps, function<unsigned(unsigned)> allocationSize, unsigned numThreads,
              unsigned count, function<void()> sleep)
{
    thread* threads = new thread[numThreads];
    pas_primitive_heap_ref* heaps = new pas_primitive_heap_ref[numHeaps];

    for (unsigned i = numHeaps; i--;)
        heaps[i] = BMALLOC_FLEX_HEAP_REF_INITIALIZER(new bmalloc_type(BMALLOC_TYPE_INITIALIZER(1, 1, "test")));

    mutex lock;
    unsigned numThreadsDone = 0;

    for (unsigned i = numThreads; i--;) {
        threads[i] = thread([&] () {
            for (unsigned j = 0; j < count; ++j) {
                pas_primitive_heap_ref* heap = heaps + deterministicRandomNumber(numHeaps);
                size_t size = allocationSize(j);
                void* ptr = bmalloc_allocate_flex(heap, size, pas_non_compact_allocation_mode);
                CHECK(ptr);
                CHECK_GREATER_EQUAL(bmalloc_get_allocation_size(ptr), size);
                CHECK_EQUAL(bmalloc_get_heap(ptr),
                            bmalloc_flex_heap_ref_get_heap(heap));
                bmalloc_deallocate(ptr);
            }
            lock_guard<mutex> locker(lock);
            numThreadsDone++;
        });
    }

    while (numThreadsDone < numThreads) {
        pas_scavenger_decommit_expendable_memory();
        sleep();
    }
}

void testRematerializeAfterSearchOfDecommitted()
{
    static constexpr unsigned initialSize = 16;
    static constexpr unsigned size = 10752;
    static constexpr unsigned someOtherSize = 5000;
    
    pas_primitive_heap_ref heapRef = BMALLOC_FLEX_HEAP_REF_INITIALIZER(
        new bmalloc_type(BMALLOC_TYPE_INITIALIZER(1, 1, "test")));
    pas_heap* heap = bmalloc_flex_heap_ref_get_heap(&heapRef);

    void* ptr = bmalloc_allocate_flex(&heapRef, initialSize, pas_non_compact_allocation_mode);
    CHECK_EQUAL(bmalloc_get_allocation_size(ptr), initialSize);
    CHECK_EQUAL(bmalloc_get_heap(ptr), heap);
    CHECK_EQUAL(heapRef.cached_index, pas_segregated_heap_index_for_size(initialSize, BMALLOC_HEAP_CONFIG));

    ptr = bmalloc_allocate_flex(&heapRef, size, pas_non_compact_allocation_mode);
    CHECK_EQUAL(bmalloc_get_allocation_size(ptr), size);
    CHECK_EQUAL(bmalloc_get_heap(ptr), heap);

    pas_segregated_view view = pas_segregated_view_for_object(
        reinterpret_cast<uintptr_t>(ptr), &bmalloc_heap_config);
    pas_segregated_size_directory* directory = pas_segregated_view_get_size_directory(view);

    pas_segregated_heap_medium_directory_tuple* tuple =
        pas_segregated_heap_medium_directory_tuple_for_index(
            &heap->segregated_heap,
            pas_segregated_heap_index_for_size(size, BMALLOC_HEAP_CONFIG),
            pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
            pas_lock_is_not_held);

    CHECK(tuple);
    CHECK_EQUAL(pas_compact_atomic_segregated_size_directory_ptr_load(&tuple->directory),
                directory);

    pas_scavenger_fake_decommit_expendable_memory();

    tuple->begin_index = 0;

    pas_segregated_heap_medium_directory_tuple* someOtherTuple =
        pas_segregated_heap_medium_directory_tuple_for_index(
            &heap->segregated_heap,
            pas_segregated_heap_index_for_size(someOtherSize, BMALLOC_HEAP_CONFIG),
            pas_segregated_heap_medium_size_directory_search_within_size_class_progression,
            pas_lock_is_not_held);

    if (someOtherTuple) {
        cout << "Unexpectedly found a tuple: " << someOtherTuple << "\n";
        cout << "It points at directory = "
             << pas_compact_atomic_segregated_size_directory_ptr_load(&someOtherTuple->directory) << "\n";
        cout << "Our original directory is = " << directory << "\n";
    }
    
    CHECK(!someOtherTuple);
}

void testBasicSizeClass(unsigned firstSize, unsigned secondSize)
{
    static constexpr bool verbose = false;
    
    pas_primitive_heap_ref heapRef = BMALLOC_FLEX_HEAP_REF_INITIALIZER(
        new bmalloc_type(BMALLOC_TYPE_INITIALIZER(1, 1, "test")));

    if (verbose)
        cout << "Allocating " << firstSize << "\n";
    void* ptr = bmalloc_allocate_flex(&heapRef, firstSize, pas_non_compact_allocation_mode);
    if (verbose)
        cout << "Allocating " << secondSize << "\n";
    bmalloc_allocate_flex(&heapRef, secondSize, pas_non_compact_allocation_mode);

    if (verbose)
        cout << "Doing some checks.\n";
    CHECK(pas_thread_local_cache_try_get());
    CHECK_EQUAL(heapRef.cached_index, pas_segregated_heap_index_for_size(firstSize, BMALLOC_HEAP_CONFIG));
    CHECK(heapRef.base.allocator_index);
    CHECK(pas_thread_local_cache_try_get_local_allocator_or_unselected_for_uninitialized_index(
              pas_thread_local_cache_try_get(), heapRef.base.allocator_index).did_succeed);
    if (verbose)
        cout << "Did some checks.\n";

    pas_segregated_view view = pas_segregated_view_for_object(
        reinterpret_cast<uintptr_t>(ptr), &bmalloc_heap_config);
    pas_segregated_size_directory* directory = pas_segregated_view_get_size_directory(view);
    pas_segregated_size_directory_select_allocator(
        directory, firstSize, pas_avoid_size_lookup, &bmalloc_heap_config, &heapRef.cached_index);
}

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addExpendableMemoryTests()
{
#if PAS_ENABLE_BMALLOC
    ForceTLAs forceTLAs;
    
    ADD_TEST(testSynchronousScavengingExpendsExpendableMemory());
    ADD_TEST(testScavengerExpendsExpendableMemory());
    ADD_TEST(testSoManyHeaps());
    ADD_TEST(testRage(10, [] (unsigned) { return deterministicRandomNumber(100000); }, 10, 100000, [] () { sched_yield(); }));
    ADD_TEST(testRage(10, [] (unsigned j) { return deterministicRandomNumber(j); }, 2, 1000000, [] () { sched_yield(); }));
    ADD_TEST(testRage(10, [] (unsigned j) { return j; }, 10, 100000, [] () { sched_yield(); }));
    ADD_TEST(testRage(10, [] (unsigned j) { return j; }, 1, 1000000, [] () { sched_yield(); }));
    ADD_TEST(testRage(1000, [] (unsigned) { return deterministicRandomNumber(100000); }, 10, 100000, [] () { sched_yield(); }));
    ADD_TEST(testRage(1000, [] (unsigned j) { return deterministicRandomNumber(j); }, 2, 1000000, [] () { sched_yield(); }));
    ADD_TEST(testRage(1000, [] (unsigned j) { return j; }, 10, 100000, [] () { sched_yield(); }));
    ADD_TEST(testRage(10, [] (unsigned) { return deterministicRandomNumber(100000); }, 2, 1000000, [] () { sched_yield(); }));
    ADD_TEST(testRage(10, [] (unsigned j) { return deterministicRandomNumber(j); }, 10, 100000, [] () { usleep(1); }));
    ADD_TEST(testRage(10, [] (unsigned j) { return j; }, 1, 100000, [] () { usleep(1); }));
    ADD_TEST(testRage(1000, [] (unsigned) { return deterministicRandomNumber(100000); }, 2, 1000000, [] () { usleep(1); }));
    ADD_TEST(testRage(1000, [] (unsigned j) { return deterministicRandomNumber(j); }, 2, 1000000, [] () { usleep(1); }));
    ADD_TEST(testRage(1000, [] (unsigned j) { return j; }, 10, 100000, [] () { usleep(1); }));
    ADD_TEST(testRematerializeAfterSearchOfDecommitted());
    ADD_TEST(testBasicSizeClass(0, 16));
#endif // PAS_ENABLE_BMALLOC
}

