/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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
#include "bmalloc_heap_config.h"
#include "pas_page_base_inlines.h"
#include "pas_page_granule_use_count.h"
#include "pas_utils.h"
#include <vector>

using namespace std;

namespace {

void testIsDivisibleBy3(unsigned value)
{
    static const uint64_t magic_constant = PAS_IS_DIVISIBLE_BY_MAGIC_CONSTANT(3);
    CHECK_EQUAL(pas_is_divisible_by(value, magic_constant),
                !(value % 3));
}

// Commit-granule use counts on a medium segregated page, for objects whose size is at, just above, or just below the
// commit granule. A medium segregated page config's largest object size is its page size divided by its
// min-objects-per-page, so a smaller divisor can make objects as large as (or larger than) a granule. Objects are laid out
// back to back from the start of the page, as the medium segregated payload starts at offset zero. The page's use counts
// must always equal the number of live objects overlapping each granule, and freeing an object must report an empty
// granule exactly when one of the granules it overlaps drops to zero.
void testGranuleUseCountsForObjectSize(size_t objectSize)
{
    const pas_page_base_config& config = bmalloc_heap_config.medium_segregated_config.base;
    size_t pageSize = config.page_size;
    size_t granuleSize = config.granule_size;
    size_t numGranules = pageSize / granuleSize;
    size_t numObjects = pageSize / objectSize;

    CHECK_EQUAL(pageSize % granuleSize, static_cast<size_t>(0));
    CHECK_GREATER(numObjects, static_cast<size_t>(0));

    std::vector<pas_page_granule_use_count> useCounts(numGranules, 0);
    std::vector<unsigned> expected(numGranules, 0);

    auto firstGranule = [&](size_t objectIndex) {
        return objectIndex * objectSize / granuleSize;
    };
    auto lastGranule = [&](size_t objectIndex) {
        return ((objectIndex + 1) * objectSize - 1) / granuleSize;
    };
    auto checkUseCounts = [&] {
        for (size_t granule = 0; granule < numGranules; ++granule)
            CHECK_EQUAL(static_cast<unsigned>(useCounts[granule]), expected[granule]);
    };

    for (size_t objectIndex = 0; objectIndex < numObjects; ++objectIndex) {
        pas_page_granule_increment_uses_for_range(
            useCounts.data(), objectIndex * objectSize, (objectIndex + 1) * objectSize, pageSize, granuleSize);
        for (size_t granule = firstGranule(objectIndex); granule <= lastGranule(objectIndex); ++granule)
            ++expected[granule];
    }
    checkUseCounts();

    for (size_t objectIndex = 0; objectIndex < numObjects; ++objectIndex) {
        bool shouldFindEmptyGranule = false;
        for (size_t granule = firstGranule(objectIndex); granule <= lastGranule(objectIndex); ++granule) {
            if (!--expected[granule])
                shouldFindEmptyGranule = true;
        }
        bool didFindEmptyGranule = pas_page_base_free_granule_uses_in_range(
            useCounts.data(), objectIndex * objectSize, (objectIndex + 1) * objectSize, config);
        CHECK_EQUAL(didFindEmptyGranule, shouldFindEmptyGranule);
        checkUseCounts();
    }
}

} // anonymous namespace

void addUtilsTests()
{
    ADD_TEST(testGranuleUseCountsForObjectSize(bmalloc_heap_config.medium_segregated_config.base.granule_size));
    ADD_TEST(testGranuleUseCountsForObjectSize(bmalloc_heap_config.medium_segregated_config.base.granule_size + 512));
    ADD_TEST(testGranuleUseCountsForObjectSize(bmalloc_heap_config.medium_segregated_config.base.granule_size - 512));
    ADD_TEST(testGranuleUseCountsForObjectSize(bmalloc_heap_config.medium_segregated_config.base.max_object_size));
    ADD_TEST(testIsDivisibleBy3(0));
    ADD_TEST(testIsDivisibleBy3(1));
    ADD_TEST(testIsDivisibleBy3(2));
    ADD_TEST(testIsDivisibleBy3(3));
    ADD_TEST(testIsDivisibleBy3(4));
    ADD_TEST(testIsDivisibleBy3(5));
    ADD_TEST(testIsDivisibleBy3(6));
    ADD_TEST(testIsDivisibleBy3(7));
    ADD_TEST(testIsDivisibleBy3(8));
    ADD_TEST(testIsDivisibleBy3(9));
    ADD_TEST(testIsDivisibleBy3(10));
    ADD_TEST(testIsDivisibleBy3(11));
    ADD_TEST(testIsDivisibleBy3(12));
    ADD_TEST(testIsDivisibleBy3(13));
    ADD_TEST(testIsDivisibleBy3(14));
    ADD_TEST(testIsDivisibleBy3(15));
    ADD_TEST(testIsDivisibleBy3(16));
    ADD_TEST(testIsDivisibleBy3(17));
    ADD_TEST(testIsDivisibleBy3(18));
    ADD_TEST(testIsDivisibleBy3(19));
    ADD_TEST(testIsDivisibleBy3(20));
    ADD_TEST(testIsDivisibleBy3(21));
    ADD_TEST(testIsDivisibleBy3(22));
    ADD_TEST(testIsDivisibleBy3(23));
    ADD_TEST(testIsDivisibleBy3(24));
    ADD_TEST(testIsDivisibleBy3(25));
    ADD_TEST(testIsDivisibleBy3(26));
    ADD_TEST(testIsDivisibleBy3(27));
}
