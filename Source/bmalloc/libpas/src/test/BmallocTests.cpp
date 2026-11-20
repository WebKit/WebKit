/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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
#include "bmalloc_heap_innards.h"
#include "pas_internal_config.h"
#include "bmalloc_heap_utils.h"
#include "bmalloc_prefault_supply.h"
#include "pas_committed_pages_vector.h"
#include "pas_heap.h"
#include "pas_large_heap.h"
#include "pas_page_malloc.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_scavenger.h"
#include "tagged_bmalloc_heap.h"
#include "tagged_bmalloc_heap_config.h"
#include "tagged_bmalloc_heap_utils.h"

#if PAS_OS(DARWIN)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <malloc/malloc.h>
#include <sys/mman.h>
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sched.h>
#include <thread>
#include <vector>

using namespace std;

namespace {

enum class BmallocHeapVariant {
    Untagged,
    Tagged,
};

void testBmallocAllocate(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size);
    };

    void* mem = try_allocate(100);
    CHECK(mem);
}

void testBmallocAllocationZeroing(BmallocHeapVariant variant)
{
    auto try_allocate_zeroed = [variant](size_t size) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_zeroed(size);
        return bmalloc_try_allocate_zeroed(size);
    };
    auto try_allocate_zeroed_with_alignment = [variant](size_t size, size_t alignment) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_zeroed_with_alignment(size, alignment);
        return bmalloc_try_allocate_zeroed_with_alignment(size, alignment);
    };

    auto checkBufferIsZeroed = [](void* buff, size_t size) -> void {
        for (size_t i = 0; i < size; i++) {
            auto* ptr { reinterpret_cast<uint8_t*>(buff) + i };
            uint8_t byte { };
            std::memcpy(&byte, ptr, sizeof(byte));
            CHECK(!byte);
        }
    };

    auto sizes = std::array<size_t, 6> {
        7, 100, 128, 2003, 4096, 1024 * 32
    };
    for (auto size : sizes) {
        void* memA = try_allocate_zeroed(size);
        checkBufferIsZeroed(memA, size);
        void* memB = try_allocate_zeroed_with_alignment(size, 1);
        checkBufferIsZeroed(memB, size);
        void* memC = try_allocate_zeroed_with_alignment(size, 64);
        checkBufferIsZeroed(memC, size);
    }
}

void testBmallocAllocationAlignment(BmallocHeapVariant variant)
{
    auto try_allocate_with_alignment = [variant](size_t size, size_t alignment) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_with_alignment(size, alignment);
        return bmalloc_try_allocate_with_alignment(size, alignment);
    };
    auto try_allocate_zeroed_with_alignment = [variant](size_t size, size_t alignment) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_zeroed_with_alignment(size, alignment);
        return bmalloc_try_allocate_zeroed_with_alignment(size, alignment);
    };

    auto checkBufferIsAligned = [](void* buff, size_t alignment)  {
        auto buffAddr { reinterpret_cast<uintptr_t>(buff) };
        CHECK(!(buffAddr % alignment));
    };

    auto sizes = std::array<size_t, 7> {
        7, 100, 128, 2003, 4096, 1024 * 32, 2 * PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
    };
    auto alignments = std::array<size_t, 5> {
        1, 8, 128, 1024, 4096
    };
    for (auto size : sizes) {
        for (auto align : alignments) {
            void* memA = try_allocate_with_alignment(size, align);
            checkBufferIsAligned(memA, align);
            void* memB = try_allocate_zeroed_with_alignment(size, align);
            checkBufferIsAligned(memB, align);
        }
    }
}


void testBmallocDeallocate(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size);
    };
    auto deallocate = [variant](void* ptr) {
        if (variant == BmallocHeapVariant::Tagged)
            tagged_bmalloc_deallocate(ptr);
        else
            bmalloc_deallocate(ptr);
    };

    void* mem = try_allocate(100);
    CHECK(mem);
    deallocate(mem);
}

