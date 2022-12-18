/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#if PAS_ENABLE_THINGY

#include "HeapLocker.h"
#include "SuspendScavenger.h"
#include "thingy_heap_config.h"
#include <functional>
#include <map>
#include "pas_all_heaps.h"
#include "pas_baseline_allocator_table.h"
#include "pas_bootstrap_free_heap.h"
#include "pas_get_object_kind.h"
#include "pas_heap.h"
#include "pas_heap_config.h"
#include "pas_heap_lock.h"
#include "pas_heap_ref.h"
#include "pas_intrinsic_heap_support.h"
#include "pas_large_free_heap_helpers.h"
#include "pas_large_sharing_pool.h"
#include "pas_large_utility_free_heap.h"
#include "pas_megapage_cache.h"
#include "pas_page_malloc.h"
#include "pas_scavenger.h"
#include "pas_segregated_size_directory.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_thread_local_cache.h"
#include "pas_utility_heap.h"
#include "thingy_heap.h"
#include <set>
#include <thread>
#include <vector>

using namespace std;

namespace {

#if SEGHEAP
void flushDeallocationLog()
{
#if TLC
    pas_thread_local_cache_flush_deallocation_log(pas_thread_local_cache_try_get(),
                                                  pas_lock_is_not_held);
#endif
}
#endif

void flushDeallocationLogAndStopAllocators()
{
#if TLC
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(), pas_lock_is_not_held);
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
#endif
}

// FIXME: Once we add real scavenging, we will want to add a shrinkHeaps() function that does
// flushDeallocationLogAndStopAllocators() plus the actual scavenging.

pas_segregated_size_directory* sizeClassFor(void* object)
{
#if SEGHEAP
    return pas_segregated_size_directory_for_object(
        reinterpret_cast<uintptr_t>(object), &thingy_heap_config);
#else
    return nullptr;
#endif
}

bool isLarge(void* object)
{
    return pas_get_object_kind(object, THINGY_HEAP_CONFIG) == pas_large_object_kind;
}

bool forEachLiveUtilityObjectAdapter(uintptr_t begin,
                                     size_t size,
                                     void* arg)
{
    function<bool(void*, size_t)>* callback = reinterpret_cast<function<bool(void*, size_t)>*>(arg);
    return (*callback)(reinterpret_cast<void*>(begin), size);
}

void forEachLiveUtilityObject(function<bool(void*, size_t)> callback)
{
    SuspendScavenger suspendScavenger;
    HeapLocker heapLocker;
    pas_utility_heap_for_each_live_object(forEachLiveUtilityObjectAdapter, &callback);
}

#if SEGHEAP
bool forEachCommittedViewAdapter(pas_segregated_heap* heap,
                                 pas_segregated_size_directory* sizeClass,
                                 pas_segregated_view view,
                                 void* arg)
{
    function<bool(pas_segregated_view)>* callback =
        reinterpret_cast<function<bool(pas_segregated_view)>*>(arg);
    return (*callback)(view);
}

void forEachCommittedView(pas_heap* heap, function<bool(pas_segregated_view)> callback)
{
    pas_segregated_heap_for_each_committed_view(&heap->segregated_heap,
                                                forEachCommittedViewAdapter,
                                                &callback);
}

size_t numCommittedViews(pas_heap* heap)
{
    return pas_segregated_heap_num_committed_views(&heap->segregated_heap);
}
#endif // SEGHEAP

#if TLC
size_t numViews(pas_heap* heap)
{
    return pas_segregated_heap_num_views(&heap->segregated_heap);
}
#endif // TLC

void verifyMinimumObjectDistance(const set<void*>& objects,
                                 size_t minimumDistance)
{
    uintptr_t lastBegin = 0;
    for (void* object : objects) {
        uintptr_t begin = reinterpret_cast<uintptr_t>(object);
        
        CHECK_GREATER_EQUAL(begin, lastBegin);
        
        if (begin < lastBegin + minimumDistance) {
            cout << "Object " << reinterpret_cast<void*>(lastBegin) << " is too close to " << object << endl;
            dumpObjectSet(objects);
        }
        
        CHECK_GREATER_EQUAL(begin - lastBegin, minimumDistance);
        
        lastBegin = begin;
    }
}

template<typename ObjectVerificationFunc>
void verifyObjectSet(const set<void*>& expectedObjects,
                     const vector<pas_heap*>& heaps,
                     const ObjectVerificationFunc& objectVerificationFunc)
{
    static constexpr bool verbose = false;
    
    flushDeallocationLogAndStopAllocators();

    if (verbose)
        cout << "Iterating live objects...\n";
    
    set<void*> foundObjects;
    for (pas_heap* heap : heaps) {
        forEachLiveObject(
            heap,
            [&] (void* object, size_t size) -> bool {
                if (verbose)
                    cout << "Found " << object << " with size " << size << "\n";
                CHECK(expectedObjects.count(object));
                CHECK(!foundObjects.count(object));
                objectVerificationFunc(object, size);
                foundObjects.insert(object);
                return true;
            });
    }
    CHECK_EQUAL(foundObjects.size(), expectedObjects.size());
}

template<typename ObjectVerificationFunc>
void verifyUtilityObjectSet(const set<void*>& expectedObjects,
                            const ObjectVerificationFunc& objectVerificationFunc)
{
    flushDeallocationLogAndStopAllocators();
    
    set<void*> foundObjects;
    set<void*> objectsToFind = expectedObjects;
    forEachLiveUtilityObject(
        [&] (void* object, size_t size) -> bool {
            if (!objectsToFind.count(object))
                return true;
            CHECK(!foundObjects.count(object));
            objectVerificationFunc(object, size);
            foundObjects.insert(object);
            objectsToFind.erase(object);
            return true;
        });
    CHECK(!objectsToFind.size());
    CHECK_GREATER_EQUAL(foundObjects.size(), expectedObjects.size());
}

template<typename ObjectVerificationFunc>
void verifyObjectSet(const set<void*>& expectedObjects,
                     pas_heap* heap,
                     const ObjectVerificationFunc& objectVerificationFunc)
{
    vector<pas_heap*> heaps = { heap };
    verifyObjectSet(expectedObjects, heaps, objectVerificationFunc);
}

void verifyObjectSet(const set<void*>& expectedObjects,
                     pas_heap* heap,
                     size_t resultingObjectSize)
{
    verifyObjectSet(
        expectedObjects,
        heap,
        [&] (void* object, size_t size) {
            CHECK_EQUAL(size, resultingObjectSize);
        });
}

void verifyObjectSet(const map<void*, size_t>& objectByPtr,
                     pas_heap* heap)
{
    set<void*> expectedObjects;
    for (const auto& pair : objectByPtr)
        expectedObjects.insert(pair.first);
    
    verifyObjectSet(
        expectedObjects,
        heap,
        [] (void*, size_t) { });
}

void verifyHeapEmpty(pas_heap* heap)
{
    pas_scavenger_suspend();
    
    flushDeallocationLogAndStopAllocators();
    
    forEachLiveObject(
        heap,
        [&] (void* object, size_t size) -> bool {
            CHECK(!"should not have found any objects");
            return true;
        });
    
#if SEGHEAP
    forEachCommittedView(
        heap,
        [&] (pas_segregated_view view) -> bool {
            CHECK(pas_segregated_view_is_eligible(view));
#if TLC
            CHECK(pas_segregated_view_is_empty(view));
#endif
            return true;
        });
#endif // SEGHEAP

    pas_scavenger_resume();
}

class Allocator {
public:
    virtual ~Allocator() = default;
    virtual void* allocate() const = 0;
    virtual pas_heap* heap() const = 0;
#if PAS_ENABLE_TESTING
    virtual uint64_t allocateSlowPathCount() const = 0;
#endif // PAS_ENABLE_TESTING
};

class PrimitiveAllocator : public Allocator {
public:
    PrimitiveAllocator(size_t objectSize)
        : m_objectSize(objectSize)
    {
    }
    
    void* allocate() const override
    {
        return thingy_try_allocate_primitive(m_objectSize);
    }

    pas_heap* heap() const override
    {
        return &thingy_primitive_heap;
    }

#if PAS_ENABLE_TESTING
    uint64_t allocateSlowPathCount() const override
    {
        return thingy_allocator_counts.slow_paths +
            thingy_allocator_counts.slow_refills;
    }
#endif // PAS_ENABLE_TESTING
    
private:
    size_t m_objectSize;
};

class PrimitiveReallocAllocator : public Allocator {
public:
    PrimitiveReallocAllocator(size_t objectSize)
        : m_objectSize(objectSize)
    {
    }
    
    void* allocate() const override
    {
        return thingy_try_reallocate_primitive(nullptr, m_objectSize);
    }

    pas_heap* heap() const override
    {
        return &thingy_primitive_heap;
    }

#if PAS_ENABLE_TESTING
    uint64_t allocateSlowPathCount() const override
    {
        return thingy_allocator_counts.slow_paths +
            thingy_allocator_counts.slow_refills;
    }
#endif // PAS_ENABLE_TESTING
    
private:
    size_t m_objectSize;
};

class AlignedPrimitiveAllocator : public Allocator {
public:
    AlignedPrimitiveAllocator(size_t objectSize, size_t alignment)
        : m_objectSize(objectSize)
        , m_alignment(alignment)
    {
    }
    
    void* allocate() const override
    {
        void* result = thingy_try_allocate_primitive_with_alignment(m_objectSize, m_alignment);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(result), m_alignment));
        return result;
    }
    
    pas_heap* heap() const override
    {
        return &thingy_primitive_heap;
    }

#if PAS_ENABLE_TESTING
    uint64_t allocateSlowPathCount() const override
    {
        return thingy_allocator_counts.slow_paths +
            thingy_allocator_counts.slow_refills;
    }
#endif // PAS_ENABLE_TESTING
    
private:
    size_t m_objectSize;
    size_t m_alignment;
};

pas_heap_ref* createIsolatedHeapRef(size_t size, size_t alignment)
{
    pas_heap_ref* heapRef = new pas_heap_ref;
    heapRef->type = reinterpret_cast<const pas_heap_type*>(PAS_SIMPLE_TYPE_CREATE(size, alignment));
    heapRef->heap = nullptr;
#if TLC
    heapRef->allocator_index = 0;
#endif
    
    return heapRef;
}

class IsolatedHeapAllocator : public Allocator {
public:
    IsolatedHeapAllocator(pas_heap_ref* heapRef)
        : m_heapRef(heapRef)
    {
    }
    
    IsolatedHeapAllocator(size_t size, size_t alignment)
        : IsolatedHeapAllocator(createIsolatedHeapRef(size, alignment))
    {
    }
    
    void* allocate() const override
    {
        void* result = thingy_try_allocate(m_heapRef);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(result),
                             pas_simple_type_alignment(reinterpret_cast<pas_simple_type>(m_heapRef->type))));
        return result;
    }
    
    pas_heap* heap() const override
    {
        return thingy_heap_ref_get_heap(m_heapRef);
    }

#if PAS_ENABLE_TESTING
    uint64_t allocateSlowPathCount() const override
    {
        return thingy_allocator_counts.slow_paths +
            thingy_allocator_counts.slow_refills;
    }
#endif // PAS_ENABLE_TESTING
    
private:
    pas_heap_ref* m_heapRef;
};

class IsolatedHeapArrayAllocator : public Allocator {
public:
    IsolatedHeapArrayAllocator(pas_heap_ref* heapRef, size_t count, size_t alignment)
        : m_heapRef(heapRef)
        , m_count(count)
        , m_alignment(alignment)
    {
    }
    
    IsolatedHeapArrayAllocator(size_t size, size_t typeAlignment, size_t count, size_t alignment)
        : IsolatedHeapArrayAllocator(createIsolatedHeapRef(size, typeAlignment), count, alignment)
    {
    }
    
    void* allocate() const override
    {
        void* result = thingy_try_allocate_array(m_heapRef, m_count, m_alignment);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(result),
                             pas_simple_type_alignment(reinterpret_cast<pas_simple_type>(m_heapRef->type))));
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(result), m_alignment));
        return result;
    }
    
    pas_heap* heap() const override
    {
        return thingy_heap_ref_get_heap(m_heapRef);
    }

#if PAS_ENABLE_TESTING
    uint64_t allocateSlowPathCount() const override
    {
        return thingy_allocator_counts.slow_paths +
            thingy_allocator_counts.slow_refills;
    }
#endif // PAS_ENABLE_TESTING
    
private:
    pas_heap_ref* m_heapRef;
    size_t m_count;
    size_t m_alignment;
};

class CounterScope {
public:
    CounterScope(function<uint64_t()> getCount, uint64_t expectedCountAtEnd)
        : m_getCount(getCount)
        , m_countAtBegin(getCount())
        , m_expectedCountAtEnd(expectedCountAtEnd)
    {
    }
    
    ~CounterScope()
    {
        CHECK_EQUAL(m_getCount() - m_countAtBegin, m_expectedCountAtEnd);
    }

private:
    function<uint64_t()> m_getCount;
    uint64_t m_countAtBegin;
    uint64_t m_expectedCountAtEnd;
};

void testDeallocateNull()
{
    thingy_deallocate(nullptr);
}

