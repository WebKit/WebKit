/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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
#include <condition_variable>
#include <functional>
#include "iso_heap.h"
#include "iso_heap_config.h"
#include "iso_heap_ref.h"
#include <map>
#include <mutex>
#include "pas_heap.h"
#include "pas_primitive_heap_ref.h"
#include "pas_race_test_hooks.h"
#include "pas_segregated_heap_inlines.h"
#include "pas_segregated_size_directory.h"
#include "pas_thread_local_cache.h"
#include <set>
#include <thread>

using namespace std;

#if PAS_ENABLE_TESTING && PAS_ENABLE_ISO

namespace {

mutex globalLock;
condition_variable globalCond;
map<thread::id, set<pas_lock*>> locksHeldForThread;
map<pas_lock*, thread::id> threadHoldingLock;
map<pas_lock*, unsigned> locksUpForContention;
function<void(pas_race_test_hook_kind)> hookCallback;

void hookCallbackAdapter(pas_race_test_hook_kind kind)
{
    hookCallback(kind);
}

void willLockCallback(pas_lock* lock)
{
    lock_guard<mutex> locker(globalLock);
    locksUpForContention[lock]++;
    globalCond.notify_all();
}

void recordLockAcquisition(pas_lock* lock)
{
    thread::id myId = this_thread::get_id();

    locksHeldForThread[myId].insert(lock);
    threadHoldingLock[lock] = myId;
}

void didLockCallback(pas_lock* lock)
{
    lock_guard<mutex> locker(globalLock);
    
    auto iter = locksUpForContention.find(lock);
    PAS_ASSERT(iter != locksUpForContention.end());
    PAS_ASSERT(iter->second);
    if (!--iter->second)
        locksUpForContention.erase(iter);

    recordLockAcquisition(lock);
}

void didTryLockCallback(pas_lock* lock)
{
    lock_guard<mutex> locker(globalLock);
    recordLockAcquisition(lock);
}

void willUnlockCallback(pas_lock* lock)
{
    lock_guard<mutex> locker(globalLock);

    thread::id myId = this_thread::get_id();

    locksHeldForThread[myId].erase(lock);
    threadHoldingLock.erase(lock);
}

// Call holding the lock.
bool isHoldingContendedLocks()
{
    thread::id myId = this_thread::get_id();

    for (pas_lock* lock : locksHeldForThread[myId]) {
        if (locksUpForContention.count(lock))
            return true;
    }

    return false;
}

class InstallRaceHooks : public TestScope {
public:
    InstallRaceHooks()
        : TestScope(
            "install-race-hooks",
            [] () {
                pas_race_test_hook_callback_instance = hookCallbackAdapter;
                pas_race_test_will_lock_callback = willLockCallback;
                pas_race_test_did_lock_callback = didLockCallback;
                pas_race_test_did_try_lock_callback = didTryLockCallback;
                pas_race_test_will_unlock_callback = willUnlockCallback;
            })
    {
    }
};

void testLocalAllocatorStopRace(pas_race_test_hook_kind kindToStopOn)
{
    pas_scavenger_suspend();
    
    bool okToGetToHook = false;
    bool didGetToHook = false;
    bool hookShouldStop = false;
    bool hookDidStop = false;
    bool shrinkDidFinish = false;
    void* thePtr;
    
    hookCallback =
        [&] (pas_race_test_hook_kind kind) {
            if (kind != kindToStopOn)
                return;

            CHECK_EQUAL(locksHeldForThread[this_thread::get_id()].size(), 3);
            CHECK(okToGetToHook);

            unique_lock<mutex> locker(globalLock);
            didGetToHook = true;
            globalCond.notify_all();
            while (!hookShouldStop)
                globalCond.wait(locker);
            hookDidStop = true;
        };

    pas_heap_ref heap = ISO_HEAP_REF_INITIALIZER(sizeof(int));

    thread thread1 = thread(
        [&] () {
            void* ptr = iso_allocate(&heap, pas_non_compact_allocation_mode);
            CHECK(ptr);
            CHECK(pas_segregated_view_is_exclusive(
                      pas_segregated_view_for_object(
                          reinterpret_cast<uintptr_t>(ptr),
                          &iso_heap_config)));
            iso_deallocate(ptr);
            okToGetToHook = true;
            thePtr = ptr;
            pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                          pas_lock_is_not_held);
            shrinkDidFinish = true;
        });

    {
        unique_lock<mutex> locker(globalLock);
        while (!didGetToHook)
            globalCond.wait(locker);
        hookShouldStop = true;
        globalCond.notify_all();
    }

    void* ptr = iso_allocate(&heap, pas_non_compact_allocation_mode);
    if (kindToStopOn == pas_race_test_hook_local_allocator_stop_before_unlock)
        CHECK_EQUAL(ptr, thePtr);
    thread1.join();
    CHECK(hookDidStop);
    CHECK(shrinkDidFinish);
}

