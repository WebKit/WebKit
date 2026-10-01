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
#include "bmalloc_heap_innards.h"
#include "pas_bitfit_heap.h"
#include "pas_compact_heap_ptr.h"
#include "pas_compact_heap_reservation.h"
#include "pas_compact_segregated_exclusive_view_ptr.h"
#include "pas_compact_segregated_size_directory_ptr.h"
#include "pas_heap.h"
#include "pas_heap_lock.h"
#include "pas_segregated_directory.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_view.h"

using namespace std;

namespace {

static const bmalloc_type theType = BMALLOC_TYPE_INITIALIZER(64, 8, "CompactHeapReservationTests");
pas_heap_ref theHeap = BMALLOC_HEAP_REF_INITIALIZER(&theType);

enum class Front {
    Bottom,
    Top
};

// Both fronts are measured as offsets from the reservation base, just like compact pointers.
uintptr_t offsetInReservation(const void* ptr)
{
    return reinterpret_cast<uintptr_t>(ptr) - pas_compact_heap_reservation_base;
}

void checkInFront(const void* ptr, Front front)
{
    uintptr_t offset = offsetInReservation(ptr);

    CHECK_LESS(offset, pas_compact_heap_reservation_available_size);
    if (front == Front::Bottom) {
        CHECK_GREATER_EQUAL(offset, pas_compact_heap_reservation_guard_size);
        CHECK_LESS(offset, pas_compact_heap_reservation_bump);
    } else
        CHECK_GREATER_EQUAL(offset, pas_compact_heap_reservation_top_bump);
}

void checkFrontInvariants()
{
    CHECK_LESS_EQUAL(pas_compact_heap_reservation_guard_size, pas_compact_heap_reservation_bump);
    CHECK_LESS_EQUAL(pas_compact_heap_reservation_bump, pas_compact_heap_reservation_top_bump);
    CHECK_LESS_EQUAL(pas_compact_heap_reservation_top_bump, pas_compact_heap_reservation_available_size);

    // Everything in the bottom front must be reachable by PAS_DEFINE_COMPACT_PTR.
    CHECK_LESS_EQUAL(pas_compact_heap_reservation_bump,
                     (PAS_COMPACT_PTR_MASK + 1) << PAS_INTERNAL_MIN_ALIGN_SHIFT);
}

void testReservationSize()
{
    CHECK_EQUAL(pas_compact_heap_reservation_size, PAS_COMPACT_HEAP_RESERVATION_SIZE);
#if PAS_PLATFORM(MAC)
    CHECK_EQUAL(pas_compact_heap_reservation_size, static_cast<size_t>(256) << 20);
#else
    CHECK_EQUAL(pas_compact_heap_reservation_size, static_cast<size_t>(128) << 20);
#endif
}

void testTryAllocateSelectsFront()
{
    pas_heap_lock_lock();

    pas_aligned_allocation_result bottom1 = pas_compact_heap_reservation_try_allocate(24, PAS_INTERNAL_MIN_ALIGN);
    pas_aligned_allocation_result bottom2 = pas_compact_heap_reservation_try_allocate(24, PAS_INTERNAL_MIN_ALIGN);
    pas_aligned_allocation_result top1 = pas_compact_heap_reservation_try_allocate(24, PAS_OVERALIGNED_COMPACT_PTR_ALIGN);
    pas_aligned_allocation_result top2 = pas_compact_heap_reservation_try_allocate(24, PAS_OVERALIGNED_COMPACT_PTR_ALIGN);
    pas_aligned_allocation_result page = pas_compact_heap_reservation_try_allocate(16384, 16384);

    CHECK(bottom1.result);
    CHECK(bottom2.result);
    CHECK(top1.result);
    CHECK(top2.result);
    CHECK(page.result);

    checkFrontInvariants();

    checkInFront(bottom1.result, Front::Bottom);
    checkInFront(bottom2.result, Front::Bottom);
    checkInFront(top1.result, Front::Top);
    checkInFront(top2.result, Front::Top);
    checkInFront(page.result, Front::Top);

    // The bottom front grows up and the top front grows down.
    CHECK_LESS(offsetInReservation(bottom1.result), offsetInReservation(bottom2.result));
    CHECK_GREATER(offsetInReservation(top1.result), offsetInReservation(top2.result));
    CHECK_GREATER(offsetInReservation(top2.result), offsetInReservation(page.result));

    CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(bottom1.result), PAS_INTERNAL_MIN_ALIGN));
    CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(top1.result), PAS_OVERALIGNED_COMPACT_PTR_ALIGN));
    CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(top2.result), PAS_OVERALIGNED_COMPACT_PTR_ALIGN));
    CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(page.result), 16384));

    // Top front allocations are padded on the right, up to where the previous allocation began.
    CHECK_EQUAL(top2.left_padding_size, 0);
    CHECK_EQUAL(reinterpret_cast<uintptr_t>(top2.right_padding) + top2.right_padding_size,
                reinterpret_cast<uintptr_t>(top1.result));

    pas_heap_lock_unlock();
}

