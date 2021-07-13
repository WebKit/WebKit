/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#if PAS_ENABLE_ISO && PAS_ENABLE_ISO_TEST

#include <functional>
#include "iso_heap.h"
#include "iso_heap_config.h"
#include "iso_test_heap.h"
#include "iso_test_heap_config.h"
#include "pas_baseline_allocator_table.h"
#include "pas_heap.h"
#include "pas_random.h"
#include "pas_scavenger.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_shared_page_directory.h"
#include "pas_segregated_view.h"
#include <set>
#include <thread>
#include <vector>

using namespace std;

namespace {

class FreeOrder {
public:
    FreeOrder() = default;
    virtual ~FreeOrder() = default;

    void setCount(size_t count) const
    {
        m_count = count;
        didSetCount();
    }

    virtual size_t getNext() const = 0;

protected:
    virtual void didSetCount() const { }
    
    mutable size_t m_count { 0 };
};

class FreeInOrder : public FreeOrder {
public:
    size_t getNext() const override
    {
        return m_index++;
    }
    
private:
    mutable size_t m_index { 0 };
};

class FreeBackwards : public FreeOrder {
public:
    size_t getNext() const override
    {
        return --m_count;
    }
};

class FreeRandom : public FreeOrder {
public:
    size_t getNext() const override
    {
        PAS_ASSERT(!m_indices.empty());
        size_t index = pas_get_random(pas_cheesy_random, static_cast<unsigned>(m_indices.size()));
        size_t result = m_indices[index];
        m_indices[index] = m_indices.back();
        m_indices.pop_back();
        return result;
    }

protected:
    void didSetCount() const override
    {
        for (size_t index = 0; index < m_count; ++index)
            m_indices.push_back(index);
    }
    
