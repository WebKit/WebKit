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

#if PAS_ENABLE_JS_MARKED_BLOCK

#include <set>
#include <sys/mman.h>
#include <vector>
#include "js_marked_block_heap.h"
#include "js_marked_block_heap_config.h"
#include "pas_baseline_allocator_table.h"
#include "pas_get_page_base.h"
#include "pas_heap.h"
#include "pas_page_base.h"
#include "pas_page_kind.h"
#include "pas_scavenger.h"
#include "pas_thread_local_cache.h"
#include "pas_utils.h"

using namespace std;

namespace {

void testBlocksAreBlockAlignedAndDistinct(unsigned numBlocks)
{
    set<void*> blocks;

    for (unsigned i = numBlocks; i--;) {
        void* block = js_marked_block_heap_try_allocate();
        CHECK(block);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(block), JS_MARKED_BLOCK_SIZE));
        CHECK_EQUAL(js_marked_block_heap_get_allocation_size(block), JS_MARKED_BLOCK_SIZE);
        CHECK(blocks.find(block) == blocks.end());
        blocks.insert(block);
    }

    for (void* block : blocks)
        js_marked_block_heap_deallocate(block);
}

/* The heap is only worth having if the page config that serves it is the one whose minimum alignment
   is the block size. Any other kind means the block size fell through to a page config that has to
   search for an aligned run, or to the large heap. */
void testBlocksComeFromTheTailoredPageConfig()
{
    void* block = js_marked_block_heap_try_allocate();
    CHECK(block);

    pas_page_base* pageBase = pas_get_page_base(block, JS_MARKED_BLOCK_HEAP_CONFIG);
    CHECK(pageBase);
    CHECK_EQUAL(pas_page_base_get_kind(pageBase), pas_medium_exclusive_segregated_page_kind);

    js_marked_block_heap_deallocate(block);
}

constexpr size_t blocksPerPage = JS_MARKED_BLOCK_MEDIUM_SEGREGATED_PAGE_SIZE / JS_MARKED_BLOCK_SIZE;

/* Cycling a fixed number of blocks must not keep taking fresh memory. A freed block only comes back
   once its page stops being allocated out of, so the bound here is only that the heap recycles at
   all: how much it recycles is a measurement, not a correctness property. */
void testRepeatedAllocateAndFreeStaysBounded(unsigned numBlocks, unsigned numRounds)
{
    set<void*> everSeen;

    for (unsigned round = numRounds; round--;) {
        vector<void*> blocks;
        for (unsigned i = numBlocks; i--;) {
            void* block = js_marked_block_heap_try_allocate();
            CHECK(block);
            CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(block), JS_MARKED_BLOCK_SIZE));
            everSeen.insert(block);
            blocks.push_back(block);
        }
        for (void* block : blocks)
            js_marked_block_heap_deallocate(block);
    }

    /* A heap that never reused a block would reach numBlocks * numRounds. The slack is two pages: the
       one being filled, plus the one waiting to become eligible again. */
    CHECK_LESS_EQUAL(everSeen.size(), numBlocks + 2 * blocksPerPage);
}

/* One block per granule is the property that lets the scavenger hand back memory in units of exactly
   one block. Hold one block out of a page's worth and the rest must come back. */
void testFreeingAllButOneBlockGivesBackTheRestOfThePage(unsigned numBlocks)
{
    vector<void*> blocks;

    for (unsigned i = numBlocks; i--;) {
        void* block = js_marked_block_heap_try_allocate();
        CHECK(block);
        blocks.push_back(block);
    }

    pas_heap* heap = js_marked_block_heap_get_heap(blocks[0]);
    CHECK(heap);

    for (size_t i = 1; i < blocks.size(); ++i)
        js_marked_block_heap_deallocate(blocks[i]);

    /* The freed blocks are not the heap's to give back while a local allocator still holds their
       pages, so stand the allocators down before asking the scavenger for anything. */
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);
    pas_thread_local_cache_shrink(pas_thread_local_cache_get(&js_marked_block_heap_config),
                                  pas_lock_is_not_held);
    pas_scavenger_run_synchronously_now();

    pas_heap_summary summary = pas_heap_compute_summary(heap, pas_lock_is_not_held);
    CHECK_EQUAL(summary.allocated, JS_MARKED_BLOCK_SIZE);
    CHECK_GREATER_EQUAL(summary.decommitted, (numBlocks - 1) * JS_MARKED_BLOCK_SIZE);

    js_marked_block_heap_deallocate(blocks[0]);
}

void testReservedMemoryHeap(size_t reservationSize, unsigned numBlocks, bool shouldSucceed)
{
    /* Aligned to the reservation size the way JavaScriptCore's Structure heap reserves, and mapped
       rather than malloced so that the heap can commit and decommit inside it. */
    void* reservation = mmap(nullptr, reservationSize * 2, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK(reservation != MAP_FAILED);

    uintptr_t begin = pas_round_up_to_power_of_2(reinterpret_cast<uintptr_t>(reservation),
                                                reservationSize);
    uintptr_t end = begin + reservationSize;

    pas_primitive_heap_ref heapRef = {
        .base = {
            .type = reinterpret_cast<const pas_heap_type*>(PAS_SIMPLE_TYPE_CREATE(1, 1)),
            .heap = nullptr,
            .allocator_index = 0
        },
        .cached_index = UINT_MAX
    };

    js_marked_block_heap_force_into_reserved_memory(&heapRef, begin, end);

    for (unsigned i = numBlocks; i--;) {
        void* block = js_marked_block_heap_try_allocate_from(&heapRef);

        if (!shouldSucceed) {
            if (!block)
                return;
            continue;
        }

        CHECK(block);
        CHECK(pas_is_aligned(reinterpret_cast<uintptr_t>(block), JS_MARKED_BLOCK_SIZE));
        CHECK_GREATER_EQUAL(reinterpret_cast<uintptr_t>(block), begin);
        CHECK_LESS_EQUAL(reinterpret_cast<uintptr_t>(block) + JS_MARKED_BLOCK_SIZE, end);
    }

    CHECK(shouldSucceed);
}

} // anonymous namespace

#endif // PAS_ENABLE_JS_MARKED_BLOCK

void addJSMarkedBlockHeapTests()
{
#if PAS_ENABLE_JS_MARKED_BLOCK
    ADD_TEST(testBlocksComeFromTheTailoredPageConfig());
    ADD_TEST(testBlocksAreBlockAlignedAndDistinct(1));
    ADD_TEST(testBlocksAreBlockAlignedAndDistinct(10));
    ADD_TEST(testBlocksAreBlockAlignedAndDistinct(100));
    ADD_TEST(testBlocksAreBlockAlignedAndDistinct(1000));
    ADD_TEST(testRepeatedAllocateAndFreeStaysBounded(10, 40));
    ADD_TEST(testRepeatedAllocateAndFreeStaysBounded(100, 20));
    ADD_TEST(testFreeingAllButOneBlockGivesBackTheRestOfThePage(64));
    ADD_TEST(testReservedMemoryHeap(64 * 1024 * 1024, 100, true));
    ADD_TEST(testReservedMemoryHeap(4 * 1024 * 1024, 10000, false));
#endif // PAS_ENABLE_JS_MARKED_BLOCK
}