void deallocationFailureCallback(const char* reason, void* begin)
{
    cout << "    Failed deallocation: " << reason << ": " << begin << endl;
    testSucceeded();
}

class DeallocationShouldFail {
public:
    DeallocationShouldFail()
    {
        pas_set_deallocation_did_fail_callback(deallocationFailureCallback);
    }
    
    ~DeallocationShouldFail()
    {
        CHECK(!"Should not have succeeded.");
    }
};

void testDeallocateMalloc()
{
    DeallocationShouldFail deallocationShouldFail;
    thingy_deallocate(malloc(1));
}

void testDeallocateStack()
{
    DeallocationShouldFail deallocationShouldfail;
    int stack;
    thingy_deallocate(&stack);
}

template<typename ObjectVerifyFunc, typename BeforeFreeVerifyFunc>
void testSimpleAllocation(const Allocator& allocator,
                          size_t resultingObjectSize,
                          size_t count,
                          bool verifyBeforeFreeing,
                          const ObjectVerifyFunc& objectVerifyFunc,
                          const BeforeFreeVerifyFunc& beforeFreeVerifyFunc)
{
    set<void*> objects;
    
#if TLC
    pas_scavenger_suspend();
    pas_large_sharing_pool_validate_each_splat = true;
#endif
    
    for (size_t index = count; index--;) {
        void* object = allocator.allocate();
        CHECK(object);
        objectVerifyFunc(object);
        CHECK(pas_is_aligned(
                  reinterpret_cast<uintptr_t>(object),
                  pas_simple_type_alignment(reinterpret_cast<pas_simple_type>(allocator.heap()->type))));
        CHECK(!objects.count(object));
        CHECK_EQUAL(thingy_get_allocation_size(object), resultingObjectSize);
        objects.insert(object);
    }
    
    verifyMinimumObjectDistance(objects, resultingObjectSize);
    
    beforeFreeVerifyFunc();
    
    CHECK_EQUAL(objects.size(), count);
    
    if (verifyBeforeFreeing)
        verifyObjectSet(objects, allocator.heap(), resultingObjectSize);
    
    for (void* object : objects)
        thingy_deallocate(object);
    
    verifyHeapEmpty(allocator.heap());
    
    CHECK_EQUAL(pas_scavenger_current_state, pas_scavenger_state_no_thread);
}

#if SEGHEAP
void testSmallOrMediumAllocation(const Allocator& allocator,
                                 size_t resultingObjectSize,
                                 pas_object_kind expectedObjectKind,
                                 size_t count,
                                 bool verifyBeforeFreeing,
                                 size_t expectedNumberOfCommittedPages,
                                 uint64_t expectedAllocateSlowPathCount)
{
    testSimpleAllocation(
        allocator,
        resultingObjectSize,
        count,
        verifyBeforeFreeing,
        [&] (void* object) {
            CHECK_EQUAL(pas_get_object_kind(object, THINGY_HEAP_CONFIG),
                        expectedObjectKind);
        },
        [&] () {
            CHECK_EQUAL(numCommittedViews(allocator.heap()), expectedNumberOfCommittedPages);
        });
    
    CHECK_EQUAL(numCommittedViews(allocator.heap()), expectedNumberOfCommittedPages);
#if PAS_ENABLE_TESTING
    CHECK_EQUAL(allocator.allocateSlowPathCount(), expectedAllocateSlowPathCount);
#endif // PAS_ENABLE_TESTING
}

void testSmallAllocation(const Allocator& allocator,
                         size_t resultingObjectSize,
                         size_t count,
                         bool verifyBeforeFreeing,
                         size_t expectedNumberOfCommittedPages,
                         uint64_t expectedAllocateSlowPathCount)
{
    testSmallOrMediumAllocation(allocator, resultingObjectSize, pas_small_segregated_object_kind,
                                count, verifyBeforeFreeing, expectedNumberOfCommittedPages,
                                expectedAllocateSlowPathCount);
}
#endif // SEGHEAP

void testLargeAllocation(const Allocator& allocator,
                         size_t resultingObjectSize,
                         size_t count,
                         size_t expectedNumberOfAllocatedPageBytes,
                         size_t expectedNumberOfBootstrapBytes)
{
    static constexpr bool verbose = false;
    uintptr_t lastObject = 0;
    testSimpleAllocation(
        allocator,
        resultingObjectSize,
        count,
        true,
        [&] (void* object) {
            if (verbose) {
                cout << "Allocated object at " << object << " at delta "
                     << static_cast<intptr_t>(reinterpret_cast<uintptr_t>(object) - lastObject)
                     << "\n";
            }
            lastObject = reinterpret_cast<uintptr_t>(object);
            CHECK(isLarge(object));
        },
        [&] () {
            CHECK_EQUAL(
                allocator.heap()->large_heap.free_heap.num_mapped_bytes,
                expectedNumberOfAllocatedPageBytes);
            CHECK_LESS_EQUAL(pas_bootstrap_free_heap.num_mapped_bytes,
                             expectedNumberOfBootstrapBytes);
        });
    
    CHECK_EQUAL(allocator.heap()->large_heap.free_heap.num_mapped_bytes,
                expectedNumberOfAllocatedPageBytes);
    CHECK_LESS_EQUAL(pas_bootstrap_free_heap.num_mapped_bytes,
                     expectedNumberOfBootstrapBytes);
}

#if SEGHEAP
void testAllocationWithInterleavedFragmentation(const Allocator& allocator,
                                                size_t resultingObjectSize,
                                                size_t count,
                                                bool verifyBeforeFreeing,
                                                size_t expectedNumberOfCommittedPages,
                                                size_t expectedNumberOfCommittedPagesAtEnd,
                                                uint64_t expectedAllocateSlowPathCount)
{
    static constexpr bool verbose = false;
    
    set<void*> objects;
    vector<void*> objectList;
    
#if TLC
    pas_scavenger_suspend();
    pas_large_sharing_pool_validate_each_splat = true;
#endif
    
    for (size_t index = count; index--;) {
        void* object = allocator.allocate();
        CHECK(object);
        CHECK(!objects.count(object));
        objects.insert(object);
        objectList.push_back(object);
    }
    
    verifyMinimumObjectDistance(objects, resultingObjectSize);
    
    CHECK_EQUAL(numCommittedViews(allocator.heap()), expectedNumberOfCommittedPages);
    CHECK_EQUAL(objects.size(), count);
    
    if (verifyBeforeFreeing)
        verifyObjectSet(objects, allocator.heap(), resultingObjectSize);
    
    for (size_t index = 0; index < objectList.size(); index += 2) {
        thingy_deallocate(objectList[index]);
        objects.erase(objectList[index]);
    }
    
    CHECK_EQUAL(numCommittedViews(allocator.heap()), expectedNumberOfCommittedPages);

    if (verifyBeforeFreeing)
        verifyObjectSet(objects, allocator.heap(), resultingObjectSize);

    if (verbose)
        printStatusReport();
    
    for (size_t index = count / 2; index--;) {
        void* object = allocator.allocate();
        CHECK(object);
        CHECK(!objects.count(object));
        objects.insert(object);
    }
    
    verifyMinimumObjectDistance(objects, resultingObjectSize);

    if (verbose)
        printStatusReport();
    CHECK_EQUAL(numCommittedViews(allocator.heap()), expectedNumberOfCommittedPagesAtEnd);
    
    if (verifyBeforeFreeing)
        verifyObjectSet(objects, allocator.heap(), resultingObjectSize);
    
    for (void* object : objects)
        thingy_deallocate(object);
    
    CHECK_EQUAL(numCommittedViews(allocator.heap()), expectedNumberOfCommittedPagesAtEnd);
    
    verifyHeapEmpty(allocator.heap());

#if PAS_ENABLE_TESTING
    CHECK_EQUAL(allocator.allocateSlowPathCount(), expectedAllocateSlowPathCount);
#endif // PAS_ENABLE_TESTING

    CHECK_EQUAL(pas_scavenger_current_state, pas_scavenger_state_no_thread);
}

void testFreeListRefillSpans(unsigned prewarmObjectSize,
                             unsigned objectSize,
                             unsigned numberOfSpans,
                             unsigned firstSpanToFree,
                             pas_segregated_page_config_variant variant)
{
    size_t numExtraCommittedPages = 0;
    
#if TLC
    pas_scavenger_suspend();
    pas_large_sharing_pool_validate_each_splat = true;
#endif

    if (prewarmObjectSize) {
        // Need this to skip the weird page at the start of the megapage.
        thingy_try_allocate_primitive(prewarmObjectSize);

#if PAS_ENABLE_TESTING
        thingy_allocator_counts.slow_paths = 0;
        thingy_allocator_counts.slow_refills = 0;
#endif // PAS_ENABLE_TESTING
        CHECK_EQUAL(numCommittedViews(&thingy_primitive_heap), 1);
        numExtraCommittedPages = 1;
    }
    
    unsigned numberOfObjects = pas_segregated_page_number_of_objects(
        objectSize,
        *pas_heap_config_segregated_page_config_ptr_for_variant(
            &thingy_heap_config,
            variant),
        pas_segregated_page_exclusive_role);
    unsigned numberOfObjectsPerSpan = numberOfObjects / numberOfSpans;
    CHECK(numberOfObjectsPerSpan >= 1);
    unsigned numberOfObjectsInLastSpan = numberOfObjects - numberOfObjectsPerSpan * (numberOfSpans - 1);
    
    vector<vector<void*>> objects;
    unsigned actualNumberOfObjects = 0;
    for (unsigned span = numberOfSpans; span--;) {
        unsigned numberOfObjectsInThisSpan = span ? numberOfObjectsPerSpan : numberOfObjectsInLastSpan;
        vector<void*> objectsInThisSpan;
        for (unsigned objectIndex = numberOfObjectsInThisSpan; objectIndex--;) {
            void* ptr = thingy_try_allocate_primitive(objectSize);
            CHECK(ptr);
            CHECK_EQUAL(pas_get_object_kind(ptr, THINGY_HEAP_CONFIG),
                        pas_object_kind_for_segregated_variant(variant));
            objectsInThisSpan.push_back(ptr);
            actualNumberOfObjects++;
        }
        objects.push_back(std::move(objectsInThisSpan));
    }

#if PAS_ENABLE_TESTING
    CHECK_EQUAL(thingy_allocator_counts.slow_paths, 1);
    CHECK_EQUAL(thingy_allocator_counts.slow_refills, 1);
#endif // PAS_ENABLE_TESTING

    CHECK_EQUAL(objects.size(), numberOfSpans);
    CHECK_EQUAL(actualNumberOfObjects, numberOfObjects);
    
    unsigned numberOfObjectsFreed = 0;
    set<void*> freedObject;
    for (unsigned span = firstSpanToFree; span < objects.size(); span += 2) {
        for (void* object : objects[span]) {
            thingy_deallocate(object);
            freedObject.insert(object);
            numberOfObjectsFreed++;
        }
    }
    
    flushDeallocationLogAndStopAllocators();
    
    for (unsigned objectIndex = numberOfObjectsFreed; objectIndex--;) {
        void* ptr = thingy_try_allocate_primitive(objectSize);
        CHECK(ptr);
        CHECK(freedObject.count(ptr));
        freedObject.erase(ptr);
    }

#if PAS_ENABLE_TESTING
    CHECK_EQUAL(thingy_allocator_counts.slow_paths, 1);
    CHECK_EQUAL(thingy_allocator_counts.slow_refills, 2);
#endif // PAS_ENABLE_TESTING
    CHECK_EQUAL(numCommittedViews(&thingy_primitive_heap), 1 + numExtraCommittedPages);

    CHECK_EQUAL(pas_scavenger_current_state, pas_scavenger_state_no_thread);
}
#endif // SEGHEAP

