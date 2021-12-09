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

#include "TestHarness.h"

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap.h"
#include <condition_variable>
#include <mutex>
#include "pas_compact_expendable_memory.h"
#include "pas_large_expendable_memory.h"
#include "pas_segregated_heap.h"

using namespace std;

namespace {

bmalloc_type theType = BMALLOC_TYPE_INITIALIZER(42, 2, "foo");
pas_heap_ref theHeap = BMALLOC_HEAP_REF_INITIALIZER(&theType);

void testPayloadImpl(pas_heap_ref& heap, bool firstRun)
{
    void* object = bmalloc_iso_allocate(&heap);
    CHECK(object);
    CHECK_EQUAL(bmalloc_get_allocation_size(object), 48);

    if (firstRun) {
        // Check that so far, we don't need any expendable memories.
        CHECK(!pas_compact_expendable_memory_header.header.size);
        CHECK(!pas_compact_expendable_memory_header.header.bump);
        CHECK(!pas_compact_expendable_memory_payload);
        CHECK(!pas_large_expendable_memory_head);
    }

    void* smallArray = bmalloc_iso_allocate_array_by_size(&heap, 100);
    CHECK(smallArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(smallArray), 112);

    void* mediumArray = bmalloc_iso_allocate_array_by_size(&heap, 400);
    CHECK(mediumArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(mediumArray), 400);

    void* largeArray = bmalloc_iso_allocate_array_by_size(&heap, 2000);
    CHECK(largeArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(largeArray), 2016);

    void* largerArray = bmalloc_iso_allocate_array_by_size(&heap, 10000);
    CHECK(largerArray);
    CHECK_EQUAL(bmalloc_get_allocation_size(largerArray), 10752);

    void* largestArray = bmalloc_iso_allocate_array_by_size(&heap, 100000);
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

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addExpendableMemoryTests()
{
#if PAS_ENABLE_BMALLOC
    ADD_TEST(testSynchronousScavengingExpendsExpendableMemory());
    ADD_TEST(testScavengerExpendsExpendableMemory());
    ADD_TEST(testSoManyHeaps());
#endif // PAS_ENABLE_BMALLOC
}