void testBmallocForceBitfitAfterAlloc(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size);
    };

    auto& intrinsic_runtime_config = (variant == BmallocHeapVariant::Tagged)
        ? tagged_bmalloc_intrinsic_runtime_config
        : bmalloc_intrinsic_runtime_config;
    auto& primitive_runtime_config = (variant == BmallocHeapVariant::Tagged)
        ? tagged_bmalloc_primitive_runtime_config
        : bmalloc_primitive_runtime_config;

    void* mem0 = try_allocate(28616);
    CHECK(mem0);

    void* mem1 = try_allocate(20768);
    CHECK(mem1);

    // Simulate entering mini mode by forcing bitfit only.
    intrinsic_runtime_config.base.max_segregated_object_size = 0;
    intrinsic_runtime_config.base.max_bitfit_object_size = UINT_MAX;
    primitive_runtime_config.base.max_segregated_object_size = 0;
    primitive_runtime_config.base.max_bitfit_object_size = UINT_MAX;

    void* mem2 = try_allocate(20648);
    CHECK(mem2);
}

void testBmallocDisableAllocationsAboveMTETaggingCeiling(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size);
    };

    auto do_allocate_and_check = [&]() {
        const std::array<size_t, 8> sizes = {
            4096,
            8,
            743,
            PAS_SMALL_PAGE_DEFAULT_SIZE,
            PAS_SMALL_PAGE_DEFAULT_SIZE * 2,
            PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
            PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE + 1,
            PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 4
        };
        for (auto size : sizes) {
            void* mem = try_allocate(size);
            CHECK(mem);
        }
    };

    do_allocate_and_check();

    // Simulate the effects of MTE enablement by forcing larger allocations
    // into the large heap or system heap
    pas_mte_force_nontaggable_user_allocations_into_large_heap();

    do_allocate_and_check();
}

void testBmallocSmallIndexOverlap(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size);
    };
    auto try_allocate_with_alignment = [variant](size_t size, size_t alignment) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_with_alignment(size, alignment);
        return bmalloc_try_allocate_with_alignment(size, alignment);
    };

    // object_size = 16 * index for this heap.
    // Creates directory A with min_index = 97, object_size = 1616
    void* mem0 = try_allocate(1552);
    CHECK(mem0);
    // Extends directory A to have min_index = 96, object_size = 1616
    void* mem1 = try_allocate(1536);
    CHECK(mem1);
    // Install index is 94. Directory A is a "candidate" but doesn't satisfy alignment,
    // so new directory B is created with min_index = 94, object_size = 1536.
    // Directory B overlaps directory A at index 96 (1536 / 16).
    void* mem2 = try_allocate_with_alignment(1504, 32);
    CHECK(mem2);
}

// tagged_bmalloc_owns_object is how the bmalloc realloc path recovers which heap an object
// came from when the caller's arguments can't say (see reallocOutOfLine). Both heap configs
// exist regardless of whether MTE is enabled at runtime, so this is testable anywhere.
void testBmallocOwnsObject(BmallocHeapVariant variant)
{
    const bool expected = variant == BmallocHeapVariant::Tagged;

    auto try_allocate = [variant](size_t size) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size);
    };

    // Sizes chosen to land in each of the three lookups the predicate performs: the megapage
    // table, the page header table, and -- above the taggable ceiling -- the global large map.
    const std::array<size_t, 4> sizes = {
        8,
        743,
        PAS_SMALL_PAGE_DEFAULT_SIZE * 2,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 4,
    };

    for (auto size : sizes) {
        void* mem = try_allocate(size);
        CHECK(mem);
        CHECK_EQUAL(tagged_bmalloc_owns_object(mem), expected);
    }

    // An object from the other heap must not be claimed, and neither must null.
    void* other = (variant == BmallocHeapVariant::Tagged)
        ? bmalloc_try_allocate(743)
        : tagged_bmalloc_try_allocate(743);
    CHECK(other);
    CHECK_EQUAL(tagged_bmalloc_owns_object(other), !expected);
    CHECK(!tagged_bmalloc_owns_object(nullptr));
    // Make sure we don't falsely claim non-nullptr pointers either
    CHECK(!tagged_bmalloc_owns_object((void*)0x100));
}