#if TLC
void testInternalScavenge(unsigned firstObjectSize,
                          size_t resultingFirstObjectSize,
                          size_t firstCount,
                          bool verifyBeforeFreeing,
                          bool verifyBeforeReusing,
                          unsigned secondObjectSize,
                          size_t resultingSecondObjectSize,
                          size_t secondCount,
                          bool verifyAfterReusing,
                          size_t thirdCount,
                          bool verifyAfterEverything,
                          size_t expectedNumberOfCommittedPages,
                          size_t expectedNumberOfPageIndices,
                          size_t expectedNumberOfCommittedPagesAfterReusing,
                          size_t expectedNumberOfPageIndicesAfterReusing,
                          size_t expectedNumberOfCommittedPagesAtEnd,
                          size_t expectedNumberOfPageIndicesAtEnd,
                          bool checkHeapLock)
{
    pas_scavenger_suspend();
    pas_large_sharing_pool_validate_each_splat = true;
    pas_physical_page_sharing_pool_balancing_enabled = true;
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;

    set<void*> objects;
    vector<void*> objectList;
    
    pas_page_sharing_pool_verify(
        &pas_physical_page_sharing_pool,
        pas_lock_is_not_held);
    
    for (size_t index = firstCount; index--;) {
        pas_page_sharing_pool_verify(
            &pas_physical_page_sharing_pool,
            pas_lock_is_not_held);
        void* object = thingy_try_allocate_primitive(firstObjectSize);
        CHECK(object);
        CHECK(!objects.count(object));
        objects.insert(object);
        objectList.push_back(object);
        CHECK_EQUAL(sizeClassFor(object)->object_size, resultingFirstObjectSize);
    }
    
    if (verifyBeforeFreeing)
        verifyObjectSet(objects, &thingy_primitive_heap, resultingFirstObjectSize);
    pas_page_sharing_pool_verify(
        &pas_physical_page_sharing_pool,
        pas_lock_is_not_held);
    
    for (void* object : objectList) {
        thingy_deallocate(object);
        objects.erase(object);
    }
    objectList.clear();
    
    CHECK_EQUAL(numCommittedViews(&thingy_primitive_heap),
                expectedNumberOfCommittedPages);
    CHECK_EQUAL(numViews(&thingy_primitive_heap),
                expectedNumberOfPageIndices);

    if (verifyBeforeReusing)
        verifyHeapEmpty(&thingy_primitive_heap);
    pas_page_sharing_pool_verify(
        &pas_physical_page_sharing_pool,
        pas_lock_is_not_held);

    // This test assumes that we will allocate and free enough objects of the first size that even
    // if we don't verifyBeforeReusing (which shrinks the heap), we will end up having free pages
    // in the first size class. That will have to trigger a delta if the use_epoch didn't do that
    // already.
    CHECK(pas_page_sharing_pool_has_delta(&pas_physical_page_sharing_pool));

    for (size_t index = 0; index < secondCount; ++index) {
        pas_page_sharing_pool_verify(
            &pas_physical_page_sharing_pool,
            pas_lock_is_not_held);
        if (index == 1) {
            // This test assumes that we will allocate and free enough objects of the first size
            // that even if we don't verifyBeforeReusing (which shrinks the heap), we will end up
            // reusing pages from the first size class.
            CHECK(pas_page_sharing_pool_has_current_participant(&pas_physical_page_sharing_pool));
        }
        void* object = thingy_try_allocate_primitive(secondObjectSize);
    
        CHECK(object);
        CHECK(!objects.count(object));
        objects.insert(object);
        objectList.push_back(object);
        CHECK_EQUAL(sizeClassFor(object)->object_size, resultingSecondObjectSize);
    }
    
    if (verifyAfterReusing)
        verifyObjectSet(objects, &thingy_primitive_heap, resultingSecondObjectSize);
    pas_page_sharing_pool_verify(
        &pas_physical_page_sharing_pool,
        pas_lock_is_not_held);
    
    CHECK_EQUAL(numCommittedViews(&thingy_primitive_heap),
                expectedNumberOfCommittedPagesAfterReusing);
    CHECK_EQUAL(numViews(&thingy_primitive_heap),
                expectedNumberOfPageIndicesAfterReusing);
    
    if (checkHeapLock) {
        for (void* object : objectList) {
            thingy_deallocate(object);
            objects.erase(object);
        }
        objectList.clear();
        
        flushDeallocationLogAndStopAllocators();
    }
    
    for (size_t index = 0; index < thirdCount; ++index) {
        pas_page_sharing_pool_verify(
            &pas_physical_page_sharing_pool,
            pas_lock_is_not_held);
        void* object = thingy_try_allocate_primitive(firstObjectSize);
        CHECK(object);
        CHECK(!objects.count(object));
        objects.insert(object);
        CHECK_EQUAL(sizeClassFor(object)->object_size, resultingFirstObjectSize);
    }
    
    if (verifyAfterEverything) {
        verifyObjectSet(
            objects, &thingy_primitive_heap, 
            [&] (void* object, size_t size) {
                CHECK(size == resultingFirstObjectSize || size == resultingSecondObjectSize);
            });
    }
    pas_page_sharing_pool_verify(
        &pas_physical_page_sharing_pool,
        pas_lock_is_not_held);
    
    CHECK_EQUAL(numCommittedViews(&thingy_primitive_heap),
                expectedNumberOfCommittedPagesAtEnd);
    CHECK_EQUAL(numViews(&thingy_primitive_heap),
                expectedNumberOfPageIndicesAtEnd);
    
    CHECK_EQUAL(pas_scavenger_current_state, pas_scavenger_state_no_thread);
}

void testInternalScavengeFromCorrectDirectory(size_t firstSize, size_t secondSize, size_t thirdSize)
{
    pas_scavenger_suspend();
    pas_large_sharing_pool_validate_each_splat = true;
    pas_physical_page_sharing_pool_balancing_enabled = true;
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = false;
    pas_large_utility_free_heap_talks_to_large_sharing_pool = false;

    void* object1 = thingy_try_allocate_primitive(firstSize);
    CHECK(object1);

    pas_segregated_view view1 = pas_segregated_view_for_object(reinterpret_cast<uintptr_t>(object1),
                                                               &thingy_heap_config);
    
    pas_segregated_size_directory* directory1 = pas_segregated_size_directory_for_object(
        reinterpret_cast<uintptr_t>(object1), &thingy_heap_config);
    
    CHECK_EQUAL(directory1->object_size, firstSize);
    CHECK_EQUAL(pas_segregated_directory_size(&directory1->base), 1);
    CHECK(!pas_segregated_directory_is_eligible(&directory1->base, 0));
    CHECK(!pas_segregated_directory_is_empty(&directory1->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory1->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory1->base, 0),
        view1);
    
    flushDeallocationLogAndStopAllocators();

    CHECK_EQUAL(pas_segregated_directory_size(&directory1->base), 1);
    CHECK(pas_segregated_directory_is_eligible(&directory1->base, 0));
    CHECK(!pas_segregated_directory_is_empty(&directory1->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory1->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory1->base, 0),
        view1);
    
    void* object2 = thingy_try_allocate_primitive(secondSize);
    CHECK(object2);

    pas_segregated_view view2 = pas_segregated_view_for_object(reinterpret_cast<uintptr_t>(object2),
                                                               &thingy_heap_config);
    
    pas_segregated_size_directory* directory2 = pas_segregated_size_directory_for_object(
        reinterpret_cast<uintptr_t>(object2), &thingy_heap_config);
    
    CHECK_EQUAL(directory2->object_size, secondSize);

    CHECK_EQUAL(pas_segregated_directory_size(&directory1->base), 1);
    CHECK(pas_segregated_directory_is_eligible(&directory1->base, 0));
    CHECK(!pas_segregated_directory_is_empty(&directory1->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory1->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory1->base, 0),
        view1);
    
    CHECK_EQUAL(pas_segregated_directory_size(&directory2->base), 1);
    CHECK(!pas_segregated_directory_is_eligible(&directory2->base, 0));
    CHECK(!pas_segregated_directory_is_empty(&directory2->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory2->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory2->base, 0),
        view2);
    
    flushDeallocationLogAndStopAllocators();

    CHECK_EQUAL(pas_segregated_directory_size(&directory1->base), 1);
    CHECK(pas_segregated_directory_is_eligible(&directory1->base, 0));
    CHECK(!pas_segregated_directory_is_empty(&directory1->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory1->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory1->base, 0),
        view1);
    
    CHECK_EQUAL(pas_segregated_directory_size(&directory2->base), 1);
    CHECK(pas_segregated_directory_is_eligible(&directory2->base, 0));
    CHECK(!pas_segregated_directory_is_empty(&directory2->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory2->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory2->base, 0),
        view2);
    
    thingy_deallocate(object1);
    thingy_deallocate(object2);

    flushDeallocationLogAndStopAllocators();

    CHECK_EQUAL(pas_segregated_directory_size(&directory1->base), 1);
    CHECK(pas_segregated_directory_is_eligible(&directory1->base, 0));
    CHECK(pas_segregated_directory_is_empty(&directory1->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory1->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory1->base, 0),
        view1);

    CHECK_EQUAL(pas_segregated_directory_size(&directory2->base), 1);
    CHECK(pas_segregated_directory_is_eligible(&directory2->base, 0));
    CHECK(pas_segregated_directory_is_empty(&directory2->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory2->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory2->base, 0),
        view2);
    
    void* object3 = thingy_try_allocate_primitive(thirdSize);
    CHECK(object3);

    pas_segregated_view view3 = pas_segregated_view_for_object(reinterpret_cast<uintptr_t>(object3),
                                                               &thingy_heap_config);
    
    pas_segregated_size_directory* directory3 = pas_segregated_size_directory_for_object(
        reinterpret_cast<uintptr_t>(object3), &thingy_heap_config);
    CHECK_EQUAL(directory3->object_size, thirdSize);
    
    CHECK_EQUAL(pas_segregated_directory_size(&directory1->base), 1);
    CHECK(pas_segregated_directory_is_eligible(&directory1->base, 0));
    CHECK(!pas_segregated_directory_is_empty(&directory1->base, 0));
    CHECK(!pas_segregated_directory_is_committed(&directory1->base, 0));

    CHECK_EQUAL(pas_segregated_directory_size(&directory2->base), 1);
    CHECK(pas_segregated_directory_is_eligible(&directory2->base, 0));
    CHECK(pas_segregated_directory_is_empty(&directory2->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory2->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory2->base, 0),
        view2);
    
    CHECK_EQUAL(pas_segregated_directory_size(&directory3->base), 1);
    CHECK(!pas_segregated_directory_is_eligible(&directory3->base, 0));
    CHECK(!pas_segregated_directory_is_empty(&directory3->base, 0));
    CHECK(pas_segregated_directory_is_committed(&directory3->base, 0));
    CHECK_EQUAL(
        pas_segregated_directory_get(&directory3->base, 0),
        view3);

    CHECK_EQUAL(pas_scavenger_current_state, pas_scavenger_state_no_thread);
}
#endif // TLC

#if SEGHEAP
struct SizeClassProgram {
    SizeClassProgram(const string& name,
                     const Allocator& allocator,
                     size_t resultingSize,
                     pas_object_kind objectKind,
                     size_t numAllocations,
                     size_t slowPathCount)
        : name(name)
        , allocator(&allocator)
        , resultingSize(resultingSize)
        , objectKind(objectKind)
        , numAllocations(numAllocations)
        , slowPathCount(slowPathCount)
    {
    }
    
    string name;
    const Allocator* allocator;
    size_t resultingSize;
    pas_object_kind objectKind;
    size_t numAllocations;
    size_t slowPathCount;
};

#define SIZE_CLASS_PROGRAM(allocator, resultingSize, objectKind, numAllocations, slowPathCount) \
    SizeClassProgram(#allocator ", " #resultingSize ", " #objectKind ", " #numAllocations ", " #slowPathCount, \
                     allocator, resultingSize, objectKind, numAllocations, slowPathCount)

void testSizeClassCreationImpl(const vector<SizeClassProgram>& programs)
{
    pas_scavenger_suspend(); // Because otherwise the CounterScope might see slow paths due to the scavenger collecting TLAs.
#if TLC
    pas_large_sharing_pool_validate_each_splat = true;
#endif
    map<size_t, pas_segregated_size_directory*> sizeMap;
    for (const SizeClassProgram& program : programs) {
        cout << "    Program: " << program.name << endl;
        
        pas_segregated_size_directory* resultingSizeClass = nullptr;
        
        {
#if PAS_ENABLE_TESTING
            CounterScope counterScope([&] { return program.allocator->allocateSlowPathCount(); },
                                      program.slowPathCount);
#endif // PAS_ENABLE_TESTING
            
            for (size_t i = 0; i < program.numAllocations; ++i) {
                void* allocation = program.allocator->allocate();
                CHECK(allocation);
                CHECK_EQUAL(thingy_get_allocation_size(allocation), program.resultingSize);
                
                CHECK_EQUAL(pas_get_object_kind(allocation, THINGY_HEAP_CONFIG),
                            program.objectKind);
                
                if (!resultingSizeClass)
                    resultingSizeClass = sizeClassFor(allocation);
                else
                    CHECK_EQUAL(sizeClassFor(allocation), resultingSizeClass);
            }
        }
        
        if (!sizeMap.count(program.resultingSize))
            sizeMap[program.resultingSize] = resultingSizeClass;
        else
            CHECK_EQUAL(resultingSizeClass, sizeMap[program.resultingSize]);
    }
}

template<typename... Arguments>
void testSizeClassCreation(Arguments... programs)
{
    testSizeClassCreationImpl({ programs... });
}

