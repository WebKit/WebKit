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

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <set>
#include <span>
#include <vector>

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

    // The argument semantics of `malloc_logger_t` are quite messy and "defined" by the MSL
    // and libmalloc implementations. Use these readable names in checks.
    bool isAlloc() const { return type == pas_stack_logging_type_alloc; }
    bool isDealloc() const { return type == pas_stack_logging_type_dealloc; }
    malloc_zone_t* zone() const { return (malloc_zone_t*)arg1; }
    size_t requestedSize() const { CHECK(isAlloc()); return arg2; }
    void* allocatedBlock() const { CHECK(isAlloc()); return (void*)result; }
    void* blockToFree() const { CHECK(isDealloc()); return (void*)arg2; }

    // Extra info
    pas_object_kind objectKind;
};

// The logger is called from inside malloc, so it must not allocate and must not CHECK.
// On Darwin malloc_logger is libmalloc's global, which means installing it also hands us
// every system malloc in the process, from every thread. Use fixed storage and avoid
// anything that allocates, including CHECK_* macros.
constexpr unsigned maxEvents = 512;
Event recordedEvents[maxEvents];
std::atomic<size_t> recordedEventCount;

void recordEvent(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                 uintptr_t result, uint32_t numHotFramesToSkip)
{
    if (type != pas_stack_logging_type_alloc && type != pas_stack_logging_type_dealloc)
        return;
    if (arg1)
        return;

    size_t idx = recordedEventCount.fetch_add(1, std::memory_order_relaxed);
    if (idx >= maxEvents)
        return;

    void* ptr = (void *)(type == pas_stack_logging_type_alloc ? result : arg2);
    pas_object_kind objectKind = pas_get_object_kind(ptr, bmalloc_heap_config);

    recordedEvents[idx] = { type, arg1, arg2, arg3, result, numHotFramesToSkip, objectKind };
}

void startRecording()
{
    CHECK_EQUAL(malloc_logger, nullptr);
    recordedEventCount.store(0, std::memory_order_relaxed);
    malloc_logger = recordEvent;
}

void stopRecording()
{
    CHECK_EQUAL(malloc_logger, recordEvent);
    malloc_logger = nullptr;
}

// Only safe once recording has stopped: this allocates.
std::vector<Event> recordedEventsFor(const std::set<void*>& ptrs, size_t expectedCount)
{
    CHECK(!malloc_logger);
    CHECK(std::ranges::all_of(ptrs, [](void* p) {
        return p != nullptr;
    }));

    size_t count = recordedEventCount.load(std::memory_order_relaxed);
    CHECK_LESS_EQUAL(count, maxEvents);

    std::vector<Event> events;
    for (const Event& e : std::span(recordedEvents, count)) {
        if (e.isAlloc() && ptrs.contains(e.allocatedBlock()))
            events.push_back(e);
        else if (e.isDealloc() && ptrs.contains(e.blockToFree()))
            events.push_back(e);
    }

    CHECK_EQUAL(events.size(), expectedCount);
    return events;
}

std::vector<Event> recordedEventsFor(void* ptr, size_t expectedCount)
{
    return recordedEventsFor(std::set { ptr }, expectedCount);
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

// pas_msl_malloc_logging_slow() reports (type_alloc, 0, size, 0, ptr, 0). The size is the
// requested size.
void checkAlloc(const Event& event, void* ptr, size_t requestedSize)
{
    CHECK_EQUAL(event.isAlloc(), true);
    CHECK_EQUAL(event.zone(), nullptr);
    CHECK_EQUAL(event.requestedSize(), requestedSize);
    CHECK_EQUAL(event.arg3, 0);
    CHECK_EQUAL(event.allocatedBlock(), ptr);
    CHECK_EQUAL(event.numHotFramesToSkip, 0);
    CHECK_NOT_EQUAL(event.objectKind, pas_not_an_object_kind);
}

// pas_msl_free_logging_slow() reports (type_dealloc, 0, ptr, 0, 0, 0).
void checkDealloc(const Event& event, void* ptr)
{
    CHECK_EQUAL(event.isDealloc(), true);
    CHECK_EQUAL(event.zone(), nullptr);
    CHECK_EQUAL(event.blockToFree(), ptr);
    CHECK_EQUAL(event.arg3, 0);
    CHECK_EQUAL(event.result, 0);
    CHECK_EQUAL(event.numHotFramesToSkip, 0);
    CHECK_NOT_EQUAL(event.objectKind, pas_not_an_object_kind);
}

pas_object_kind allocateAndDeallocate(size_t requestedSize)
{
    startRecording();
    void* ptr = bmalloc_try_allocate(requestedSize, pas_non_compact_allocation_mode);
    bmalloc_deallocate(ptr);
    stopRecording();

    CHECK(ptr);
    // Nothing along the way should have installed a TLC; if it had, the callouts we are
    // about to look for would have been skipped.
    CHECK(!pas_thread_local_cache_try_get());

    auto events = recordedEventsFor(ptr, /*expectedCount=*/2);
    checkAlloc(events[0], ptr, requestedSize);
    checkDealloc(events[1], ptr);

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
    const size_t oldSize = 32, newSize = 1024 * 1024;

    startRecording();
    void* oldPtr = bmalloc_try_allocate(oldSize, pas_non_compact_allocation_mode);
    void* newPtr = bmalloc_try_reallocate(oldPtr, newSize, pas_non_compact_allocation_mode,
                                          pas_reallocate_free_always);
    bmalloc_deallocate(newPtr);
    stopRecording();

    CHECK(oldPtr);
    CHECK(newPtr);
    CHECK_NOT_EQUAL(newPtr, oldPtr);

    auto events = recordedEventsFor({ oldPtr, newPtr }, /*expectedCount=*/4);
    checkAlloc(events[0], oldPtr, oldSize);
    checkAlloc(events[1], newPtr, newSize);
    checkDealloc(events[2], oldPtr);
    checkDealloc(events[3], newPtr);
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
    void* ptr = bmalloc_try_allocate(100, pas_non_compact_allocation_mode);
    bmalloc_deallocate(ptr);
    stopRecording();

    auto events = recordedEventsFor(ptr, 0);
    CHECK(events.empty());
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