    mutable vector<size_t> m_indices;
};

static bool forEachSharedPageDirectoryCallbackAdaptor(
    pas_segregated_shared_page_directory* directory,
    void* arg)
{
    function<void(pas_segregated_shared_page_directory*)>* callback =
        static_cast<function<void(pas_segregated_shared_page_directory*)>*>(arg);

    (*callback)(directory);
    
    return true;
}

template<typename Func>
void forEachSharedPageDirectory(const Func& func)
{
    function<void(pas_segregated_shared_page_directory*)> callback = func;
    pas_shared_page_directory_by_size_for_each(
        &iso_page_caches.small_shared_page_directories,
        forEachSharedPageDirectoryCallbackAdaptor,
        &callback);
    pas_shared_page_directory_by_size_for_each(
        &iso_page_caches.medium_shared_page_directories,
        forEachSharedPageDirectoryCallbackAdaptor,
        &callback);
}

size_t numSharedPages()
{
    size_t result = 0;
    forEachSharedPageDirectory(
        [&] (pas_segregated_shared_page_directory* directory) {
            result += pas_segregated_directory_size(&directory->base);
        });
    return result;
}

size_t numCommittedSharedPages()
{
    size_t result = 0;
    forEachSharedPageDirectory(
        [&] (pas_segregated_shared_page_directory* directory) {
            result += pas_segregated_directory_num_committed_views(&directory->base);
        });
    return result;
}

void testSimplePartialAllocations(size_t size,
                                  size_t alignment,
                                  pas_segregated_page_config_variant expectedMode,
                                  size_t count,
                                  size_t secondCount,
                                  size_t numViews,
                                  size_t numPages,
                                  size_t secondNumViews,
                                  size_t secondNumPages,
                                  const FreeOrder& freeOrder)
{
    static constexpr bool verbose = false;
    
    struct ObjectData {
        ObjectData() = default;

        ObjectData(void* ptr, pas_segregated_view view)
            : ptr(ptr)
            , view(view)
        {
        }
        
        void* ptr { nullptr };
        pas_segregated_view view { nullptr };
    };

    freeOrder.setCount(count);

    vector<ObjectData> objects;
    set<void*> objectSet;
    set<pas_segregated_view> viewSet;
    set<pas_segregated_page*> pageSet;
    set<pas_segregated_global_size_directory*> sizeDirectorySet;
    
    pas_heap_ref heap = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(size, alignment);

    for (size_t index = 0; index < count; ++index) {
        void* ptr = iso_allocate(&heap);
        CHECK(ptr);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), alignment));
        pas_segregated_view view =
            pas_segregated_view_for_object(reinterpret_cast<uintptr_t>(ptr),
                                           &iso_heap_config);
        CHECK(view);
        if (verbose)
            cout << "View kind: " << pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)) << "\n";
        CHECK(pas_segregated_view_is_partial(view));
        CHECK_EQUAL(pas_segregated_view_get_page_config(view)->variant,
                    expectedMode);
        if (verbose)
            cout << "Allocated " << ptr << "\n";
        objects.push_back(ObjectData(ptr, view));
        CHECK(!objectSet.count(ptr));
        objectSet.insert(ptr);
        viewSet.insert(view);
        pageSet.insert(pas_segregated_view_get_page(view));
        pas_segregated_global_size_directory* sizeDirectory = pas_segregated_view_get_global_size_directory(view);
        CHECK_EQUAL(sizeDirectory->object_size, size);
        CHECK(pas_is_aligned(pas_segregated_global_size_directory_alignment(sizeDirectory), alignment));
        sizeDirectorySet.insert(sizeDirectory);
    }

    verifyMinimumObjectDistance(objectSet, size);

    pas_heap* impl = iso_heap_ref_get_heap(&heap);

    CHECK_EQUAL(viewSet.size(), numViews);
    CHECK_EQUAL(pageSet.size(), numPages);
    CHECK_EQUAL(sizeDirectorySet.size(), 1);
    CHECK_EQUAL(pas_segregated_heap_num_views(&impl->segregated_heap), numViews);
    CHECK_EQUAL(numSharedPages(), numPages);
    CHECK_EQUAL(numCommittedSharedPages(), numPages);

    scavenge();

    if (verbose) {
        cout << "After scavenging but before freeing:\n";
        printStatusReport();
    }

    CHECK_EQUAL(viewSet.size(), numViews);
    CHECK_EQUAL(pageSet.size(), numPages);
    CHECK_EQUAL(pas_segregated_heap_num_views(&impl->segregated_heap), numViews);
    CHECK_EQUAL(numSharedPages(), numPages);
    CHECK_EQUAL(numCommittedSharedPages(), numPages);

    for (size_t index = 0; index < count; ++index) {
        void* ptr = objects[freeOrder.getNext()].ptr;
        if (verbose)
            cout << "Deallocating " << ptr << "\n";
        iso_deallocate(ptr);
    }

    if (verbose) {
        cout << "After freeing but before scavenging:\n";
        printStatusReport();
    }
    
    scavenge();
    
    if (verbose) {
        cout << "After freeing and scavenging:\n";
        printStatusReport();
    }

    CHECK_EQUAL(viewSet.size(), numViews);
    CHECK_EQUAL(pageSet.size(), numPages);
    CHECK_EQUAL(pas_segregated_heap_num_views(&impl->segregated_heap), numViews);
    CHECK_EQUAL(numSharedPages(), numPages);
    CHECK_EQUAL(
        numCommittedSharedPages(),
        scavengeKind == pas_scavenger_run_synchronously_now_kind ? 0 : numPages);

    for (size_t index = 0; index < secondCount; ++index) {
        void* ptr = iso_allocate(&heap);
        CHECK(ptr);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(ptr), alignment));
        if (verbose)
            cout << "Reallocated " << ptr << "\n";
        pas_segregated_view view =
            pas_segregated_view_for_object(reinterpret_cast<uintptr_t>(ptr),
                                           &iso_heap_config);
        CHECK(view);
        CHECK(pas_segregated_view_is_partial(view));
        CHECK_EQUAL(pas_segregated_view_get_page_config(view)->variant,
                    expectedMode);
        if (index < count) {
            CHECK_EQUAL(ptr, objects[index].ptr);
            CHECK_EQUAL(view, objects[index].view);
            continue;
        }
        objectSet.insert(ptr);
        viewSet.insert(view);
        pageSet.insert(pas_segregated_view_get_page(view));
        pas_segregated_global_size_directory* sizeDirectory = pas_segregated_view_get_global_size_directory(view);
        CHECK_EQUAL(sizeDirectory->object_size, size);
        CHECK(pas_is_aligned(pas_segregated_global_size_directory_alignment(sizeDirectory), alignment));
        sizeDirectorySet.insert(sizeDirectory);
    }

    verifyMinimumObjectDistance(objectSet, size);

    CHECK_EQUAL(viewSet.size(), secondNumViews);
    CHECK_EQUAL(pageSet.size(), secondNumPages);
    CHECK_EQUAL(sizeDirectorySet.size(), 1);
    CHECK_EQUAL(pas_segregated_heap_num_views(&impl->segregated_heap), secondNumViews);
    CHECK_EQUAL(numSharedPages(), secondNumPages);
    CHECK_EQUAL(numCommittedSharedPages(), secondNumPages);
}