void testSpuriousEligibility()
{
    static constexpr bool verbose = false;
    
#if TLC
    pas_scavenger_suspend();
    pas_large_sharing_pool_validate_each_splat = true;
#endif
    
    unsigned objectSize = 16;
    unsigned numberOfObjects =
        pas_segregated_page_number_of_objects(objectSize,
                                              THINGY_HEAP_CONFIG.small_segregated_config,
                                              pas_segregated_page_exclusive_role);
    unsigned numberOfObjectsInFirstSpan = numberOfObjects / 2;
    unsigned numberOfObjectsInSecondSpan = numberOfObjects - numberOfObjectsInFirstSpan;

    if (verbose)
        cout << "Allocating first span of objects.\n";
    
    vector<void*> firstObjects;
    for (unsigned i = numberOfObjectsInFirstSpan; i--;)
        firstObjects.push_back(thingy_try_allocate_primitive(objectSize));
    
    if (verbose)
        cout << "Allocating second span of objects.\n";
    
    vector<void*> secondObjects;
    for (unsigned i = numberOfObjectsInSecondSpan; i--;)
        secondObjects.push_back(thingy_try_allocate_primitive(objectSize));
    
    if (verbose)
        cout << "Shrinking.\n";
    
    flushDeallocationLogAndStopAllocators();
    
    if (verbose)
        cout << "Deallocating first span of objects.\n";
    
    for (void* object : firstObjects)
        thingy_deallocate(object);
    
    if (verbose)
        cout << "Shrinking.\n";
    
    flushDeallocationLogAndStopAllocators();
    
    if (verbose)
        cout << "Reallocating first span of objects.\n";
    
    for (void* object : firstObjects) {
        void* reallocatedObject = thingy_try_allocate_primitive(objectSize);
        CHECK_EQUAL(reallocatedObject, object);
    }

    if (verbose)
        cout << "Deallocating second span of objects.\n";
    
    for (void* object : secondObjects)
        thingy_deallocate(object);

    if (verbose)
        cout << "Flushing deallocation log.\n";
    
    flushDeallocationLog();

    if (verbose)
        cout << "Reallocating second span of objects.\n";
    
    for (void* object : secondObjects) {
        void* reallocatedObject = thingy_try_allocate_primitive(objectSize);
        CHECK_EQUAL(reallocatedObject, object);
    }

    if (verbose)
        cout << "Shrinking.\n";
    
    flushDeallocationLogAndStopAllocators();

    thingy_try_allocate_primitive(objectSize);
    
    flushDeallocationLogAndStopAllocators();
    
    for (void* object : firstObjects)
        thingy_deallocate(object);
    for (void* object : secondObjects)
        thingy_deallocate(object);
    flushDeallocationLog();
    
    flushDeallocationLogAndStopAllocators();
    
    for (void* object : firstObjects) {
        void* reallocatedObject = thingy_try_allocate_primitive(objectSize);
        CHECK_EQUAL(reallocatedObject, object);
    }
    for (void* object : secondObjects) {
        void* reallocatedObject = thingy_try_allocate_primitive(objectSize);
        CHECK_EQUAL(reallocatedObject, object);
    }

    CHECK_EQUAL(pas_scavenger_current_state, pas_scavenger_state_no_thread);
}

void testBasicSizeClassNotSet()
{
    (thread([&] () { thingy_try_allocate_primitive(2); })).join();
    (thread([&] () { thingy_try_allocate_primitive(1); })).join();
}

void testSmallDoubleFree()
{
    void* ptr = thingy_try_allocate_primitive(1);
    CHECK(ptr);
    thingy_deallocate(ptr);
    DeallocationShouldFail deallocationShouldFail;
    thingy_deallocate(ptr);
    flushDeallocationLogAndStopAllocators();
}

void testSmallFreeInner()
{
    void* ptr = thingy_try_allocate_primitive(32);
    CHECK(ptr);
    DeallocationShouldFail deallocationShouldFail;
    thingy_deallocate(reinterpret_cast<char*>(ptr) + 16);
}

void testSmallFreeNextWithShrink()
{
    void* ptr = thingy_try_allocate_primitive(16);
    CHECK(ptr);
    flushDeallocationLogAndStopAllocators();
    DeallocationShouldFail deallocationShouldFail;
    thingy_deallocate(reinterpret_cast<char*>(ptr) + 16);
}

void testSmallFreeNextWithoutShrink()
{
    void* ptr = thingy_try_allocate_primitive(16);
    CHECK(ptr);
    thingy_deallocate(ptr);
#if !TLC
    DeallocationShouldFail deallocationShouldFail;
#endif    
    thingy_deallocate(reinterpret_cast<char*>(ptr) + 16);
    void* ptr2 = thingy_try_allocate_primitive(16);
    CHECK(ptr2 == reinterpret_cast<char*>(ptr) + 16);
    verifyHeapEmpty(&thingy_primitive_heap);
}

void testSmallFreeNextBeforeShrink()
{
    void* ptr = thingy_try_allocate_primitive(16);
    CHECK(ptr);
#if !TLC
    DeallocationShouldFail deallocationShouldFail;
#endif    
    thingy_deallocate(reinterpret_cast<char*>(ptr) + 16);
#if TLC
    DeallocationShouldFail deallocationShouldFail;
#endif
    flushDeallocationLogAndStopAllocators();
}
#endif // SEGHEAP

class AllocationProgram {
public:
    enum Kind { Allocate, Free };
    
    static AllocationProgram allocate(const string& key, size_t count, size_t alignment)
    {
        AllocationProgram result;
        result.m_kind = Allocate;
        result.m_key = key;
        result.m_count = count;
        result.m_alignment = alignment;
        return result;
    }
    
    static AllocationProgram free(const string& key)
    {
        AllocationProgram result;
        result.m_kind = Free;
        result.m_key = key;
        return result;
    }
    
    Kind kind() const { return m_kind; }
    
    const string& key() const { return m_key; }
    
    bool isAllocate() const { return m_kind == Kind::Allocate; }
    
    size_t count() const
    {
        PAS_ASSERT(isAllocate());
        return m_count;
    }
    
    size_t alignment() const
    {
        PAS_ASSERT(isAllocate());
        return m_alignment;
    }
    
    bool isFree() const { return m_kind == Kind::Free; }
    
private:
    Kind m_kind { Allocate };
    string m_key;
    size_t m_count { 0 };
    size_t m_alignment { 0 };
};

class ComplexAllocator {
public:
    virtual ~ComplexAllocator() = default;
    virtual void* allocate(size_t count, size_t alignment) const = 0;
    virtual pas_heap* heap() const = 0;
};

class PrimitiveComplexAllocator : public ComplexAllocator {
public:
    void* allocate(size_t count, size_t alignment) const override
    {
        return thingy_try_allocate_primitive_with_alignment(count, alignment);
    }
    
    pas_heap* heap() const override
    {
        return &thingy_primitive_heap;
    }
};

class IsolatedComplexAllocator : public ComplexAllocator {
public:
    IsolatedComplexAllocator(pas_heap_ref* heapRef)
        : m_heapRef(heapRef)
    {
    }
    
    IsolatedComplexAllocator(size_t size, size_t alignment)
        : IsolatedComplexAllocator(createIsolatedHeapRef(size, alignment))
    {
    }
    
    void* allocate(size_t count, size_t alignment) const override
    {
        return thingy_try_allocate_array(m_heapRef, count, alignment);
    }
    
    pas_heap* heap() const override
    {
        return thingy_heap_ref_get_heap(m_heapRef);
    }
    
private:
    pas_heap_ref* m_heapRef;
};

class IsolatedUnitComplexAllocator : public ComplexAllocator {
public:
    IsolatedUnitComplexAllocator(pas_heap_ref* heapRef)
        : m_heapRef(heapRef)
    {
    }
    
    IsolatedUnitComplexAllocator(size_t size, size_t alignment)
        : IsolatedUnitComplexAllocator(createIsolatedHeapRef(size, alignment))
    {
    }
    
    void* allocate(size_t count, size_t alignment) const override
    {
        if (count == 1 && alignment == 1)
            return thingy_try_allocate(m_heapRef);
        return thingy_try_allocate_array(m_heapRef, count, alignment);
    }
    
    pas_heap* heap() const override
    {
        return thingy_heap_ref_get_heap(m_heapRef);
    }
    
private:
    pas_heap_ref* m_heapRef;
};

void checkObjectDistances(const map<void*, size_t>& objectByPtr)
{
    uintptr_t lastEnd = 0;
    for (auto& pair : objectByPtr) {
        uintptr_t begin = reinterpret_cast<uintptr_t>(pair.first);
        uintptr_t end = begin + pair.second;
        CHECK(begin >= lastEnd);
        lastEnd = end;
    }
}

void checkObjectBeginnings(pas_simple_type type, const set<uintptr_t>& objectBeginnings)
{
    // Test that the objects don't overlap, since that would be a type system violation.
    uintptr_t lastBegin = 0;
    for (uintptr_t begin : objectBeginnings) {
        CHECK(begin >= lastBegin);
        CHECK(begin - lastBegin >= pas_simple_type_size(type));
        lastBegin = begin;
    }
}

void addObjectAllocation(map<void*, size_t>& objectByPtr,
                         set<uintptr_t>* objectBeginnings,
                         pas_simple_type type,
                         void* ptr,
                         size_t count)
{
    CHECK(ptr);
    CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), pas_simple_type_alignment(type)));
    CHECK(!objectByPtr.count(ptr));
    objectByPtr[ptr] = count * pas_simple_type_size(type);
    if (objectBeginnings) {
        for (uintptr_t begin = reinterpret_cast<uintptr_t>(ptr);
             begin < reinterpret_cast<uintptr_t>(ptr) + count * pas_simple_type_size(type);
             begin += pas_simple_type_size(type))
            objectBeginnings->insert(begin);
    }
}

struct ExpectedBytes {
    enum Kind { Exact, UpperBound };
    
    static ExpectedBytes exact(size_t bytes)
    {
        ExpectedBytes result;
        result.bytes = bytes;
        result.kind = Exact;
        return result;
    }
    
    static ExpectedBytes upperBound(size_t bytes)
    {
        ExpectedBytes result;
        result.bytes = bytes;
        result.kind = UpperBound;
        return result;
    }
    
    void check(size_t actualBytes)
    {
        switch (kind) {
        case Exact:
            CHECK_EQUAL(actualBytes, bytes);
            return;
        case UpperBound:
            CHECK_LESS_EQUAL(actualBytes, bytes);
            return;
        }
        PAS_ASSERT(!"unexpected kind");
    }
    
    size_t bytes { 0 };
    Kind kind { Exact };
};

void testComplexLargeAllocationImpl(const ComplexAllocator& allocator,
                                    ExpectedBytes expectedNumMappedBytes,
                                    const vector<AllocationProgram>& programs)
{
    static constexpr bool verbose = false;
    
#if TLC
    pas_large_sharing_pool_validate_each_splat = true;
#endif
    map<void*, size_t> objectByPtr;
    map<string, void*> objectByKey;
    set<uintptr_t> objectBeginnings;
    for (const AllocationProgram& program : programs) {
        switch (program.kind()) {
        case AllocationProgram::Kind::Allocate: {
            cout << "    Program: allocate(" << program.key() << ", " << program.count() << ", "
                 << program.alignment() << ")" << endl;
            void* ptr = allocator.allocate(program.count(), program.alignment());
            if (verbose) {
                cout << "Allocated ptr = " << ptr << " with kind "
                     << pas_get_object_kind(ptr, THINGY_HEAP_CONFIG) << endl;
            }
            CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), program.alignment()));
            CHECK(!objectByKey.count(program.key()));
            objectByKey[program.key()] = ptr;
            
            addObjectAllocation(objectByPtr,
                                &objectBeginnings,
                                reinterpret_cast<pas_simple_type>(allocator.heap()->type),
                                ptr,
                                program.count());
            break;
        }
            
        case AllocationProgram::Kind::Free: {
            cout << "    Program: free(" << program.key() << ")" << endl;
            CHECK(objectByKey.count(program.key()));
            void* ptr = objectByKey[program.key()];
            CHECK(objectByPtr.count(ptr));
            thingy_deallocate(ptr);
            objectByPtr.erase(ptr);
            break;
        } }
        
        checkObjectDistances(objectByPtr);
        verifyObjectSet(objectByPtr, allocator.heap());
    }
    
    checkObjectBeginnings(reinterpret_cast<pas_simple_type>(allocator.heap()->type),
                          objectBeginnings);

#if SEGHEAP
    CHECK_EQUAL(numCommittedViews(allocator.heap()), 0);
#endif
    expectedNumMappedBytes.check(allocator.heap()->large_heap.free_heap.num_mapped_bytes);
}

template<typename... Arguments>
void testComplexLargeAllocation(const ComplexAllocator& allocator,
                                ExpectedBytes expectedNumMappedBytes,
                                Arguments... arguments)
{
    testComplexLargeAllocationImpl(allocator, expectedNumMappedBytes, { arguments... });
}

void testAllocationCountProgression(const ComplexAllocator& allocator,
                                    size_t beginCount,
                                    size_t endCount,
                                    size_t countStep,
                                    size_t alignment,
                                    size_t numPerSize,
                                    bool doObjectBeginnings)
{
#if TLC
    pas_scavenger_suspend();
    pas_large_sharing_pool_validate_each_splat = true;
#endif
    map<void*, size_t> objectByPtr;
    set<uintptr_t> objectBeginnings;
    
    for (size_t count = beginCount; count < endCount; count += countStep) {
        for (size_t countPerSize = numPerSize; countPerSize--;) {
            void* ptr = allocator.allocate(count, alignment);
            addObjectAllocation(objectByPtr,
                                doObjectBeginnings ? &objectBeginnings : nullptr,
                                reinterpret_cast<pas_simple_type>(allocator.heap()->type),
                                ptr,
                                count);
        }
    }
    
    checkObjectDistances(objectByPtr);
    if (doObjectBeginnings) {
        checkObjectBeginnings(
            reinterpret_cast<pas_simple_type>(allocator.heap()->type), objectBeginnings);
    }
    flushDeallocationLogAndStopAllocators();
    verifyObjectSet(objectByPtr, allocator.heap());
    
    for (auto& pair : objectByPtr)
        thingy_deallocate(pair.first);
    
    verifyHeapEmpty(allocator.heap());
}