#if PAS_OS(DARWIN) && PAS_ENABLE_TESTING
// Large user allocations are handed off to the system malloc once they exceed the MTE tagging
// ceiling, for heap configs that opt into it via delegate_large_user_allocations.
// malloc_zone_from_ptr is the ground truth for "did this come from libmalloc": libpas registers a
// zone, but its size hook always reports 0 ("we tell libmalloc that we own no pointers"), so a
// non-null zone means the pointer really is the system malloc's.
//
// Delegation normally also requires MTE to be enabled and hardened and a real bmalloc SystemHeap to
// be installed, neither of which a standalone libpas test can arrange, so we force those with
// pas_large_object_delegation_is_enabled_override. The size threshold and the per-config opt-in still apply,
// and those are what this test is about.
void testBmallocLargeObjectDelegation(BmallocHeapVariant variant)
{
    const bool tagged = variant == BmallocHeapVariant::Tagged;

    // Read the opt-in from the config rather than hardcoding which heap delegates, so this keeps
    // testing the size threshold even if the opt-in moves between heaps.
    const bool expectDelegation =
        (tagged ? tagged_bmalloc_heap_config : bmalloc_heap_config).delegate_large_user_allocations;

    auto try_allocate = [tagged](size_t size) -> void* {
        if (tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size);
    };
    auto deallocate = [tagged](void* ptr) {
        if (tagged)
            tagged_bmalloc_deallocate(ptr);
        else
            bmalloc_deallocate(ptr);
    };
    auto isFromSystemMalloc = [](void* ptr) -> bool {
        return !!malloc_zone_from_ptr(ptr);
    };

    // Delegation is only reachable from the large heap, and an allocation only reaches the large
    // heap once it exceeds the segregated/bitfit directory ceiling. This is what clamps that ceiling
    // to the tagging ceiling, for every bmalloc heap, so that "above
    // PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE" really does mean "goes large". It is what runs for real when
    // MTE comes up hardened.
    pas_mte_force_nontaggable_user_allocations_into_large_heap();

    pas_large_object_delegation_is_enabled_override = true;

    // At or below the tagging ceiling nothing is delegated, whichever heap we ask. The clamp above
    // leaves the directory ceiling at exactly PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE, and an allocation
    // only goes large when it is strictly greater, so that size is still served by libpas.
    const std::array<size_t, 6> undelegatedSizes = {
        8,
        743,
        4096,
        PAS_SMALL_PAGE_DEFAULT_SIZE,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE / 2,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
    };
    for (auto size : undelegatedSizes) {
        void* mem = try_allocate(size);
        CHECK(mem);
        CHECK(!isFromSystemMalloc(mem));
        deallocate(mem);
    }

    // Above the ceiling, delegation follows the config's opt-in.
    const std::array<size_t, 5> delegatedSizes = {
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE + 1,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE + PAS_SMALL_PAGE_DEFAULT_SIZE,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 2,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 4,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 17,
    };
    for (auto size : delegatedSizes) {
        void* mem = try_allocate(size);
        CHECK(mem);
        CHECK_EQUAL(isFromSystemMalloc(mem), expectDelegation);
        // Also covers the delegated free path, which has to route the object back to the system
        // malloc rather than into the large free heap.
        deallocate(mem);
    }

    // The two heap-ref heaps that the reserved-memory flag has to tell apart. Both live in the
    // untagged config, so only check them there.
    if (!tagged) {
        // The compact primitive heap delegates like any other heap: compactness only requires that
        // the address carry no MTE tag, and this config's memory flavor is plain, so
        // delegated_allocate_impl asks the system malloc for canonically-tagged memory.
        for (auto size : delegatedSizes) {
            void* mem = bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, size);
            CHECK(mem);
            CHECK(isFromSystemMalloc(mem));
            bmalloc_deallocate(mem);
        }

        // A heap forced into a caller-supplied reservation -- what Gigacage is -- must never be
        // delegated, however large the object: the whole point of the reservation is that its
        // addresses lie inside it, and the system malloc cannot promise that.
        static const bmalloc_type gigacageType = BMALLOC_TYPE_INITIALIZER(1, 1, "Test Gigacage");
        static pas_primitive_heap_ref gigacageHeapRef =
            BMALLOC_AUXILIARY_HEAP_REF_INITIALIZER(&gigacageType);

        // Reserved with mmap rather than malloc, as the real Gigacage is: a malloc'd reservation
        // would make libmalloc claim the interior pointers we are about to check.
        const size_t reservationSize = 32 * 1024 * 1024;
        void* reservation = mmap(nullptr, reservationSize, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON, -1, 0);
        CHECK(reservation != MAP_FAILED);
        const uintptr_t reservationBegin = reinterpret_cast<uintptr_t>(reservation);
        const uintptr_t reservationEnd = reservationBegin + reservationSize;

        bmalloc_force_auxiliary_heap_into_reserved_memory(
            &gigacageHeapRef, reservationBegin, reservationEnd);

        for (auto size : delegatedSizes) {
            void* mem = bmalloc_try_allocate_auxiliary(&gigacageHeapRef, size);
            CHECK(mem);
            // The real invariant: still inside the reservation.
            const uintptr_t address = reinterpret_cast<uintptr_t>(mem);
            CHECK_GREATER_EQUAL(address, reservationBegin);
            CHECK_LESS(address, reservationEnd);
            CHECK(!isFromSystemMalloc(mem));
        }
    }

    // Clearing the override must stop delegation, so a large allocation comes from libpas again.
    pas_large_object_delegation_is_enabled_override = false;
    void* undelegated = try_allocate(PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 2);
    CHECK(undelegated);
    CHECK(!isFromSystemMalloc(undelegated));
    deallocate(undelegated);
}
#endif // PAS_OS(DARWIN) && PAS_ENABLE_TESTING


