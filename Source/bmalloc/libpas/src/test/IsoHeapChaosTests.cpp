/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"
#include "bmalloc_heap_innards.h"
#include "hotbit_heap.h"
#include "hotbit_heap_config.h"
#include "hotbit_heap_innards.h"
#include "iso_heap.h"
#include "iso_heap_config.h"
#include "iso_heap_innards.h"
#include "iso_test_heap.h"
#include "iso_test_heap_config.h"
#include "jit_heap.h"
#include "jit_heap_config.h"
#include <mach/thread_act.h>
#include <map>
#include "minalign32_heap.h"
#include "minalign32_heap_config.h"
#include "pagesize64k_heap.h"
#include "pagesize64k_heap_config.h"
#include "pas_all_heaps.h"
#include "pas_baseline_allocator_table.h"
#include "pas_enumerable_page_malloc.h"
#include "pas_epoch.h"
#include "pas_get_page_base.h"
#include "pas_heap.h"
#include "pas_root.h"
#include "pas_segregated_page.h"
#include "pas_thread_local_cache.h"
#include "pas_utility_heap.h"
#include <set>
#include <sys/mman.h>
#include <vector>
#include <thread>

using namespace std;

namespace {

constexpr bool verbose = false;

pas_heap_config* selectedHeapConfig;
void* (*selectedAllocateCommonPrimitive)(size_t size);
void* (*selectedAllocate)(pas_heap_ref* heapRef);
void* (*selectedAllocateArray)(pas_heap_ref* heapRef, size_t count, size_t alignment);
void (*selectedShrink)(void* ptr, size_t newSize);
void (*selectedDeallocate)(void* ptr);
pas_heap* selectedCommonPrimitiveHeap;
pas_heap* (*selectedHeapRefGetHeap)(pas_heap_ref* heapRef);

void flushDeallocationLogAndStopAllocators()
{
#if TLC
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(), pas_lock_is_not_held);
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_utility_heap_for_all_allocators(pas_allocator_scavenge_force_stop_action,
                                        pas_lock_is_not_held);
#endif
}

template<typename ObjectVerificationFunc>
void verifyObjectSet(const set<void*>& expectedObjects,
                     const vector<pas_heap*>& heaps,
                     const ObjectVerificationFunc& objectVerificationFunc)
{
    flushDeallocationLogAndStopAllocators();

    if (verbose) {
        cout << "Expected objects:\n";
        dumpObjectSet(expectedObjects);
        
        dumpObjectsInHeaps(heaps);
    }

    set<void*> foundObjects;
    bool expectedAllFoundObjects = true;
    bool foundFirstUnexpectedObjectInBitfit = false;
    for (pas_heap* heap : heaps) {
        forEachLiveObject(
            heap,
            [&] (void* object, size_t size) -> bool {
                if (!expectedObjects.count(object)) {
                    cout << "Found unexpected object: " << object << " of size " << size << "\n";
                    expectedAllFoundObjects = false;

                    if (!foundFirstUnexpectedObjectInBitfit) {
                        pas_page_base* page_base = pas_get_page_base(object, *selectedHeapConfig);
                        if (pas_page_base_is_bitfit(page_base)) {
                            pas_bitfit_page* page = pas_page_base_get_bitfit(page_base);
                            uintptr_t offset = reinterpret_cast<uintptr_t>(object)
                                - reinterpret_cast<uintptr_t>(
                                    pas_bitfit_page_boundary(
                                        page,
                                        *pas_heap_config_bitfit_page_config_ptr_for_variant(
                                            selectedHeapConfig,
                                            pas_page_kind_get_bitfit_variant(
                                                pas_page_base_get_kind(page_base)))));
                            pas_bitfit_page_log_bits(page, offset, offset + 1);
                            foundFirstUnexpectedObjectInBitfit = true;
                        }
                    }
                }
                CHECK(!foundObjects.count(object));
                objectVerificationFunc(object, size);
                foundObjects.insert(object);
                return true;
            });
    }

    if (foundObjects.size() != expectedObjects.size() || !expectedAllFoundObjects) {
        for (void* object : expectedObjects) {
            if (!foundObjects.count(object))
                cout << "Did not find expected object: " << object << "\n";
        }
        CHECK_EQUAL(foundObjects.size(), expectedObjects.size());
        CHECK(expectedAllFoundObjects);
    }
}