void testAllocationChaos(unsigned numThreads, unsigned numIsolatedHeaps,
                         unsigned numInitialAllocations, unsigned numActions, unsigned maxTotalSize,
                         bool validateEachSplat)
{
#if TLC
    if (validateEachSplat)
        pas_large_sharing_pool_validate_each_splat = true;
#endif

    static constexpr bool verbose = false;
    
    mutex lock;
    
    struct Object {
        Object() = default;
        
        Object(void* ptr, size_t size)
            : ptr(ptr)
            , size(size)
        {
        }
        
        void* ptr { nullptr };
        size_t size { 0 };
    };
    
    vector<Object> objects;
    vector<thread> threads;
    unsigned totalSize;
    
    vector<pas_heap_ref*> isolatedHeaps;
    for (unsigned isolatedHeapIndex = numIsolatedHeaps; isolatedHeapIndex--;)
        isolatedHeaps.push_back(createIsolatedHeapRef(deterministicRandomNumber(100), 1));
    
    auto addObject =
        [&] (void* ptr, unsigned size) {
            if (verbose)
                cout << "Allocated " << ptr << " with size = " << size << "\n";
            CHECK(ptr);
            for (unsigned i = size; i--;) {
                char value = reinterpret_cast<char*>(ptr)[i];
                CHECK(value == 0 || value == 0x42);
            }
            memset(ptr, 0x42, size);
            {
                lock_guard<mutex> locker(lock);
                objects.push_back(Object(ptr, size));
                totalSize += size;
            }
        };
    
    auto allocateSomething = [&] () {
        unsigned heapIndex = deterministicRandomNumber(static_cast<unsigned>(isolatedHeaps.size() + 1));
        unsigned size = deterministicRandomNumber(5000);
        
        if (heapIndex == isolatedHeaps.size()) {
            void* ptr = thingy_try_allocate_primitive(size);
            addObject(ptr, size);
            return;
        }
        
        pas_heap_ref* heap = isolatedHeaps[heapIndex];
        unsigned count = size / pas_simple_type_size(reinterpret_cast<pas_simple_type>(heap->type));
        size = static_cast<unsigned>(
            count * pas_simple_type_size(reinterpret_cast<pas_simple_type>(heap->type)));
        void* ptr;
        if (count <= 1)
            ptr = thingy_try_allocate(heap);
        else
            ptr = thingy_try_allocate_array(heap, count, 1);
        addObject(ptr, size);
    };
    
    auto freeSomething = [&] () {
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
        }
        
        if (verbose)
            cout << "Deallocating " << object.ptr << "\n";
        
        thingy_deallocate(object.ptr);
    };
    
    for (unsigned allocationIndex = numInitialAllocations; allocationIndex--;)
        allocateSomething();

    unsigned numThreadsStopped = 0;
    
    auto threadFunc = [&] () {
        if (verbose)
            cout << "    Thread " << (void*)pthread_self() << " starting.\n";
        for (unsigned actionIndex = numActions; actionIndex--;) {
            if (totalSize >= maxTotalSize) {
                freeSomething();
                continue;
            }
            
            switch (deterministicRandomNumber(2)) {
            case 0:
                allocateSomething();
                break;
            case 1:
                freeSomething();
                break;
            default:
                PAS_ASSERT(!"bad random number");
            }
        }
        if (verbose)
            cout << "    Thread " << (void*)pthread_self() << " stopping.\n";

        {
            lock_guard<mutex> locker(lock);
            numThreadsStopped++;
        }
    };
    
    {
        lock_guard<mutex> locker(lock);
        for (unsigned threadIndex = numThreads; threadIndex--;)
            threads.push_back(thread(threadFunc));
    }
    
    for (thread& thread : threads)
        thread.join();

    CHECK_EQUAL(numThreadsStopped, threads.size());
    
    vector<pas_heap*> heaps;
    heaps.push_back(&thingy_primitive_heap);
    for (pas_heap_ref* heapRef : isolatedHeaps)
        heaps.push_back(thingy_heap_ref_get_heap(heapRef));
    
    set<void*> expectedObjects;
    for (Object object : objects)
        expectedObjects.insert(object.ptr);
    
    verifyObjectSet(expectedObjects, heaps, [] (void*, size_t) { });

    pas_heap_lock_lock();
    pas_large_sharing_pool_validate();
    pas_heap_lock_unlock();
}

void testUtilityAllocationChaos(unsigned numThreads,
                                unsigned numInitialAllocations, unsigned numActions,
                                unsigned maxTotalSize)
{
    mutex lock;
    
    struct Object {
        Object() = default;
        
        Object(void* ptr, size_t size)
            : ptr(ptr)
            , size(size)
        {
        }
        
        void* ptr { nullptr };
        size_t size { 0 };
    };
    
    vector<Object> objects;
    vector<thread> threads;
    unsigned totalSize;
    
    auto addObject = [&] (void* ptr, unsigned size) {
        CHECK(ptr);
        memset(ptr, 0x42, size);
        {
            lock_guard<mutex> locker(lock);
            objects.push_back(Object(ptr, size));
            totalSize += size;
        }
    };
    
    auto allocateSomething = [&] () {
        unsigned size = deterministicRandomNumber(500);
        
        pas_heap_lock_lock();
        void* ptr = pas_utility_heap_allocate(size, "test");
        pas_heap_lock_unlock();
        addObject(ptr, size);
    };
    
    auto freeSomething = [&] () {
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
        }
        
        pas_heap_lock_lock();
        pas_utility_heap_deallocate(object.ptr);
        pas_heap_lock_unlock();
    };
    
    for (unsigned allocationIndex = numInitialAllocations; allocationIndex--;)
        allocateSomething();

    unsigned numThreadsStopped = 0;
    
    auto threadFunc = [&] () {
        for (unsigned actionIndex = numActions; actionIndex--;) {
            if (totalSize >= maxTotalSize) {
                freeSomething();
                continue;
            }
            
            switch (deterministicRandomNumber(2)) {
            case 0:
                allocateSomething();
                break;
            case 1:
                freeSomething();
                break;
            default:
                PAS_ASSERT(!"bad random number");
            }
        }

        {
            lock_guard<mutex> locker(lock);
            numThreadsStopped++;
        }
    };
    
    {
        lock_guard<mutex> locker(lock);
        for (unsigned threadIndex = numThreads; threadIndex--;)
            threads.push_back(thread(threadFunc));
    }
    
    for (thread& thread : threads)
        thread.join();

    CHECK_EQUAL(numThreadsStopped, threads.size());
    
    pas_utility_heap_for_all_allocators(pas_allocator_scavenge_force_stop_action,
                                        pas_lock_is_not_held);
    
    set<void*> expectedObjects;
    for (Object object : objects)
        expectedObjects.insert(object.ptr);
    
    verifyUtilityObjectSet(expectedObjects, [] (void*, size_t) { });
#if TLC
    pas_heap_lock_lock();
    pas_large_sharing_pool_validate();
    pas_heap_lock_unlock();
#endif
}

void testCombinedAllocationChaos(unsigned numThreads, unsigned numIsolatedHeaps,
                                 unsigned numInitialAllocations, unsigned numActions,
                                 unsigned maxTotalSize,
                                 bool calloc)
{
    mutex lock;
    
    struct Object {
        Object() = default;
        
        Object(void* ptr, size_t size, bool isUtility)
            : ptr(ptr)
            , size(size)
            , isUtility(isUtility)
        {
        }
        
        void* ptr { nullptr };
        size_t size { 0 };
        bool isUtility { false };
    };
    
    vector<Object> objects;
    vector<thread> threads;
    unsigned totalSize;
    
    vector<pas_heap_ref*> isolatedHeaps;
    for (unsigned isolatedHeapIndex = numIsolatedHeaps; isolatedHeapIndex--;)
        isolatedHeaps.push_back(createIsolatedHeapRef(deterministicRandomNumber(100), 1));
    
    auto addObject = [&] (void* ptr, unsigned size, bool isUtility) {
        CHECK(ptr);
        if (!isUtility) {
            for (unsigned i = size; i--;) {
                char value = reinterpret_cast<char*>(ptr)[i];
                if (calloc)
                    CHECK(value == 0);
                else
                    CHECK(value == 0 || value == 0x42);
            }
        }
        memset(ptr, 0x42, size);
        {
            lock_guard<mutex> locker(lock);
            objects.push_back(Object(ptr, size, isUtility));
            totalSize += size;
        }
    };
    
    auto allocateSomething = [&] () {
        unsigned heapIndex = deterministicRandomNumber(static_cast<unsigned>(isolatedHeaps.size() + 2));
        unsigned size = deterministicRandomNumber(5000);
        
        if (heapIndex == isolatedHeaps.size() + 1) {
            void* ptr;
            size %= 500;
            pas_heap_lock_lock();
            ptr = pas_utility_heap_try_allocate(size, "test");
            pas_heap_lock_unlock();
            addObject(ptr, size, true);
            return;
        }
        if (heapIndex == isolatedHeaps.size()) {
            void* ptr;
            if (calloc)
                ptr = thingy_try_allocate_primitive_zeroed(size);
            else
                ptr = thingy_try_allocate_primitive(size);
            addObject(ptr, size, false);
            return;
        }
        
        pas_heap_ref* heap = isolatedHeaps[heapIndex];
        unsigned count = size / pas_simple_type_size(reinterpret_cast<pas_simple_type>(heap->type));
        size = static_cast<unsigned>(
            count * pas_simple_type_size(reinterpret_cast<pas_simple_type>(heap->type)));
        void* ptr;
        if (count <= 1) {
            if (calloc)
                ptr = thingy_try_allocate_zeroed(heap);
            else
                ptr = thingy_try_allocate(heap);
        } else {
            if (calloc)
                ptr = thingy_try_allocate_zeroed_array(heap, count, 1);
            else
                ptr = thingy_try_allocate_array(heap, count, 1);
        }
        addObject(ptr, size, false);
    };
    
    auto freeSomething = [&] () {
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
        }
        
        if (object.isUtility) {
            pas_heap_lock_lock();
            pas_utility_heap_deallocate(object.ptr);
            pas_heap_lock_unlock();
        } else
            thingy_deallocate(object.ptr);
    };
    
    for (unsigned allocationIndex = numInitialAllocations; allocationIndex--;)
        allocateSomething();

    unsigned numThreadsStopped = 0;
    
    auto threadFunc = [&] () {
        for (unsigned actionIndex = numActions; actionIndex--;) {
            if (totalSize >= maxTotalSize) {
                freeSomething();
                continue;
            }
            
            switch (deterministicRandomNumber(2)) {
            case 0:
                allocateSomething();
                break;
            case 1:
                freeSomething();
                break;
            default:
                PAS_ASSERT(!"bad random number");
            }
        }
        {
            lock_guard<mutex> locker(lock);
            numThreadsStopped++;
        }
    };
    
    {
        lock_guard<mutex> locker(lock);
        for (unsigned threadIndex = numThreads; threadIndex--;)
            threads.push_back(thread(threadFunc));
    }
    
    for (thread& thread : threads)
        thread.join();

    CHECK_EQUAL(numThreadsStopped, threads.size());
    
    vector<pas_heap*> heaps;
    heaps.push_back(&thingy_primitive_heap);
    for (pas_heap_ref* heapRef : isolatedHeaps)
        heaps.push_back(thingy_heap_ref_get_heap(heapRef));
    
    set<void*> expectedObjects;
    set<void*> expectedUtilityObjects;
    for (Object object : objects) {
        if (object.isUtility)
            expectedUtilityObjects.insert(object.ptr);
        else
            expectedObjects.insert(object.ptr);
    }
    
    verifyObjectSet(expectedObjects, heaps, [] (void*, size_t) { });
    verifyUtilityObjectSet(expectedUtilityObjects, [] (void*, size_t) { });
#if TLC
    pas_heap_lock_lock();
    pas_large_sharing_pool_validate();
    pas_heap_lock_unlock();
#endif
}

void testLargeDoubleFree()
{
    void* ptr = thingy_try_allocate_primitive(10000);
    CHECK(ptr);
    thingy_deallocate(ptr);
    DeallocationShouldFail deallocationShouldFail;
    thingy_deallocate(ptr);
}

void testLargeOffsetFree()
{
    void* ptr = thingy_try_allocate_primitive(10000);
    CHECK(ptr);
    DeallocationShouldFail deallocationShouldFail;
    thingy_deallocate(reinterpret_cast<char*>(ptr) + 1);
}

void addDeallocationTests()
{
    ADD_TEST(testDeallocateNull());
    ADD_TEST(testDeallocateMalloc());
    ADD_TEST(testDeallocateStack());
}

