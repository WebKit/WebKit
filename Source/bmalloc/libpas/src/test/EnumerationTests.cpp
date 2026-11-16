/*
 * Copyright (c) 2023-2025 Apple Inc. All rights reserved.
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
#include "pas_root.h"
#include "bmalloc_heap.h"
#include "pas_compact_heap_reservation.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_heap.h"
#include "iso_heap.h"
#include "iso_heap_config.h"
#include "pas_heap_ref_kind.h"
#include "pas_enumerator_internal.h"
#include "pas_heap_lock.h"
#include "pas_segregated_directory.h"
#include "pas_segregated_exclusive_view.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_view.h"

#include <map>
#include <stdlib.h>
#include <set>
#include <sys/mman.h>

using namespace std;

namespace {

const bool verbose = false;

struct PageRange {
    PageRange() = default;

    PageRange(void* base, size_t size)
        : base(base)
        , size(size)
    {
        PAS_ASSERT(pas_is_aligned(reinterpret_cast<uintptr_t>(base), pas_page_malloc_alignment()));
        PAS_ASSERT(pas_is_aligned(size, pas_page_malloc_alignment()));
        PAS_ASSERT(!!base == !!size);
    }

    bool operator<(PageRange range) const
    {
        return base < range.base;
    }

    void* end() const
    {
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) + size);
    }

    void* base { nullptr };
    size_t size { 0 };
};

set<PageRange> pageRanges;

struct RecordedRange {
    RecordedRange() = default;

    RecordedRange(void* base, size_t size)
        : base(base)
        , size(size)
    {
        PAS_ASSERT(base);
    }

    bool operator<(RecordedRange other) const
    {
        return base < other.base;
    }

    void* end() const
    {
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) + size);
    }

    void* base { nullptr };
    size_t size { 0 };
};

struct ReaderRange {
    ReaderRange() = default;

    ReaderRange(void* base, size_t size)
        : base(base)
        , size(size)
    {
        PAS_ASSERT(base);
        PAS_ASSERT(size);
    }

    void* base { nullptr };
    size_t size { 0 };
};

map<pas_enumerator_record_kind, set<RecordedRange>> recordedRanges;
ReaderRange readerPreviousBuffer;

void* enumeratorReader(pas_enumerator* enumerator, void* address, size_t size, void* arg)
{
    CHECK(!arg);
    CHECK(size);

    // Scribble the previously returned buffer to simulate the previously mapped region
    // being invalidated as per the specification of memory_reader_t in malloc.h:
    //
    // typedef kern_return_t memory_reader_t(task_t remote_task, vm_address_t remote_address, vm_size_t size, void * __sized_by(size) *local_memory);
    //
    // given a task, "reads" the memory at the given address and size
    // local_memory: set to a contiguous chunk of memory; validity of local_memory is assumed to be limited (until next call)
    //
    if (readerPreviousBuffer.base && readerPreviousBuffer.size)
        memset(readerPreviousBuffer.base, 0xda, readerPreviousBuffer.size);

    void* result = pas_enumerator_allocate(enumerator, size);

    void* pageAddress = reinterpret_cast<void*>(
        pas_round_down_to_power_of_2(
            reinterpret_cast<uintptr_t>(address),
            pas_page_malloc_alignment()));
    size_t pagesSize =
        pas_round_up_to_power_of_2(
            reinterpret_cast<uintptr_t>(address) + size,
            pas_page_malloc_alignment())
        - reinterpret_cast<uintptr_t>(pageAddress);
    void* pageEndAddress = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(pageAddress) + pagesSize);

    bool areProtectedPages = false;
    if (!pageRanges.empty()) {
        auto pageRangeIter = pageRanges.upper_bound(PageRange(pageAddress, pagesSize));
        if (pageRangeIter != pageRanges.begin()) {
            --pageRangeIter;
            areProtectedPages =
                pageRangeIter->base <= pageAddress
                && pageRangeIter->end() > pageAddress;
        }
    }

    if (verbose) {
        cout << "address = " << address << "..."
             << reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address) + size)
             << ", pageAddress = " << pageAddress << "..." << pageEndAddress
             << ", areProtectedPages = " << areProtectedPages << "\n";
    }

    if (areProtectedPages) {
        int systemResult = mprotect(pageAddress, pagesSize, PROT_READ);
        PAS_ASSERT(!systemResult);
    }

    memcpy(result, address, size);

    if (areProtectedPages) {
        int systemResult = mprotect(pageAddress, pagesSize, PROT_NONE);
        PAS_ASSERT(!systemResult);
    }

    readerPreviousBuffer = ReaderRange(result, size);
    return result;
}

void enumeratorRecorder(pas_enumerator* enumerator, void* address, size_t size, pas_enumerator_record_kind kind, void* arg)
{
    PAS_UNUSED_PARAM(enumerator);
    CHECK(size);
    CHECK(!arg);

    if (verbose) {
        cout << "Recording " << pas_enumerator_record_kind_get_string(kind) << ":" << address
             << "..." << reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address) + size) << "\n";
    }

    RecordedRange range = RecordedRange(address, size);

    CHECK(!recordedRanges[kind].count(range));
    recordedRanges[kind].insert(range);
}


void testBasicEnumeration() {

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    auto size = 25;
    void* arr[size];
    for (auto i = 0; i < size; i++) {
        arr[i] = bmalloc_try_allocate(1000000);
        PAS_ASSERT(arr[i]);
    }

    pas_enumerator* enumerator = pas_enumerator_create(root, enumeratorReader, nullptr, enumeratorRecorder, nullptr, pas_enumerator_record_meta_records, pas_enumerator_record_payload_records, pas_enumerator_record_object_records);
    pas_enumerator_enumerate_all(enumerator);

    pas_enumerator_destroy(enumerator);
}

void testPGMEnumerationBasic() {

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, getpagesize());
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    size_t alloc_size = 16384;
    pas_allocation_result result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, &iso_heap_config, &transaction);
    CHECK(result.begin);

    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, &iso_heap_config, &transaction);
    CHECK(result.begin);

    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, &iso_heap_config, &transaction);
    CHECK(result.begin);

    pas_heap_lock_unlock();

    pas_enumerator* enumerator = pas_enumerator_create(root, enumeratorReader, nullptr, enumeratorRecorder, nullptr, pas_enumerator_record_meta_records, pas_enumerator_record_payload_records, pas_enumerator_record_object_records);
    pas_enumerator_enumerate_all(enumerator);
    CHECK(enumerator);
    pas_enumerator_destroy(enumerator);
}

void testPGMEnumerationAddAndFree() {

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    pas_heap_ref heapRef = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(getpagesize() * 100, getpagesize());
    pas_heap* heap = iso_heap_ref_get_heap(&heapRef);
    pas_physical_memory_transaction transaction;
    pas_physical_memory_transaction_construct(&transaction);

    pas_heap_lock_lock();

    size_t alloc_size = 16384;
    pas_allocation_result result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, &iso_heap_config, &transaction);
    CHECK(result.begin);

    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, &iso_heap_config, &transaction);
    CHECK(result.begin);

    result = pas_probabilistic_guard_malloc_allocate(&heap->large_heap, alloc_size, 1, &iso_heap_config, &transaction);
    CHECK(result.begin);

    pas_probabilistic_guard_malloc_deallocate((void*) result.begin);

    pas_heap_lock_unlock();

    pas_enumerator* enumerator = pas_enumerator_create(root, enumeratorReader, nullptr, enumeratorRecorder, nullptr, pas_enumerator_record_meta_records, pas_enumerator_record_payload_records, pas_enumerator_record_object_records);
    pas_enumerator_enumerate_all(enumerator);

    CHECK(enumerator);
    pas_enumerator_destroy(enumerator);

}

void testEnumerationInvalidCompactHeapBump()
{
    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    // Do an allocation to ensure the compact heap is initialized.
    void* p = bmalloc_try_allocate(16);
    PAS_ASSERT(p);

    auto createEnumerator = [&] () {
        return pas_enumerator_create(
            root, enumeratorReader, nullptr, enumeratorRecorder, nullptr,
            pas_enumerator_do_not_record_meta_records,
            pas_enumerator_do_not_record_payload_records,
            pas_enumerator_do_not_record_object_records);
    };

    // Corrupts one of the reservation's variables, checks that enumeration refuses to start, and
    // then restores it.
    auto checkCreationFailsWith = [&] (size_t& variable, size_t invalidValue) {
        size_t savedValue = variable;
        variable = invalidValue;
        pas_enumerator* enumerator = createEnumerator();
        variable = savedValue;
        CHECK(!enumerator);
    };

    // The bottom bump is past the end of the reservation.
    checkCreationFailsWith(pas_compact_heap_reservation_bump, pas_compact_heap_reservation_size + 1);
    // The bottom bump is below the guard.
    checkCreationFailsWith(pas_compact_heap_reservation_bump, pas_compact_heap_reservation_guard_size - 1);
    // The fronts have crossed.
    checkCreationFailsWith(pas_compact_heap_reservation_top_bump, pas_compact_heap_reservation_bump - 1);
    // The top bump is past the end of the reservation.
    checkCreationFailsWith(pas_compact_heap_reservation_top_bump, pas_compact_heap_reservation_available_size + 1);
    // The available size is bigger than the reservation.
    checkCreationFailsWith(pas_compact_heap_reservation_available_size, pas_compact_heap_reservation_size + 1);

    // Verify normal creation still works.
    pas_enumerator* enumerator = createEnumerator();
    CHECK(enumerator);
    pas_enumerator_destroy(enumerator);
}

static const bmalloc_type bothFrontsType = BMALLOC_TYPE_INITIALIZER(64, 8, "EnumerationTests");
pas_heap_ref bothFrontsHeap = BMALLOC_HEAP_REF_INITIALIZER(&bothFrontsType);

bool recordedRangesCover(pas_enumerator_record_kind kind, const void* ptr)
{
    for (const RecordedRange& range : recordedRanges[kind]) {
        if (ptr >= range.base && ptr < range.end())
            return true;
    }
    return false;
}

struct FindHeapData {
    const pas_heap_type* type { nullptr };
    bool found { false };
};

bool findHeapCallback(pas_enumerator*, pas_heap* heap, void* arg)
{
    FindHeapData* data = static_cast<FindHeapData*>(arg);
    if (heap->type == data->type)
        data->found = true;
    return true;
}

void testEnumerationCopiesBothCompactHeapFronts()
{
    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    // This creates a heap and size directory in the top front and exclusive views in the bottom front.
    void* object = bmalloc_iso_allocate(&bothFrontsHeap);
    CHECK(object);

    pas_heap* heap = bothFrontsHeap.heap;
    CHECK(heap);
    CHECK_GREATER_EQUAL(reinterpret_cast<uintptr_t>(heap) - pas_compact_heap_reservation_base,
                        pas_compact_heap_reservation_top_bump);

    pas_segregated_exclusive_view* view = nullptr;
    pas_heap_lock_lock();
    pas_segregated_heap_for_each_size_directory(
        &heap->segregated_heap,
        [] (pas_segregated_heap*, pas_segregated_size_directory* directory, void* arg) -> bool {
            for (size_t index = pas_segregated_directory_size(&directory->base); index--;) {
                pas_segregated_view view = pas_segregated_directory_get(&directory->base, index);
                if (pas_segregated_view_is_some_exclusive(view)) {
                    *static_cast<pas_segregated_exclusive_view**>(arg) = pas_segregated_view_get_exclusive(view);
                    return false;
                }
            }
            return true;
        },
        &view);
    pas_heap_lock_unlock();
    CHECK(view);
    CHECK_LESS(reinterpret_cast<uintptr_t>(view) - pas_compact_heap_reservation_base,
               pas_compact_heap_reservation_bump);

    recordedRanges.clear();
    pas_enumerator* enumerator = pas_enumerator_create(
        root, enumeratorReader, nullptr, enumeratorRecorder, nullptr,
        pas_enumerator_record_meta_records,
        pas_enumerator_record_payload_records,
        pas_enumerator_record_object_records);
    CHECK(enumerator);

    // The enumerator's copy of the reservation must include both fronts.
    pas_heap* heapCopy = static_cast<pas_heap*>(pas_enumerator_read_compact(enumerator, heap));
    CHECK(heapCopy);
    CHECK_EQUAL(heapCopy->type, heap->type);
    CHECK_EQUAL(heapCopy->heap_ref, heap->heap_ref);

    pas_segregated_exclusive_view* viewCopy = static_cast<pas_segregated_exclusive_view*>(
        pas_enumerator_read_compact(enumerator, view));
    CHECK(viewCopy);
    CHECK_EQUAL(viewCopy->index, view->index);
    CHECK(!memcmp(&viewCopy->directory, &view->directory, sizeof(view->directory)));

    // Walking the heap list follows pas_compact_heap_ptr, which is overaligned, through the copy.
    FindHeapData findHeapData;
    findHeapData.type = heap->type;
    CHECK(pas_enumerator_for_each_heap(enumerator, findHeapCallback, &findHeapData));
    CHECK(findHeapData.found);

    // Both fronts' pages are libpas metadata.
    CHECK(pas_enumerator_enumerate_all(enumerator));
    CHECK(recordedRangesCover(pas_enumerator_meta_record, heap));
    CHECK(recordedRangesCover(pas_enumerator_meta_record, view));

    pas_enumerator_destroy(enumerator);
    bmalloc_deallocate(object);
}

} // end namespace

void addEnumerationTests() {
    ADD_TEST(testBasicEnumeration());
    ADD_TEST(testPGMEnumerationBasic());
    ADD_TEST(testPGMEnumerationAddAndFree());
    ADD_TEST(testEnumerationInvalidCompactHeapBump());
    ADD_TEST(testEnumerationCopiesBothCompactHeapFronts());
}