void testLocalAllocatorStopRaceAgainstScavenge(pas_race_test_hook_kind kindToStopOn)
{
    pas_scavenger_suspend();
    
    bool okToGetToHook = false;
    bool didGetToHook = false;
    bool hookShouldStop = false;
    bool hookDidStop = false;
    bool shrinkDidFinish = false;
    void* thePtr;
    
    hookCallback =
        [&] (pas_race_test_hook_kind kind) {
            if (kind != kindToStopOn)
                return;

            CHECK_EQUAL(locksHeldForThread[this_thread::get_id()].size(), 3);
            CHECK(okToGetToHook);

            unique_lock<mutex> locker(globalLock);
            didGetToHook = true;
            globalCond.notify_all();
            while (!hookShouldStop && !isHoldingContendedLocks())
                globalCond.wait(locker);
            hookDidStop = true;
        };

    pas_heap_ref heap = ISO_HEAP_REF_INITIALIZER(sizeof(int));

    thread thread1 = thread(
        [&] () {
            void* ptr = iso_allocate(&heap, pas_non_compact_allocation_mode);
            CHECK(ptr);
            CHECK(pas_segregated_view_is_exclusive(
                      pas_segregated_view_for_object(
                          reinterpret_cast<uintptr_t>(ptr),
                          &iso_heap_config)));
            iso_deallocate(ptr);
            okToGetToHook = true;
            thePtr = ptr;
            pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                          pas_lock_is_not_held);
            shrinkDidFinish = true;
        });

    {
        unique_lock<mutex> locker(globalLock);
        while (!didGetToHook)
            globalCond.wait(locker);
        hookShouldStop = true;
        globalCond.notify_all();
    }

    pas_scavenger_decommit_free_memory();

    void* ptr = iso_allocate(&heap, pas_non_compact_allocation_mode);
    if (kindToStopOn == pas_race_test_hook_local_allocator_stop_before_unlock)
        CHECK_EQUAL(ptr, thePtr);
    hookShouldStop = true;
    thread1.join();
    CHECK(hookDidStop);
    CHECK(shrinkDidFinish);
}

void testMediumDirectoryTornInsertRace()
{
    pas_scavenger_suspend();

    pas_primitive_heap_ref heap = ISO_PRIMITIVE_HEAP_REF_INITIALIZER;
    constexpr size_t kBigSize = 8192;
    constexpr size_t kNewSize = 1024;

    /* This is a bit of a hack: we want to catch a race on the medium-tuple
       insertion path, but in order to set up the race we need to go through
       the insertion path once without getting stopped by the hook.
       So we start with a no-op hook and only afterwards install the effectful one. */
    hookCallback = [] (pas_race_test_hook_kind) { };
    void* seedBig = iso_allocate_primitive(&heap, kBigSize, pas_non_compact_allocation_mode);
    CHECK(seedBig);

    bool didGetToHook = false;
    bool hookShouldStop = false;
    bool hookDidStop = false;

    hookCallback =
        [&] (pas_race_test_hook_kind kind) {
            if (kind != pas_race_test_hook_medium_directory_after_directory_store)
                return;

            unique_lock<mutex> locker(globalLock);
            didGetToHook = true;
            globalCond.notify_all();
            while (!hookShouldStop && !isHoldingContendedLocks())
                globalCond.wait(locker);
            hookDidStop = true;
        };

    thread writer = thread(
        [&] () {
            /* Inserts a smaller medium tuple at slot 0, shifting the original
               tuple to slot 1.
               Once the hook fires, slot[0]'s directory ptr will have been overwritten,
               but its begin_index/end_index fields won't yet have been.
               If there is a race here, then the reader will see
               { new_directory_ptr, old_begin_index, old_end_index } */
            void* small = iso_allocate_primitive(&heap, kNewSize, pas_non_compact_allocation_mode);
            CHECK(small);
        });

    {
        unique_lock<mutex> locker(globalLock);
        while (!didGetToHook)
            globalCond.wait(locker);
    }

    /* A NULL cached_index forces the lookup to skip the
       basic-size-directory path, which ensures we take the medium path.
       Since the slot-write hasn't finished yet, we should expect to see
       the original directory, with object_size == kBigSize.
       If we see a directory with kNewSize, then there is a race.
       If the lock-free medium path doesn't respect the seqlock, it will
       observe the torn slot we created above. */
    size_t bigIndex = pas_segregated_heap_index_for_size(kBigSize, iso_heap_config);
    pas_segregated_size_directory* observed =
        pas_segregated_heap_size_directory_for_index(
            &heap.base.heap->segregated_heap, bigIndex, NULL, &iso_heap_config,
            pas_lock_is_not_held);

    {
        unique_lock<mutex> locker(globalLock);
        hookShouldStop = true;
        globalCond.notify_all();
    }
    writer.join();

    CHECK(hookDidStop);
    CHECK(observed);
    CHECK_GREATER_EQUAL((size_t)observed->object_size, kBigSize);
}

} // anonymous namespace

#endif // PAS_ENABLE_TESTING && PAS_ENABLE_ISO

void addRaceTests()
{
#if PAS_ENABLE_TESTING && PAS_ENABLE_ISO
    InstallRaceHooks installRaceHooks;

    {
        DisableBitfit disableBitfit;
        ForceTLAs forceTLAs;

        ADD_TEST(testLocalAllocatorStopRace(pas_race_test_hook_local_allocator_stop_before_did_stop_allocating));
        ADD_TEST(testLocalAllocatorStopRace(pas_race_test_hook_local_allocator_stop_before_unlock));
        ADD_TEST(testLocalAllocatorStopRaceAgainstScavenge(pas_race_test_hook_local_allocator_stop_before_did_stop_allocating));
        ADD_TEST(testLocalAllocatorStopRaceAgainstScavenge(pas_race_test_hook_local_allocator_stop_before_unlock));

        ADD_TEST(testMediumDirectoryTornInsertRace());
    }
#endif // PAS_ENABLE_TESTING && PAS_ENABLE_ISO
}

