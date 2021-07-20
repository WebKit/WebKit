/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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
#include <functional>
#include <memory>
#include "pas_bootstrap_free_heap.h"
#include "pas_fast_large_free_heap.h"
#include "pas_heap_lock.h"
#include "pas_large_free.h"
#include "pas_large_free_heap_config.h"
#include "pas_page_malloc.h"
#include "pas_simple_large_free_heap.h"
#include <set>
#include <vector>

using namespace std;

namespace {

pas_alignment alignSimple(size_t size)
{
    return pas_alignment_create_traditional(size);
}

pas_aligned_allocation_result trappingAllocator(size_t size, pas_alignment alignment)
{
    CHECK(!"Should not have called allocator");
    return pas_aligned_allocation_result_create_empty();
}

void trappingDeallocator(void* base, size_t size)
{
    CHECK(!"Should not have called deallocator");
}

pas_aligned_allocation_result failingAllocator(size_t size, pas_alignment alignment)
{
    return pas_aligned_allocation_result_create_empty();
}

struct Action {
    enum Kind {
        Allocate,
        Deallocate
    };
    
    Action() = default;
    
    static Action allocate(
        size_t size, pas_alignment alignment, uintptr_t expectedResult,
        function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)> allocator = trappingAllocator,
        function<void(void* base, size_t size)> deallocator = trappingDeallocator)
    {
        Action result;
        result.kind = Allocate;
        result.size = size;
        result.alignment = alignment;
        result.allocator = allocator;
        result.deallocator = deallocator;
        result.result = expectedResult;
        return result;
    }
    
    static Action deallocate(
        uintptr_t base, size_t size,
        function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)> allocator = trappingAllocator,
        function<void(void* base, size_t size)> deallocator = trappingDeallocator)
    {
        Action result;
        result.kind = Deallocate;
        result.base = base;
        result.size = size;
        result.allocator = allocator;
        result.deallocator = deallocator;
        return result;
    }
    
    Kind kind { Allocate };
    uintptr_t base { 0 };
    size_t size { 0 };
    pas_alignment alignment { 0 };
    function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)> allocator;
    function<void(void* base, size_t size)> deallocator;
    uintptr_t result { 0 };
};

struct Free {
    Free() = default;
    
    Free(uintptr_t begin, uintptr_t end)
        : begin(begin)
        , end(end)
    {
    }
    
    bool operator==(const Free& other) const
    {
        return begin == other.begin
            && end == other.end;
    }
    
    bool operator<(const Free& other) const
    {
        if (begin != other.begin)
            return begin < other.begin;
        return end < other.end;
    }
    
    uintptr_t begin;
    uintptr_t end;
};

struct Allocation {
    Allocation() = default;
    
    Allocation(size_t expectedSize, pas_alignment expectedAlignment,
               uintptr_t leftPadding, uintptr_t resultBase,
               uintptr_t rightPadding, uintptr_t end)
        : expectedSize(expectedSize)
        , expectedAlignment(expectedAlignment)
        , leftPadding(leftPadding)
        , resultBase(resultBase)
        , rightPadding(rightPadding)
        , end(end)
    {
    }
    
    size_t expectedSize { 0 };
    pas_alignment expectedAlignment { 0 };
    uintptr_t leftPadding { 0 };
    uintptr_t resultBase { 0 };
    uintptr_t rightPadding { 0 };
    uintptr_t end { 0 };
};

function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)>
allocateFinite(vector<Allocation> allocations)
{
    shared_ptr<size_t> allocationCursor = make_shared<size_t>(0);
    return [=] (size_t size, pas_alignment alignment) -> pas_aligned_allocation_result {
        CHECK_LESS(*allocationCursor, allocations.size());
        
        Allocation allocation = allocations[(*allocationCursor)++];
        
        CHECK_EQUAL(size, allocation.expectedSize);
        CHECK_ALIGNMENT_EQUAL(alignment, allocation.expectedAlignment);
        
        pas_aligned_allocation_result result;
        result.left_padding = (void*)allocation.leftPadding;
        result.left_padding_size = allocation.resultBase - allocation.leftPadding;
        result.result = (void*)allocation.resultBase;
        result.result_size = allocation.rightPadding - allocation.resultBase;
        result.right_padding = (void*)allocation.rightPadding;
        result.right_padding_size = allocation.end - allocation.rightPadding;
        result.zero_mode = pas_zero_mode_may_have_non_zero;
        
        return result;
    };
}

