/*
 * Copyright (c) 2018-2019, 2026 Apple Inc. All rights reserved.
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
#include "pas_large_free_heap_helpers.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include "pas_simple_large_free_heap.h"
#include "pas_zero_mode.h"
#include <set>
#include <vector>

using namespace std;

namespace {

std::ostream& operator<<(std::ostream& out, pas_zero_mode mode)
{
    out << pas_zero_mode_get_string(mode);
    return out;
}

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
        function<void(void* base, size_t size)> deallocator = trappingDeallocator,
        pas_zero_mode expectedResultZeroMode = pas_zero_mode_may_have_non_zero)
    {
        Action result;
        result.kind = Allocate;
        result.size = size;
        result.alignment = alignment;
        result.allocator = allocator;
        result.deallocator = deallocator;
        result.result = expectedResult;
        result.expectedResultZeroMode = expectedResultZeroMode;
        return result;
    }

    static Action deallocate(
        uintptr_t base, size_t size,
        function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)> allocator = trappingAllocator,
        function<void(void* base, size_t size)> deallocator = trappingDeallocator,
        pas_zero_mode zeroMode = pas_zero_mode_may_have_non_zero)
    {
        Action result;
        result.kind = Deallocate;
        result.base = base;
        result.size = size;
        result.allocator = allocator;
        result.deallocator = deallocator;
        result.deallocZeroMode = zeroMode;
        return result;
    }

    Kind kind { Allocate };
    uintptr_t base { 0 };
    size_t size { 0 };
    pas_alignment alignment { 0 };
    function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)> allocator;
    function<void(void* base, size_t size)> deallocator;
    uintptr_t result { 0 };
    pas_zero_mode deallocZeroMode { pas_zero_mode_may_have_non_zero };
    pas_zero_mode expectedResultZeroMode { pas_zero_mode_may_have_non_zero };
};

struct Free {
    Free() = default;

    Free(uintptr_t begin, uintptr_t end,
         pas_zero_mode zeroMode = pas_zero_mode_may_have_non_zero)
        : begin(begin)
        , end(end)
        , zeroMode(zeroMode)
    {
    }

    bool operator==(const Free& other) const
    {
        return begin == other.begin
            && end == other.end
            && zeroMode == other.zeroMode;
    }

    bool operator<(const Free& other) const
    {
        if (begin != other.begin)
            return begin < other.begin;
        if (end != other.end)
            return end < other.end;
        return zeroMode < other.zeroMode;
    }

    uintptr_t begin;
    uintptr_t end;
    pas_zero_mode zeroMode { pas_zero_mode_may_have_non_zero };
};

struct Allocation {
    Allocation() = default;
    
    Allocation(size_t expectedSize, pas_alignment expectedAlignment,
               uintptr_t leftPadding, uintptr_t resultBase,
               uintptr_t rightPadding, uintptr_t end,
               pas_zero_mode zeroMode = pas_zero_mode_may_have_non_zero)
        : expectedSize(expectedSize)
        , expectedAlignment(expectedAlignment)
        , leftPadding(leftPadding)
        , resultBase(resultBase)
        , rightPadding(rightPadding)
        , end(end)
        , zeroMode(zeroMode)
    {
    }

    size_t expectedSize { 0 };
    pas_alignment expectedAlignment { 0 };
    uintptr_t leftPadding { 0 };
    uintptr_t resultBase { 0 };
    uintptr_t rightPadding { 0 };
    uintptr_t end { 0 };
    pas_zero_mode zeroMode { pas_zero_mode_may_have_non_zero };
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
        result.zero_mode = allocation.zeroMode;

        return result;
    };
}

function<pas_aligned_allocation_result(size_t size, pas_alignment alignment)>
allocateOnce(size_t expectedSize,
             pas_alignment expectedAlignment,
             uintptr_t leftPadding,
             uintptr_t resultBase,
             uintptr_t rightPadding,
             uintptr_t end,
             pas_zero_mode zeroMode = pas_zero_mode_may_have_non_zero)
{
    return allocateFinite({ Allocation(expectedSize, expectedAlignment,
                                       leftPadding, resultBase,
                                       rightPadding, end, zeroMode) });
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
        << ", size = " << (free.end - free.begin)
        << ", zero_mode = " << free.zeroMode << "}";
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
            pas_allocation_result allocationResult =
                allocateFunc(heap, action.size, action.alignment, &config);
            CHECK_EQUAL(allocationResult.begin, action.result);
            CHECK_EQUAL(allocationResult.zero_mode, action.expectedResultZeroMode);
            break;
        }
        case Action::Deallocate:
            cout << "    Freeing base = " << action.base
                 << ", zero_mode = " << action.deallocZeroMode << endl;
            deallocateFunc(heap, action.base, action.base + action.size,
                           action.deallocZeroMode,
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
            Free free(largeFree.begin, largeFree.end, largeFree.zero_mode);
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
        slabSize, alignSimple(slabSize), false).result;
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

pas_allocation_result failingMemorySource(size_t, pas_alignment, const char*, pas_allocation_kind)
{
    return pas_allocation_result_create_failure();
}

void testGiveBackGuardOnAllocationFailure(bool talksToLargeSharingPool)
{
    pas_fast_large_free_heap heap;
    pas_fast_large_free_heap_construct(&heap);
    size_t numAllocatedObjectBytes = 0;
    size_t numAllocatedObjectBytesPeak = 0;

    pas_large_utility_free_heap_talks_to_large_sharing_pool = talksToLargeSharingPool;
    pas_physical_page_sharing_pool_balancing_enabled = true;
    pas_physical_page_sharing_pool_balance = 0;

    pas_heap_lock_lock();
    void* result = pas_large_free_heap_helpers_try_allocate_with_alignment(
        &heap, failingMemorySource, &numAllocatedObjectBytes, &numAllocatedObjectBytesPeak,
        100, alignSimple(1), "test-give-back-guard");
    pas_heap_lock_unlock();

    CHECK(!result);

    // The allocator's failure path must net out to zero against its setup path.
    // When talks_to_large_sharing_pool is true, take_later subtracts aligned_size
    // and give_back adds it back. When it is false, take_later is skipped, so
    // give_back must be skipped too -- otherwise the balance drifts upward by
    // aligned_size on every failed allocation.
    CHECK_EQUAL(pas_physical_page_sharing_pool_balance, static_cast<intptr_t>(0));
}

void testMergeZeroModeOnCoalesce(pas_zero_mode leftMode, pas_zero_mode rightMode,
                                 pas_zero_mode expectedMode,
                                 bool deallocateRightFirst, bool useFastHeap)
{
    // Free two adjacent regions [1000, 1100) and [1100, 1200) carrying the given zero_modes. The
    // heap must coalesce them into a single [1000, 1200) free region whose zero_mode is
    // pas_zero_mode_merge(leftMode, rightMode) -- is_all_zero only if BOTH sides are is_all_zero
    // (see pas_large_free_create_merged). We deallocate in both orders to exercise coalescing with
    // the new region arriving on either side of the existing one.
    Action lower = Action::deallocate(1000, 100, trappingAllocator, trappingDeallocator, leftMode);
    Action upper = Action::deallocate(1100, 100, trappingAllocator, trappingDeallocator, rightMode);
    vector<Action> actions = deallocateRightFirst
        ? vector<Action> { upper, lower }
        : vector<Action> { lower, upper };
    set<Free> frees = { Free(1000, 1200, expectedMode) };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
}

void testSplitPreservesZeroMode(pas_zero_mode mode, bool useFastHeap)
{
    // Seed a free region [1000, 2000) carrying `mode`, then allocate a 512-aligned sub-range from
    // it. The split (pas_large_free_split) must copy `mode` to both leftover halves -- [1000, 1024)
    // and [1124, 2000) -- and to the allocation result at 1024. Align 512 forces both a left and a
    // right leftover.
    vector<Action> actions = {
        Action::deallocate(1000, 1000, trappingAllocator, trappingDeallocator, mode),
        Action::allocate(100, alignSimple(512), 1024, trappingAllocator, trappingDeallocator, mode),
    };
    set<Free> frees = { Free(1000, 1024, mode), Free(1124, 2000, mode) };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
}

void testSplitPreservesZeroModeFromSource(pas_zero_mode mode, bool useFastHeap)
{
    // Same split, but the region comes fresh from the memory source: allocating into an empty heap
    // invokes the mock allocator, which returns a chunk [1000, 2000) carrying `mode` with the object
    // at 1024. With no deallocator wired (paddings are kept, not given back), the left/right padding
    // must enter the free list carrying `mode` and the allocation result must carry `mode`.
    vector<Action> actions = {
        Action::allocate(100, alignSimple(512), 1024,
                         allocateOnce(100, alignSimple(512), 1000, 1024, 1124, 2000, mode),
                         { }, mode),
    };
    set<Free> frees = { Free(1000, 1024, mode), Free(1124, 2000, mode) };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
}

void testFragmentationSplitRecoalescePreservesZero(bool useFastHeap)
{
    // An all-is_all_zero region survives a split-then-recoalesce cycle. Seed [1000, 4000)
    // is_all_zero, carve two sub-ranges out (splits preserve zero, so the results and the leftover
    // stay zero), then free both back is_all_zero so everything recoalesces. The final single free
    // region must still be is_all_zero.
    vector<Action> actions = {
        Action::deallocate(1000, 3000, trappingAllocator, trappingDeallocator, pas_zero_mode_is_all_zero),
        Action::allocate(1000, alignSimple(1), 1000, trappingAllocator, trappingDeallocator, pas_zero_mode_is_all_zero),
        Action::allocate(1000, alignSimple(1), 2000, trappingAllocator, trappingDeallocator, pas_zero_mode_is_all_zero),
        Action::deallocate(1000, 1000, trappingAllocator, trappingDeallocator, pas_zero_mode_is_all_zero),
        Action::deallocate(2000, 1000, trappingAllocator, trappingDeallocator, pas_zero_mode_is_all_zero),
    };
    set<Free> frees = { Free(1000, 4000, pas_zero_mode_is_all_zero) };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
}

void testFragmentationSplitRecoalesceContaminatedByNonZero(bool useFastHeap)
{
    // Recoalescing a may_have_non_zero sub-range into an is_all_zero neighbor must yield
    // may_have_non_zero. Seed [1000, 4000) is_all_zero, carve two sub-ranges out (splits preserve
    // zero), then free them back may_have_non_zero so they recoalesce with the still-zero leftover
    // [3000, 4000). Per pas_zero_mode_merge the recoalesced region must be may_have_non_zero --
    // is_all_zero survives a coalesce only when every part is is_all_zero; otherwise a later
    // zeroedMalloc that trusts is_all_zero would skip zeroing and return stale bytes.
    vector<Action> actions = {
        Action::deallocate(1000, 3000, trappingAllocator, trappingDeallocator, pas_zero_mode_is_all_zero),
        Action::allocate(1000, alignSimple(1), 1000, trappingAllocator, trappingDeallocator, pas_zero_mode_is_all_zero),
        Action::allocate(1000, alignSimple(1), 2000, trappingAllocator, trappingDeallocator, pas_zero_mode_is_all_zero),
        Action::deallocate(1000, 1000, trappingAllocator, trappingDeallocator, pas_zero_mode_may_have_non_zero),
        Action::deallocate(2000, 1000, trappingAllocator, trappingDeallocator, pas_zero_mode_may_have_non_zero),
    };
    set<Free> frees = { Free(1000, 4000, pas_zero_mode_may_have_non_zero) };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
}

void testFreshChunkCoalesceZeroMode(pas_zero_mode existingMode, pas_zero_mode freshMode,
                                    pas_zero_mode expectedMergedMode, bool useFastHeap)
{
    // The existing free [4096, 4300) is too small to satisfy a 512-aligned 300-byte request on its
    // own, so the allocator is invoked. The mock source returns a fresh chunk whose left padding
    // [4300, 4608) abuts the existing free's end (4300); find_by_end merges the two
    // (pas_large_free_create_merged), and since the merge exposes a lower aligned address (4096)
    // than the fresh chunk's own result (4608), the allocation is served from the merged range. The
    // result and the leftover [4396, 5120) must carry pas_zero_mode_merge(existing, fresh).
    vector<Action> actions = {
        Action::deallocate(4096, 204, trappingAllocator, trappingDeallocator, existingMode),
        Action::allocate(300, alignSimple(512), 4096,
                         allocateOnce(300, alignSimple(512), 4300, 4608, 4908, 5120, freshMode),
                         { }, expectedMergedMode),
    };
    set<Free> frees = { Free(4396, 5120, expectedMergedMode) };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
}

void testBestFitCandidateZeroMode(bool winnerIsZero, bool useFastHeap)
{
    // Two free regions of differing modes: a small [1000, 1100) and a larger [2000, 2400). A
    // 350-byte request fits only the larger one, so best-fit must pick it; the result and its
    // leftover [2350, 2400) carry the larger (winning) region's mode, while the small region is
    // left untouched with its own mode.
    pas_zero_mode largeMode = winnerIsZero ? pas_zero_mode_is_all_zero : pas_zero_mode_may_have_non_zero;
    pas_zero_mode smallMode = winnerIsZero ? pas_zero_mode_may_have_non_zero : pas_zero_mode_is_all_zero;
    vector<Action> actions = {
        Action::deallocate(1000, 100, trappingAllocator, trappingDeallocator, smallMode),
        Action::deallocate(2000, 400, trappingAllocator, trappingDeallocator, largeMode),
        Action::allocate(350, alignSimple(1), 2000, trappingAllocator, trappingDeallocator, largeMode),
    };
    set<Free> frees = { Free(1000, 1100, smallMode), Free(2350, 2400, largeMode) };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
}

void testThreeWayCoalesceZeroMode(pas_zero_mode leftMode, pas_zero_mode middleMode,
                                  pas_zero_mode rightMode, pas_zero_mode expectedMode,
                                  bool useFastHeap)
{
    // Seed two non-adjacent frees [1000, 1100) and [1200, 1300); freeing the middle [1100, 1200)
    // coalesces with BOTH neighbors at once. The resulting [1000, 1300) must carry
    // pas_zero_mode_merge folded over all three ranges (is_all_zero only if all three are).
    vector<Action> actions = {
        Action::deallocate(1000, 100, trappingAllocator, trappingDeallocator, leftMode),
        Action::deallocate(1200, 100, trappingAllocator, trappingDeallocator, rightMode),
        Action::deallocate(1100, 100, trappingAllocator, trappingDeallocator, middleMode),
    };
    set<Free> frees = { Free(1000, 1300, expectedMode) };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
}

void testZeroSizeAllocationIsAllZero(bool useFastHeap)
{
    // A zero-size allocation succeeds with begin 0 and zero_mode is_all_zero, touching nothing.
    vector<Action> actions = {
        Action::allocate(0, alignSimple(1), 0, trappingAllocator, trappingDeallocator,
                         pas_zero_mode_is_all_zero),
    };
    set<Free> frees = { };
    if (useFastHeap)
        testFastLargeFreeHeap(actions, frees, 1);
    else
        testSimpleLargeFreeHeap(actions, frees, 1);
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

    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::deallocate(1000, 100),
                     Action::deallocate(3000, 200),
                     Action::deallocate(2000, 400),
                     Action::allocate(350, alignSimple(1), 2000),
                     Action::allocate(150, alignSimple(1), 3000)
                 },
                 {
                     Free(1000, 1100),
                     Free(2350, 2400),
                     Free(3150, 3200)
                 },
                 1));

    ADD_TEST(testFastLargeFreeHeap(
                 {
                     Action::deallocate(1000, 100),
                     Action::deallocate(3000, 1000),
                     Action::deallocate(5000, 200),
                     Action::deallocate(5200, 3000),
                     Action::allocate(2000, alignSimple(1), 5000)
                 },
                 {
                     Free(1000, 1100),
                     Free(3000, 4000),
                     Free(7000, 8200)
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

    ADD_TEST(testGiveBackGuardOnAllocationFailure(true));
    ADD_TEST(testGiveBackGuardOnAllocationFailure(false));

    // zero_mode merge/coalesce truth table. Coalescing two adjacent frees must merge their
    // zero_modes conservatively: is_all_zero only when BOTH are is_all_zero, else may_have_non_zero.
    // All four (left, right) combinations, both deallocation orders, on the simple and fast heaps.
    for (bool useFastHeap : { false, true }) {
        for (bool rightFirst : { false, true }) {
            ADD_TEST(testMergeZeroModeOnCoalesce(pas_zero_mode_is_all_zero, pas_zero_mode_is_all_zero,
                                                 pas_zero_mode_is_all_zero, rightFirst, useFastHeap));
            ADD_TEST(testMergeZeroModeOnCoalesce(pas_zero_mode_is_all_zero, pas_zero_mode_may_have_non_zero,
                                                 pas_zero_mode_may_have_non_zero, rightFirst, useFastHeap));
            ADD_TEST(testMergeZeroModeOnCoalesce(pas_zero_mode_may_have_non_zero, pas_zero_mode_is_all_zero,
                                                 pas_zero_mode_may_have_non_zero, rightFirst, useFastHeap));
            ADD_TEST(testMergeZeroModeOnCoalesce(pas_zero_mode_may_have_non_zero, pas_zero_mode_may_have_non_zero,
                                                 pas_zero_mode_may_have_non_zero, rightFirst, useFastHeap));
        }
    }

    // zero_mode split preservation. Allocating a sub-range from a free region must copy that
    // region's zero_mode to both leftover halves and to the allocation result -- pas_large_free_split
    // copies the parent mode to both halves. Covered both for a region seeded via deallocate and for
    // a fresh chunk from the memory source, on the simple and fast heaps.
    for (bool useFastHeap : { false, true }) {
        ADD_TEST(testSplitPreservesZeroMode(pas_zero_mode_is_all_zero, useFastHeap));
        ADD_TEST(testSplitPreservesZeroMode(pas_zero_mode_may_have_non_zero, useFastHeap));
        ADD_TEST(testSplitPreservesZeroModeFromSource(pas_zero_mode_is_all_zero, useFastHeap));
        ADD_TEST(testSplitPreservesZeroModeFromSource(pas_zero_mode_may_have_non_zero, useFastHeap));
    }

    // zero_mode through a fragmentation split-then-recoalesce sequence: an all-is_all_zero region
    // survives the cycle, while freeing a may_have_non_zero sub-range back into it makes the
    // recoalesced whole may_have_non_zero.
    for (bool useFastHeap : { false, true }) {
        ADD_TEST(testFragmentationSplitRecoalescePreservesZero(useFastHeap));
        ADD_TEST(testFragmentationSplitRecoalesceContaminatedByNonZero(useFastHeap));
    }

    // Coalescing a fresh source chunk with an adjacent existing free must merge their zero_modes:
    // is_all_zero only when both the existing free and the fresh chunk are is_all_zero.
    for (bool useFastHeap : { false, true }) {
        ADD_TEST(testFreshChunkCoalesceZeroMode(pas_zero_mode_is_all_zero, pas_zero_mode_is_all_zero,
                                                pas_zero_mode_is_all_zero, useFastHeap));
        ADD_TEST(testFreshChunkCoalesceZeroMode(pas_zero_mode_is_all_zero, pas_zero_mode_may_have_non_zero,
                                                pas_zero_mode_may_have_non_zero, useFastHeap));
        ADD_TEST(testFreshChunkCoalesceZeroMode(pas_zero_mode_may_have_non_zero, pas_zero_mode_is_all_zero,
                                                pas_zero_mode_may_have_non_zero, useFastHeap));
        ADD_TEST(testFreshChunkCoalesceZeroMode(pas_zero_mode_may_have_non_zero, pas_zero_mode_may_have_non_zero,
                                                pas_zero_mode_may_have_non_zero, useFastHeap));
    }

    // When best-fit selects among free ranges of differing modes, the chosen range's mode flows to
    // the allocation result and its leftover, while the unchosen range keeps its own mode.
    for (bool useFastHeap : { false, true }) {
        ADD_TEST(testBestFitCandidateZeroMode(true, useFastHeap));
        ADD_TEST(testBestFitCandidateZeroMode(false, useFastHeap));
    }

    // A free coalescing with two neighbors at once (three-way merge) folds all three zero_modes:
    // is_all_zero only when all three are. Exercise every (left, middle, right) combination.
    for (bool useFastHeap : { false, true }) {
        for (pas_zero_mode leftMode : { pas_zero_mode_may_have_non_zero, pas_zero_mode_is_all_zero }) {
            for (pas_zero_mode middleMode : { pas_zero_mode_may_have_non_zero, pas_zero_mode_is_all_zero }) {
                for (pas_zero_mode rightMode : { pas_zero_mode_may_have_non_zero, pas_zero_mode_is_all_zero }) {
                    pas_zero_mode expectedMode =
                        (leftMode == pas_zero_mode_is_all_zero
                         && middleMode == pas_zero_mode_is_all_zero
                         && rightMode == pas_zero_mode_is_all_zero)
                        ? pas_zero_mode_is_all_zero
                        : pas_zero_mode_may_have_non_zero;
                    ADD_TEST(testThreeWayCoalesceZeroMode(leftMode, middleMode, rightMode,
                                                          expectedMode, useFastHeap));
                }
            }
        }
    }

    // A zero-size allocation returns is_all_zero.
    for (bool useFastHeap : { false, true })
        ADD_TEST(testZeroSizeAllocationIsAllZero(useFastHeap));
}