mutex lock;

set<pthread_t> runningThreads;

void scavengerDidStart()
{
    lock_guard<mutex> locker(lock);
    runningThreads.insert(pthread_self());
}

void scavengerWillShutDown()
{
    lock_guard<mutex> locker(lock);
    runningThreads.erase(pthread_self());
}

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

void addPageRange(pas_range range)
{
    if (verbose) {
        cout << "libpas page range: " << reinterpret_cast<void*>(range.begin) << "..."
             << reinterpret_cast<void*>(range.end) << "\n";
    }
    pageRanges.insert(PageRange(reinterpret_cast<void*>(range.begin), pas_range_size(range)));
}

bool addPageRangeCallback(pas_range range, void* arg)
{
    PAS_ASSERT(!arg);
    addPageRange(range);
    return true;
}

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

    bool operator<(ReaderRange other) const
    {
        if (base != other.base)
            return base < other.base;
        return size < other.size;
    }

    void* base { nullptr };
    size_t size { 0 };
};

map<pas_enumerator_record_kind, set<RecordedRange>> recordedRanges;
map<ReaderRange, void*> readerCache;

void* enumeratorReader(pas_enumerator* enumerator,
                       void* address,
                       size_t size,
                       void* arg)
{
    CHECK(!arg);
    CHECK(size);

    ReaderRange range = ReaderRange(address, size);

    auto readerCacheIter = readerCache.find(range);
    if (readerCacheIter != readerCache.end())
        return readerCacheIter->second;

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

    readerCache[range] = result;
    return result;
}

void enumeratorRecorder(pas_enumerator* enumerator,
                        void* address,
                        size_t size,
                        pas_enumerator_record_kind kind,
                        void* arg)
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

