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
#include <map>
#include <mutex>
#include "pas_race_test_hooks.h"
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

} // anonymous namespace

#endif // PAS_ENABLE_TESTING && PAS_ENABLE_ISO

void addRaceTests()
{
#if PAS_ENABLE_TESTING && PAS_ENABLE_ISO
    InstallRaceHooks installRaceHooks;

    {
        DisableBitfit disableBitfit;
        ForceExclusives forceExclusives;
        ForceTLAs forceTLAs;

        ADD_TEST(testLocalAllocatorStopRace(pas_race_test_hook_local_allocator_stop_before_did_stop_allocating));
        ADD_TEST(testLocalAllocatorStopRace(pas_race_test_hook_local_allocator_stop_before_unlock));
        ADD_TEST(testLocalAllocatorStopRaceAgainstScavenge(pas_race_test_hook_local_allocator_stop_before_did_stop_allocating));
        ADD_TEST(testLocalAllocatorStopRaceAgainstScavenge(pas_race_test_hook_local_allocator_stop_before_unlock));
    }
#endif // PAS_ENABLE_TESTING && PAS_ENABLE_ISO
}

