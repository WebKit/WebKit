/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if !USE(SYSTEM_MALLOC)

#include <bmalloc/bmalloc.h>
#include <bmalloc/Environment.h>
#include <bmalloc/IsoHeapInlines.h>

#include <cmath>
#include <cstdlib>
#include <set>
#include <vector>

using namespace bmalloc;
using namespace bmalloc::api;

#define RUN(test) do {                          \
if (!shouldRun(#test))                  \
break;                              \
puts(#test "...");                      \
test;                                   \
puts(#test ": OK!");                    \
} while (false)

// Nothing fancy for now; we just use the existing WTF assertion machinery.
#define CHECK(x) do {                                                   \
if (!!(x))                                                      \
break;                                                      \
fprintf(stderr, "%s:%d: in %s: assertion %s failed.\n",         \
__FILE__, __LINE__, __PRETTY_FUNCTION__, #x);               \
abort();                                                        \
} while (false)

static std::set<void*> toptrset(const std::vector<void*>& ptrs)
{
    std::set<void*> result;
    for (void* ptr : ptrs) {
        if (ptr)
            result.insert(ptr);
    }
    return result;
}

static void assertEmptyPointerSet(const std::set<void*>& pointers)
{
    if (PerProcess<Environment>::get()->isDebugHeapEnabled()) {
        printf("    skipping checks because DebugHeap.\n");
        return;
    }
    if (pointers.empty())
        return;
    printf("Pointer set not empty!\n");
    printf("Pointers:");
    for (void* ptr : pointers)
        printf(" %p", ptr);
    printf("\n");
    CHECK(pointers.empty());
}

template<typename heapType>
static void assertHasObjects(IsoHeap<heapType>& heap, std::set<void*> pointers)
{
    if (PerProcess<Environment>::get()->isDebugHeapEnabled()) {
        printf("    skipping checks because DebugHeap.\n");
        return;
    }
    auto& impl = heap.impl();
    std::lock_guard<Mutex> locker(impl.lock);
    impl.forEachLiveObject(
        [&] (void* object) {
            pointers.erase(object);
        });
    assertEmptyPointerSet(pointers);
}

template<typename heapType>
static void assertHasOnlyObjects(IsoHeap<heapType>& heap, std::set<void*> pointers)
{
    if (PerProcess<Environment>::get()->isDebugHeapEnabled()) {
        printf("    skipping checks because DebugHeap.\n");
        return;
    }
    auto& impl = heap.impl();
    std::lock_guard<Mutex> locker(impl.lock);
    impl.forEachLiveObject(
        [&] (void* object) {
            CHECK(pointers.erase(object) == 1);
        });
    assertEmptyPointerSet(pointers);
}

template<typename heapType>
static void assertClean(IsoHeap<heapType>& heap)
{
    scavengeThisThread();
    if (!PerProcess<Environment>::get()->isDebugHeapEnabled()) {
        auto& impl = heap.impl();
        {
            std::lock_guard<Mutex> locker(impl.lock);
            CHECK(!impl.numLiveObjects());
        }
    }
    heap.scavenge();
    if (!PerProcess<Environment>::get()->isDebugHeapEnabled()) {
        auto& impl = heap.impl();
        std::lock_guard<Mutex> locker(impl.lock);
        CHECK(!impl.numCommittedPages());
    }
}

TEST(bmalloc, IsoSimple)
{
    static IsoHeap<double> heap;
    void* ptr1 = heap.allocate();
    CHECK(ptr1);
    void* ptr2 = heap.allocate();
    CHECK(ptr2);
    CHECK(ptr1 != ptr2);
    CHECK(std::abs(static_cast<char*>(ptr1) - static_cast<char*>(ptr2)) >= 8);
    assertHasObjects(heap, {ptr1, ptr2});
    heap.deallocate(ptr1);
    heap.deallocate(ptr2);
    assertClean(heap);
}

TEST(bmalloc, IsoSimpleScavengeBeforeDealloc)
{
    static IsoHeap<double> heap;
    void* ptr1 = heap.allocate();
    CHECK(ptr1);
    void* ptr2 = heap.allocate();
    CHECK(ptr2);
    CHECK(ptr1 != ptr2);
    CHECK(std::abs(static_cast<char*>(ptr1) - static_cast<char*>(ptr2)) >= 8);
    scavengeThisThread();
    assertHasOnlyObjects(heap, {ptr1, ptr2});
    heap.deallocate(ptr1);
    heap.deallocate(ptr2);
    assertClean(heap);
}

TEST(bmalloc, IsoFlipFlopFragmentedPages)
{
    static IsoHeap<double> heap;
    std::vector<void*> ptrs;
    for (unsigned i = 100000; i--;) {
        void* ptr = heap.allocate();
        CHECK(ptr);
        ptrs.push_back(ptr);
    }
    for (unsigned i = 0; i < ptrs.size(); i += 2) {
        heap.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }
    for (unsigned i = ptrs.size() / 2; i--;)
        ptrs.push_back(heap.allocate());
    for (void* ptr : ptrs)
        heap.deallocate(ptr);
    assertClean(heap);
}

TEST(bmalloc, IsoFlipFlopFragmentedPagesScavengeInMiddle)
{
    static IsoHeap<double> heap;
    std::vector<void*> ptrs;
    for (unsigned i = 100000; i--;) {
        void* ptr = heap.allocate();
        CHECK(ptr);
        ptrs.push_back(ptr);
    }
    CHECK(toptrset(ptrs).size() == ptrs.size());
    for (unsigned i = 0; i < ptrs.size(); i += 2) {
        heap.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }
    heap.scavenge();
    unsigned numCommittedPagesBefore;
    auto& impl = heap.impl();
    {
        std::lock_guard<Mutex> locker(impl.lock);
        numCommittedPagesBefore = impl.numCommittedPages();
    }
    assertHasOnlyObjects(heap, toptrset(ptrs));
    for (unsigned i = ptrs.size() / 2; i--;)
        ptrs.push_back(heap.allocate());
    {
        std::lock_guard<Mutex> locker(impl.lock);
        CHECK(numCommittedPagesBefore == impl.numCommittedPages());
    }
    for (void* ptr : ptrs)
        heap.deallocate(ptr);
    assertClean(heap);
}

TEST(bmalloc, IsoFlipFlopFragmentedPagesScavengeInMiddle288)
{
    static IsoHeap<char[288]> heap;
    std::vector<void*> ptrs;
    for (unsigned i = 100000; i--;) {
        void* ptr = heap.allocate();
        CHECK(ptr);
        ptrs.push_back(ptr);
    }
    CHECK(toptrset(ptrs).size() == ptrs.size());
    for (unsigned i = 0; i < ptrs.size(); i += 2) {
        heap.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }
    heap.scavenge();
    unsigned numCommittedPagesBefore;
    auto& impl = heap.impl();
    {
        std::lock_guard<Mutex> locker(impl.lock);
        numCommittedPagesBefore = impl.numCommittedPages();
    }
    assertHasOnlyObjects(heap, toptrset(ptrs));
    for (unsigned i = ptrs.size() / 2; i--;)
        ptrs.push_back(heap.allocate());
    {
        std::lock_guard<Mutex> locker(impl.lock);
        CHECK(numCommittedPagesBefore == impl.numCommittedPages());
    }
    for (void* ptr : ptrs)
        heap.deallocate(ptr);
    assertClean(heap);
}

TEST(bmalloc, IsoMallocAndFreeFast)
{
    static IsoHeap<char[256]> heap;
    void* ptr = nullptr;
    for (int i = 0; i < 1e6; ++i) {
        ptr = heap.allocate();
        heap.deallocate(ptr);
    }
    CHECK(!IsoPageBase::pageFor(ptr)->isShared());
}

class BisoMalloced {
    MAKE_BISO_MALLOCED(BisoMalloced);
public:
    BisoMalloced(int x, float y)
        : x(x)
        , y(y)
    {
    }

    int x;
    float y;
};

MAKE_BISO_MALLOCED_IMPL(BisoMalloced);

TEST(bmalloc, BisoMalloced)
{
    BisoMalloced* ptr = new BisoMalloced(4, 5);
    assertHasObjects(BisoMalloced::bisoHeap(), { ptr });
    delete ptr;
    assertClean(BisoMalloced::bisoHeap());
}

class BisoMallocedInline {
    MAKE_BISO_MALLOCED_INLINE(BisoMalloced);
public:
    BisoMallocedInline(int x, float y)
        : x(x)
        , y(y)
    {
    }

    int x;
    float y;
};

TEST(bmalloc, BisoMallocedInline)
{
    BisoMallocedInline* ptr = new BisoMallocedInline(4, 5);
    assertHasObjects(BisoMallocedInline::bisoHeap(), { ptr });
    delete ptr;
    assertClean(BisoMallocedInline::bisoHeap());
}


TEST(bmalloc, ScavengedMemoryShouldBeReused)
{
    static IsoHeap<double> heap;

    auto run = [] (unsigned numPagesToCommit) {
        auto* ptr1 = heap.allocate();

        std::vector<void*> ptrs;

        for (unsigned i = 0; ;i++) {
            void* ptr = heap.allocate();
            CHECK(ptr);
            ptrs.push_back(ptr);
            if (heap.impl().numCommittedPages() == numPagesToCommit)
                break;
        }

        std::set<void*> uniquedPtrs = toptrset(ptrs);

        heap.deallocate(ptr1);
        for (unsigned i = 0; i < IsoPage<decltype(heap)::Config>::numObjects - 1; i++) {
            heap.deallocate(ptrs[i]);
            uniquedPtrs.erase(ptrs[i]);
        }

        scavenge();
        assertHasOnlyObjects(heap, uniquedPtrs);

        // FIXME: This only seems to pass when lldb is attached but the scavenger thread isn't running...
        // see: https://bugs.webkit.org/show_bug.cgi?id=198384
        // auto* ptr2 = heap.allocate();
        // CHECK(ptr1 == ptr2);
    };

    run(2);
}

#endif
