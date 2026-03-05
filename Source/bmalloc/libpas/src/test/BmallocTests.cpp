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
#include "pas_internal_config.h"

#include <array>
#include <cstdlib>

using namespace std;

namespace {

void testBmallocAllocate()
{
    void* mem = bmalloc_try_allocate(100, pas_non_compact_allocation_mode);
    CHECK(mem);
}

void testBmallocAllocationZeroing()
{
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
    auto allocationModes = std::array<pas_allocation_mode, 3> {
        pas_non_compact_allocation_mode,
        pas_maybe_compact_allocation_mode,
        pas_always_compact_allocation_mode,
    };
    for (auto size : sizes) {
        for (auto mode : allocationModes) {
            void* memA = bmalloc_try_allocate_zeroed(size, mode);
            checkBufferIsZeroed(memA, size);
            void* memB = bmalloc_try_allocate_zeroed_with_alignment(size, 1, mode);
            checkBufferIsZeroed(memB, size);
            void* memC = bmalloc_try_allocate_zeroed_with_alignment(size, 64, mode);
            checkBufferIsZeroed(memC, size);
        }
    }
}

void testBmallocAllocationAlignment()
{
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
    auto allocationModes = std::array<pas_allocation_mode, 3> {
        pas_non_compact_allocation_mode,
        pas_maybe_compact_allocation_mode,
        pas_always_compact_allocation_mode,
    };
    for (auto size : sizes) {
        for (auto align : alignments) {
            for (auto mode : allocationModes) {
                void* memA = bmalloc_try_allocate_with_alignment(size, align, mode);
                checkBufferIsAligned(memA, align);
                void* memB = bmalloc_try_allocate_zeroed_with_alignment(size, align, mode);
                checkBufferIsAligned(memB, align);
            }
        }
    }
}


void testBmallocDeallocate()
{
    void* mem = bmalloc_try_allocate(100, pas_non_compact_allocation_mode);
    CHECK(mem);
    bmalloc_deallocate(mem);
}

void testBmallocForceBitfitAfterAlloc()
{
    void* mem0 = bmalloc_try_allocate(28616, pas_non_compact_allocation_mode);
    CHECK(mem0);

    void* mem1 = bmalloc_try_allocate(20768, pas_non_compact_allocation_mode);
    CHECK(mem1);

    // Simulate entering mini mode by forcing bitfit only.
    bmalloc_intrinsic_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_intrinsic_runtime_config.base.max_bitfit_object_size = UINT_MAX;
    bmalloc_primitive_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_primitive_runtime_config.base.max_bitfit_object_size = UINT_MAX;

    void* mem2 = bmalloc_try_allocate(20648, pas_non_compact_allocation_mode);
    CHECK(mem2);
}

void testBmallocDisableAllocationsAboveMTETaggingCeiling()
{
    auto do_allocate_and_check = [](pas_allocation_mode mode) {
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
            void* mem = bmalloc_try_allocate(size, mode);
            CHECK(mem);
        }
    };

    do_allocate_and_check(pas_non_compact_allocation_mode);
    do_allocate_and_check(pas_always_compact_allocation_mode);

    // Simulate the effects of MTE enablement by forcing larger allocations
    // into the large heap or system heap
    pas_mte_force_nontaggable_user_allocations_into_large_heap();

    do_allocate_and_check(pas_non_compact_allocation_mode);
    do_allocate_and_check(pas_always_compact_allocation_mode);
}

void testBmallocSmallIndexOverlap()
{
    // object_size = 16 * index for this heap.
    // Creates directory A with min_index = 97, object_size = 1616
    void* mem0 = bmalloc_try_allocate(1552, pas_non_compact_allocation_mode);
    CHECK(mem0);
    // Extends directory A to have min_index = 96, object_size = 1616
    void* mem1 = bmalloc_try_allocate(1536, pas_non_compact_allocation_mode);
    CHECK(mem1);
    // Install index is 94. Directory A is a "candidate" but doesn't satisfy alignment,
    // so new directory B is created with min_index = 94, object_size = 1536.
    // Directory B overlaps directory A at index 96 (1536 / 16).
    void* mem2 = bmalloc_try_allocate_with_alignment(1504, 32, pas_non_compact_allocation_mode);
    CHECK(mem2);
}

} // anonymous namespace

void addBmallocTests()
{
    ADD_TEST(testBmallocAllocate());
    ADD_TEST(testBmallocDeallocate());
    ADD_TEST(testBmallocAllocationZeroing());
    ADD_TEST(testBmallocAllocationAlignment());
    ADD_TEST(testBmallocForceBitfitAfterAlloc());
    ADD_TEST(testBmallocDisableAllocationsAboveMTETaggingCeiling());
    ADD_TEST(testBmallocSmallIndexOverlap());
}