void testFreeAroundPrimordialStop(size_t size, size_t alignment, size_t numObjectsToAllocate, bool scavengeAfterAllocating, size_t freeObjectStart, size_t freeObjectEnd, size_t repeat, size_t numAdditionalObjects)
{
    static constexpr bool verbose = false;
    
    vector<void*> objects;
    set<void*> objectSet;

    pas_heap_ref heap = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(size, alignment);

    for (size_t index = 0; index < numObjectsToAllocate; ++index) {
        void* ptr = iso_test_allocate(&heap);
        if (verbose)
            cout << "Allocated " << ptr << "\n";
        objects.push_back(ptr);
        CHECK(!objectSet.count(ptr));
        objectSet.insert(ptr);
    }

    while (repeat--) {
        if (scavengeAfterAllocating)
            scavenge();
        
        for (size_t index = freeObjectStart; index < freeObjectEnd; ++index) {
            if (verbose)
                cout << "Deallocating " << objects[index] << "\n";
            iso_test_deallocate(objects[index]);
        }
        
        scavenge();
        
        for (size_t index = freeObjectStart; index < freeObjectEnd; ++index) {
            void* ptr = iso_test_allocate(&heap);
            if (verbose)
                cout << "Reallocated " << ptr << "\n";
            CHECK_EQUAL(ptr, objects[index]);
        }
    }

    for (size_t index = numAdditionalObjects; index--;) {
        void* ptr = iso_test_allocate(&heap);
        CHECK(!objectSet.count(ptr));
        objectSet.insert(ptr);
    }

    verifyExactObjectDistance(objectSet, size);
}

void testFreeInterleavedAroundPrimordialStop(size_t size, size_t alignment, size_t numObjectsToAllocate, bool scavengeAfterAllocating, size_t freeObjectStart, size_t freeRunLength, size_t freeStrideLength, size_t repeat, size_t numAdditionalObjects)
{
    static constexpr bool verbose = false;
    
    vector<void*> objects;
    set<void*> objectSet;

    pas_heap_ref heap = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(size, alignment);

    for (size_t index = 0; index < numObjectsToAllocate; ++index) {
        void* ptr = iso_test_allocate(&heap);
        if (verbose)
            cout << "Allocated " << ptr << "\n";
        objects.push_back(ptr);
        CHECK(!objectSet.count(ptr));
        objectSet.insert(ptr);
    }

    while (repeat--) {
        if (scavengeAfterAllocating)
            scavenge();
        
        for (size_t index = freeObjectStart; index < numObjectsToAllocate;) {
            for (size_t count = freeRunLength; count-- && index < numObjectsToAllocate; ++index) {
                if (verbose)
                    cout << "Deallocating " << objects[index] << "\n";
                iso_test_deallocate(objects[index]);
            }
            
            index += freeStrideLength;
        }
        
        scavenge();
        
        for (size_t index = freeObjectStart; index < numObjectsToAllocate;) {
            for (size_t count = freeRunLength; count-- && index < numObjectsToAllocate; ++index) {
                void* ptr = iso_test_allocate(&heap);
                if (verbose)
                    cout << "Reallocated " << ptr << "\n";
                CHECK_EQUAL(ptr, objects[index]);
            }
            
            index += freeStrideLength;
        }
    }

    for (size_t index = numAdditionalObjects; index--;) {
        void* ptr = iso_test_allocate(&heap);
        CHECK(!objectSet.count(ptr));
        objectSet.insert(ptr);
    }

    verifyExactObjectDistance(objectSet, size);
}

struct PartialProgram {
    PartialProgram() = default;

    PartialProgram(size_t size,
                   size_t alignment,
                   size_t numObjectsFirst,
                   size_t freeBegin,
                   size_t freeEnd,
                   size_t numObjectsSecond)
        : size(size)
        , alignment(alignment)
        , numObjectsFirst(numObjectsFirst)
        , freeBegin(freeBegin)
        , freeEnd(freeEnd)
        , numObjectsSecond(numObjectsSecond)
    {
    }
    
