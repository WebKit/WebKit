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

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"
#include "bmalloc_heap_utils.h"
#include "pas_internal_config.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>

using namespace std;

namespace {

void fillPattern(void* ptr, size_t size, uint8_t seed)
{
    auto* bytes = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < size; ++i)
        bytes[i] = static_cast<uint8_t>(seed + (i & 0xff));
}

void checkPattern(void* ptr, size_t size, uint8_t seed)
{
    auto* bytes = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < size; ++i)
        CHECK_EQUAL(bytes[i], static_cast<uint8_t>(seed + (i & 0xff)));
}

void testReallocFastPathGrowWithinSizeClass()
{
    // A 10-byte request rounds up to some usable size >= 10. As long as the
    // new request still fits within that slot the fast path must return the
    // same pointer.
    void* ptr = bmalloc_try_allocate(10, pas_non_compact_allocation_mode);
    CHECK(ptr);

    size_t usable = bmalloc_get_allocation_size(ptr);
    CHECK_GREATER_EQUAL(usable, static_cast<size_t>(10));

    fillPattern(ptr, 10, 0xA1);

    void* grown = bmalloc_try_reallocate(
        ptr, usable, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK_EQUAL(grown, ptr);
    checkPattern(grown, 10, 0xA1);
}

void testReallocFastPathShrinkWithinSizeClass()
{
    void* ptr = bmalloc_try_allocate(200, pas_non_compact_allocation_mode);
    CHECK(ptr);

    size_t usable = bmalloc_get_allocation_size(ptr);
    CHECK_GREATER_EQUAL(usable, static_cast<size_t>(200));

    fillPattern(ptr, 200, 0x5A);

    size_t shrink_to = usable / 2 + 1;
    void* shrunk = bmalloc_try_reallocate(
        ptr, shrink_to, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK_EQUAL(shrunk, ptr);
    checkPattern(shrunk, std::min(shrink_to, static_cast<size_t>(200)), 0x5A);
}

void testReallocFastPathToExactUsableSize()
{
    void* ptr = bmalloc_try_allocate(100, pas_non_compact_allocation_mode);
    CHECK(ptr);

    size_t usable = bmalloc_get_allocation_size(ptr);
    fillPattern(ptr, usable, 0x33);

    void* result = bmalloc_try_reallocate(
        ptr, usable, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK_EQUAL(result, ptr);
    checkPattern(result, usable, 0x33);
}

void testReallocGrowBeyondSizeClassCopiesData()
{
    // Force a real move: ask for a new size that clearly does not fit in the
    // original slot. The fast path must not trigger; data must survive the copy.
    void* ptr = bmalloc_try_allocate(32, pas_non_compact_allocation_mode);
    CHECK(ptr);

    size_t usable = bmalloc_get_allocation_size(ptr);
    fillPattern(ptr, 32, 0xC7);

    size_t grown_size = usable * 16 + 1;
    void* grown = bmalloc_try_reallocate(
        ptr, grown_size, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK(grown);
    checkPattern(grown, 32, 0xC7);
}

void testReallocFastPathSweepAcrossSizes()
{
    // For a wide range of sizes, any realloc whose new size stays within the
    // in-place shrinkage envelope must return the same pointer.
    const std::array<size_t, 10> request_sizes = {
        1, 7, 16, 24, 48, 100, 128, 256, 1024, 3000,
    };

    for (size_t request : request_sizes) {
        void* ptr = bmalloc_try_allocate(request, pas_non_compact_allocation_mode);
        CHECK(ptr);

        size_t usable = bmalloc_get_allocation_size(ptr);
        CHECK_GREATER_EQUAL(usable, request);

        fillPattern(ptr, request, static_cast<uint8_t>(request));

        size_t min_in_place = (usable + PAS_MAX_IN_PLACE_REALLOC_SHRINKAGE - 1)
            / PAS_MAX_IN_PLACE_REALLOC_SHRINKAGE;
        for (size_t candidate : { min_in_place, usable / 2 + 1, usable }) {
            if (!candidate || candidate > usable || candidate < min_in_place)
                continue;
            void* after = bmalloc_try_reallocate(
                ptr, candidate, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
            CHECK_EQUAL(after, ptr);
        }

        checkPattern(ptr, request, static_cast<uint8_t>(request));
        bmalloc_deallocate(ptr);
    }
}

void testReallocBitfitFastPath()
{
    // Bitfit allocations also support the same-usable-size fast path. Force
    // bitfit by disabling the segregated path.
    bmalloc_intrinsic_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_intrinsic_runtime_config.base.max_bitfit_object_size = UINT_MAX;
    bmalloc_primitive_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_primitive_runtime_config.base.max_bitfit_object_size = UINT_MAX;

    void* ptr = bmalloc_try_allocate(8192, pas_non_compact_allocation_mode);
    CHECK(ptr);

    size_t usable = bmalloc_get_allocation_size(ptr);
    CHECK_GREATER_EQUAL(usable, static_cast<size_t>(8192));

    fillPattern(ptr, 8192, 0x77);

    void* grown = bmalloc_try_reallocate(
        ptr, usable, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK_EQUAL(grown, ptr);
    checkPattern(grown, 8192, 0x77);

    void* shrunk = bmalloc_try_reallocate(
        ptr, usable / 2 + 1, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK_EQUAL(shrunk, ptr);
    checkPattern(shrunk, usable / 2 + 1, 0x77);
}

void testReallocExcessiveShrinkForcesCopy()
{
    // Shrinking by more than PAS_MAX_IN_PLACE_REALLOC_SHRINKAGE should skip
    // the in-place fast path and go through allocate+copy+free so we don't
    // pin a much larger slot for a tiny request. We can't assert the pointer
    // moved (the freed slot could be reused), but the data must survive and
    // the new usable size must actually be smaller than the old slot.
    void* ptr = bmalloc_try_allocate(4096, pas_non_compact_allocation_mode);
    CHECK(ptr);

    size_t old_usable = bmalloc_get_allocation_size(ptr);
    CHECK_GREATER_EQUAL(old_usable, static_cast<size_t>(4096));

    fillPattern(ptr, 64, 0xE3);

    size_t shrink_to = old_usable / (PAS_MAX_IN_PLACE_REALLOC_SHRINKAGE + 1);
    CHECK_GREATER_EQUAL(shrink_to, static_cast<size_t>(1));

    void* shrunk = bmalloc_try_reallocate(
        ptr, shrink_to, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK(shrunk);
    checkPattern(shrunk, std::min(shrink_to, static_cast<size_t>(64)), 0xE3);

    size_t new_usable = bmalloc_get_allocation_size(shrunk);
    CHECK_LESS(new_usable, old_usable);

    bmalloc_deallocate(shrunk);
}

void testReallocLargeDataPreserved()
{
    // Large heap has no in-place fast path today; this regression-guards the
    // allocate+copy+free path for sizes above the bitfit/segregated tiers.
    const size_t small_size = 1024 * 1024;
    const size_t large_size = small_size * 4;

    void* ptr = bmalloc_try_allocate(small_size, pas_non_compact_allocation_mode);
    CHECK(ptr);
    fillPattern(ptr, small_size, 0x19);

    void* grown = bmalloc_try_reallocate(
        ptr, large_size, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK(grown);
    checkPattern(grown, small_size, 0x19);
}

void testReallocFromNullAllocates()
{
    // realloc(NULL, n) must behave like malloc(n); it must not crash on the
    // same-size-class lookup.
    void* ptr = bmalloc_try_reallocate(
        nullptr, 128, pas_non_compact_allocation_mode, pas_reallocate_free_if_successful);
    CHECK(ptr);
    fillPattern(ptr, 128, 0x2D);
    checkPattern(ptr, 128, 0x2D);
    bmalloc_deallocate(ptr);
}

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addReallocFastPathTests()
{
#if PAS_ENABLE_BMALLOC
    ADD_TEST(testReallocFastPathGrowWithinSizeClass());
    ADD_TEST(testReallocFastPathShrinkWithinSizeClass());
    ADD_TEST(testReallocFastPathToExactUsableSize());
    ADD_TEST(testReallocGrowBeyondSizeClassCopiesData());
    ADD_TEST(testReallocFastPathSweepAcrossSizes());
    ADD_TEST(testReallocBitfitFastPath());
    ADD_TEST(testReallocExcessiveShrinkForcesCopy());
    ADD_TEST(testReallocLargeDataPreserved());
    ADD_TEST(testReallocFromNullAllocates());
#endif // PAS_ENABLE_BMALLOC
}