function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)>
allocateOnce(size_t expectedSize,
             pas_alignment expectedAlignment,
             uintptr_t leftPadding,
             uintptr_t resultBase,
             uintptr_t rightPadding,
             uintptr_t end)
{
    return allocateFinite({ Allocation(expectedSize, expectedAlignment,
                                       leftPadding, resultBase,
                                       rightPadding, end) });
}

function<void(void* base, size_t size)> deallocateFinite(vector<Free> frees)
{
    shared_ptr<size_t> freeCursor = make_shared<size_t>(0);
    return [=] (void* base, size_t size) {
        CHECK_LESS(*freeCursor, frees.size());
        CHECK_EQUAL(reinterpret_cast<uintptr_t>(base), frees[*freeCursor].begin);
        CHECK_EQUAL(reinterpret_cast<uintptr_t>(base) + size, frees[*freeCursor].end);
        (*freeCursor)++;
    };
}

ostream& operator<<(ostream& out, const Free& free)
{
    out << "{begin = " << free.begin << ", end = " << free.end
        << ", size = " << (free.end - free.begin) << "}";
    return out;
}

pas_aligned_allocation_result allocatorAdapter(size_t size, pas_alignment alignment, void* arg)
{
    const function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)>* allocator =
        reinterpret_cast<const function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)>*>(arg);
    return (*allocator)(size, alignment);
}

void deallocatorAdapter(void* base, size_t size, void* arg)
{
    const function<void(void* base, size_t size)>* deallocator =
        reinterpret_cast<const function<void(void* base, size_t size)>*>(arg);
    (*deallocator)(base, size);
}

bool iterateAdapter(pas_large_free free,
                    void* arg)
{
    std::function<bool(pas_large_free)>* capturedFunc =
        static_cast<std::function<bool(pas_large_free)>*>(arg);
    return (*capturedFunc)(free);
}

template<typename HeapType,
         typename IterateFunc,
         typename Func>
void iterateHeap(HeapType* heap,
                 const IterateFunc& iterateFunc,
                 const Func& func)
{
    std::function<bool(pas_large_free)> capturedFunc = func;
    iterateFunc(heap, iterateAdapter, &capturedFunc);
}

template<typename HeapType,
         typename AllocateFunc,
         typename DeallocateFunc,
         typename IterateFunc>
void testLargeFreeHeapImpl(HeapType* heap,
                           const vector<Action>& actions,
                           const set<Free>& frees,
                           size_t typeSize,
                           const AllocateFunc& allocateFunc,
                           const DeallocateFunc& deallocateFunc,
                           const IterateFunc& iterateFunc)
{
    for (const Action& action : actions) {
        pas_large_free_heap_config config;
        
        memset(&config, 0, sizeof(config));
        
        config.type_size = typeSize;
        config.min_alignment = 1;
        CHECK(!!action.allocator);
        config.aligned_allocator = allocatorAdapter;
        config.aligned_allocator_arg = const_cast<void*>(reinterpret_cast<const void*>(&action.allocator));
        if (action.deallocator) {
            config.deallocator = deallocatorAdapter;
            config.deallocator_arg = const_cast<void*>(reinterpret_cast<const void*>(&action.deallocator));
        } else {
            config.deallocator = nullptr;
            config.deallocator_arg = nullptr;
        }
        
        pas_heap_lock_lock();
        
        switch (action.kind) {
        case Action::Allocate: {
            pas_string_stream stream;
            stringStreamConstruct(&stream);
            pas_alignment_dump(action.alignment, reinterpret_cast<pas_stream*>(&stream));
            cout << "    Allocating size = " << action.size
                 << ", alignment = " << pas_string_stream_get_string(&stream) << endl;
            pas_string_stream_destruct(&stream);
            CHECK_EQUAL(allocateFunc(heap, action.size, action.alignment, &config).begin,
                        action.result);
            break;
        }
        case Action::Deallocate:
            cout << "    Freeing base = " << action.base << endl;
            deallocateFunc(heap, action.base, action.base + action.size,
                           pas_zero_mode_may_have_non_zero,
                           &config);
            break;
        }
        
        pas_heap_lock_unlock();
    }
    
    bool ok = true;
    set<Free> freesFound;
    iterateHeap(
        heap,
        iterateFunc,
        [&] (pas_large_free largeFree) {
            Free free(largeFree.begin, largeFree.end);
            if (freesFound.count(free)) {
                cout << "    FAIL: Found duplicate entry " << free << endl;
                ok = false;
            }
            freesFound.insert(free);
            return true;
        });
    
    if (freesFound != frees || !ok) {
        cout << "    FAIL: Free validation failed." << endl;
        
        auto dumpFree = [] (const set<Free>& frees) {
            bool first = true;
            for (const Free& free : frees) {
                if (first)
                    first = false;
                else
                    cout << ", ";
                cout << free;
            }
        };
        
        cout << "    Expected: ";
        dumpFree(frees);
        cout << endl;
        cout << "    Actual: ";
        dumpFree(freesFound);
        cout << endl;
        CHECK(!"Free validation failed.");
    }
}