    size_t size { 0 };
    size_t alignment { 0 };
    size_t numObjectsFirst { 0 };
    size_t freeBegin { 0 };
    size_t freeEnd { 0 };
    size_t numObjectsSecond { 0 };
};

void testMultiplePartialsFromDifferentHeapsPerShared(const vector<PartialProgram>& programs,
                                                     bool collate,
                                                     bool scavengeAfterAllocating,
                                                     size_t repeat)
{
    static constexpr bool verbose = false;
    
    vector<pas_heap_ref*> heaps;
    vector<vector<void*>> objects;
    vector<set<pas_segregated_view>> viewSets;
    set<void*> objectSet;
    set<pas_segregated_page*> pageSet;

    for (size_t index = 0; index < programs.size(); ++index) {
        heaps.push_back(new pas_heap_ref(ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(
                                             programs[index].size,
                                             programs[index].alignment)));
        objects.push_back({ });
        viewSets.push_back({ });
    }

    auto allocatePrimordial =
        [&] (size_t programIndex, size_t objectIndex) {
            PAS_ASSERT(objectIndex == objects[programIndex].size());
            void* ptr = iso_test_allocate(heaps[programIndex]);
            if (verbose)
                cout << "Allocated ptr = " << ptr << "\n";
            objects[programIndex].push_back(ptr);
            CHECK(!objectSet.count(ptr));
            objectSet.insert(ptr);
            pas_segregated_view view = pas_segregated_view_for_object(
                reinterpret_cast<uintptr_t>(ptr), &iso_test_heap_config);
            CHECK(pas_segregated_view_is_partial(view));
            viewSets[programIndex].insert(view);
            pageSet.insert(pas_segregated_view_get_page(view));
        };

    auto loopOverObjects =
        [&] (const auto& callback) {
            if (collate) {
                for (size_t objectIndex = 0; ; objectIndex++) {
                    bool didDoAny = false;
                    for (size_t index = 0; index < programs.size(); ++index) {
                        if (objectIndex >= programs[index].numObjectsFirst)
                            continue;
                        didDoAny = true;
                        callback(index, objectIndex);
                    }
                    if (!didDoAny)
                        break;
                }
            } else {
                for (size_t index = 0; index < programs.size(); ++index) {
                    for (size_t objectIndex = 0;
                         objectIndex < programs[index].numObjectsFirst;
                         ++objectIndex)
                        callback(index, objectIndex);
                }
            }
        };

    loopOverObjects(allocatePrimordial);

    for (size_t programIndex = 0; programIndex < programs.size(); ++programIndex)
        CHECK_EQUAL(viewSets[programIndex].size(), 1);
    CHECK_EQUAL(pageSet.size(), 1);

    while (repeat--) {
        if (scavengeAfterAllocating)
            scavenge();

        auto deallocate =
            [&] (size_t programIndex, size_t objectIndex) {
                if (objectIndex < programs[programIndex].freeBegin
                    || objectIndex >= programs[programIndex].freeEnd)
                    return;

                if (verbose)
                    cout << "Deallocating: " << objects[programIndex][objectIndex] << "\n";
                iso_test_deallocate(objects[programIndex][objectIndex]);
            };
        
        loopOverObjects(deallocate);

        scavenge();

        auto reallocate =
            [&] (size_t programIndex, size_t objectIndex) {
                if (objectIndex < programs[programIndex].freeBegin
                    || objectIndex >= programs[programIndex].freeEnd)
                    return;

                void* ptr = iso_test_allocate(heaps[programIndex]);
                if (verbose)
                    cout << "Reallocated ptr = " << ptr << "\n";
                CHECK_EQUAL(ptr, objects[programIndex][objectIndex]);
            };

        loopOverObjects(reallocate);
    }

    for (size_t programIndex = 0; programIndex < programs.size(); ++programIndex) {
        void* ptr = iso_test_allocate(heaps[programIndex]);
        CHECK(!objectSet.count(ptr));
        objects[programIndex].push_back(ptr);
        objectSet.insert(ptr);
    }

    for (size_t programIndex = 0; programIndex < programs.size(); ++programIndex) {
        for (void* object : objects[programIndex]) {
            auto iter = objectSet.find(object);
            CHECK(iter != objectSet.end());
            ++iter;
            if (iter != objectSet.end()) {
                CHECK_GREATER_EQUAL(
                    reinterpret_cast<uintptr_t>(*iter),
                    reinterpret_cast<uintptr_t>(object) + programs[programIndex].size);
            }
        }
    }
}