// Test that the bmalloc-size lookup works for objects allocated in any one of bmalloc's
// individual heap configs.
void testBmallocMallocSizeAcrossHeaps(BmallocHeapVariant variant)
{
    const bool tagged = variant == BmallocHeapVariant::Tagged;

    auto try_allocate = [tagged](size_t size) -> void* {
        if (tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size);
    };
    auto deallocate = [tagged](void* ptr) {
        if (tagged)
            tagged_bmalloc_deallocate(ptr);
        else
            bmalloc_deallocate(ptr);
    };

    // Spans the small/medium segregated and bitfit size classes plus the large heap, so each of the
    // lookups the size query can bottom out in is covered.
    const std::array<size_t, 6> sizes = {
        8,
        743,
        4096,
        PAS_SMALL_PAGE_DEFAULT_SIZE,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 4,
    };

    for (auto size : sizes) {
        void* mem = try_allocate(size);
        CHECK(mem);
        // Must be usable, i.e. at least what was asked for, and never zero.
        CHECK_GREATER_EQUAL(bmalloc_get_allocation_size(mem), size);
        deallocate(mem);
    }

    // The compact heap is a third pas_heap, in the untagged config.
    for (auto size : sizes) {
        void* mem = bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, size);
        CHECK(mem);
        CHECK_GREATER_EQUAL(bmalloc_get_allocation_size(mem), size);
        bmalloc_deallocate(mem);
    }
}

unsigned activeFaultInCount()
{
    return __atomic_load_n(&bmalloc_prefault_supply_num_active_fault_ins.value, __ATOMIC_ACQUIRE);
}

void waitForFaultInRequestsToBeServed()
{
    auto deadline = chrono::steady_clock::now() + chrono::seconds(30);
    while (activeFaultInCount() && chrono::steady_clock::now() < deadline)
        sched_yield();
    CHECK_EQUAL(activeFaultInCount(), 0u);
}

void testFaultInPreservesContents()
{
    size_t size = 4 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
    uint8_t* memory = static_cast<uint8_t*>(bmalloc_try_allocate_auxiliary_with_background_prefault(&bmalloc_compact_primitive_heap_ref, size));
    CHECK(memory);
    for (size_t index = 0; index < size; ++index)
        memory[index] = static_cast<uint8_t>(index * 7 + 3);
    waitForFaultInRequestsToBeServed();

    for (size_t index = 0; index < size; ++index)
        CHECK_EQUAL(memory[index], static_cast<uint8_t>(index * 7 + 3));
    bmalloc_deallocate(memory);
}

#if PAS_PAGE_MALLOC_CAN_FAULT_IN_WITHOUT_WRITING
// Only a Linux kernel before 5.14, which predates MADV_POPULATE_WRITE even when the headers define
// it, has no way to fault the pages in.
bool faultInIsSupportedByTheKernel()
{
    size_t page = pas_page_malloc_alignment();
    void* probe = mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    CHECK(probe != MAP_FAILED);
    bool isSupported = pas_page_malloc_fault_in_without_writing(probe, page, pas_memory_flavor_plain);
    munmap(probe, page);
    return isSupported;
}
#endif