void testReallocatePrimitive(size_t originalSize, size_t expectedOriginalSize,
                             size_t newSize, size_t expectedNewSize,
                             size_t count)
{
    pas_scavenger_suspend();

    for (size_t i = count; i--;) {
        void* originalObject = thingy_try_allocate_primitive(originalSize);
        CHECK(originalObject);
        CHECK_EQUAL(thingy_get_allocation_size(originalObject), expectedOriginalSize);
        
        void* newObject = thingy_try_reallocate_primitive(originalObject, newSize);
        CHECK(newObject);
        CHECK_EQUAL(thingy_get_allocation_size(newObject), expectedNewSize);
        
        thingy_deallocate(newObject);
    }
    
    verifyHeapEmpty(&thingy_primitive_heap);
}

void testReallocateArray(size_t typeSize, size_t typeAlignment,
                         size_t originalCount, size_t expectedOriginalSize,
                         size_t newCount, size_t expectedNewSize,
                         size_t count)
{
    pas_scavenger_suspend();

    pas_heap_ref* heapRef = createIsolatedHeapRef(typeSize, typeAlignment);
    
    for (size_t i = count; i--;) {
        void* originalObject = thingy_try_allocate_array(heapRef, originalCount, 1);
        CHECK(originalObject);
        CHECK_EQUAL(thingy_get_allocation_size(originalObject), expectedOriginalSize);
        
        void* newObject = thingy_try_reallocate_array(originalObject, heapRef, newCount);
        CHECK(newObject);
        CHECK_EQUAL(thingy_get_allocation_size(newObject), expectedNewSize);
        
        thingy_deallocate(newObject);
    }
    
    verifyHeapEmpty(thingy_heap_ref_get_heap(heapRef));
}

#if SEGHEAP
void addSmallHeapTests()
{
    DisableBitfit disableBitfit;
    
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(0), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(0), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(8), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(8), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(16), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(16), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(24), 24, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(24), 24, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(32), 32, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(32), 32, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(40), 40, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(40), 40, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(16), 16, 1000, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(16), 16, 1000, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(16), 16, 2000, false, 2, 3));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(16), 16, 2000, true, 2, 3));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(0), 8, 20000, false, 10, 11));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(0), 8, 20000, true, 10, 11));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(16), 16, 20000, false, 20, 21));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(16), 16, 20000, true, 20, 21));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(100), 104, 20000, false, 129 + TLC, 130 + TLC));
    ADD_TEST(testSmallAllocation(PrimitiveAllocator(100), 104, 20000, true, 129 + TLC, 130 + TLC));
    
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(0), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(0), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(8), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(8), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(16), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(16), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(24), 24, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(24), 24, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(16), 16, 1000, false, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(16), 16, 1000, true, 1, 2));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(16), 16, 2000, false, 2, 3));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(16), 16, 2000, true, 2, 3));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(0), 8, 20000, false, 10, 11));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(0), 8, 20000, true, 10, 11));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(16), 16, 20000, false, 20, 21));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(16), 16, 20000, true, 20, 21));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(100), 104, 20000, false, 129 + TLC, 130 + TLC));
    ADD_TEST(testSmallAllocation(PrimitiveReallocAllocator(100), 104, 20000, true, 129 + TLC, 130 + TLC));
    
    ADD_TEST(testAllocationWithInterleavedFragmentation(PrimitiveAllocator(1), 8, 20000, false, 10, 10, 21));
    ADD_TEST(testAllocationWithInterleavedFragmentation(PrimitiveAllocator(1), 8, 20000, true, 10, 10, 21));
    ADD_TEST(testAllocationWithInterleavedFragmentation(PrimitiveAllocator(16), 16, 20000, false, 20, 20, 41));
    ADD_TEST(testAllocationWithInterleavedFragmentation(PrimitiveAllocator(16), 16, 20000, true, 20, 20, 41));
    ADD_TEST(testAllocationWithInterleavedFragmentation(PrimitiveAllocator(100), 104, 20000, false, 129 + TLC, 130, 261));
    ADD_TEST(testAllocationWithInterleavedFragmentation(PrimitiveAllocator(100), 104, 20000, true, 129 + TLC, 129 + TLC, TLC ? 261 : 257));
    
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(0, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(0, 16), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(0, 32), 32, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(1, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(1, 16), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(1, 32), 32, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(16, 1), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(7, 16), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(16, 16), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(16, 32), 32, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(24, 32), 32, 1, true, 1, 2));
    if (hasScope("only-small")) {
        ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(24, 1024), 1024, 1, true, 1, 2));
        ADD_TEST(testSmallAllocation(AlignedPrimitiveAllocator(666, 1024), 1024, 1, true, 1, 2));
    }
    
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 8), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 8), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 8), 8, 10000, false, 5, 6));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 8), 8, 20000, false, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 8), 8, 20000, true, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 1), 8, 20000, false, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(8, 1), 8, 20000, true, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(1, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(1, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(1, 1), 8, 20000, false, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(1, 1), 8, 20000, true, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(5, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(5, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(5, 1), 8, 20000, false, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(5, 1), 8, 20000, true, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(2, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(2, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(3, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(3, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(4, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(4, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(6, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(6, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(7, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(7, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(9, 1), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(9, 1), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(10, 1), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(10, 1), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(11, 1), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(11, 1), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(12, 1), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(12, 1), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(13, 1), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(13, 1), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(14, 1), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(14, 1), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(15, 1), 16, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(15, 1), 16, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(24, 8), 24, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(24, 8), 24, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(24, 8), 24, 20000, false, 30, 31));
    ADD_TEST(testSmallAllocation(IsolatedHeapAllocator(24, 8), 24, 20000, true, 30, 31));
    
    ADD_TEST(testAllocationWithInterleavedFragmentation(IsolatedHeapAllocator(24, 8), 24, 20000, false, 30, 30, 61));
    ADD_TEST(testAllocationWithInterleavedFragmentation(IsolatedHeapAllocator(24, 8), 24, 20000, true, 30, 30, 61));
    
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(8, 8, 1, 1), 8, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(8, 8, 1, 1), 8, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(8, 8, 1, 1), 8, 20000, false, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(8, 8, 1, 1), 8, 20000, true, 10, 11));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(8, 8, 5, 1), 40, 1, false, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(8, 8, 5, 1), 40, 1, true, 1, 2));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(8, 8, 5, 1), 40, 20000, false, 50, 51));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(8, 8, 5, 1), 40, 20000, true, 50, 51));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(24, 8, 1, 32), 32, 20000, false, 40, 41));
    ADD_TEST(testSmallAllocation(IsolatedHeapArrayAllocator(24, 8, 1, 32), 32, 20000, true, 40, 41));
    
    ADD_TEST(testFreeListRefillSpans(0, 16, 6, 0, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 16, 6, 1, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 16, 5, 0, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 16, 5, 1, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 16, 1, 0, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 24, 6, 0, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 24, 6, 1, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 24, 5, 0, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 24, 5, 1, pas_small_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(0, 24, 1, 0, pas_small_segregated_page_config_variant));
    
#if TLC
    SKIP_TEST(testInternalScavenge(32, 32, 10000, false, false, 64, 64, 10000, false, 10000, false, 20, 20, 41, 60, 60, 60, false));
    SKIP_TEST(testInternalScavenge(32, 32, 10000, true, true, 64, 64, 10000, true, 10000, true, 20, 20, 40, 60, 60, 60, false));
    SKIP_TEST(testInternalScavenge(32, 32, 10000, true, true, 64, 64, 5000, true, 10000, true, 20, 20, 20, 40, 20, 40, true));
    SKIP_TEST(testInternalScavengeFromCorrectDirectory(32, 64, 128));
    SKIP_TEST(testInternalScavengeFromCorrectDirectory(128, 64, 32));
    SKIP_TEST(testInternalScavengeFromCorrectDirectory(64, 32, 128));
    SKIP_TEST(testInternalScavengeFromCorrectDirectory(128, 32, 64));
#endif

    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(24), 24, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(24), 24, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(24), 24, pas_small_segregated_object_kind, 100, 0)));
    
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(24), 24, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(20), 24, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(24), 24, pas_small_segregated_object_kind, 100, 0)));
    
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(0), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(0), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(1), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(1), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(2), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(2), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(3), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(3), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(4), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(4), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(5), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(5), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(6), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(6), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(7), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(7), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(9), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(9), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(10), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(10), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(11), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(11), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(12), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(12), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(13), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(13), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(14), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(14), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(15), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(15), 16, pas_small_segregated_object_kind, 100, 0)));

    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(0), 8, pas_small_segregated_object_kind, 100, 1),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(1), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(2), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(3), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(4), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(5), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(6), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(7), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(8), 8, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(9), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(10), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(11), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(12), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(13), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(14), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(15), 16, pas_small_segregated_object_kind, 100, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(16), 16, pas_small_segregated_object_kind, 100, 0)));

    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(56), 56, pas_small_segregated_object_kind, 50, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(48), 56, pas_small_segregated_object_kind, 50, 1),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(56), 56, pas_small_segregated_object_kind, 50, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(56), 56, pas_small_segregated_object_kind, 50, 2),
                                   SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(48, 16), 48, pas_small_segregated_object_kind, 50, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(56), 56, pas_small_segregated_object_kind, 50, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(56), 56, pas_small_segregated_object_kind, 20, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(48), 56, pas_small_segregated_object_kind, 20, 1),
                                   SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(48, 16), 48, pas_small_segregated_object_kind, 20, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(56), 56, pas_small_segregated_object_kind, 20, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(48), 48, pas_small_segregated_object_kind, 20, 0),
                                   SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(48, 16), 48, pas_small_segregated_object_kind, 20, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(248), 248, pas_small_segregated_object_kind, 3, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(240), 248, pas_small_segregated_object_kind, 3, 1),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(232), 248, pas_small_segregated_object_kind, 3, 1),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(224), 248, pas_small_segregated_object_kind, 3, 1),
                                   SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(240, 16), 240, pas_small_segregated_object_kind, 3, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(248), 248, pas_small_segregated_object_kind, 3, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(240), 240, pas_small_segregated_object_kind, 3, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(232), 240, pas_small_segregated_object_kind, 3, 1),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(224), 240, pas_small_segregated_object_kind, 3, 1),
                                   SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(240, 16), 240, pas_small_segregated_object_kind, 3, 0),
                                   SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(224, 32), 224, pas_small_segregated_object_kind, 3, 2),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(248), 248, pas_small_segregated_object_kind, 3, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(240), 240, pas_small_segregated_object_kind, 3, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(232), 240, pas_small_segregated_object_kind, 3, 0),
                                   SIZE_CLASS_PROGRAM(PrimitiveAllocator(224), 224, pas_small_segregated_object_kind, 3, 0),
                                   SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(240, 16), 240, pas_small_segregated_object_kind, 3, 0),
                                   SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(224, 32), 224, pas_small_segregated_object_kind, 3, 0)));
    if (hasScope("only-small")) {
        ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(1000, 16), 1008, pas_small_segregated_object_kind, 3, 2),
                                       SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(1008, 16), 1008, pas_small_segregated_object_kind, 3, 0)));
    } else {
        ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(1000, 16), 1024, pas_medium_segregated_object_kind, 3, 2),
                                       SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(1008, 16), 1024, pas_medium_segregated_object_kind, 3, 0)));
    }
    if (hasScope("only-small")) {
        ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(PrimitiveAllocator(1024), 1072, pas_small_segregated_object_kind, 3, 2),
                                       SIZE_CLASS_PROGRAM(PrimitiveAllocator(1072), 1072, pas_small_segregated_object_kind, 3, 0),
                                       SIZE_CLASS_PROGRAM(AlignedPrimitiveAllocator(1024, 1024), 1024, pas_small_segregated_object_kind, 3, 2),
                                       SIZE_CLASS_PROGRAM(PrimitiveAllocator(1024), 1024, pas_small_segregated_object_kind, 3, 0)));
    }
    
    pas_heap_ref* heap4Bytes = createIsolatedHeapRef(4, 4);
    
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(IsolatedHeapAllocator(heap4Bytes), 8, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap4Bytes, 2, 1), 8, pas_small_segregated_object_kind, 200, 0),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapAllocator(heap4Bytes), 8, pas_small_segregated_object_kind, 200, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap4Bytes, 2, 1), 8, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapAllocator(heap4Bytes), 8, pas_small_segregated_object_kind, 200, 0),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap4Bytes, 2, 1), 8, pas_small_segregated_object_kind, 200, 0)));
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap4Bytes, 14, 1), 56, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap4Bytes, 12, 1), 56, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap4Bytes, 12, 16), 48, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap4Bytes, 14, 1), 56, pas_small_segregated_object_kind, 200, 1),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap4Bytes, 12, 1), 48, pas_small_segregated_object_kind, 200, 1)));
    
    pas_heap_ref* heap24Bytes = createIsolatedHeapRef(24, 8);
    
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(IsolatedHeapAllocator(heap24Bytes), 24, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap24Bytes, 1, 32), 32, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapAllocator(heap24Bytes), 24, pas_small_segregated_object_kind, 200, 0)));
    
    pas_heap_ref* heap3Bytes = createIsolatedHeapRef(3, 1);
    
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(IsolatedHeapAllocator(heap3Bytes), 8, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap3Bytes, 2, 1), 8, pas_small_segregated_object_kind, 200, 0),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap3Bytes, 3, 1), 16, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap3Bytes, 4, 1), 16, pas_small_segregated_object_kind, 200, 0)));

    pas_heap_ref* heap7Bytes = createIsolatedHeapRef(7, 1);
    
    ADD_TEST(testSizeClassCreation(SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap7Bytes, 8, 1), 56, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap7Bytes, 7, 1), 56, pas_small_segregated_object_kind, 10, 0),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap7Bytes, 6, 1), 56, pas_small_segregated_object_kind, 10, 1),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap7Bytes, 6, 16), 48, pas_small_segregated_object_kind, 200, 2),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap7Bytes, 8, 1), 56, pas_small_segregated_object_kind, 10, 0),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap7Bytes, 7, 1), 56, pas_small_segregated_object_kind, 10, 0),
                                   SIZE_CLASS_PROGRAM(IsolatedHeapArrayAllocator(heap7Bytes, 6, 1), 48, pas_small_segregated_object_kind, 10, 0)));

    if (TLC)
        ADD_TEST(testSpuriousEligibility());
    ADD_TEST(testBasicSizeClassNotSet());
    ADD_TEST(testSmallDoubleFree());
    ADD_TEST(testSmallFreeInner());
    ADD_TEST(testSmallFreeNextWithShrink());
    ADD_TEST(testSmallFreeNextWithoutShrink());
    ADD_TEST(testSmallFreeNextBeforeShrink());
}
#endif // SEGHEAP