void addMultiplePartialsFromDifferentHeapsPerSharedTests(bool collate,
                                                         bool scavengeAfterAllocating)
{
    ADD_TEST(testMultiplePartialsFromDifferentHeapsPerShared(
                 {
                     PartialProgram(32, 8, 17, 5, 13, 100),
                     PartialProgram(64, 64, 11, 3, 9, 100),
                     PartialProgram(48, 16, 43, 21, 34, 100)
                 },
                 collate, scavengeAfterAllocating, 5));
    ADD_TEST(testMultiplePartialsFromDifferentHeapsPerShared(
                 {
                     PartialProgram(32, 8, 17, 0, 17, 100),
                     PartialProgram(64, 64, 11, 0, 11, 100),
                     PartialProgram(48, 16, 43, 0, 43, 100)
                 },
                 collate, scavengeAfterAllocating, 5));
    ADD_TEST(testMultiplePartialsFromDifferentHeapsPerShared(
                 {
                     PartialProgram(32, 8, 17, 0, 1, 100),
                     PartialProgram(64, 64, 11, 0, 1, 100),
                     PartialProgram(48, 16, 43, 0, 1, 100)
                 },
                 collate, scavengeAfterAllocating, 5));
    ADD_TEST(testMultiplePartialsFromDifferentHeapsPerShared(
                 {
                     PartialProgram(32, 8, 17, 16, 17, 100),
                     PartialProgram(64, 64, 11, 10, 11, 100),
                     PartialProgram(48, 16, 43, 42, 43, 100)
                 },
                 collate, scavengeAfterAllocating, 5));
    ADD_TEST(testMultiplePartialsFromDifferentHeapsPerShared(
                 {
                     PartialProgram(3072, 8, 13, 5, 13, 100),
                     PartialProgram(4096, 4096, 7, 3, 6, 100),
                     PartialProgram(10752, 512, 3, 1, 3, 100)
                 },
                 collate, scavengeAfterAllocating, 5));
}