void testAllocationChaos(unsigned numThreads, unsigned numIsolatedHeaps,
                         unsigned numInitialAllocations, unsigned numActions, unsigned maxTotalSize,
                         bool testEnumerator)
{
    PAS_ASSERT(!pas_epoch_is_counter); // We don't want to run these tests in the fake scavenger mode.
    
    if (verbose)
        cout << "Starting.\n";
    
    struct OptionalObject {
        OptionalObject() = default;

        OptionalObject(void* ptr, size_t size)
            : isValid(true)
            , ptr(ptr)
            , size(size)
        {
        }

        bool isValid { false };
        void* ptr { nullptr };
        size_t size { 0 };
    };
    
    struct Object {
        Object() = default;
        
        Object(void* ptr, size_t size, uint8_t startValue)
            : ptr(ptr)
            , size(size)
            , startValue(startValue)
        {
        }
        
        void* ptr { nullptr };
        size_t size { 0 };
        uint8_t startValue { 0 };
    };
    
    vector<Object> objects;
    vector<thread> threads;
    unsigned totalSize;

    vector<OptionalObject> optionalObjects;
    if (testEnumerator) {
        pas_scavenger_did_start_callback = scavengerDidStart;
        pas_scavenger_will_shut_down_callback = scavengerWillShutDown;
        
        for (unsigned threadIndex = numThreads; threadIndex--;)
            optionalObjects.push_back(OptionalObject());
    }

    if (!selectedAllocate)
        numIsolatedHeaps = 0;
    
    vector<pas_heap_ref*> isolatedHeaps;
    for (unsigned isolatedHeapIndex = numIsolatedHeaps; isolatedHeapIndex--;) {
        isolatedHeaps.push_back(new pas_heap_ref(ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(
                                                     deterministicRandomNumber(100), 1)));
    }
    
    auto addObject =
        [&] (unsigned threadIndex, void* ptr, unsigned size) {
            CHECK(ptr);
            uint8_t startValue = static_cast<uint8_t>(deterministicRandomNumber(255));
            for (unsigned i = 0; i < size; ++i)
                static_cast<uint8_t*>(ptr)[i] = static_cast<uint8_t>(startValue + i);
            {
                lock_guard<mutex> locker(lock);
                if (verbose)
                    cout << "Adding object " << ptr << "\n";
                if (testEnumerator)
                    optionalObjects[threadIndex] = OptionalObject();
                objects.push_back(Object(ptr, size, startValue));
                totalSize += size;
            }
        };

    auto logOptionalObject =
        [&] (unsigned threadIndex, unsigned size) {
            if (testEnumerator) {
                lock_guard<mutex> locker(lock);
                if (verbose)
                    cout << "Allocating object of size = " << size << "\n";
                optionalObjects[threadIndex] = OptionalObject(nullptr, size);
            }
        };
    
    auto allocateSomething = [&] (unsigned threadIndex) {
        unsigned heapIndex = deterministicRandomNumber(static_cast<unsigned>(isolatedHeaps.size() + 1));
        unsigned size = deterministicRandomNumber(5000);

        if (heapIndex == isolatedHeaps.size()) {
            logOptionalObject(threadIndex, size);
            void* ptr = selectedAllocateCommonPrimitive(size);
            addObject(threadIndex, ptr, size);
            return;
        }

        PAS_ASSERT(selectedAllocate);
        
        pas_heap_ref* heap = isolatedHeaps[heapIndex];
        unsigned count;
        if (selectedAllocateArray)
            count = size / pas_simple_type_size(reinterpret_cast<pas_simple_type>(heap->type));
        else
            count = 1;
        size = static_cast<unsigned>(
            count * pas_simple_type_size(reinterpret_cast<pas_simple_type>(heap->type)));
        logOptionalObject(threadIndex, size);
        void* ptr;
        if (count <= 1)
            ptr = selectedAllocate(heap);
        else
            ptr = selectedAllocateArray(heap, count, 1);
        addObject(threadIndex, ptr, size);
    };
    
    auto freeSomething = [&] (unsigned threadIndex) {
        Object object;

        {
            lock_guard<mutex> locker(lock);
            if (objects.empty())
                return;
            unsigned index = deterministicRandomNumber(static_cast<unsigned>(objects.size()));
            object = objects[index];
            objects[index] = objects.back();
            objects.pop_back();
            totalSize -= object.size;

            if (verbose)
                cout << "Freeing object " << object.ptr << "\n";

            if (testEnumerator)
                optionalObjects[threadIndex] = OptionalObject(object.ptr, object.size);
        }
        
        for (unsigned i = 0; i < object.size; ++i) {
            CHECK_EQUAL(static_cast<unsigned>(static_cast<uint8_t*>(object.ptr)[i]),
                        static_cast<unsigned>(static_cast<uint8_t>(object.startValue + i)));
        }

        selectedDeallocate(object.ptr);

        if (testEnumerator) {
            lock_guard<mutex> locker(lock);
            if (verbose)
                cout << "Freed object " << object.ptr << "\n";
            optionalObjects[threadIndex] = OptionalObject();
        }
    };

    auto shrinkSomething = [&] (unsigned threadIndex) {
        // We could actually test the enumerator here, but we don't because that would require adding
        // hacks to be tolerant of the weirdness the enumerator might see during the shrink. So for
        // now we just don't test it. FIXME: At least test that the enumerator doesn't crash when
        // dealing with shrink.
        PAS_ASSERT(!testEnumerator);

        Object object;
        size_t newSize;

        {
            lock_guard<mutex> locker(lock);
            if (objects.empty())
                return;
            unsigned index = deterministicRandomNumber(static_cast<unsigned>(objects.size()));
            object = objects[index];
            objects[index] = objects.back();
            objects.pop_back();
            totalSize -= object.size;

            if (verbose)
                cout << "Shrinking object " << object.ptr << "\n";
        }

        if (object.size)
            newSize = deterministicRandomNumber(static_cast<unsigned>(object.size));
        else
            newSize = 0;
        selectedShrink(object.ptr, newSize);
        object.size = newSize;

        {
            lock_guard<mutex> locker(lock);
            objects.push_back(object);
        }
    };

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_heap_lock_unlock();

    unsigned numEnumerations = 0;
    
    auto runEnumerator =
        [&] () {
            lock.lock();

            if (verbose) {
                cout << "Beginning enumeration, optionalObjects = " << "\n";
                for (size_t index = 0; index < optionalObjects.size(); ++index) {
                    if (!optionalObjects[index].isValid)
                        continue;
                    cout << "    " << index << ": ptr = " << optionalObjects[index].ptr
                         << ", size " << optionalObjects[index].size << "\n";
                }
            }
            
            for (pthread_t thread : runningThreads) {
                kern_return_t result = thread_suspend(pthread_mach_thread_np(thread));
                PAS_ASSERT(result == KERN_SUCCESS);
            }

            pageRanges.clear();
            readerCache.clear();
            recordedRanges.clear();

            addPageRange(
                pas_range_create(
                    pas_compact_heap_reservation_base + pas_compact_heap_reservation_guard_size,
                    pas_compact_heap_reservation_base + pas_compact_heap_reservation_guard_size
                    + pas_compact_heap_reservation_size));

            bool enumerateResult = pas_enumerable_range_list_iterate(
                &pas_enumerable_page_malloc_page_list, addPageRangeCallback, nullptr);
            PAS_ASSERT(enumerateResult);
            
            for (PageRange range : pageRanges) {
                int result = mprotect(range.base, range.size, PROT_NONE);
                PAS_ASSERT(!result);
            }

            pas_enumerator* enumerator = pas_enumerator_create(root,
                                                               enumeratorReader,
                                                               nullptr,
                                                               enumeratorRecorder,
                                                               nullptr,
                                                               pas_enumerator_record_meta_records,
                                                               pas_enumerator_record_payload_records,
                                                               pas_enumerator_record_object_records);
            
            pas_enumerator_enumerate_all(enumerator);

            auto checkNonOverlapping =
                [&] (const char* aSetName, set<RecordedRange>& aSet,
                     const char* bSetName, set<RecordedRange>& bSet,
                     bool allowIdentical) {
                    for (RecordedRange range : aSet) {
                        auto iter = bSet.lower_bound(range);
                        if (iter == bSet.end())
                            continue;
                        
                        if (allowIdentical
                            && iter->base == range.base
                            && iter->size == range.size)
                            continue;
                        
                        if (iter->base < range.end()) {
                            cout << "Recorded " << aSetName
                                 << " range " << range.base << "..." << range.end()
                                 << " overlaps with " << bSetName << " "<< iter->base << "..."
                                 << iter->end() << "\n";
                            CHECK_GREATER_EQUAL(iter->base, range.end());
                        }
                    }
                };

            // Check that the ranges are non-overlapping.
            for (auto& setEntry : recordedRanges) {
                checkNonOverlapping(pas_enumerator_record_kind_get_string(setEntry.first),
                                    setEntry.second,
                                    pas_enumerator_record_kind_get_string(setEntry.first),
                                    setEntry.second,
                                    true);
            }

            checkNonOverlapping("meta",
                                recordedRanges[pas_enumerator_meta_record],
                                "payload",
                                recordedRanges[pas_enumerator_payload_record],
                                false);
            checkNonOverlapping("payload",
                                recordedRanges[pas_enumerator_payload_record],
                                "meta",
                                recordedRanges[pas_enumerator_meta_record],
                                false);
            
            // Check that each object range is contained within a payload range.
            for (RecordedRange objectRange : recordedRanges[pas_enumerator_object_record]) {
                if (recordedRanges[pas_enumerator_payload_record].empty()) {
                    cout << "Recorded object range " << objectRange.base << "..." << objectRange.end()
                         << " is not within any payload range (no payload ranges)\n";
                    CHECK(!"Object range but no payload ranges");
                }
                auto iter = recordedRanges[pas_enumerator_payload_record].upper_bound(objectRange);
                if (iter == recordedRanges[pas_enumerator_payload_record].begin()) {
                    cout << "Recorded object range " << objectRange.base << "..." << objectRange.end()
                         << " is not within any payload range (no range to the left, range to the "
                         << "right is " << iter->base << "..." << iter->end() << ")\n";
                    CHECK(!"Object range not in any payload range");
                }
                --iter;
                PAS_ASSERT(iter->base <= objectRange.base);
                if (iter->end() < objectRange.base) {
                    cout << "Recorded object range " << objectRange.base << "..." << objectRange.end()
                         << " is not within any payload range (range to the left is " << iter->base
                         << "..." << iter->end() << ", ";
                    ++iter;
                    if (iter == recordedRanges[pas_enumerator_payload_record].end())
                        cout << "no range to the right";
                    else
                        cout << "range to the right is " << iter->base << "..." << iter->end();
                    cout << ")\n";
                    CHECK_GREATER_EQUAL(iter->end(), objectRange.base);
                }
                if (iter->end() < objectRange.end()) {
                    cout << "Recorded object range " << objectRange.base << "..." << objectRange.end()
                         << " is not entirely within the closest payload range " << iter->base
                         << "..." << iter->end() << "\n";
                    CHECK_GREATER_EQUAL(iter->end(), objectRange.end());
                }
            }

            // Make sure that the enumerator found all of the expected objects.
            set<void*> expectedObjects;
            for (Object object : objects) {
                expectedObjects.insert(object.ptr);
                auto iter = recordedRanges[pas_enumerator_object_record].find(
                    RecordedRange(object.ptr, object.size));
                if (iter == recordedRanges[pas_enumerator_object_record].end()) {
                    cout << "Expected object " << object.ptr << " with size " << object.size
                         << " not found by enumerator.\n";
                    CHECK(iter != recordedRanges[pas_enumerator_object_record].end());
                }
                if (iter->size < object.size) {
                    cout << "Expected object " << object.ptr << " with size " << object.size
                         << " was found by enumerator, but with size " << iter->size << ".\n";
                    CHECK_GREATER_EQUAL(iter->size, object.size);
                }
            }

            // Make sure that the enumerator only found objects that we expected, plus optional objects.
            vector<bool> foundOptionalObjects;
            for (size_t count = optionalObjects.size(); count--;)
                foundOptionalObjects.push_back(false);
            for (RecordedRange objectRange : recordedRanges[pas_enumerator_object_record]) {
                if (expectedObjects.count(objectRange.base))
                    continue;

                if (verbose) {
                    cout << "Found object " << objectRange.base << " with size " << objectRange.size
                         << " is either unexpected or optional.\n";
                }

                // Now check if the object is something we could be allocating or freeing. Note that if
                // we think that we're freeing an object, then we cannot really treat it differently, since
                // athough we know the address of freed objects, that address could be reused for
                // allocation just before we do the enumeration.
                size_t bestIndex = SIZE_MAX;
                for (size_t i = optionalObjects.size(); i--;) {
                    if (foundOptionalObjects[i])
                        continue;
                    if (optionalObjects[i].isValid
                        && optionalObjects[i].size <= objectRange.size) {
                        if (bestIndex == SIZE_MAX
                            || optionalObjects[i].size > optionalObjects[bestIndex].size)
                            bestIndex = i;
                    }
                }

                if (bestIndex == SIZE_MAX) {
                    cout << "Found object " << objectRange.base << " is not expected or optional.\n";
                    CHECK(bestIndex != SIZE_MAX);
                }
                
                if (verbose)
                    cout << "Checking off as optional index = " << bestIndex << "\n";
                // Don't allow the same allocating object to be found twice.
                foundOptionalObjects[bestIndex] = true;
            }

            pas_enumerator_destroy(enumerator);
            
            for (PageRange range : pageRanges) {
                int result = mprotect(range.base, range.size, PROT_READ | PROT_WRITE);
                PAS_ASSERT(!result);
            }

            if (verbose)
                cout << "Done enumerating!\n";

            ++numEnumerations;
            if (!(numEnumerations % 50))
                cout << "    Did " << numEnumerations << " enumerations.\n";
            
            for (pthread_t thread : runningThreads) {
                kern_return_t result = thread_resume(pthread_mach_thread_np(thread));
                PAS_ASSERT(result == KERN_SUCCESS);
            }

            lock.unlock();
        };

    if (testEnumerator)
        runEnumerator();
    
    for (unsigned allocationIndex = numInitialAllocations; allocationIndex--;)
        allocateSomething(0);

    if (testEnumerator)
        runEnumerator();
    
    unsigned numThreadsStopped = 0;
    bool didStartThreads = false;

    auto threadFunc = [&] (unsigned threadIndex) {
        if (verbose)
            cout << "    Thread " << (void*)pthread_self() << " starting.\n";
        {
            lock_guard<mutex> locker(lock);
            PAS_ASSERT(threads[threadIndex].get_id() == this_thread::get_id());
            runningThreads.insert(pthread_self());
        }
        for (unsigned actionIndex = numActions; actionIndex--;) {
            if (totalSize >= maxTotalSize) {
                freeSomething(threadIndex);
                continue;
            }
            
            switch (deterministicRandomNumber(selectedShrink ? 3 : 2)) {
            case 0:
                allocateSomething(threadIndex);
                break;
            case 1:
                freeSomething(threadIndex);
                break;
            case 2:
                PAS_ASSERT(selectedShrink);
                shrinkSomething(threadIndex);
                break;
            default:
                PAS_ASSERT(!"bad random number");
            }
        }
        if (verbose)
            cout << "    Thread " << (void*)pthread_self() << " stopping.\n";
        if (testEnumerator)
            pas_thread_local_cache_destroy(pas_lock_is_not_held);
        {
            lock_guard<mutex> locker(lock);
            runningThreads.erase(pthread_self());
            numThreadsStopped++;
        }
    };

    thread enumerationThread;
    if (testEnumerator) {
        enumerationThread = thread(
            [&] () {
                while (!didStartThreads || numThreadsStopped < threads.size()) {
                    // Sleep for a tiny bit.
                    for (unsigned count = 1000; count--;)
                        sched_yield();
                    
                    runEnumerator();
                }
            });
    }
    
    {
        lock_guard<mutex> locker(lock);
        for (unsigned threadIndex = 0; threadIndex < numThreads; ++threadIndex)
            threads.push_back(thread(threadFunc, threadIndex));
        didStartThreads = true;
    }

    if (verbose)
        cout << "Joining.\n";
    
    for (thread& thread : threads)
        thread.join();
    if (testEnumerator)
        enumerationThread.join();

    if (verbose)
        cout << "Done.\n";

    if (testEnumerator)
        cout << "    Test done; did " << numEnumerations << " enumerations.\n";
    
    CHECK_EQUAL(numThreadsStopped, threads.size());
    
    vector<pas_heap*> heaps;
    heaps.push_back(selectedCommonPrimitiveHeap);
    for (pas_heap_ref* heapRef : isolatedHeaps)
        heaps.push_back(selectedHeapRefGetHeap(heapRef));
    
    set<void*> expectedObjects;
    for (Object object : objects)
        expectedObjects.insert(object.ptr);
    
    verifyObjectSet(expectedObjects, heaps, [] (void*, size_t) { });

    pas_scavenger_run_synchronously_now();

    if ((false))
        printStatusReport();

    pas_heap_lock_lock();
    pas_all_heaps_verify_in_steady_state();
    pas_heap_lock_unlock();
}

