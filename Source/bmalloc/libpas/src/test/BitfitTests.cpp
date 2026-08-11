/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#if PAS_ENABLE_ISO

#include "iso_heap.h"
#include "iso_heap_innards.h"
#include "pas_bitfit_directory.h"
#include "pas_bitfit_heap.h"
#include "pas_bitfit_size_class.h"
#include "pas_deferred_decommit_log.h"
#include "pas_heap.h"
#include "pas_race_test_hooks.h"
#include "pas_scavenger.h"
#include "pas_versioned_field.h"
#include <algorithm>
#include <functional>
#include <vector>

using namespace std;

namespace {

vector<size_t> getBitfitSizeClasses()
{
    vector<size_t> result;
    
    pas_bitfit_heap* heap = pas_compact_atomic_bitfit_heap_ptr_load_non_null(
        &iso_common_primitive_heap.segregated_heap.bitfit_heap);

    pas_bitfit_page_config_variant variant;
    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(variant)) {
        pas_bitfit_directory* directory = pas_bitfit_heap_get_directory(heap, variant);

        for (pas_bitfit_size_class* sizeClass =
                 pas_compact_atomic_bitfit_size_class_ptr_load(&directory->largest_size_class);
             sizeClass;
             sizeClass = pas_compact_atomic_bitfit_size_class_ptr_load(&sizeClass->next_smaller))
            result.push_back(sizeClass->size);
    }

    sort(result.begin(), result.end());

    return result;
}

template<typename... Arguments>
void assertSizeClasses(Arguments... sizeClasses)
{
    vector<size_t> expectedSizeClasses { sizeClasses... };
    vector<size_t> actualSizeClasses = getBitfitSizeClasses();

    if (actualSizeClasses.size() == expectedSizeClasses.size()) {
        bool allGood = true;
        for (size_t index = 0; index < actualSizeClasses.size(); ++index) {
            if (actualSizeClasses[index] != expectedSizeClasses[index]) {
                allGood = false;
                break;
            }
        }
        if (allGood)
            return;
    }
    
    cout << "Expected size classes: ";
    for (size_t index = 0; index < expectedSizeClasses.size(); ++index) {
        if (index)
            cout << ", ";
        cout << expectedSizeClasses[index];
    }
    cout << "\n";
    cout << "But got: ";
    for (size_t index = 0; index < actualSizeClasses.size(); ++index) {
        if (index)
            cout << ", ";
        cout << actualSizeClasses[index];
    }
    cout << "\n";
    CHECK(!"Test failed");
}

void testAllocateAlignedSmallerThanSizeClassAndSmallerThanLargestAvailable(
    size_t firstSize, size_t numFirstObjects, size_t indexOfObjectToFree,
    size_t fillerObjectSize, size_t alignedSize, size_t numAlignedObjects)
{
    static constexpr bool verbose = false;
    
    vector<void*> objects;
    void* freedObject;
    uintptr_t freedObjectBegin;
    uintptr_t freedObjectEnd;

    for (size_t index = 0; index < numFirstObjects; ++index) {
        void* ptr = iso_allocate_common_primitive(firstSize, pas_non_compact_allocation_mode);
        if (verbose)
            cout << "Adding first object " << ptr << "\n";
        objects.push_back(ptr);
    }

    assertSizeClasses(firstSize);

    freedObject = objects[indexOfObjectToFree];
    iso_deallocate(freedObject);

    freedObjectBegin = reinterpret_cast<uintptr_t>(freedObject);
    freedObjectEnd = freedObjectBegin + firstSize;

    vector<void*> fillerObjects;
    bool didStartAllocatingInFreedObject = false;
    for (;;) {
        void* fillerObject = iso_allocate_common_primitive(fillerObjectSize, pas_non_compact_allocation_mode);
        uintptr_t fillerObjectBegin = reinterpret_cast<uintptr_t>(fillerObject);
        uintptr_t fillerObjectEnd = fillerObjectBegin + fillerObjectSize;
        if (verbose)
            cout << "Allocated filler object " << fillerObject << "\n";
        if (fillerObjectBegin >= freedObjectBegin && fillerObjectEnd <= freedObjectEnd) {
            didStartAllocatingInFreedObject = true;
            fillerObjects.push_back(fillerObject);
        } else {
            if (didStartAllocatingInFreedObject)
                break;
        }
    }

    CHECK_EQUAL(fillerObjects.size(), firstSize / fillerObjectSize);
    assertSizeClasses(fillerObjectSize, firstSize);

    for (size_t index = 1; index < fillerObjects.size(); ++index)
        iso_deallocate(fillerObjects[index]);

    for (size_t index = 0; index < numAlignedObjects; ++index) {
        void* ptr = iso_allocate_common_primitive_with_alignment(alignedSize, alignedSize, pas_non_compact_allocation_mode);;
        if (verbose)
            cout << "Allocated aligned object " << ptr << "\n";
    }

    assertSizeClasses(fillerObjectSize, firstSize);
}