void testFaultInFaultsInOnlyTheRequestedPages()
{
#if PAS_PAGE_MALLOC_CAN_FAULT_IN_WITHOUT_WRITING
    if (!faultInIsSupportedByTheKernel())
        return;

    size_t page = pas_page_malloc_alignment();
    size_t size = 8 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
    size_t offset = 2 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
    void* memory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    CHECK(memory != MAP_FAILED);
#if PAS_OS(LINUX)
    // A transparent huge page straddling offset would fault in pages on both sides of it.
    madvise(memory, size, MADV_NOHUGEPAGE);
#endif
    CHECK_EQUAL(pas_count_committed_pages(memory, size, &allocationConfig), 0u);

    bmalloc_prefault_supply_request_fault_in_for_testing(static_cast<char*>(memory) + offset, size - offset);
    waitForFaultInRequestsToBeServed();

    CHECK_EQUAL(pas_count_committed_pages(memory, offset, &allocationConfig), 0u);
    CHECK_EQUAL(pas_count_committed_pages(static_cast<char*>(memory) + offset, size - offset, &allocationConfig), (size - offset) / page);
    bmalloc_prefault_supply_cancel_fault_in(static_cast<char*>(memory) + offset);
    munmap(memory, size);
#endif
}

void testFaultInSkipsSmallAllocations()
{
    void* memory = bmalloc_try_allocate_auxiliary_with_background_prefault(&bmalloc_compact_primitive_heap_ref, BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE - 1);
    CHECK(memory);
    CHECK_EQUAL(activeFaultInCount(), 0u);
    bmalloc_deallocate(memory);
}

void testFaultInCancellationOfNothingLeavesCountAlone()
{
    waitForFaultInRequestsToBeServed();
    bmalloc_prefault_supply_cancel_fault_in_slow(nullptr);
    CHECK_EQUAL(activeFaultInCount(), 0u);
}

void testFaultInIsCancelledByFree()
{
    for (unsigned iteration = 0; iteration < 100; ++iteration) {
        size_t size = (1 + iteration % 4) * 4 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
        void* memory = bmalloc_try_allocate_auxiliary_with_background_prefault(&bmalloc_compact_primitive_heap_ref, size);
        CHECK(memory);
        bmalloc_deallocate(memory);
        CHECK_EQUAL(bmalloc_prefault_supply_queued_fault_in_count(), 0u);
        // The thread may have picked the slot just before the free cleared it, and skips the
        // chunk once it notices.
        waitForFaultInRequestsToBeServed();
    }
}

void testFaultInIsCancelledByReallocate()
{
    for (unsigned iteration = 0; iteration < 32; ++iteration) {
        size_t size = 4 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
        char* memory = static_cast<char*>(bmalloc_try_allocate_auxiliary_with_background_prefault(&bmalloc_compact_primitive_heap_ref, size));
        CHECK(memory);
        memory[0] = 'x';
        size_t newSize = iteration % 2 ? 2 * size : iteration + 1;
        char* newMemory = static_cast<char*>(bmalloc_try_reallocate_auxiliary(memory, &bmalloc_compact_primitive_heap_ref, newSize, pas_reallocate_free_if_successful));
        CHECK(newMemory);
        CHECK_EQUAL(newMemory[0], 'x');
        CHECK_EQUAL(bmalloc_prefault_supply_queued_fault_in_count(), 0u);
        waitForFaultInRequestsToBeServed();
        bmalloc_deallocate(newMemory);
    }
}

