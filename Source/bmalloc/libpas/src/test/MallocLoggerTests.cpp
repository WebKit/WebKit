/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"
#include "pas_darwin_spi.h"
#include "pas_get_object_kind.h"
#include "pas_malloc_stack_logging.h"
#include "pas_platform.h"
#include "pas_reallocate_free_mode.h"
#include "pas_thread_local_cache.h"

#include <atomic>
#include <cstdlib>
#include <pthread.h>
#include <set>

namespace {

// libpas supports libmalloc's `malloc_logger` hook to allow interested parties to observe
// allocation and deallocation events. See pas_malloc_stack_logging.h for the hook, and
// pas_darwin_spi.h for the event encoding shared with libmalloc.
//
// The callouts only exist on the slow paths. libpas forces (almost) all allocation requests
// to the slow path (via pas_thread_local_cache_can_set()).
//
// Enablement is currently also gated by the MallocStackLogging environment variable, except
// for MallocStackLogging=lite, for which we switch to the system allocator.
//
// Currently Darwin-only:
// pas_thread_local_cache_can_set() only consults pas_msl_is_enabled() under PAS_OS(DARWIN),
// so on other platforms allocations keep taking the (unlogged) fast path.

#if PAS_ENABLE_MALLOC_STACK_LOGGER && PAS_OS(DARWIN)

struct Event {
    // malloc_logger arguments
    uint32_t type;
    uintptr_t arg1;
    uintptr_t arg2;
    uintptr_t arg3;
    uintptr_t result;
    uint32_t numHotFramesToSkip;

    // pas_msl_malloc_logging_slow() reports (type_alloc, zone, size, 0, ptr, 0). The size is the requested size.
    // pas_msl_free_logging_slow() reports (type_dealloc, zone, ptr, 0, 0, 0).

    // The argument semantics of `malloc_logger_t` are quite messy and "defined" by the MSL
    // and libmalloc implementations. Use these readable names in checks.
    bool isAlloc() const { return type & pas_stack_logging_type_alloc; }
    bool isDealloc() const { return type & pas_stack_logging_type_dealloc; }
    malloc_zone_t* zone() const { return (malloc_zone_t*)arg1; }
    size_t requestedSize() const { CHECK(isAlloc()); return arg2; }
    void* allocatedBlock() const { CHECK(isAlloc()); return (void*)result; }
    void* blockToFree() const { CHECK(isDealloc()); return (void*)arg2; }