#if PAS_ENABLE_TESTING
function<void(pas_race_test_hook_kind)> hookCallback;

void hookCallbackAdapter(pas_race_test_hook_kind kind)
{
    hookCallback(kind);
}

class InstallBitfitRaceHook : public TestScope {
public:
    InstallBitfitRaceHook()
        : TestScope(
            "install-bitfit-race-hook",
            [] () {
                pas_race_test_hook_callback_instance = hookCallbackAdapter;
            })
    {
    }
};

void testTakeLastEmptyLastEmptyPlusOneWatchRace()
{
    pas_scavenger_suspend();

    void* obj = iso_allocate_common_primitive(1056, pas_non_compact_allocation_mode);
    CHECK(obj);

    pas_bitfit_heap* bitfitHeap = pas_compact_atomic_bitfit_heap_ptr_load_non_null(
        &iso_common_primitive_heap.segregated_heap.bitfit_heap);
    pas_bitfit_directory* directory = nullptr;
    pas_bitfit_page_config_variant variant;
    for (PAS_EACH_BITFIT_PAGE_CONFIG_VARIANT_ASCENDING(variant)) {
        pas_bitfit_directory* candidate = pas_bitfit_heap_get_directory(bitfitHeap, variant);
        if (pas_bitfit_directory_size(candidate)) {
            directory = candidate;
            break;
        }
    }
    CHECK(directory);
    CHECK_GREATER_EQUAL(pas_bitfit_directory_size(directory), 1u);

    pas_bitfit_directory_set_empty_bit_at_index(directory, 0, false);
    directory->last_empty_plus_one = pas_versioned_field_create(1, 0);

    hookCallback =
        [directory] (pas_race_test_hook_kind kind) {
            if (kind != pas_race_test_hook_bitfit_directory_take_last_empty_after_loop)
                return;
            pas_bitfit_directory_view_did_become_empty_at_index(directory, 0);
        };

    pas_deferred_decommit_log log;
    pas_deferred_decommit_log_construct(&log, nullptr, 0, nullptr);
    pas_page_sharing_pool_take_result result =
        pas_bitfit_directory_take_last_empty(directory, &log, pas_lock_is_not_held);
    pas_deferred_decommit_log_destruct(&log, pas_lock_is_not_held);

    hookCallback = nullptr;

    CHECK_EQUAL(result, pas_page_sharing_pool_take_none_available);

    // The hook re-reported view[0] as empty after the scan but before the final try_write.
    // If take_last_empty's read of last_empty_plus_one isn't watched, the hook's maximize()
    // is a no-op, the try_write succeeds, and the directory ends with last_empty_plus_one == 0
    // while empty_bits[0] is set: a permanently stranded page.
    CHECK_EQUAL(directory->last_empty_plus_one.value, static_cast<uintptr_t>(1));
}
#endif // PAS_ENABLE_TESTING

} // anonymous namespace

#endif // PAS_ENABLE_ISO

void addBitfitTests()
{
#if PAS_ENABLE_ISO
    ForceBitfit forceBitfit;

    ADD_TEST(testAllocateAlignedSmallerThanSizeClassAndSmallerThanLargestAvailable(
                 1056, 100, 0, 16, 1024, 100));
#if PAS_ENABLE_TESTING
    InstallBitfitRaceHook installBitfitRaceHook;
    ADD_TEST(testTakeLastEmptyLastEmptyPlusOneWatchRace());
#endif
#endif // PAS_ENABLE_ISO
}