void addLargeHeapTests()
{
    DisableBitfit disableBitfit;
    
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(10000), 10000, 1, 10000, 200000000));
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(10000), 10000, 10, 100000, 200000000));
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(10000), 10000, 40, 400000, 200000000));
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(10000), 10000, 5000, 50000000, 200000000));
    ADD_TEST(testLargeAllocation(AlignedPrimitiveAllocator(10000, 16384), 16384, 1, 16384, 200000000));
    ADD_TEST(testLargeAllocation(AlignedPrimitiveAllocator(10000, 16384), 16384, 10, 163840, 200000000));
    
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 40960, 1)));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot")));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("small", 4096, 1),
                                        AllocationProgram::free("small")));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 4096, 1),
                                        AllocationProgram::allocate("two", 4096, 1),
                                        AllocationProgram::allocate("three", 4096, 1),
                                        AllocationProgram::allocate("four", 4096, 1),
                                        AllocationProgram::allocate("five", 4096, 1),
                                        AllocationProgram::allocate("six", 4096, 1),
                                        AllocationProgram::allocate("seven", 4096, 1),
                                        AllocationProgram::allocate("eight", 4096, 1),
                                        AllocationProgram::allocate("nine", 4096, 1),
                                        AllocationProgram::allocate("ten", 4096, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::free("nine"),
                                        AllocationProgram::free("ten")));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 4096, 1),
                                        AllocationProgram::allocate("two", 4096, 1),
                                        AllocationProgram::allocate("three", 4096, 1),
                                        AllocationProgram::allocate("four", 4096, 1),
                                        AllocationProgram::allocate("five", 4096, 1),
                                        AllocationProgram::allocate("six", 4096, 1),
                                        AllocationProgram::allocate("seven", 4096, 1),
                                        AllocationProgram::allocate("eight", 4096, 1),
                                        AllocationProgram::allocate("nine", 4096, 1),
                                        AllocationProgram::allocate("ten", 4096, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::free("nine"),
                                        AllocationProgram::free("ten"),
                                        AllocationProgram::allocate("big", 40960, 1)));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 4096, 1),
                                        AllocationProgram::allocate("two", 36864, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::allocate("big", 40960, 1)));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 4096, 1),
                                        AllocationProgram::allocate("two", 36864, 1),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::allocate("big", 40960, 1)));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 8192, 1),
                                        AllocationProgram::allocate("two", 4096, 1),
                                        AllocationProgram::allocate("three", 28672, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::allocate("big", 40960, 1)));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::exact(65536),
                                        AllocationProgram::allocate("boot", 65536, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("aligned", 32768, 32768),
                                        AllocationProgram::free("aligned"),
                                        AllocationProgram::allocate("big", 65536, 1)));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::upperBound(65536 + 4096),
                                        AllocationProgram::allocate("boot", 65536, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("aligned", 32768, 32768),
                                        AllocationProgram::allocate("one", 4096, 1),
                                        AllocationProgram::allocate("two", 4096, 1),
                                        AllocationProgram::allocate("three", 4096, 1),
                                        AllocationProgram::allocate("four", 4096, 1),
                                        AllocationProgram::allocate("five", 4096, 1),
                                        AllocationProgram::allocate("six", 4096, 1),
                                        AllocationProgram::allocate("seven", 4096, 1),
                                        AllocationProgram::allocate("eight", 4096, 1),
                                        AllocationProgram::free("aligned"),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::allocate("big", 65536, 1)));
    ADD_TEST(testComplexLargeAllocation(PrimitiveComplexAllocator(),
                                        ExpectedBytes::upperBound(131072),
                                        AllocationProgram::allocate("aligned", 32768, 32768),
                                        AllocationProgram::allocate("one", 4096, 1),
                                        AllocationProgram::allocate("two", 4096, 1),
                                        AllocationProgram::allocate("three", 4096, 1),
                                        AllocationProgram::allocate("four", 4096, 1),
                                        AllocationProgram::allocate("five", 4096, 1),
                                        AllocationProgram::allocate("six", 4096, 1),
                                        AllocationProgram::allocate("seven", 4096, 1),
                                        AllocationProgram::allocate("eight", 4096, 1),
                                        AllocationProgram::free("aligned"),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::allocate("big", 65536, 1)));
    
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 5851, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 5851, 1),
                                        AllocationProgram::free("boot")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 5851, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("small", 585, 1),
                                        AllocationProgram::free("small")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 5851, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 585, 1),
                                        AllocationProgram::allocate("two", 585, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 5851, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 585, 1),
                                        AllocationProgram::allocate("two", 585, 1),
                                        AllocationProgram::allocate("three", 585, 1),
                                        AllocationProgram::allocate("four", 585, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::exact(286720),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 585, 1),
                                        AllocationProgram::allocate("two", 585, 1),
                                        AllocationProgram::allocate("three", 585, 1),
                                        AllocationProgram::allocate("four", 585, 1),
                                        AllocationProgram::allocate("five", 585, 1),
                                        AllocationProgram::allocate("six", 585, 1),
                                        AllocationProgram::allocate("seven", 585, 1),
                                        AllocationProgram::allocate("eight", 585, 1),
                                        AllocationProgram::allocate("nine", 585, 1),
                                        AllocationProgram::allocate("ten", 585, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::free("nine"),
                                        AllocationProgram::free("ten"),
                                        AllocationProgram::allocate("firstHalf", 20480, 1),
                                        AllocationProgram::allocate("secondHalf", 20480, 1),
                                        AllocationProgram::free("firstHalf"),
                                        AllocationProgram::free("secondHalf")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::upperBound(638976),
                                        AllocationProgram::allocate("boot", 40960, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("big", 9362, 65536),
                                        AllocationProgram::allocate("two", 585, 1),
                                        AllocationProgram::allocate("three", 585, 1),
                                        AllocationProgram::allocate("four", 585, 1),
                                        AllocationProgram::allocate("five", 585, 1),
                                        AllocationProgram::allocate("six", 585, 1),
                                        AllocationProgram::allocate("seven", 585, 1),
                                        AllocationProgram::allocate("eight", 585, 1),
                                        AllocationProgram::allocate("nine", 585, 1),
                                        AllocationProgram::allocate("ten", 585, 1),
                                        AllocationProgram::free("big"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::free("nine"),
                                        AllocationProgram::free("ten"),
                                        AllocationProgram::allocate("firstHalf", 20480, 1),
                                        AllocationProgram::allocate("secondHalf", 20480, 1),
                                        AllocationProgram::free("firstHalf"),
                                        AllocationProgram::free("secondHalf")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::upperBound(589824),
                                        AllocationProgram::allocate("boot", 65536, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("big", 9362, 65536),
                                        AllocationProgram::allocate("two", 585, 1),
                                        AllocationProgram::allocate("three", 585, 1),
                                        AllocationProgram::allocate("four", 585, 1),
                                        AllocationProgram::allocate("five", 585, 1),
                                        AllocationProgram::allocate("six", 585, 1),
                                        AllocationProgram::allocate("seven", 585, 1),
                                        AllocationProgram::allocate("eight", 585, 1),
                                        AllocationProgram::allocate("nine", 585, 1),
                                        AllocationProgram::allocate("ten", 585, 1),
                                        AllocationProgram::free("big"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::free("nine"),
                                        AllocationProgram::free("ten"),
                                        AllocationProgram::allocate("firstHalf", 32768, 1),
                                        AllocationProgram::allocate("secondHalf", 32768, 1),
                                        AllocationProgram::free("firstHalf"),
                                        AllocationProgram::free("secondHalf")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(7, 1),
                                        ExpectedBytes::exact(458752),
                                        AllocationProgram::allocate("boot", 65536, 65536),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("big", 9362, 65536),
                                        AllocationProgram::allocate("two", 585, 1),
                                        AllocationProgram::allocate("three", 585, 1),
                                        AllocationProgram::allocate("four", 585, 1),
                                        AllocationProgram::allocate("five", 585, 1),
                                        AllocationProgram::allocate("six", 585, 1),
                                        AllocationProgram::allocate("seven", 585, 1),
                                        AllocationProgram::allocate("eight", 585, 1),
                                        AllocationProgram::allocate("nine", 585, 1),
                                        AllocationProgram::allocate("ten", 585, 1),
                                        AllocationProgram::free("big"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::free("nine"),
                                        AllocationProgram::free("ten"),
                                        AllocationProgram::allocate("firstHalf", 32768, 1),
                                        AllocationProgram::allocate("secondHalf", 32768, 1),
                                        AllocationProgram::free("firstHalf"),
                                        AllocationProgram::free("secondHalf")));
    
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 10, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 10, 1),
                                        AllocationProgram::free("boot")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 10, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("small", 1, 1),
                                        AllocationProgram::free("small")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 10, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 1, 1),
                                        AllocationProgram::allocate("two", 1, 1),
                                        AllocationProgram::allocate("three", 1, 1),
                                        AllocationProgram::allocate("four", 1, 1),
                                        AllocationProgram::allocate("five", 1, 1),
                                        AllocationProgram::allocate("six", 1, 1),
                                        AllocationProgram::allocate("seven", 1, 1),
                                        AllocationProgram::allocate("eight", 1, 1),
                                        AllocationProgram::allocate("nine", 1, 1),
                                        AllocationProgram::allocate("ten", 1, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::free("nine"),
                                        AllocationProgram::free("ten")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 10, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 1, 1),
                                        AllocationProgram::allocate("two", 1, 1),
                                        AllocationProgram::allocate("three", 1, 1),
                                        AllocationProgram::allocate("four", 1, 1),
                                        AllocationProgram::allocate("five", 1, 1),
                                        AllocationProgram::allocate("six", 1, 1),
                                        AllocationProgram::allocate("seven", 1, 1),
                                        AllocationProgram::allocate("eight", 1, 1),
                                        AllocationProgram::allocate("nine", 1, 1),
                                        AllocationProgram::allocate("ten", 1, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::free("nine"),
                                        AllocationProgram::free("ten"),
                                        AllocationProgram::allocate("big", 10, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 10, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 1, 1),
                                        AllocationProgram::allocate("two", 9, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::allocate("big", 10, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 10, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 1, 1),
                                        AllocationProgram::allocate("two", 9, 1),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::allocate("big", 10, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::exact(40960),
                                        AllocationProgram::allocate("boot", 10, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 2, 1),
                                        AllocationProgram::allocate("two", 1, 1),
                                        AllocationProgram::allocate("three", 7, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::allocate("big", 10, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::upperBound(98304),
                                        AllocationProgram::allocate("boot", 16, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("aligned", 8, 32768),
                                        AllocationProgram::free("aligned"),
                                        AllocationProgram::allocate("big", 16, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::upperBound(98304),
                                        AllocationProgram::allocate("boot", 16, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("aligned", 8, 32768),
                                        AllocationProgram::allocate("one", 1, 1),
                                        AllocationProgram::allocate("two", 1, 1),
                                        AllocationProgram::allocate("three", 1, 1),
                                        AllocationProgram::allocate("four", 1, 1),
                                        AllocationProgram::allocate("five", 1, 1),
                                        AllocationProgram::allocate("six", 1, 1),
                                        AllocationProgram::allocate("seven", 1, 1),
                                        AllocationProgram::allocate("eight", 1, 1),
                                        AllocationProgram::free("aligned"),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::allocate("big", 16, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(4096, 8),
                                        ExpectedBytes::upperBound(131072),
                                        AllocationProgram::allocate("aligned", 8, 32768),
                                        AllocationProgram::allocate("one", 1, 1),
                                        AllocationProgram::allocate("two", 1, 1),
                                        AllocationProgram::allocate("three", 1, 1),
                                        AllocationProgram::allocate("four", 1, 1),
                                        AllocationProgram::allocate("five", 1, 1),
                                        AllocationProgram::allocate("six", 1, 1),
                                        AllocationProgram::allocate("seven", 1, 1),
                                        AllocationProgram::allocate("eight", 1, 1),
                                        AllocationProgram::free("aligned"),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::free("five"),
                                        AllocationProgram::free("six"),
                                        AllocationProgram::free("seven"),
                                        AllocationProgram::free("eight"),
                                        AllocationProgram::allocate("big", 16, 1)));
    
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(32768, 32768),
                                        ExpectedBytes::exact(32768),
                                        AllocationProgram::allocate("boot", 1, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedUnitComplexAllocator(32768, 32768),
                                        ExpectedBytes::exact(32768),
                                        AllocationProgram::allocate("boot", 1, 1)));

    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(5000, 8),
                                        ExpectedBytes::exact(5000),
                                        AllocationProgram::allocate("boot", 1, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(5000, 8),
                                        ExpectedBytes::exact(5000),
                                        AllocationProgram::allocate("boot", 1, 1),
                                        AllocationProgram::free("boot")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(5000, 8),
                                        ExpectedBytes::upperBound(15000),
                                        AllocationProgram::allocate("boot", 1, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("reboot", 2, 1),
                                        AllocationProgram::free("reboot")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(5000, 8),
                                        ExpectedBytes::exact(5120000),
                                        AllocationProgram::allocate("boot", 1024, 1),
                                        AllocationProgram::free("boot")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(5000, 8),
                                        ExpectedBytes::exact(5120000),
                                        AllocationProgram::allocate("boot", 1024, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("firstHalf", 512, 1),
                                        AllocationProgram::allocate("secondHalf", 512, 1),
                                        AllocationProgram::free("firstHalf"),
                                        AllocationProgram::free("secondHalf"),
                                        AllocationProgram::allocate("reboot", 1024, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(5000, 8),
                                        ExpectedBytes::exact(5120000),
                                        AllocationProgram::allocate("boot", 1024, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("firstHalf", 512, 1),
                                        AllocationProgram::allocate("secondHalf", 512, 1),
                                        AllocationProgram::free("firstHalf"),
                                        AllocationProgram::free("secondHalf"),
                                        AllocationProgram::allocate("reboot", 1023, 1)));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(5000, 8),
                                        ExpectedBytes::exact(5120000),
                                        AllocationProgram::allocate("boot", 1024, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 1, 1),
                                        AllocationProgram::allocate("two", 1, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two")));
    ADD_TEST(testComplexLargeAllocation(IsolatedComplexAllocator(5000, 8),
                                        ExpectedBytes::exact(5120000),
                                        AllocationProgram::allocate("boot", 1024, 1),
                                        AllocationProgram::free("boot"),
                                        AllocationProgram::allocate("one", 1, 1),
                                        AllocationProgram::allocate("two", 1, 1),
                                        AllocationProgram::allocate("three", 1, 1),
                                        AllocationProgram::allocate("four", 1, 1),
                                        AllocationProgram::free("one"),
                                        AllocationProgram::free("two"),
                                        AllocationProgram::free("three"),
                                        AllocationProgram::free("four"),
                                        AllocationProgram::allocate("oneAgain", 1, 1),
                                        AllocationProgram::allocate("twoAgain", 1, 1),
                                        AllocationProgram::allocate("threeAgain", 1, 1),
                                        AllocationProgram::allocate("fourAgain", 1, 1),
                                        AllocationProgram::free("oneAgain"),
                                        AllocationProgram::free("twoAgain"),
                                        AllocationProgram::free("threeAgain"),
                                        AllocationProgram::free("fourAgain")));
    ADD_TEST(testLargeDoubleFree());
    ADD_TEST(testLargeOffsetFree());
}

#if SEGHEAP
void addMediumHeapTests()
{
    DisableBitfit disableBitfit;
    
    ADD_TEST(testFreeListRefillSpans(4096, 2048, 6, 0, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(4096, 2048, 6, 1, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(4096, 2048, 5, 0, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(4096, 2048, 5, 1, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(4096, 2048, 1, 0, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(2048, 4096, 6, 0, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(2048, 4096, 6, 1, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(2048, 4096, 5, 0, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(2048, 4096, 5, 1, pas_medium_segregated_page_config_variant));
    ADD_TEST(testFreeListRefillSpans(2048, 4096, 1, 0, pas_medium_segregated_page_config_variant));
    
#if TLC
    ADD_TEST(testInternalScavenge(4096, 4096, 100, false, false, 8192, 8192, 100, false, 100, false, 4, 4, 8, 11, 11, 11, false));
    ADD_TEST(testInternalScavenge(4096, 4096, 100, true, true, 8192, 8192, 100, true, 100, true, 4, 4, 7, 11, 11, 11, false));
#endif

    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(2048), 2048, pas_medium_segregated_object_kind, 10, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(2072), 2296, pas_small_segregated_object_kind, 10, 3),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(1600), 1608, pas_small_segregated_object_kind, 10, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(1617), 2048, pas_medium_segregated_object_kind, 10, 1),
                 SIZE_CLASS_PROGRAM(
                     AlignedPrimitiveAllocator(2048, 2048), 2048, pas_medium_segregated_object_kind, 10, 0),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(2072), 2296, pas_small_segregated_object_kind, 10, 1),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(1617), 2048, pas_medium_segregated_object_kind, 10, 0)));
    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(4608), 4608, pas_medium_segregated_object_kind, 5, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(4096), 4608, pas_medium_segregated_object_kind, 5, 1),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(3584), 4608, pas_medium_segregated_object_kind, 5, 1),
                 SIZE_CLASS_PROGRAM(
                     AlignedPrimitiveAllocator(4096, 4096), 4096, pas_medium_segregated_object_kind, 5, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(4608), 4608, pas_medium_segregated_object_kind, 5, 0),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(3584), 4096, pas_medium_segregated_object_kind, 5, 1),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(4096), 4096, pas_medium_segregated_object_kind, 5, 0)));
    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16384), 16384, pas_medium_segregated_object_kind, 4, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16000), 16384, pas_medium_segregated_object_kind, 3, 1)));
    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16384), 16384, pas_medium_segregated_object_kind, 4, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(18712), 21504, pas_medium_segregated_object_kind, 3, 2)));
    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16384), 16384, pas_medium_segregated_object_kind, 3, 2),
                 SIZE_CLASS_PROGRAM(
                     AlignedPrimitiveAllocator(16384, 16384), 16384, pas_medium_segregated_object_kind, 3, 0),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(18712), 21504, pas_medium_segregated_object_kind, 3, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16384), 16384, pas_medium_segregated_object_kind, 3, 1)));
    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16896), 18432, pas_medium_segregated_object_kind, 3, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16384), 18432, pas_medium_segregated_object_kind, 3, 1),
                 SIZE_CLASS_PROGRAM(
                     AlignedPrimitiveAllocator(16384, 16384), 16384, pas_medium_segregated_object_kind, 4, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16896), 18432, pas_medium_segregated_object_kind, 3, 1),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16384), 16384, pas_medium_segregated_object_kind, 3, 0)));
    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16384), 16384, pas_medium_segregated_object_kind, 3, 2),
                 SIZE_CLASS_PROGRAM(
                     AlignedPrimitiveAllocator(16384, 16384), 16384, pas_medium_segregated_object_kind, 1, 0),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(16000), 16384, pas_medium_segregated_object_kind, 3, 1)));
}