void testSimpleLargeFreeHeap(const vector<Action>& actions,
                             const set<Free>& frees,
                             size_t typeSize)
{
    pas_simple_large_free_heap heap;
    pas_simple_large_free_heap_construct(&heap);
    testLargeFreeHeapImpl(
        &heap, actions, frees, typeSize,
        pas_simple_large_free_heap_try_allocate,
        pas_simple_large_free_heap_deallocate,
        pas_simple_large_free_heap_for_each_free);
}

void testFastLargeFreeHeap(const vector<Action>& actions,
                           const set<Free>& frees,
                           size_t typeSize)
{
    pas_fast_large_free_heap heap;
    pas_fast_large_free_heap_construct(&heap);
    testLargeFreeHeapImpl(
        &heap, actions, frees, typeSize,
        pas_fast_large_free_heap_try_allocate,
        pas_fast_large_free_heap_deallocate,
        pas_fast_large_free_heap_for_each_free);
}

void testBootstrapHeap(const vector<Action>& actions,
                       const set<Free>& frees,
                       size_t typeSize)
{
    static constexpr size_t slabSize = 1lu << 20;
    void* slabPtr = pas_page_malloc_try_allocate_without_deallocating_padding(
        slabSize, alignSimple(slabSize)).result;
    CHECK(slabPtr);
    uintptr_t slab = reinterpret_cast<uintptr_t>(slabPtr);
    
    cout << "    Slab at " << slab << endl;
    
    vector<Action> newActions;
    set<Free> newFrees;
    
    for (const Action& action : actions) {
        Action newAction = action;
        
        if (newAction.base)
            newAction.base += slab;
        
        newAction.allocator = [=] (size_t size, pas_alignment alignment) -> pas_aligned_allocation_result {
            pas_aligned_allocation_result result = action.allocator(size, alignment);
            if (!result.result)
                return result;
            result.result = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(result.result) + slab);
            result.left_padding = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(result.left_padding) + slab);
            result.right_padding = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(result.right_padding) + slab);
            return result;
        };
        
        if (action.deallocator) {
            newAction.deallocator = [=] (void* base, size_t size) {
                action.deallocator(reinterpret_cast<void*>(
                                       reinterpret_cast<uintptr_t>(base) - slab),
                                   size);
            };
        }
        
        if (newAction.result)
            newAction.result += slab;
        
        newActions.push_back(newAction);
    }
    
    for (const Free& free : frees)
        newFrees.insert(Free(free.begin + slab, free.end + slab));
    
    testLargeFreeHeapImpl(
        &pas_bootstrap_free_heap, newActions, newFrees, typeSize,
        pas_simple_large_free_heap_try_allocate,
        pas_simple_large_free_heap_deallocate,
        pas_simple_large_free_heap_for_each_free);
}

size_t freeListSize(size_t size)
{
    return PAS_MAX(size, PAS_BOOTSTRAP_FREE_LIST_MINIMUM_SIZE) * sizeof(pas_large_free);
}

} // anonymous namespace