void testMultiplePartialsFromDifferentThreadsPerShared(size_t size,
                                                       size_t alignment,
                                                       size_t numObjects,
                                                       size_t freeBegin,
                                                       size_t freeEnd,
                                                       size_t repeat,
                                                       size_t numThreads)
{
    static constexpr bool verbose = false;

    pas_segregated_size_directory_use_tabling = false;
    
    pas_heap_ref heap = ISO_HEAP_REF_INITIALIZER_WITH_ALIGNMENT(size, alignment);

    vector<thread> threads;
    vector<vector<void*>> objects;
    set<void*> objectSet;
    set<pas_segregated_view> viewSet;
    mutex startLock;
    mutex lock;
    mutex logLock;
    unsigned numThreadsStopped = 0;

    auto launch =
        [&] (const auto& func) {
            {
                lock_guard<mutex> locker(startLock);
                for (size_t index = 0; index < numThreads; ++index)
                    threads.push_back(thread([&, index] () {
                                                 {
                                                     lock_guard<mutex> locker(startLock);
                                                 }

                                                 func(index);

                                                 {
                                                     lock_guard<mutex> locker(startLock);
                                                     numThreadsStopped++;
                                                 }
                                             }));
            }
            
            for (thread& myThread : threads)
                myThread.join();
            
            threads.clear();

            CHECK_EQUAL(numThreadsStopped, numThreads);
            numThreadsStopped = 0;
        };

    launch(
        [&] (size_t threadIndex) {
            vector<void*> localObjects;
            set<void*> localObjectSet;

            for (size_t index = 0; index < numObjects; index++) {
                void* ptr = iso_test_allocate(&heap);
                pas_segregated_view view = pas_segregated_view_for_object(
                    reinterpret_cast<uintptr_t>(ptr),
                    &iso_test_heap_config);
                if (verbose) {
                    lock_guard<mutex> locker(logLock);
                    cout << "Thread " << threadIndex << " allocated " << index << ": " << ptr
                         << ", view = " << view << "\n";
                }
                localObjects.push_back(ptr);
                CHECK(!localObjectSet.count(ptr));
                localObjectSet.insert(ptr);
            }

            {
                lock_guard<mutex> locker(lock);
                objects.push_back(localObjects);
                for (void* object : localObjectSet) {
                    CHECK(!objectSet.count(object));
                    objectSet.insert(object);
                }
            }
        });

    if (verbose)
        cout << "Done with the allocation.\n";

    verifyMinimumObjectDistance(objectSet, size);

    for (void* object : objectSet) {
        viewSet.insert(pas_segregated_view_for_object(
                           reinterpret_cast<uintptr_t>(object), &iso_test_heap_config));
    }
    
    for (size_t repeatIndex = 0; repeatIndex < repeat; ++repeatIndex) {
        launch(
            [&] (size_t threadIndex) {
                for (size_t index = freeBegin; index < freeEnd; ++index) {
                    if (verbose) {
                        lock_guard<mutex> locker(logLock);
                        cout << "Thread " << threadIndex << " freeing " << objects[threadIndex][index] << "\n";
                    }
                    iso_test_deallocate(objects[threadIndex][index]);
                }
            });
        
        set<void*> freedObjects;
        for (size_t threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            for (size_t index = freeBegin; index < freeEnd; ++index)
                freedObjects.insert(objects[threadIndex][index]);
        }

        // If we launch numThreads and have each allocate numObjects, will that give us what we
        // want?
        //
        // No: we may get a different breakdown. Any view that is a continuation of a run of
        // allocations by some thread could now get acquired by a different thread. That thread
        // will use up that whole run and then it will want another run - at which point it might
        // get an *unbroken* run.
        //
        // We avert this by doing the rest of the test from a single thread.

        while (freedObjects.size()) {
            void* ptr = iso_test_allocate(&heap);
            if (verbose)
                cout << "Allocated " << ptr << "\n";
            pas_segregated_view view = pas_segregated_view_for_object(
                reinterpret_cast<uintptr_t>(ptr),
                &iso_test_heap_config);
            if (verbose)
                cout << "    view = " << view << "\n";
            if (freedObjects.count(ptr))
                freedObjects.erase(ptr);
            else {
                // The case where we allocate an object that we hadn't allocated in the primordial
                // run can _only_ happen on the first iteration.
                CHECK(!repeatIndex);
                if (!viewSet.count(view)) {
                    cout << "Still have freed objects: ";
                    bool first = true;
                    for (void* ptr : freedObjects) {
                        if (first)
                            first = false;
                        else
                            cout << ", ";
                        cout << ptr;
                    }
                    cout << "\n";
                }
                CHECK(viewSet.count(view));
            }
        }

        scavenge();
    }
}

unsigned incrementalRandom()
{
    static unsigned state;
    return state++;
}

unsigned zeroRandom()
{
    return 0;
}

void testTwoBaselinesEvictions(size_t size1, size_t size2, size_t count,
                               unsigned (*random)(), size_t numEvictions)
{
    pas_heap_ref heap1 = ISO_HEAP_REF_INITIALIZER(size1);
    pas_heap_ref heap2 = ISO_HEAP_REF_INITIALIZER(size2);
    vector<void*> objects;

    pas_mock_cheesy_random = random;

    for (size_t index = 0; index < count; ++index) {
        objects.push_back(iso_allocate(&heap1));
        objects.push_back(iso_allocate(&heap2));
    }

    CHECK_EQUAL(pas_num_baseline_allocator_evictions, numEvictions);

    scavenge();

    for (void* object : objects)
        iso_deallocate(object);

    scavenge();

    for (size_t index = 0; index < count; ++index) {
        CHECK_EQUAL(iso_allocate(&heap1),
                    objects[index * 2 + 0]);
        CHECK_EQUAL(iso_allocate(&heap2),
                    objects[index * 2 + 1]);
    }
}