    // Extra info
    size_t blockSize;
    pas_object_kind objectKind;
};

// The logger is called from inside malloc, so it must not allocate and must not CHECK.
// On Darwin malloc_logger is libmalloc's global, which means installing it also hands us
// every system malloc in the process, from every thread. Use fixed storage and avoid
// anything that allocates, including CHECK_* macros, and filter out events from other
// threads.
constexpr unsigned maxEvents = 16;
Event events[maxEvents];
size_t recordedEventCount;

// Limit recording to the thread that called startRecording().
std::atomic<pthread_t> recordingThread;

bool isRecordingThread()
{
    return pthread_equal(pthread_self(), recordingThread.load(std::memory_order_relaxed));
}

// Implementation details; declaration copied from pas_malloc_stack_logging.c
typedef struct {
    malloc_zone_t zone; /* Must be first, size() casts back to the wrapper. */
    const pas_heap_config* config;
    pas_lock_hold_mode heap_lock_hold_mode;
} pas_msl_zone;

void recordEvent(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                 uintptr_t result, uint32_t numHotFramesToSkip)
{
    if (!isRecordingThread())
        return;

    bool hasZone = (type & pas_stack_logging_flag_zone);
    bool isAlloc = (type & pas_stack_logging_type_alloc);
    bool isDealloc = (type & pas_stack_logging_type_dealloc);

    if (!hasZone)
        return;
    if (!isAlloc && !isDealloc)
        return;

    size_t idx = recordedEventCount++;
    if (idx >= maxEvents)
        return;

    void* ptr = (void *)(isAlloc ? result : arg2);
    malloc_zone_t* zone = (malloc_zone_t*)arg1;
    size_t blockSize = zone->size(zone, ptr);

    pas_msl_zone* pas_zone = (pas_msl_zone*)arg1;
    pas_object_kind objectKind = pas_get_object_kind_with_heap_lock_hold_mode(ptr, *pas_zone->config, pas_zone->heap_lock_hold_mode);

    events[idx] = { type, arg1, arg2, arg3, result, numHotFramesToSkip, blockSize, objectKind };
}

void startRecording()
{
    CHECK_EQUAL(malloc_logger, nullptr);
    recordedEventCount = 0;
    recordingThread.store(pthread_self(), std::memory_order_relaxed);
    malloc_logger = recordEvent;
}

void stopRecording()
{
    CHECK_EQUAL(malloc_logger, recordEvent);
    malloc_logger = nullptr;
    recordingThread.store(nullptr, std::memory_order_relaxed);
    CHECK_LESS(recordedEventCount, maxEvents);
}

// Enable malloc_logger callouts (forcing on the slow path) without needing to enable
// MallocStackLogging in the tests.
void enableMallocLoggerCallouts()
{
    CHECK(!getenv("MallocStackLogging"));
    CHECK(!pas_msl_is_enabled());

    pas_msl_is_enabled_flag_value = pas_msl_is_enabled_flag_enabled;

    CHECK(pas_msl_is_enabled());
    CHECK(!pas_thread_local_cache_can_set());
    // If a TLC already existed we would take the fast path and log nothing, so the rest of
    // the test would be measuring the wrong thing.
    CHECK(!pas_thread_local_cache_try_get());
}

void checkAlloc(const Event& event, void* ptr, size_t requestedSize, size_t blockSize)
{
    CHECK_EQUAL(event.type, pas_stack_logging_flag_zone | pas_stack_logging_type_alloc);
    CHECK_EQUAL(event.isAlloc(), true);
    CHECK_NOT_EQUAL(event.zone(), nullptr);
    CHECK_EQUAL(event.requestedSize(), requestedSize);
    CHECK_EQUAL(event.arg3, 0);
    CHECK_EQUAL(event.allocatedBlock(), ptr);
    CHECK_EQUAL(event.numHotFramesToSkip, 0);
    CHECK_EQUAL(event.blockSize, blockSize);
    CHECK_GREATER_EQUAL(event.blockSize, event.requestedSize());
    CHECK_GREATER_EQUAL(event.blockSize, 16);
    CHECK_EQUAL(event.blockSize % 16, 0);
    CHECK_NOT_EQUAL(event.objectKind, pas_not_an_object_kind);
}

void checkDealloc(const Event& event, const Event& allocationEvent)
{
    CHECK_EQUAL(event.type, pas_stack_logging_flag_zone | pas_stack_logging_type_dealloc);
    CHECK_EQUAL(event.isDealloc(), true);
    CHECK_NOT_EQUAL(event.zone(), nullptr);
    CHECK_EQUAL(event.blockToFree(), allocationEvent.allocatedBlock());
    CHECK_EQUAL(event.arg3, 0);
    CHECK_EQUAL(event.result, 0);
    CHECK_EQUAL(event.numHotFramesToSkip, 0);
    CHECK_EQUAL(event.blockSize, allocationEvent.blockSize);
    CHECK_NOT_EQUAL(event.objectKind, pas_not_an_object_kind);
}

pas_object_kind allocateAndDeallocate(size_t requestedSize)
{
    startRecording();
    void* ptr = bmalloc_try_allocate(requestedSize);
    size_t blockSize = bmalloc_get_allocation_size(ptr);
    bmalloc_deallocate(ptr);
    stopRecording();

    CHECK(ptr);
    // Nothing along the way should have installed a TLC; if it had, the callouts we are
    // about to look for would have been skipped.
    CHECK(!pas_thread_local_cache_try_get());

    CHECK_EQUAL(recordedEventCount, 2);
    checkAlloc(events[0], ptr, requestedSize, blockSize);
    checkDealloc(events[1], events[0]);

    return events[0].objectKind;
}

// The different heaps/size classes have their separate codepath and callouts. We use pas_object_kind
// as a proxy for "coverage" to make sure we cover most of the code paths.
const std::set<pas_object_kind> defaultObjectKinds {
    pas_small_segregated_object_kind,
    pas_medium_segregated_object_kind,
    pas_marge_bitfit_object_kind,
    pas_large_object_kind,
};
const std::set<pas_object_kind> bitfitObjectKinds {
    pas_small_bitfit_object_kind,
    pas_medium_bitfit_object_kind,
    pas_marge_bitfit_object_kind,
    pas_large_object_kind,
};

constexpr size_t requestedSizes[] {
    0,
    1,
    16,
    100,
    1000,
    4000,
    10000,
    50000,
    100000,
    300000,
    1024 * 1024,
    16 * 1024 * 1024
};

void testMallocLoggerCalloutsForSizeClasses(const std::set<pas_object_kind>& expectedObjectKinds)
{
    enableMallocLoggerCallouts();

    std::set<pas_object_kind> objectKinds;
    for (size_t requestedSize : requestedSizes) {
        pas_object_kind kind = allocateAndDeallocate(requestedSize);
        objectKinds.insert(kind);
    }

    CHECK(objectKinds == expectedObjectKinds);
}

void testMallocLoggerReallocationCallouts()
{
    enableMallocLoggerCallouts();

    // Grow enough that the object cannot be reallocated in place, so we get a real
    // allocate-copy-free.
    const size_t oldSize = 30, newSize = (1024 * 1024 - 1);

    startRecording();
    void* oldPtr = bmalloc_try_allocate(oldSize);
    void* newPtr = bmalloc_try_reallocate(oldPtr, newSize, pas_reallocate_free_always);
    bmalloc_deallocate(newPtr);
    stopRecording();

    CHECK(oldPtr);
    CHECK(newPtr);
    CHECK_NOT_EQUAL(newPtr, oldPtr);

    CHECK_EQUAL(recordedEventCount, 4);
    checkAlloc(events[0], oldPtr, oldSize, 32);
    checkAlloc(events[1], newPtr, newSize, 1024 * 1024);
    checkDealloc(events[2], events[0]);
    checkDealloc(events[3], events[1]);
}

// Control: callouts are gated on pas_msl_is_enabled(), not merely on a logger being
// installed.
void testNoMallocLoggerCalloutsWhenMSLIsDisabled()
{
    CHECK(!getenv("MallocStackLogging"));
    pas_msl_is_enabled_flag_value = pas_msl_is_enabled_flag_disabled;
    CHECK(!pas_msl_is_enabled());
    CHECK(pas_thread_local_cache_can_set());

    startRecording();
    void* ptr = bmalloc_try_allocate(100);
    bmalloc_deallocate(ptr);
    stopRecording();

    CHECK(ptr);
    CHECK_EQUAL(recordedEventCount, 0);
}

#endif // PAS_ENABLE_MALLOC_STACK_LOGGER && PAS_OS(DARWIN)

} // anonymous namespace

void addMallocLoggerTests()
{
#if PAS_ENABLE_MALLOC_STACK_LOGGER && PAS_OS(DARWIN)
    ADD_TEST(testMallocLoggerCalloutsForSizeClasses(defaultObjectKinds));
    ADD_TEST(testMallocLoggerReallocationCallouts());
    ADD_TEST(testNoMallocLoggerCalloutsWhenMSLIsDisabled());
    {
        ForceBitfit forceBitfit;
        ADD_TEST(testMallocLoggerCalloutsForSizeClasses(bitfitObjectKinds));
        ADD_TEST(testMallocLoggerReallocationCallouts());
        ADD_TEST(testNoMallocLoggerCalloutsWhenMSLIsDisabled());
    }
#endif
}