void addLargerThanMediumHeapTests()
{
    DisableBitfit disableBitfit;
    
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(1000000), 1000000, 1, 1000000, 200000000));
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(1000000), 1000000, 10, 10000000, 200000000));
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(1000000), 1000000, 40, 40000000, 200000000));
    ADD_TEST(testLargeAllocation(AlignedPrimitiveAllocator(1000000, 16384), 1015808, 1, 1015808, 200000000));
    ADD_TEST(testLargeAllocation(AlignedPrimitiveAllocator(1000000, 16384), 1015808, 10, 10158080, 200000000));
}

void addMargeBitfitTests()
{
    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(20000), 21504, pas_medium_segregated_object_kind, 5, 2),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(30000), 32768, pas_marge_bitfit_object_kind, 5, 1),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(29000), 32768, pas_marge_bitfit_object_kind, 5, 1)));
    ADD_TEST(testSizeClassCreation(
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(300000), 303104, pas_marge_bitfit_object_kind, 5, 1),
                 SIZE_CLASS_PROGRAM(
                     PrimitiveAllocator(290000), 290816, pas_marge_bitfit_object_kind, 5, 1)));
}

void addLargerThanMargeBitfitTests()
{
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(10000000), 10000000, 1, 10000000, 11000000));
    ADD_TEST(testLargeAllocation(PrimitiveAllocator(10000000), 10000000, 10, 100000000, 101000000));
    ADD_TEST(testLargeAllocation(AlignedPrimitiveAllocator(10000000, 16384), 10010624, 1, 10010624, 11000000));
    ADD_TEST(testLargeAllocation(AlignedPrimitiveAllocator(10000000, 16384), 10010624, 10, 100106240, 101000000));
}
#endif // SEGHEAP

void addCombinedHeapTests()
{
    ADD_TEST(testAllocationCountProgression(PrimitiveComplexAllocator(), 0, 6000, 8, 1, 1, true));
    ADD_TEST(testAllocationCountProgression(PrimitiveComplexAllocator(), 0, 6000, 80, 1, 10, true));
    ADD_TEST(testAllocationCountProgression(PrimitiveComplexAllocator(), 0, 60000, 80, 1, 1, true));
    ADD_TEST(testAllocationCountProgression(PrimitiveComplexAllocator(), 0, 600000, 4000, 1, 1, false));
    ADD_TEST(testAllocationCountProgression(PrimitiveComplexAllocator(), 0, 6000000, 40000, 1, 1, false));

    ADD_TEST(testAllocationCountProgression(IsolatedComplexAllocator(7, 1), 0, 600, 1, 1, 1, true));
    ADD_TEST(testAllocationCountProgression(IsolatedComplexAllocator(7, 1), 0, 6000, 1, 1, 1, true));
    
    ADD_TEST(testAllocationChaos(1, 10, 1000, 1000000, 10000000, true));
    ADD_TEST(testAllocationChaos(10, 10, 1000, 100000, 10000000, true));
    ADD_TEST(testAllocationChaos(10, 10, 1000, 100000, 10000000, false));
    ADD_TEST(testUtilityAllocationChaos(1, 1000, 1000000, 10000000));
    ADD_TEST(testUtilityAllocationChaos(10, 1000, 100000, 10000000));
    ADD_TEST(testCombinedAllocationChaos(1, 10, 1000, 1000000, 10000000, false));
    ADD_TEST(testCombinedAllocationChaos(10, 10, 1000, 100000, 10000000, false));
    ADD_TEST(testCombinedAllocationChaos(1, 10, 1000, 1000000, 10000000, true));
    ADD_TEST(testCombinedAllocationChaos(10, 10, 1000, 100000, 10000000, true));
    
    ADD_TEST(testReallocatePrimitive(100, 104, 1000, 1000, 1000));
    ADD_TEST(testReallocatePrimitive(100, 104, 100000, 102400, 1000));
    ADD_TEST(testReallocatePrimitive(1000, 1000, 100, 104, 1000));
    ADD_TEST(testReallocatePrimitive(100000, 102400, 100, 104, 1000));
    if (hasScope("only-small"))
        ADD_TEST(testReallocateArray(7, 1, 100, 728, 1000, 7000, 1000));
    else
        ADD_TEST(testReallocateArray(7, 1, 100, 728, 1000, 7168, 1000));
    ADD_TEST(testReallocateArray(7, 1, 100, 728, 5000, 35000, 1000));
    ADD_TEST(testReallocateArray(7, 1, 5000, 35000, 100, 728, 1000));
}

void addAllTestsImpl()
{
    addDeallocationTests();
#if SEGHEAP
    addSmallHeapTests();
    if (hasScope("only-small"))
        addLargeHeapTests();
    else {
        addMediumHeapTests();
        addMargeBitfitTests();
        addLargerThanMediumHeapTests();
        addLargerThanMargeBitfitTests();
    }
#else
    addLargeHeapTests();
#endif
    addCombinedHeapTests();
}

void addAllTests()
{
    {
        TestScope testScope(
            "only-small",
            [] () {
                pas_medium_segregated_page_config_variant_is_enabled_override = false;
            });
        
        addAllTestsImpl();
    }
    
    {
        TestScope testScope(
            "small-and-medium",
            [] () {
            });
        
        addAllTestsImpl();
        addSmallHeapTests();
    }
}

} // anonymous namespace

#endif // PAS_ENABLE_THINGY

void addThingyAndUtilityHeapAllocationTests()
{
#if PAS_ENABLE_THINGY
    // It's important that we run this test with a totally clean heap. This assert reminds us
    // of this. If this assert fires, it's probably because one of the other test suites used
    // pas API outside of ADD_TEST().
    CHECK(!pas_bootstrap_free_heap.num_mapped_bytes);

    ForceExclusives forceExclusives;
    ForceTLAs forceTLAs;
    EpochIsCounter epochIsCounter;

    {
        InstallVerifier installVerifier;
        addAllTests();
    }

    addAllTests();
#endif // PAS_ENABLE_THINGY
}