void testFaultInOfPGMAllocationIsCancelled()
{
#if PAS_PAGE_MALLOC_CAN_FAULT_IN_WITHOUT_WRITING
    if (!faultInIsSupportedByTheKernel())
        return;

    pas_probabilistic_guard_malloc_initialize_pgm_as_enabled(1);

    // Untouched memory queued after the allocation, so that the thread, serving the newest first,
    // is likely to stay busy with it and leave the allocation's own fault-in queued until the free
    // withdraws it.
    size_t blockerSize = 256 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;

    for (unsigned iteration = 0; iteration < 8; ++iteration) {
        size_t size = 4 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
        char* memory = static_cast<char*>(bmalloc_try_allocate_auxiliary_with_background_prefault(&bmalloc_compact_primitive_heap_ref, size));
        CHECK(memory);
        CHECK(pas_check_pgm_entry_exists(memory));
        void* blocker = mmap(nullptr, blockerSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        CHECK(blocker != MAP_FAILED);
        bmalloc_prefault_supply_request_fault_in_for_testing(blocker, blockerSize);

        memory[0] = 'x';
        if (iteration % 2) {
            // Reallocating frees the source through pas_try_deallocate_pgm_large.
            char* newMemory = static_cast<char*>(bmalloc_try_reallocate_auxiliary(memory, &bmalloc_compact_primitive_heap_ref, 2 * size, pas_reallocate_free_if_successful));
            CHECK(newMemory);
            CHECK_EQUAL(newMemory[0], 'x');
            CHECK(!bmalloc_prefault_supply_fault_in_is_queued_for_testing(memory));
            memory = newMemory;
        } else {
            bmalloc_deallocate(memory);
            CHECK(!bmalloc_prefault_supply_fault_in_is_queued_for_testing(memory));
            memory = nullptr;
        }

        bmalloc_prefault_supply_cancel_fault_in(blocker);
        waitForFaultInRequestsToBeServed();
        munmap(blocker, blockerSize);
        if (memory)
            bmalloc_deallocate(memory);
    }
#endif
}

void testFaultInQueueOverflow()
{
    constexpr unsigned numAllocations = 40;
    size_t size = BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
    std::array<void*, numAllocations> allocations;
    for (auto& memory : allocations) {
        memory = bmalloc_try_allocate_auxiliary_with_background_prefault(&bmalloc_compact_primitive_heap_ref, size);
        CHECK(memory);
    }
    CHECK_LESS_EQUAL(bmalloc_prefault_supply_queued_fault_in_count(), 16u);
    for (void* memory : allocations)
        bmalloc_deallocate(memory);
    CHECK_EQUAL(bmalloc_prefault_supply_queued_fault_in_count(), 0u);
    waitForFaultInRequestsToBeServed();
}

// Frees race the thread's chunks and each other's evictions, and unrelated large frees run the
// lock-free check the whole time.
void testFaultInConcurrentCancellation()
{
    constexpr unsigned numThreads = 4;
    constexpr unsigned numIterations = 200;
    std::atomic<bool> isDone { false };

    std::thread unrelatedFreer([&] {
        while (!isDone.load()) {
            void* memory = bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, 2 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE);
            CHECK(memory);
            static_cast<char*>(memory)[0] = 'u';
            bmalloc_deallocate(memory);
        }
    });

    std::vector<std::thread> threads;
    for (unsigned threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
        threads.emplace_back([threadIndex] {
            std::vector<std::pair<char*, size_t>> held;
            unsigned random = threadIndex * 7919 + 1;
            for (unsigned iteration = 0; iteration < numIterations; ++iteration) {
                random = random * 1103515245 + 12345;
                size_t size = (1 + (random >> 16) % 6) * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
                char* memory = static_cast<char*>(bmalloc_try_allocate_auxiliary_with_background_prefault(&bmalloc_compact_primitive_heap_ref, size));
                CHECK(memory);
                memory[0] = static_cast<char>(iteration);
                memory[size - 1] = static_cast<char>(iteration + 1);

                switch ((random >> 24) % 3) {
                case 0:
                    bmalloc_deallocate(memory);
                    break;
                case 1: {
                    size_t newSize = (random >> 28) % 2 ? 2 * size : 4096;
                    char* newMemory = static_cast<char*>(bmalloc_try_reallocate_auxiliary(memory, &bmalloc_compact_primitive_heap_ref, newSize, pas_reallocate_free_if_successful));
                    CHECK(newMemory);
                    CHECK_EQUAL(newMemory[0], static_cast<char>(iteration));
                    bmalloc_deallocate(newMemory);
                    break;
                }
                default:
                    held.emplace_back(memory, size);
                    break;
                }

                if (held.size() > 6) {
                    for (auto& [heldMemory, heldSize] : held) {
                        CHECK_EQUAL(static_cast<char>(heldMemory[heldSize - 1] - heldMemory[0]), static_cast<char>(1));
                        bmalloc_deallocate(heldMemory);
                    }
                    held.clear();
                }
            }
            for (auto& [heldMemory, heldSize] : held)
                bmalloc_deallocate(heldMemory);
        });
    }
    for (auto& thread : threads)
        thread.join();
    isDone.store(true);
    unrelatedFreer.join();

    CHECK_EQUAL(bmalloc_prefault_supply_queued_fault_in_count(), 0u);
    waitForFaultInRequestsToBeServed();
}