struct DirectoryCheckData {
    size_t numDirectories { 0 };
    size_t numExclusiveViews { 0 };
};

bool checkDirectory(pas_segregated_heap*, pas_segregated_size_directory* directory, void* arg)
{
    DirectoryCheckData* data = static_cast<DirectoryCheckData*>(arg);

    checkInFront(directory, Front::Top);
    data->numDirectories++;

    for (size_t index = pas_segregated_directory_size(&directory->base); index--;) {
        pas_segregated_view view = pas_segregated_directory_get(&directory->base, index);
        if (!pas_segregated_view_is_some_exclusive(view))
            continue;

        pas_segregated_exclusive_view* exclusive = pas_segregated_view_get_exclusive(view);
        checkInFront(exclusive, Front::Bottom);
        data->numExclusiveViews++;

        // Exclusive views are pointed at by the non-overaligned compact pointer.
        pas_compact_segregated_exclusive_view_ptr viewPtr;
        pas_compact_segregated_exclusive_view_ptr_store(&viewPtr, exclusive);
        CHECK_EQUAL(pas_compact_segregated_exclusive_view_ptr_load(&viewPtr), exclusive);

        CHECK_EQUAL(pas_compact_segregated_size_directory_ptr_load(&exclusive->directory), directory);
    }

    return true;
}

void testCoreObjectsLandInTheRightFront()
{
    // This creates a heap, a size directory, and exclusive views.
    void* smallObject = bmalloc_iso_allocate(&theHeap);
    CHECK(smallObject);

    // Force the common heap to use bitfit so that it creates its bitfit heap.
    bmalloc_intrinsic_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_intrinsic_runtime_config.base.max_bitfit_object_size = UINT_MAX;
    void* bitfitObject = bmalloc_try_allocate(1000);
    CHECK(bitfitObject);

    pas_heap_lock_lock();

    checkFrontInvariants();

    pas_heap* heap = theHeap.heap;
    CHECK(heap);
    checkInFront(heap, Front::Top);

    pas_compact_heap_ptr heapPtr;
    pas_compact_heap_ptr_store(&heapPtr, heap);
    CHECK_EQUAL(pas_compact_heap_ptr_load(&heapPtr), heap);

    DirectoryCheckData data;
    pas_segregated_heap_for_each_size_directory(&heap->segregated_heap, checkDirectory, &data);
    CHECK_GREATER(data.numDirectories, 0);
    CHECK_GREATER(data.numExclusiveViews, 0);

    pas_bitfit_heap* bitfitHeap = pas_compact_atomic_bitfit_heap_ptr_load(
        &bmalloc_common_primitive_heap.segregated_heap.bitfit_heap);
    CHECK(bitfitHeap);
    checkInFront(bitfitHeap, Front::Top);

    pas_heap_lock_unlock();

    bmalloc_deallocate(smallObject);
    bmalloc_deallocate(bitfitObject);
}

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addCompactHeapReservationTests()
{
#if PAS_ENABLE_BMALLOC
    ADD_TEST(testReservationSize());
    ADD_TEST(testTryAllocateSelectsFront());
    ADD_TEST(testCoreObjectsLandInTheRightFront());
#endif // PAS_ENABLE_BMALLOC
}
