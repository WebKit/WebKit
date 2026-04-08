/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "pas_committed_pages_vector.h"
#include "pas_scavenger.h"
#include "pas_thread_local_cache.h"
#include "pas_thread_suspend.h"
#include "pas_thread_suspend_lock.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#if !PAS_OS(DARWIN)
#include "pas_thread_suspend_signal_handler.h"
#include <pthread.h>
#include <signal.h>
#endif

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap.h"

static std::atomic_bool allowThreadToExit { false };
static std::atomic_bool threadCanDecommit { false };
static std::atomic_bool threadOK { false };
static std::atomic_int counter { 0 };

extern "C" inline void busyThread()
{
    while (!allowThreadToExit.load())
        counter.fetch_add(1);
    threadOK.store(true);
}

inline void testThreadCanSuspend()
{
    TestScope threadCanSuspend(
        "threadCanSuspend",
        [] () {
        });

    pas_thread_suspend_initialize();
    allowThreadToExit.store(false);
    threadOK.store(false);
    counter.store(0);

    int prevCounterValue;
    {
        std::thread busy(busyThread);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
#if PAS_OS(DARWIN)
        pas_thread_suspend_lock_lock();
        pas_thread_suspend_suspend(busy.native_handle());
        pas_thread_suspend_lock_unlock();
#else
        pas_thread_suspend_data suspendData = pas_thread_suspend_data_create(busy.native_handle(), pas_machine_stack_bounds { nullptr, nullptr });
        pas_thread_suspend_lock_lock();
        pas_thread_suspend_suspend(&suspendData);
        pas_thread_suspend_lock_unlock();
#endif

        prevCounterValue = counter.load();
        CHECK_GREATER(prevCounterValue, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
            CHECK_EQUAL(counter.load(), prevCounterValue);

#if PAS_OS(DARWIN)
        pas_thread_suspend_lock_lock();
        pas_thread_suspend_resume(busy.native_handle());
        pas_thread_suspend_lock_unlock();
#else
        pas_thread_suspend_lock_lock();
        pas_thread_suspend_resume(&suspendData);
        pas_thread_suspend_lock_unlock();
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        CHECK_GREATER(counter.load(), prevCounterValue);
        allowThreadToExit.store(true);
        busy.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK_GREATER(counter.load(), prevCounterValue);
    CHECK(threadOK.load());
}

#if !PAS_OS(DARWIN)
inline void testAltSignalStackForcesRetry()
{
    TestScope scope(
        "altSignalStackForcesRetry",
        [] () {
        });

    pas_thread_suspend_initialize();
    allowThreadToExit.store(false);
    threadOK.store(false);
    counter.store(0);

    std::thread busy([] {
        // Install an alternate signal stack so suspend signals run on it.
        stack_t altStack;
        std::vector<char> altStorage(SIGSTKSZ);
        altStack.ss_sp = altStorage.data();
        altStack.ss_size = altStorage.size();
        altStack.ss_flags = 0;
        sigaltstack(&altStack, nullptr);

        struct sigaction sa { };
        sa.sa_handler = [] (int) {
            // Spin on the alt stack so a suspend arriving here hits the retry path.
            for (volatile int i = 0; i < 1000000; ++i) { }
        };
        sa.sa_flags = SA_ONSTACK;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR2, &sa, nullptr);

        // Self-signal frequently so we spend most of our time on the alt stack.
        while (!allowThreadToExit.load()) {
            counter.fetch_add(1);
            if (!(counter.load() & 0xff))
                pthread_kill(pthread_self(), SIGUSR2);
        }
        sigaltstack(nullptr, nullptr);
        threadOK.store(true);
    });

    // Capture real stack bounds so the handler's alt-stack check has something to compare against.
    pthread_attr_t attr;
    void* stack_addr = nullptr;
    size_t stack_size = 0;
    pthread_getattr_np(busy.native_handle(), &attr);
    pthread_attr_getstack(&attr, &stack_addr, &stack_size);
    pthread_attr_destroy(&attr);

    void* origin = (char*)stack_addr + stack_size;
    void* bound = stack_addr;

    // Keep suspending until one coincides with the alt stack and takes the retry path.
    unsigned retriesBefore = pas_thread_suspend_signal_handler_retry_count();
    const int maxAttempts = 1000;
    int attempts = 0;
    while (pas_thread_suspend_signal_handler_retry_count() == retriesBefore && attempts < maxAttempts) {
        ++attempts;
        pas_thread_suspend_data suspendData = pas_thread_suspend_data_create(
            busy.native_handle(), pas_machine_stack_bounds { origin, bound });
        pas_thread_suspend_lock_lock();
        bool ok = pas_thread_suspend_suspend(&suspendData);
        CHECK(ok);
        // Counter must be frozen while suspended.
        int snap = counter.load();
        for (int j = 0; j < 100; ++j)
            CHECK_EQUAL(counter.load(), snap);
        pas_thread_suspend_resume(&suspendData);
        pas_thread_suspend_lock_unlock();
    }

    // The retry-after-yield branch must have actually run
    CHECK_GREATER(pas_thread_suspend_signal_handler_retry_count(), retriesBefore);

    allowThreadToExit.store(true);
    busy.join();
    CHECK(threadOK.load());
}

#if PAS_HAVE(MACHINE_CONTEXT)
static uintptr_t spFromMachineContext(const mcontext_t& mc)
{
#if PAS_X86_64
    return static_cast<uintptr_t>(mc.gregs[REG_RSP]);
#elif PAS_ARM64
    return static_cast<uintptr_t>(mc.sp);
#elif PAS_ARM
    return static_cast<uintptr_t>(mc.arm_sp);
#elif PAS_RISCV
    return static_cast<uintptr_t>(mc.__gregs[REG_SP]);
#else
#error "Unsupported architecture for SP extraction in test"
#endif
}
#endif

// After suspend, the captured registers should describe the suspended frame: the captured SP
// must fall within the target thread's stack bounds.
inline void testRegisterCaptureSPInBounds()
{
    TestScope scope(
        "registerCaptureSPInBounds",
        [] () {
        });

    pas_thread_suspend_initialize();
    allowThreadToExit.store(false);
    threadOK.store(false);
    counter.store(0);

    std::thread busy(busyThread);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK_GREATER(counter.load(), 0);

    pthread_attr_t attr;
    void* stack_addr = nullptr;
    size_t stack_size = 0;
    pthread_getattr_np(busy.native_handle(), &attr);
    pthread_attr_getstack(&attr, &stack_addr, &stack_size);
    pthread_attr_destroy(&attr);

    void* origin = (char*)stack_addr + stack_size;
    void* bound = stack_addr;
    pas_machine_stack_bounds bounds { origin, bound };

    pas_thread_suspend_data suspendData = pas_thread_suspend_data_create(busy.native_handle(), bounds);
    pas_thread_suspend_lock_lock();
    CHECK(pas_thread_suspend_suspend(&suspendData));

    pas_machine_registers scratch;
    pas_machine_registers* registers = pas_thread_suspend_get_registers(&suspendData, &scratch);
    CHECK(registers);

#if PAS_HAVE(MACHINE_CONTEXT)
    uintptr_t sp = spFromMachineContext(registers->machineContext);
    CHECK_GREATER_EQUAL(sp, reinterpret_cast<uintptr_t>(bound));
    CHECK_LESS(sp, reinterpret_cast<uintptr_t>(origin));
#endif

    pas_thread_suspend_resume(&suspendData);
    pas_thread_suspend_lock_unlock();

    allowThreadToExit.store(true);
    busy.join();
    CHECK(threadOK.load());
}
#endif // !PAS_OS(DARWIN)

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

extern "C" inline void uncooperativeThread()
{
    bmalloc_deallocate(bmalloc_allocate(16, pas_non_compact_allocation_mode));
    size_t floorSize = numCommittedPagesInTLC();

    // Commit several TLC pages by exercising many size classes before we stop
    // cooperating. A single small allocation dirties only one page, so on
    // large-page systems the scavenger would have nothing to reclaim; growing
    // the TLC past one page makes the decommit observable at any page size.
    std::vector<void*> objects;
    for (size_t size = 16; size <= 16 * 1024; size += 16) {
        void* ptr = bmalloc_allocate(size, pas_non_compact_allocation_mode);
        CHECK(ptr);
        objects.push_back(ptr);
    }
    for (void* ptr : objects)
        bmalloc_deallocate(ptr);

    size_t startSize = numCommittedPagesInTLC();
    CHECK_GREATER(startSize, floorSize);

    while (!allowThreadToExit.load()) {
        CHECK(numCommittedPagesInTLC() == startSize || threadCanDecommit.load());
        counter.fetch_add(1);
    }

    CHECK(threadCanDecommit.load());
    CHECK_LESS(numCommittedPagesInTLC(), startSize);

    threadOK.store(true);
}

inline void testThreadCanFreeTLCWithoutCooperation()
{
    TestScope canFreeTLCWithoutCooperation(
        "CanFreeTLCWithoutCooperation",
        [] () {
        });

    allowThreadToExit.store(false);
    threadCanDecommit.store(false);
    threadOK.store(false);
    counter.store(0);

    pas_scavenger_suspend();

    int prevCounterValue;
    {
        std::thread busy(uncooperativeThread);
        // The thread grows its TLC before it starts spinning; wait for the first
        // increment so the counter and page-count checks below see the steady state.
        while (!counter.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        prevCounterValue = counter.load();
        CHECK_GREATER(prevCounterValue, 0);

        threadCanDecommit.store(true);
        pas_scavenger_clear_all_caches();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        CHECK_GREATER(counter.load(), prevCounterValue);
        allowThreadToExit.store(true);
        busy.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK_GREATER(counter.load(), prevCounterValue);
    CHECK(threadOK.load());
}

void addThreadSuspendTests()
{
    ADD_TEST(testThreadCanSuspend());
#if !PAS_OS(DARWIN)
    ADD_TEST(testAltSignalStackForcesRetry());
    ADD_TEST(testRegisterCaptureSPInBounds());
#endif
    ADD_TEST(testThreadCanFreeTLCWithoutCooperation());
}

#else

void addThreadSuspendTests()
{
    pas_log("BMALLOC is not enabled, so skipping thread suspend tests.\n");
}

#endif