#if PAS_OS(DARWIN)
size_t countPagesWithDisposition(void* memory, size_t size, int disposition)
{
    vector<int> dispositions(size / pas_page_malloc_alignment());
    mach_vm_size_t count = dispositions.size();
    CHECK_EQUAL(mach_vm_page_range_query(mach_task_self(), reinterpret_cast<mach_vm_address_t>(memory), size, reinterpret_cast<mach_vm_address_t>(dispositions.data()), &count), KERN_SUCCESS);
    size_t result = 0;
    for (mach_vm_size_t index = 0; index < count; ++index) {
        if (dispositions[index] & disposition)
            ++result;
    }
    return result;
}

// Memory the heap hands out again is usually resident, and the first write to a resident page still
// traps if a decommit left it reusable or if the kernel has aged it, so faulting in has to undo both.
void testFaultInTakesBackDecommittedPages()
{
    size_t size = 4 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
    void* memory = bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, size);
    CHECK(memory);
    memset(memory, 0x55, size);
    bmalloc_deallocate(memory);
    pas_scavenger_decommit_free_memory();

    void* reused = bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, size);
    CHECK(reused);
    CHECK_GREATER(countPagesWithDisposition(reused, size, VM_PAGE_QUERY_PAGE_REUSABLE), 0ul);

    bmalloc_prefault_supply_request_fault_in(reused, size);
    waitForFaultInRequestsToBeServed();
    CHECK_EQUAL(countPagesWithDisposition(reused, size, VM_PAGE_QUERY_PAGE_REUSABLE), 0ul);
    bmalloc_deallocate(reused);
}

void testFaultInMarksAgedPagesInUse()
{
    size_t size = 4 * BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE;
    size_t numPages = size / pas_page_malloc_alignment();
    void* memory = bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, size);
    CHECK(memory);
    CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(memory), pas_page_malloc_alignment()));
    memset(memory, 0x55, size);
    // On Darwin this ages the pages, keeping their contents, as the kernel does to memory left alone.
    madvise(memory, size, MADV_DONTNEED);
    CHECK_LESS(countPagesWithDisposition(memory, size, VM_PAGE_QUERY_PAGE_REF), numPages);

    bmalloc_prefault_supply_request_fault_in(memory, size);
    waitForFaultInRequestsToBeServed();
    CHECK_EQUAL(countPagesWithDisposition(memory, size, VM_PAGE_QUERY_PAGE_REF), numPages);
    bmalloc_deallocate(memory);
}
#endif

} // anonymous namespace

void addBmallocTests()
{
    for (auto variant : { BmallocHeapVariant::Untagged, BmallocHeapVariant::Tagged }) {
        ADD_TEST(testBmallocAllocate(variant));
        ADD_TEST(testBmallocDeallocate(variant));
        ADD_TEST(testBmallocAllocationZeroing(variant));
        ADD_TEST(testBmallocAllocationAlignment(variant));
        ADD_TEST(testBmallocForceBitfitAfterAlloc(variant));
        ADD_TEST(testBmallocDisableAllocationsAboveMTETaggingCeiling(variant));
        ADD_TEST(testBmallocSmallIndexOverlap(variant));
        ADD_TEST(testBmallocOwnsObject(variant));
        ADD_TEST(testBmallocMallocSizeAcrossHeaps(variant));
#if PAS_OS(DARWIN) && PAS_ENABLE_TESTING
        ADD_TEST(testBmallocLargeObjectDelegation(variant));
#endif
    }
    ADD_TEST(testFaultInPreservesContents());
    ADD_TEST(testFaultInFaultsInOnlyTheRequestedPages());
    ADD_TEST(testFaultInSkipsSmallAllocations());
    ADD_TEST(testFaultInCancellationOfNothingLeavesCountAlone());
    ADD_TEST(testFaultInIsCancelledByFree());
    ADD_TEST(testFaultInIsCancelledByReallocate());
    ADD_TEST(testFaultInOfPGMAllocationIsCancelled());
    ADD_TEST(testFaultInQueueOverflow());
    ADD_TEST(testFaultInConcurrentCancellation());
#if PAS_OS(DARWIN)
    ADD_TEST(testFaultInTakesBackDecommittedPages());
    ADD_TEST(testFaultInMarksAgedPagesInUse());
#endif
}
