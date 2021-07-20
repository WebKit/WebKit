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

#if TLC

#include "LargeSharingPoolDump.h"
#include "pas_epoch.h"
#include "pas_heap_lock.h"
#include "pas_large_sharing_pool.h"
#include "pas_page_malloc.h"
#include "pas_physical_memory_transaction.h"
#include "pas_scavenger.h"
#include <vector>

#define PG (pas_page_malloc_alignment())
#define END ((uintptr_t)1 << PAS_ADDRESS_BITS)

using namespace std;

namespace {

struct Range {
    Range(uintptr_t begin,
          uintptr_t end,
          pas_commit_mode isCommitted,
          size_t numLiveBytes,
          uint64_t epoch)
        : begin(begin)
        , end(end)
        , isCommitted(isCommitted)
        , numLiveBytes(numLiveBytes)
        , epoch(epoch)
    {
    }
    
    uintptr_t begin;
    uintptr_t end;
    pas_commit_mode isCommitted;
    size_t numLiveBytes;
    uint64_t epoch;
};

void assertState(const vector<Range>& ranges)
{
    size_t index = 0;
    forEachLargeSharingPoolNode(
        [&] (pas_large_sharing_node* node) -> bool {
            CHECK_LESS(index, ranges.size());
            Range expectedRange = ranges[index++];
            CHECK_EQUAL(node->range.begin, expectedRange.begin);
            CHECK_EQUAL(node->range.end, expectedRange.end);
            CHECK_EQUAL(node->is_committed, expectedRange.isCommitted);
            CHECK_EQUAL(node->num_live_bytes, expectedRange.numLiveBytes);
            CHECK_EQUAL(node->use_epoch, expectedRange.epoch);
            return true;
        });
    CHECK_EQUAL(index, ranges.size());
}

void testBadCoalesceEpochUpdate()
{
    static constexpr bool verbose = false;
    
    pas_physical_memory_transaction transaction;
    
    pas_scavenger_suspend();
    pas_physical_memory_transaction_construct(&transaction);

    CHECK_EQUAL(pas_current_epoch, 0);
    
    pas_heap_lock_lock();
    pas_large_sharing_pool_boot_free(
        pas_range_create(10 * PG, 20 * PG),
        pas_physical_memory_is_locked_by_virtual_range_common_lock);
    pas_heap_lock_unlock();

    assertState({ Range(0, 10 * PG, pas_committed, 10 * PG, 0),
                  Range(10 * PG, 20 * PG, pas_committed, 0, 1),
                  Range(20 * PG, END, pas_committed, END - 20 * PG, 0) });
    
    pas_heap_lock_lock();
    pas_large_sharing_pool_boot_free(
        pas_range_create(20 * PG, 30 * PG),
        pas_physical_memory_is_locked_by_virtual_range_common_lock);
    pas_heap_lock_unlock();

    assertState({ Range(0, 10 * PG, pas_committed, 10 * PG, 0),
                  Range(10 * PG, 20 * PG, pas_committed, 0, 1),
                  Range(20 * PG, 30 * PG, pas_committed, 0, 2),
                  Range(30 * PG, END, pas_committed, END - 30 * PG, 0) });
    
    pas_heap_lock_lock();
    CHECK(pas_large_sharing_pool_allocate_and_commit(
              pas_range_create(10 * PG, 30 * PG),
              &transaction,
              pas_physical_memory_is_locked_by_virtual_range_common_lock));
    pas_heap_lock_unlock();
    
    if (verbose)
        dumpLargeSharingPool();

    assertState({ Range(0, 10 * PG, pas_committed, 10 * PG, 0),
                  Range(10 * PG, 30 * PG, pas_committed, 20 * PG, 3),
                  Range(30 * PG, END, pas_committed, END - 30 * PG, 0) });
    
}

} // anonymous namespace

#endif // TLC

void addLargeSharingPoolTests()
{
#if TLC
    EpochIsCounter epochIsCounter;
    
    ADD_TEST(testBadCoalesceEpochUpdate());
#endif // TLC
}