void addLargeFreeHeapTests()
{
    ADD_TEST(testSimpleLargeFreeHeap({ }, { }, 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(666, alignSimple(4), 0, failingAllocator, { })
                 },
                 {
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::deallocate(4200, 3500),
                     Action::allocate(100, alignSimple(16), 4208),
                     Action::allocate(1000, alignSimple(4), 4308),
                     Action::allocate(2000, alignSimple(8), 5312),
                 },
                 {
                     Free(4200, 4208),
                     Free(5308, 5312),
                     Free(7312, 7700)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 7700),
                                      { }),
                     Action::allocate(1000, alignSimple(4), 4308),
                     Action::allocate(2000, alignSimple(8), 5312),
                 },
                 {
                     Free(4200, 4208),
                     Free(5308, 5312),
                     Free(7312, 7700)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 7700),
                                      deallocateFinite({
                                                           Free(4200, 4208),
                                                           Free(4308, 7700)
                                                       }))
                 },
                 { },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 7700),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 7700)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4500),
                                      { }),
                     Action::allocate(1000, alignSimple(4), 4308,
                                      allocateOnce(1000, alignSimple(4),
                                                   4500, 4500,
                                                   5500, 7700),
                                      { }),
                     Action::allocate(2000, alignSimple(8), 5312),
                 },
                 {
                     Free(4200, 4208),
                     Free(5308, 5312),
                     Free(7312, 7700)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4500),
                                      { }),
                     Action::deallocate(7500, 200),
                     Action::allocate(1000, alignSimple(4), 4308,
                                      allocateOnce(1000, alignSimple(4),
                                                   4500, 4500,
                                                   5500, 7500),
                                      { }),
                     Action::allocate(2000, alignSimple(8), 5312),
                 },
                 {
                     Free(4200, 4208),
                     Free(5308, 5312),
                     Free(7312, 7700)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4500),
                                      { }),
                     Action::allocate(100, alignSimple(128), 4352)
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4352),
                     Free(4452, 4500)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4208, 4208,
                                                   4308, 4500),
                                      { }),
                     Action::allocate(100, alignSimple(128), 4352)
                 },
                 {
                     Free(4308, 4352),
                     Free(4452, 4500)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(128), 4352,
                                      allocateOnce(100, alignSimple(128),
                                                   4400, 4480,
                                                   4580, 4608),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4352),
                     Free(4452, 4608)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4400, 4608,
                                                   4708, 5120),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4608),
                     Free(4708, 5120)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(4), 4308,
                                      allocateOnce(100, alignSimple(4),
                                                   4400, 4400,
                                                   4500, 4600),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4408, 4600)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4400, 4608,
                                                   4708, 4708),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4608)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4608, 4608,
                                                   4708, 5120),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4400),
                     Free(4708, 5120)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4608, 4608,
                                                   4708, 4708),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4400)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(128), 4352,
                                      allocateOnce(100, alignSimple(128),
                                                   4400, 4480,
                                                   4580, 4608),
                                      deallocateFinite({
                                                           Free(4452, 4608)
                                                       }))
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4352)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4400, 4608,
                                                   4708, 5120),
                                      deallocateFinite({
                                                           Free(4400, 4608),
                                                           Free(4708, 5120)
                                                       }))
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4400)
                 },
                 1));
    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(4), 4308,
                                      allocateOnce(100, alignSimple(4),
                                                   4400, 4400,
                                                   4500, 4600),
                                      deallocateFinite({
                                                           Free(4408, 4600)
                                                       }))
                 },
                 {
                     Free(4200, 4208)
                 },
                 1));

    ADD_TEST(testFastLargeFreeHeap({ }, { }, 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(666, alignSimple(4), 0, failingAllocator, { })
                 },
                 {
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::deallocate(4200, 3500),
                     Action::allocate(100, alignSimple(16), 4208),
                     Action::allocate(1000, alignSimple(4), 4308),
                     Action::allocate(2000, alignSimple(8), 5312),
                 },
                 {
                     Free(4200, 4208),
                     Free(5308, 5312),
                     Free(7312, 7700)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 7700),
                                      { }),
                     Action::allocate(1000, alignSimple(4), 4308),
                     Action::allocate(2000, alignSimple(8), 5312),
                 },
                 {
                     Free(4200, 4208),
                     Free(5308, 5312),
                     Free(7312, 7700)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 7700),
                                      deallocateFinite({
                                                           Free(4200, 4208),
                                                           Free(4308, 7700)
                                                       }))
                 },
                 { },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 7700),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 7700)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4500),
                                      { }),
                     Action::allocate(1000, alignSimple(4), 4308,
                                      allocateOnce(1000, alignSimple(4),
                                                   4500, 4500,
                                                   5500, 7700),
                                      { }),
                     Action::allocate(2000, alignSimple(8), 5312),
                 },
                 {
                     Free(4200, 4208),
                     Free(5308, 5312),
                     Free(7312, 7700)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4500),
                                      { }),
                     Action::deallocate(7500, 200),
                     Action::allocate(1000, alignSimple(4), 4308,
                                      allocateOnce(1000, alignSimple(4),
                                                   4500, 4500,
                                                   5500, 7500),
                                      { }),
                     Action::allocate(2000, alignSimple(8), 5312),
                 },
                 {
                     Free(4200, 4208),
                     Free(5308, 5312),
                     Free(7312, 7700)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4500),
                                      { }),
                     Action::allocate(100, alignSimple(128), 4352)
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4352),
                     Free(4452, 4500)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4208, 4208,
                                                   4308, 4500),
                                      { }),
                     Action::allocate(100, alignSimple(128), 4352)
                 },
                 {
                     Free(4308, 4352),
                     Free(4452, 4500)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(128), 4352,
                                      allocateOnce(100, alignSimple(128),
                                                   4400, 4480,
                                                   4580, 4608),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4352),
                     Free(4452, 4608)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4400, 4608,
                                                   4708, 5120),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4608),
                     Free(4708, 5120)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(4), 4308,
                                      allocateOnce(100, alignSimple(4),
                                                   4400, 4400,
                                                   4500, 4600),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4408, 4600)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4400, 4608,
                                                   4708, 4708),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4608)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4608, 4608,
                                                   4708, 5120),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4400),
                     Free(4708, 5120)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4608, 4608,
                                                   4708, 4708),
                                      { })
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4400)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(128), 4352,
                                      allocateOnce(100, alignSimple(128),
                                                   4400, 4480,
                                                   4580, 4608),
                                      deallocateFinite({
                                                           Free(4452, 4608)
                                                       }))
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4352)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(512), 4608,
                                      allocateOnce(100, alignSimple(512),
                                                   4400, 4608,
                                                   4708, 5120),
                                      deallocateFinite({
                                                           Free(4400, 4608),
                                                           Free(4708, 5120)
                                                       }))
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4400)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 4400),
                                      { }),
                     Action::allocate(100, alignSimple(4), 4308,
                                      allocateOnce(100, alignSimple(4),
                                                   4400, 4400,
                                                   4500, 4600),
                                      deallocateFinite({
                                                           Free(4408, 4600)
                                                       }))
                 },
                 {
                     Free(4200, 4208)
                 },
                 1));

    ADD_TEST(testSimpleLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 5328),
                                      { }),
                     Action::allocate(100, alignSimple(16), 4320),
                     Action::allocate(100, alignSimple(16), 4432),
                     Action::allocate(100, alignSimple(16), 4544),
                     Action::allocate(100, alignSimple(16), 4656),
                     Action::allocate(100, alignSimple(16), 4768),
                     Action::allocate(100, alignSimple(16), 4880),
                     Action::allocate(100, alignSimple(16), 4992),
                     Action::allocate(100, alignSimple(16), 5104),
                     Action::allocate(100, alignSimple(16), 5216),
                     Action::deallocate(4320, 100),
                     Action::deallocate(4544, 100),
                     Action::deallocate(4768, 100),
                     Action::deallocate(4992, 100),
                     Action::deallocate(5216, 100),
                     Action::allocate(100, alignSimple(16), 4320),
                     Action::deallocate(5104, 100)
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4320),
                     Free(4420, 4432),
                     Free(4532, 4656),
                     Free(4756, 4880),
                     Free(4980, 5328)
                 },
                 1));
    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::allocate(100, alignSimple(16), 4208,
                                      allocateOnce(100, alignSimple(16),
                                                   4200, 4208,
                                                   4308, 5328),
                                      { }),
                     Action::allocate(100, alignSimple(16), 4320),
                     Action::allocate(100, alignSimple(16), 4432),
                     Action::allocate(100, alignSimple(16), 4544),
                     Action::allocate(100, alignSimple(16), 4656),
                     Action::allocate(100, alignSimple(16), 4768),
                     Action::allocate(100, alignSimple(16), 4880),
                     Action::allocate(100, alignSimple(16), 4992),
                     Action::allocate(100, alignSimple(16), 5104),
                     Action::allocate(100, alignSimple(16), 5216),
                     Action::deallocate(4320, 100),
                     Action::deallocate(4544, 100),
                     Action::deallocate(4768, 100),
                     Action::deallocate(4992, 100),
                     Action::deallocate(5216, 100),
                     Action::allocate(100, alignSimple(16), 4320),
                     Action::deallocate(5104, 100)
                 },
                 {
                     Free(4200, 4208),
                     Free(4308, 4320),
                     Free(4420, 4432),
                     Free(4532, 4656),
                     Free(4756, 4880),
                     Free(4980, 5328)
                 },
                 1));

    ADD_TEST(testBootstrapHeap({ }, { }, 1));
    ADD_TEST(testBootstrapHeap(
                 {
                     Action::allocate(666, alignSimple(4), 0, failingAllocator, { })
                 },
                 {
                 },
                 1));
    ADD_TEST(testBootstrapHeap(
                 {
                     Action::deallocate(4200, 3500),
                     Action::allocate(104, alignSimple(8), freeListSize(1) + 4200),
                     Action::allocate(1000, alignSimple(8), freeListSize(1) + 4304),
                     Action::allocate(2000, alignSimple(8), freeListSize(1) + 5304),
                 },
                 {
                     Free(freeListSize(1) + 7304, 7700)
                 },
                 1));
    ADD_TEST(testBootstrapHeap(
                 {
                     Action::allocate(104, alignSimple(8), 4200,
                                      allocateOnce(104, alignSimple(8),
                                                   4200, 4200,
                                                   4304, 5000),
                                      { }),
                 },
                 {
                     Free(4304 + freeListSize(1), 5000)
                 },
                 1));
    ADD_TEST(testBootstrapHeap(
                 {
                     Action::allocate(104, alignSimple(8), 4200,
                                      allocateOnce(104, alignSimple(8),
                                                   4200, 4200,
                                                   4304, 4304),
                                      { }),
                 },
                 { },
                 1));
    ADD_TEST(testBootstrapHeap(
                 {
                     Action::allocate(104, alignSimple(8), 4200,
                                      allocateFinite(
                                          {
                                              Allocation(104, alignSimple(8),
                                                         4200, 4200,
                                                         4304, 4312),
                                              Allocation(freeListSize(1), alignSimple(sizeof(void*)),
                                                         4312, 4312,
                                                         4312 + freeListSize(1),
                                                         4312 + freeListSize(1))
                                          }),
                                      { }),
                 },
                 {
                     Free(4304 + freeListSize(1), 4312 + freeListSize(1))
                 },
                 1));
    ADD_TEST(testBootstrapHeap(
                 {
                     Action::deallocate(4200, 800),
                     Action::deallocate(5200, 800),
                     Action::deallocate(6200, 800),
                     Action::deallocate(7200, 800),
                     Action::deallocate(8200, 800),
                     Action::deallocate(9200, 2000),
                     Action::deallocate(12200, 800),
                     Action::deallocate(13200, 800),
                     Action::deallocate(14200, 800),
                     Action::deallocate(15200, 800)
                 },
                 {
                     Free(4200, 5000),
                     Free(5200 + freeListSize(22), 6000),
                     Free(6200, 7000),
                     Free(7200, 8000),
                     Free(8200, 9000),
                     Free(9200, 11200),
                     Free(12200, 13000),
                     Free(13200, 14000),
                     Free(14200, 15000),
                     Free(15200, 16000)
                 },
                 1));
}