void addScavengerDependentTests()
{
    DisableBitfit disableBitfit;
    DisableExplosion disableExplosion;
    
    {
        ForcePartials forcePartials;
        VerifyGranules verifyGranules;
        {
            ForceBaselines forceBaselines;
    
            // Test that we can allocate and reuse objects in partial views.
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 100, 100, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1000, 1000, 2, 2, 2, 2, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 100, 100, 1, 1, 1, 1, FreeBackwards()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1000, 1000, 2, 2, 2, 2, FreeBackwards()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 100, 100, 1, 1, 1, 1, FreeRandom()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1000, 1000, 2, 2, 2, 2, FreeRandom()));

            // Test that we can allocate and reuse objects in partial views and then allocate some more
            // objects in additional partial views.
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1, 2, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1, 4, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1, 100, 1, 1, 2, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1, 1000, 1, 1, 3, 2, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 100, 1000, 1, 1, 3, 2, FreeBackwards()));
            ADD_TEST(testSimplePartialAllocations(32, 8, pas_small_segregated_page_config_variant, 1000, 2000, 2, 2, 4, 4, FreeBackwards()));
    
            // Test that we can allocate objects with interesting sizes and that reuse works right.
            ADD_TEST(testSimplePartialAllocations(96, 8, pas_small_segregated_page_config_variant, 1000, 1000, 6, 6, 6, 6, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(112, 8, pas_small_segregated_page_config_variant, 1, 2, 1, 1, 1, 1, FreeBackwards()));
            ADD_TEST(testSimplePartialAllocations(208, 8, pas_small_segregated_page_config_variant, 1, 100, 1, 1, 3, 2, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(64, 16, pas_small_segregated_page_config_variant, 100, 1000, 1, 1, 5, 4, FreeBackwards()));
            ADD_TEST(testSimplePartialAllocations(112, 8, pas_small_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(304, 8, pas_small_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(944, 8, pas_small_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(2688, 8, pas_small_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(3072, 8, pas_medium_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(3072, 8, pas_medium_segregated_page_config_variant, 10, 10, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(3072, 8, pas_medium_segregated_page_config_variant, 100, 100, 3, 3, 3, 3, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(3072, 8, pas_medium_segregated_page_config_variant, 1, 10, 1, 1, 2, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(3072, 8, pas_medium_segregated_page_config_variant, 1, 100, 1, 1, 4, 3, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(4096, 8, pas_medium_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(10752, 8, pas_medium_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(20480, 8, pas_medium_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
    
            // Test that we can allocate objects with interesting alignment.
            ADD_TEST(testSimplePartialAllocations(32, 32, pas_small_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));
            ADD_TEST(testSimplePartialAllocations(96, 32, pas_small_segregated_page_config_variant, 1000, 1000, 6, 6, 6, 6, FreeRandom()));
            ADD_TEST(testSimplePartialAllocations(320, 64, pas_small_segregated_page_config_variant, 1000, 1000, 20, 20, 20, 20, FreeRandom()));
            ADD_TEST(testSimplePartialAllocations(256, 256, pas_small_segregated_page_config_variant, 1, 1, 1, 1, 1, 1, FreeInOrder()));

            // Test freeing objects before and after we stop the primordial allocator. Note that some of
            // these end up freeing objects after some primordials are stopped, but that's not the specific
            // goal of these tests.
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 0, 5, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 5, 10, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 2, 5, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 2, 8, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 5, 8, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 8, 10, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 5, 9, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 100, false, 99, 100, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 0, 5, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 5, 10, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 2, 5, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 2, 8, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 8, 10, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 5, 9, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 0, 10, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 0, 1, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 0, 5, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 5, 10, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 8, 10, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 8, 9, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 9, 10, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 0, 5, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 5, 10, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 2, 5, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 2, 8, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 5, 8, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 8, 10, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 5, 9, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 0, 5, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 5, 10, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 2, 5, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 2, 8, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 8, 10, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 5, 9, 1, 100));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 0, 10, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 0, 1, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 0, 5, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 5, 10, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 8, 10, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 8, 9, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 9, 10, 1, 10));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 0, 5, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 5, 10, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 2, 5, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 2, 8, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 5, 8, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 8, 10, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, false, 5, 9, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 100, false, 99, 100, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 0, 5, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 5, 10, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 2, 5, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 2, 8, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 8, 10, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, false, 5, 9, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 0, 10, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 0, 1, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 0, 5, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 5, 10, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 8, 10, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 8, 9, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, false, 9, 10, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 0, 5, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 5, 10, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 2, 5, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 2, 8, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 5, 8, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 8, 10, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(32, 8, 10, true, 5, 9, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 0, 5, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 5, 10, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 2, 5, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 2, 8, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 8, 10, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(64, 64, 10, true, 5, 9, 10, 100));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 0, 10, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 0, 1, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 0, 5, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 5, 10, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 8, 10, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 8, 9, 10, 10));
            ADD_TEST(testFreeAroundPrimordialStop(4096, 64, 10, true, 9, 10, 10, 10));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 0, 1, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 0, 1, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 0, 1, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 0, 1, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 1, 1, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 1, 1, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 1, 1, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 1, 1, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 10, 1, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 10, 1, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 10, 1, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 10, 1, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 0, 10, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 0, 10, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 0, 10, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 0, 10, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 1, 10, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 1, 10, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 1, 10, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 1, 10, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 10, 10, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 10, 10, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 10, 10, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, false, 10, 10, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 0, 1, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 0, 1, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 0, 1, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 0, 1, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 1, 1, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 1, 1, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 1, 1, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 1, 1, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 10, 1, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 10, 1, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 10, 1, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 10, 1, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 0, 10, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 0, 10, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 0, 10, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 0, 10, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 1, 10, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 1, 10, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 1, 10, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 1, 10, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 10, 10, 0, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 10, 10, 1, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 10, 10, 2, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(32, 32, 100, true, 10, 10, 10, 5, 100));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 1, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 1, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 1, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 1, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 2, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 2, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 2, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 2, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 3, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 3, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 3, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 3, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 4, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 4, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 4, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 0, 4, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 1, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 1, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 1, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 1, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 2, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 2, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 2, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 2, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 3, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 3, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 3, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 3, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 4, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 4, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 4, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, false, 1, 4, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 1, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 1, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 1, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 1, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 2, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 2, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 2, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 2, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 3, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 3, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 3, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 3, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 4, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 4, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 4, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 0, 4, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 1, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 1, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 1, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 1, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 2, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 2, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 2, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 2, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 3, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 3, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 3, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 3, 5, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 4, 0, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 4, 1, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 4, 2, 5, 20));
            ADD_TEST(testFreeInterleavedAroundPrimordialStop(4096, 4096, 20, true, 1, 4, 5, 5, 20));
        }

        {
            ForceTLAs forceTLAs;
            
            ADD_GROUP(addMultiplePartialsFromDifferentHeapsPerSharedTests(false, false));
            ADD_GROUP(addMultiplePartialsFromDifferentHeapsPerSharedTests(false, true));
            ADD_GROUP(addMultiplePartialsFromDifferentHeapsPerSharedTests(true, false));
            ADD_GROUP(addMultiplePartialsFromDifferentHeapsPerSharedTests(true, true));
        
            for (unsigned count = 10; count--;) {
                ADD_TEST(testMultiplePartialsFromDifferentThreadsPerShared(32, 8, 50, 0, 20, 10, 10));
                ADD_TEST(testMultiplePartialsFromDifferentThreadsPerShared(32, 8, 50, 0, 50, 10, 10));
                ADD_TEST(testMultiplePartialsFromDifferentThreadsPerShared(32, 8, 50, 40, 50, 10, 10));
                ADD_TEST(testMultiplePartialsFromDifferentThreadsPerShared(32, 8, 50, 0, 20, 10, 50));
                ADD_TEST(testMultiplePartialsFromDifferentThreadsPerShared(32, 8, 50, 0, 50, 10, 50));
                ADD_TEST(testMultiplePartialsFromDifferentThreadsPerShared(32, 8, 50, 40, 50, 10, 50));
            }
        }
    }

    {
        ForceBaselines forceBaselines;
        ADD_TEST(testTwoBaselinesEvictions(32, 64, 10000, incrementalRandom, 0));
        ADD_TEST(testTwoBaselinesEvictions(32, 64, 1000, zeroRandom, 1999));
    }
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO && PAS_ENABLE_ISO_TEST

void addIsoHeapPartialAndBaselineTests()
{
    TestScope commonSharedPageDirectories(
        "common-shared-page-directories",
        [] {
            iso_page_caches.small_shared_page_directories.log_shift = 31;
            iso_page_caches.medium_shared_page_directories.log_shift = 31;
            iso_test_page_caches.small_shared_page_directories.log_shift = 31;
            iso_test_page_caches.medium_shared_page_directories.log_shift = 31;
        });

    SuspendScavengerScope suspendScavenger;
    
#if PAS_ENABLE_ISO && PAS_ENABLE_ISO_TEST
    if (pas_thread_local_cache_is_guaranteed_to_destruct()) {
        RunScavengerFully runScavengerFully;
        addScavengerDependentTests();
    }
    {
        RunScavengerOnNonRemoteCaches runScavengerOnNonRemoteCaches;
        addScavengerDependentTests();
    }
#endif // PAS_ENABLE_ISO && PAS_ENABLE_ISO_TEST
}