void addTheTests(bool testEnumerator)
{
    ADD_TEST(testAllocationChaos(1, 10, 1000, 1000000, 10000000, false));
    ADD_TEST(testAllocationChaos(10, 10, 1000, 200000, 10000000, false));
    if (testEnumerator) {
        ADD_TEST(testAllocationChaos(1, 10, 1000, 7000, 10000000, true));
        ADD_TEST(testAllocationChaos(10, 10, 1000, 4000, 10000000, true));
    }
}

void addSpotTests(bool testEnumerator)
{
    addTheTests(testEnumerator);
    
    {
        ForceExclusives forceExclusives;
        DisableBitfit disableBitfit;
        addTheTests(testEnumerator);
    }
    
    {
        ForceTLAs forceTLAs;
        ForcePartials forcePartials;
        addTheTests(false);
    }
    
    {
        ForceBitfit forceBitfit;
        addTheTests(false);
    }
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO

void addIsoHeapChaosTests()
{
#if PAS_ENABLE_ISO
    {
        TestScope iso(
            "iso",
            [] () {
                selectedHeapConfig = &iso_heap_config;
                selectedAllocateCommonPrimitive = iso_try_allocate_common_primitive;
                selectedAllocate = iso_try_allocate;
                selectedAllocateArray = iso_try_allocate_array;
                selectedDeallocate = iso_deallocate;
                selectedCommonPrimitiveHeap = &iso_common_primitive_heap;
                selectedHeapRefGetHeap = iso_heap_ref_get_heap;
            });
    
        addTheTests(true);

        {
            DisableBitfit disableBitfit;
            addTheTests(false);
        }
    
        {
            ForceExclusives forceExclusives;
            addTheTests(false);
        }
    
        {
            ForceExclusives forceExclusives;
            DisableBitfit disableBitfit;
            addTheTests(true); // This tests enumeration with exclusives on for sure.
        }
    
        {
            ForceTLAs forceTLAs;
            ForcePartials forcePartials;
            addTheTests(true); // This tests enumeration with partials and TLAs on for sure.
        }
    
        {
            ForceBitfit forceBitfit;
            addTheTests(true); // This tests enumeration with bitfit on for sure.
        }
    
        {
            ForceBaselines forceBaselines;
            DisableBitfit disableBitfit;
            addTheTests(true); // This tests enumeration with baselines and segheaps on for sure.
        }
    
        {
            ForceTLAs forceTLAs;
            addTheTests(false);
        }
    
        {
            ForceBaselines forceBaselines;
            ForcePartials forcePartials;
            addTheTests(false);
        }

        {
            TestScope frequentScavenging(
                "frequenct-scavenging",
                [] () {
                    pas_scavenger_period_in_milliseconds = 1.;
                    pas_scavenger_max_epoch_delta = -1ll * 1000ll * 1000ll;
                });

            addSpotTests(false);
        }
    }

#if PAS_ENABLE_ISO_TEST
    {
        TestScope iso(
            "iso_test",
            [] () {
                selectedHeapConfig = &iso_test_heap_config;
                selectedAllocateCommonPrimitive = iso_test_allocate_common_primitive;
                selectedAllocate = iso_test_allocate;
                selectedAllocateArray = iso_test_allocate_array;
                selectedDeallocate = iso_test_deallocate;
                selectedCommonPrimitiveHeap = &iso_test_common_primitive_heap;
                selectedHeapRefGetHeap = iso_test_heap_ref_get_heap;
            });

        addSpotTests(false);
    }
#endif // PAS_ENABLE_ISO_TEST

#if PAS_ENABLE_MINALIGN32
    {
        TestScope iso(
            "minalign32",
            [] () {
                selectedHeapConfig = &minalign32_heap_config;
                selectedAllocateCommonPrimitive = minalign32_allocate_common_primitive;
                selectedAllocate = minalign32_allocate;
                selectedAllocateArray = minalign32_allocate_array;
                selectedDeallocate = minalign32_deallocate;
                selectedCommonPrimitiveHeap = &minalign32_common_primitive_heap;
                selectedHeapRefGetHeap = minalign32_heap_ref_get_heap;
            });
    
        addSpotTests(false);
    }
#endif // PAS_ENABLE_MINALIGN32

#if PAS_ENABLE_PAGESIZE64K
    {
        TestScope iso(
            "pagesize64k",
            [] () {
                selectedHeapConfig = &pagesize64k_heap_config;
                selectedAllocateCommonPrimitive = pagesize64k_allocate_common_primitive;
                selectedAllocate = pagesize64k_allocate;
                selectedAllocateArray = pagesize64k_allocate_array;
                selectedDeallocate = pagesize64k_deallocate;
                selectedCommonPrimitiveHeap = &pagesize64k_common_primitive_heap;
                selectedHeapRefGetHeap = pagesize64k_heap_ref_get_heap;
            });
    
        addSpotTests(true);
    }
#endif // PAS_ENABLE_PAGESIZE64K

#if PAS_ENABLE_BMALLOC
    {
        TestScope iso(
            "bmalloc",
            [] () {
                selectedHeapConfig = &bmalloc_heap_config;
                selectedAllocateCommonPrimitive = bmalloc_allocate;
                selectedAllocate = bmalloc_iso_allocate;
                selectedAllocateArray = nullptr;
                selectedDeallocate = bmalloc_deallocate;
                selectedCommonPrimitiveHeap = &bmalloc_common_primitive_heap;
                selectedHeapRefGetHeap = bmalloc_heap_ref_get_heap;
            });
    
        addSpotTests(false);
    }
#endif // PAS_ENABLE_BMALLOC

#if PAS_ENABLE_HOTBIT
    {
        TestScope iso(
            "hotbit",
            [] () {
                selectedHeapConfig = &hotbit_heap_config;
                selectedAllocateCommonPrimitive = hotbit_try_allocate;
                selectedAllocate = nullptr;
                selectedAllocateArray = nullptr;
                selectedDeallocate = hotbit_deallocate;
                selectedCommonPrimitiveHeap = &hotbit_common_primitive_heap;
                selectedHeapRefGetHeap = nullptr;
            });
    
        addSpotTests(false);
    }
#endif // PAS_ENABLE_HOTBIT

#if PAS_ENABLE_JIT
    {
        TestScope iso(
            "jit",
            [] () {
                selectedHeapConfig = &jit_heap_config;
                selectedAllocateCommonPrimitive = jit_heap_try_allocate;
                selectedAllocate = nullptr;
                selectedAllocateArray = nullptr;
                selectedDeallocate = jit_heap_deallocate;
                selectedCommonPrimitiveHeap = &jit_common_primitive_heap;
                selectedHeapRefGetHeap = nullptr;
            });

        BootJITHeap bootJITHeap;

        addTheTests(true);
    }

    {
        TestScope iso(
            "jit-with-shrink",
            [] () {
                selectedHeapConfig = &jit_heap_config;
                selectedAllocateCommonPrimitive = jit_heap_try_allocate;
                selectedAllocate = nullptr;
                selectedAllocateArray = nullptr;
                selectedShrink = jit_heap_shrink;
                selectedDeallocate = jit_heap_deallocate;
                selectedCommonPrimitiveHeap = &jit_common_primitive_heap;
                selectedHeapRefGetHeap = nullptr;
            });

        BootJITHeap bootJITHeap;

        addTheTests(false);
    }
#endif // PAS_ENABLE_HOTBIT
#endif // PAS_ENABLE_ISO
}

