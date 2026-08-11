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
#include "tagged_bmalloc_heap.h"
#include "tagged_bmalloc_heap_config.h"

#include <array>
#include <cstdlib>
#include <cstring>

using namespace std;

namespace {

enum class BmallocHeapVariant {
    Untagged,
    Tagged,
};

void testBmallocAllocate(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size, mode);
    };

    void* mem = try_allocate(100, pas_non_compact_allocation_mode);
    CHECK(mem);
}

void testBmallocAllocationZeroing(BmallocHeapVariant variant)
{
    auto try_allocate_zeroed = [variant](size_t size, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_zeroed(size);
        return bmalloc_try_allocate_zeroed(size, mode);
    };
    auto try_allocate_zeroed_with_alignment = [variant](size_t size, size_t alignment, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_zeroed_with_alignment(size, alignment);
        return bmalloc_try_allocate_zeroed_with_alignment(size, alignment, mode);
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
    auto allocationModes = std::array<pas_allocation_mode, 3> {
        pas_non_compact_allocation_mode,
        pas_maybe_compact_allocation_mode,
        pas_always_compact_allocation_mode,
    };
    for (auto size : sizes) {
        for (auto mode : allocationModes) {
            void* memA = try_allocate_zeroed(size, mode);
            checkBufferIsZeroed(memA, size);
            void* memB = try_allocate_zeroed_with_alignment(size, 1, mode);
            checkBufferIsZeroed(memB, size);
            void* memC = try_allocate_zeroed_with_alignment(size, 64, mode);
            checkBufferIsZeroed(memC, size);
        }
    }
}

void testBmallocAllocationAlignment(BmallocHeapVariant variant)
{
    auto try_allocate_with_alignment = [variant](size_t size, size_t alignment, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_with_alignment(size, alignment);
        return bmalloc_try_allocate_with_alignment(size, alignment, mode);
    };
    auto try_allocate_zeroed_with_alignment = [variant](size_t size, size_t alignment, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_zeroed_with_alignment(size, alignment);
        return bmalloc_try_allocate_zeroed_with_alignment(size, alignment, mode);
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
    auto allocationModes = std::array<pas_allocation_mode, 3> {
        pas_non_compact_allocation_mode,
        pas_maybe_compact_allocation_mode,
        pas_always_compact_allocation_mode,
    };
    for (auto size : sizes) {
        for (auto align : alignments) {
            for (auto mode : allocationModes) {
                void* memA = try_allocate_with_alignment(size, align, mode);
                checkBufferIsAligned(memA, align);
                void* memB = try_allocate_zeroed_with_alignment(size, align, mode);
                checkBufferIsAligned(memB, align);
            }
        }
    }
}


void testBmallocDeallocate(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size, mode);
    };
    auto deallocate = [variant](void* ptr) {
        if (variant == BmallocHeapVariant::Tagged)
            tagged_bmalloc_deallocate(ptr);
        else
            bmalloc_deallocate(ptr);
    };

    void* mem = try_allocate(100, pas_non_compact_allocation_mode);
    CHECK(mem);
    deallocate(mem);
}

void testBmallocForceBitfitAfterAlloc(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size, mode);
    };

    auto& intrinsic_runtime_config = (variant == BmallocHeapVariant::Tagged)
        ? tagged_bmalloc_intrinsic_runtime_config
        : bmalloc_intrinsic_runtime_config;
    auto& primitive_runtime_config = (variant == BmallocHeapVariant::Tagged)
        ? tagged_bmalloc_primitive_runtime_config
        : bmalloc_primitive_runtime_config;

    void* mem0 = try_allocate(28616, pas_non_compact_allocation_mode);
    CHECK(mem0);

    void* mem1 = try_allocate(20768, pas_non_compact_allocation_mode);
    CHECK(mem1);

    // Simulate entering mini mode by forcing bitfit only.
    intrinsic_runtime_config.base.max_segregated_object_size = 0;
    intrinsic_runtime_config.base.max_bitfit_object_size = UINT_MAX;
    primitive_runtime_config.base.max_segregated_object_size = 0;
    primitive_runtime_config.base.max_bitfit_object_size = UINT_MAX;

    void* mem2 = try_allocate(20648, pas_non_compact_allocation_mode);
    CHECK(mem2);
}

void testBmallocDisableAllocationsAboveMTETaggingCeiling(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size, mode);
    };

    auto do_allocate_and_check = [&](pas_allocation_mode mode) {
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
            void* mem = try_allocate(size, mode);
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

void testBmallocSmallIndexOverlap(BmallocHeapVariant variant)
{
    auto try_allocate = [variant](size_t size, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate(size);
        return bmalloc_try_allocate(size, mode);
    };
    auto try_allocate_with_alignment = [variant](size_t size, size_t alignment, pas_allocation_mode mode) -> void* {
        if (variant == BmallocHeapVariant::Tagged)
            return tagged_bmalloc_try_allocate_with_alignment(size, alignment);
        return bmalloc_try_allocate_with_alignment(size, alignment, mode);
    };

    // object_size = 16 * index for this heap.
    // Creates directory A with min_index = 97, object_size = 1616
    void* mem0 = try_allocate(1552, pas_non_compact_allocation_mode);
    CHECK(mem0);
    // Extends directory A to have min_index = 96, object_size = 1616
    void* mem1 = try_allocate(1536, pas_non_compact_allocation_mode);
    CHECK(mem1);
    // Install index is 94. Directory A is a "candidate" but doesn't satisfy alignment,
    // so new directory B is created with min_index = 94, object_size = 1536.
    // Directory B overlaps directory A at index 96 (1536 / 16).
    void* mem2 = try_allocate_with_alignment(1504, 32, pas_non_compact_allocation_mode);
    CHECK(mem2);
}

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
    }
}
