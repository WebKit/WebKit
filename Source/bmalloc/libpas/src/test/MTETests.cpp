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
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TestHarness.h"
#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"
#include "bmalloc_heap_innards.h"
#include "pas_get_object_kind.h"
#include "pas_internal_config.h"
#include "pas_mte.h"
#include "pas_mte_config.h"
#include "pas_scavenger.h"
#include "tagged_bmalloc_heap.h"
#include "tagged_bmalloc_heap_config.h"

#include <algorithm>
#include <cstring>
#include <vector>

using namespace std;

#if defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#if PAS_ENABLE_MTE

namespace {

// Extracts the 4-bit MTE tag from a tagged pointer (bits [59:56]).
static unsigned logicalTagOf(uintptr_t ptr)
{
    return static_cast<unsigned>((ptr >> PAS_MTE_TAG_SHIFT) & 0xf);
}

// Reads the tag stored in MTE tag memory for the granule at `ptr` via the LDG instruction.
// Operates on the canonical address (tag bits stripped) so it works with any pointer value,
// including ones that have been freed (the tag storage persists until the page is decommitted).
static unsigned allocationTagOf(uintptr_t ptr)
{
    uintptr_t canonical = ptr & PAS_MTE_CANONICAL_MASK;
    PAS_MTE_GET_MTAG(canonical);
    return logicalTagOf(canonical);
}

// Identifies which kind of tagged_bmalloc page the object at `ptr` lives on, so tests can
// confirm they are exercising the allocation path they claim to be.
static pas_object_kind objectKindOf(uintptr_t ptr)
{
    return pas_get_object_kind(reinterpret_cast<void*>(ptr & PAS_MTE_CANONICAL_MASK), TAGGED_BMALLOC_HEAP_CONFIG);
}

static bool isSegregatedKind(pas_object_kind kind)
{
    return kind == pas_small_segregated_object_kind || kind == pas_medium_segregated_object_kind;
}

static bool isBitfitKind(pas_object_kind kind)
{
    return kind == pas_small_bitfit_object_kind || kind == pas_medium_bitfit_object_kind || kind == pas_marge_bitfit_object_kind;
}

// Tagged-heap allocations get a nonzero MTE tag for all sizes up to PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE
void testMTETagsTaggedHeapObjectsAreTagged()
{
    static const size_t sizes[] = {
        16, 32, 48, 64, 128, 256, 512, 1024, 2048, 4096,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE / 2,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
    };
    for (auto size : sizes) {
        void* mem = tagged_bmalloc_try_allocate(size);
        CHECK(mem);
        uintptr_t ptr = reinterpret_cast<uintptr_t>(mem);
        unsigned tag = logicalTagOf(ptr);
        CHECK_NOT_EQUAL(tag, 0u);
        CHECK_EQUAL(tag, allocationTagOf(ptr));
        tagged_bmalloc_deallocate(mem);
    }
}

// Allocations from the (untagged) bmalloc_heap always return a zero-tagged pointer,
// whether they come from the common primitive heap or the compact heap.
void testMTETagsUntaggedHeapObjectsAreZeroTagged()
{
    static const size_t sizes[] = {
        16, 64, 256, 1024, 4096,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE - 1,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 2,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 200,
    };
    for (auto size : sizes) {
        void* mem = bmalloc_try_allocate(size);
        CHECK(mem);
        CHECK_EQUAL(logicalTagOf(reinterpret_cast<uintptr_t>(mem)), 0u);
        bmalloc_deallocate(mem);

        mem = bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, size);
        CHECK(mem);
        CHECK_EQUAL(logicalTagOf(reinterpret_cast<uintptr_t>(mem)), 0u);
        bmalloc_deallocate(mem);
    }
}

// Calling pas_mte_force_nontaggable_user_allocations_into_large_heap() should
// still let objects <=32k be allocated from MTE-tagged heaps.
void testMTETagsAtAndBelowCeilingAreTagged()
{
    pas_mte_force_nontaggable_user_allocations_into_large_heap();

    static const size_t sizes[] = {
        16,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE / 4,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE / 2,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
    };
    for (auto size : sizes) {
        void* mem = tagged_bmalloc_try_allocate(size);
        CHECK(mem);
        CHECK_NOT_EQUAL(logicalTagOf(reinterpret_cast<uintptr_t>(mem)), 0u);
        tagged_bmalloc_deallocate(mem);
    }
}

void testMTETagsAboveCeilingAreUntagged()
{
    pas_mte_force_nontaggable_user_allocations_into_large_heap();

    static const size_t sizes[] = {
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE + 16,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 2,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE * 4,
    };
    for (auto size : sizes) {
        void* mem = tagged_bmalloc_try_allocate(size);
        CHECK(mem);
        // Large allocations are protected by guard objects and thus untagged
        CHECK_EQUAL(objectKindOf(reinterpret_cast<uintptr_t>(mem)), pas_large_object_kind);
        CHECK_EQUAL(logicalTagOf(reinterpret_cast<uintptr_t>(mem)), 0u);
        tagged_bmalloc_deallocate(mem);
    }
}

void testMTESegregatedAdjacentObjectsHaveDifferentTags()
{
    // Allocate enough 32-byte objects to populate several segregated pages and yield
    // a large sample of truly adjacent (contiguous) pairs to check.
    const size_t objSize = 32;
    const size_t count = 512;
    vector<void*> objs(count);

    for (size_t i = 0; i < count; i++) {
        objs[i] = tagged_bmalloc_try_allocate(objSize);
        CHECK(objs[i]);
        CHECK(isSegregatedKind(objectKindOf(reinterpret_cast<uintptr_t>(objs[i]))));
    }

    sort(objs.begin(), objs.end(), [](void* x, void* y) {
        return (reinterpret_cast<uintptr_t>(x) & PAS_MTE_CANONICAL_MASK)
             < (reinterpret_cast<uintptr_t>(y) & PAS_MTE_CANONICAL_MASK);
    });

    size_t adjacentPairsChecked = 0;
    for (size_t i = 0; i + 1 < count; i++) {
        uintptr_t a = reinterpret_cast<uintptr_t>(objs[i]);
        uintptr_t b = reinterpret_cast<uintptr_t>(objs[i + 1]);
        uintptr_t aCanon = a & PAS_MTE_CANONICAL_MASK;
        uintptr_t bCanon = b & PAS_MTE_CANONICAL_MASK;
        if (bCanon - aCanon == objSize) {
            ++adjacentPairsChecked;
            CHECK_NOT_EQUAL(logicalTagOf(a), logicalTagOf(b));
        }
    }
    // Sanity: we must have found enough adjacent pairs to make the test meaningful.
    CHECK_GREATER(adjacentPairsChecked, static_cast<size_t>(64));

    for (auto* p : objs)
        tagged_bmalloc_deallocate(p);
}

void testMTEBitfitAdjacentObjectsHaveDifferentTags()
{
    // Use a mix of sizes to exercise the nonhomogeneous (ldg-based) tagging path.
    static const size_t sizes[] = { 32, 48, 64, 80, 96, 112, 128 };
    const size_t count = 512;
    vector<pair<size_t, void*>> objs;
    objs.reserve(count);

    for (size_t i = 0; i < count; i++) {
        size_t sz = sizes[i % (sizeof(sizes) / sizeof(*sizes))];
        void* mem = tagged_bmalloc_try_allocate(sz);
        CHECK(mem);
        CHECK(isBitfitKind(objectKindOf(reinterpret_cast<uintptr_t>(mem))));
        objs.push_back({ sz, mem });
    }

    sort(objs.begin(), objs.end(), [](const auto& a, const auto& b) {
        return (reinterpret_cast<uintptr_t>(a.second) & PAS_MTE_CANONICAL_MASK)
             < (reinterpret_cast<uintptr_t>(b.second) & PAS_MTE_CANONICAL_MASK);
    });

    size_t adjacentPairsChecked = 0;
    for (size_t i = 0; i + 1 < objs.size(); i++) {
        uintptr_t a = reinterpret_cast<uintptr_t>(objs[i].second);
        uintptr_t b = reinterpret_cast<uintptr_t>(objs[i + 1].second);
        uintptr_t aCanon = a & PAS_MTE_CANONICAL_MASK;
        uintptr_t bCanon = b & PAS_MTE_CANONICAL_MASK;
        size_t sza = objs[i].first;
        // Only check pairs that are truly adjacent (b starts exactly where a ends).
        if (aCanon + sza == bCanon) {
            ++adjacentPairsChecked;
            CHECK_NOT_EQUAL(logicalTagOf(a), logicalTagOf(b));
        }
    }

    // Sanity: we must have found enough adjacent pairs to make the test meaningful.
    CHECK_GREATER(adjacentPairsChecked, static_cast<size_t>(64));

    for (auto& [sz, p] : objs)
        tagged_bmalloc_deallocate(p);
}

// Reallocating a tagged-heap object to a larger size should return a pointer
// with a nonzero tag, and all data written to the original buffer is preserved at the new
// address
void testMTEReallocPreservesDataAndUsesNonzeroTag()
{
    const size_t oldSize = 64;
    const size_t newSize = 256;
    const uint8_t pattern = 0xAB;

    void* mem = tagged_bmalloc_try_allocate(oldSize);
    CHECK(mem);
    CHECK_NOT_EQUAL(logicalTagOf(reinterpret_cast<uintptr_t>(mem)), 0u);

    memset(mem, pattern, oldSize);

    void* newMem = tagged_bmalloc_try_reallocate(mem, newSize, pas_reallocate_free_if_successful);
    CHECK(newMem);
    CHECK_NOT_EQUAL(logicalTagOf(reinterpret_cast<uintptr_t>(newMem)), 0u);

    const auto* bytes = static_cast<const uint8_t*>(newMem);
    for (size_t i = 0; i < oldSize; i++)
        CHECK_EQUAL(static_cast<unsigned>(bytes[i]), static_cast<unsigned>(pattern));

    tagged_bmalloc_deallocate(newMem);
}

// Reallocating within the compact heap always returns a zero-tagged pointer.
void testMTECompactReallocGivesZeroTag()
{
    void* mem = bmalloc_try_allocate_auxiliary(&bmalloc_compact_primitive_heap_ref, 64);
    CHECK(mem);
    CHECK_EQUAL(logicalTagOf(reinterpret_cast<uintptr_t>(mem)), 0u);

    void* newMem = bmalloc_try_reallocate_auxiliary(mem, &bmalloc_compact_primitive_heap_ref, 128,
        pas_reallocate_free_if_successful);
    CHECK(newMem);
    CHECK_EQUAL(logicalTagOf(reinterpret_cast<uintptr_t>(newMem)), 0u);

    bmalloc_deallocate(newMem);
}

void testMTEBitfitObjectsGetRetaggedOnFreeManyIterations()
{
    // Scavenger is already suspended by SuspendScavengerScope before this function runs.
    const size_t objSize = 64;
    const size_t count = 256;
    const size_t iterations = 16;
    vector<uintptr_t> ptrs(count);

    for (size_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < count; i++) {
            void* mem = tagged_bmalloc_try_allocate(objSize);
            CHECK(mem);
            ptrs[i] = reinterpret_cast<uintptr_t>(mem);
            CHECK_NOT_EQUAL(logicalTagOf(ptrs[i]), 0u);
            CHECK(isBitfitKind(objectKindOf(ptrs[i])));
        }

        size_t tagChanged = 0;
        for (size_t i = 0; i < count; i++) {
            unsigned tagBefore = logicalTagOf(ptrs[i]);
            tagged_bmalloc_deallocate(reinterpret_cast<void*>(ptrs[i]));

            unsigned tagAfter = allocationTagOf(ptrs[i]);
            CHECK_NOT_EQUAL(tagAfter, 0u);

            if (tagAfter != tagBefore)
                ++tagChanged;
        }

        // This is technically a flaky check: individual slots may get retagged to
        // the same value (prior to PTE). But with 1024 allocations being freed,
        // the chance that >20% of them end up with the same tag is ~1 in 1.16 million
        // even for the segregated case, where there are only 7 tags -- even less
        // likely for bitfit.
        // FIXME: once we have PTE, remove the 80% bar
        CHECK_GREATER_EQUAL(tagChanged, static_cast<size_t>(count * 8 / 10));
    }
}

// Segregated objects are retagged when the scavenger flushes the deallocation log.
// Unlike bitfit, tagged_bmalloc_deallocate for segregated objects only enqueues the
// slot into the deallocation-log; tagging thus only happens when the log is
// actually emptied.
void testMTESegregatedObjectsGetRetaggedOnScavenge()
{
    // Use a small fixed set of sizes with the size index advancing every two
    // allocations ((i/2) % numSizes).  That way each consecutive pair of
    // allocations shares the same size class: the even one gets freed and the
    // odd one stays live.
    // This helps make it so (though does not guarantee) that one allocation
    // per pas-page stays alive, which avoids false positives from cases where
    // the whole page is decommitted and thus is 'retagged' to a tag of zero,
    // even if retag-on-scavenge is not working in general.
    static const size_t sizes[] = { 16, 32, 64, 128, 256, 512 };
    const size_t numSizes = sizeof(sizes) / sizeof(*sizes);
    const size_t count = 1024;

    vector<uintptr_t> allPtrs(count);
    for (size_t i = 0; i < count; i++) {
        size_t sz = sizes[(i / 2) % numSizes];
        void* mem = tagged_bmalloc_try_allocate(sz);
        CHECK(mem);
        allPtrs[i] = reinterpret_cast<uintptr_t>(mem);
        CHECK_NOT_EQUAL(logicalTagOf(allPtrs[i]), 0u);
        CHECK(isSegregatedKind(objectKindOf(allPtrs[i])));
    }

    // Free the even-indexed objects; odd-indexed objects stay live to pin the pages in memory.
    const size_t freedCount = count / 2;
    vector<uintptr_t> freedPtrs;
    vector<unsigned> tagsBefore;
    freedPtrs.reserve(freedCount);
    tagsBefore.reserve(freedCount);

    for (size_t i = 0; i < count; i += 2) {
        freedPtrs.push_back(allPtrs[i]);
        tagsBefore.push_back(logicalTagOf(allPtrs[i]));
        tagged_bmalloc_deallocate(reinterpret_cast<void*>(allPtrs[i]));
    }

    pas_scavenger_clear_all_caches();

    size_t tagChanged = 0;
    for (size_t i = 0; i < freedCount; i++) {
        unsigned tagAfter = allocationTagOf(freedPtrs[i]);
        CHECK_NOT_EQUAL(tagAfter, 0u);
        if (tagAfter != tagsBefore[i])
            ++tagChanged;
    }
    // FIXME: once we have PTE, remove the 80% bar
    CHECK_GREATER_EQUAL(tagChanged, static_cast<size_t>(freedCount * 8 / 10));

    // Release the live half.
    for (size_t i = 1; i < count; i += 2)
        tagged_bmalloc_deallocate(reinterpret_cast<void*>(allPtrs[i]));
}

} // anonymous namespace

#endif // PAS_ENABLE_MTE
#endif // defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE

void addMTETests()
{
#if defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#if PAS_ENABLE_MTE
    {
        // Basic tag-presence / tagging-ceiling tests (use the default runtime config).
        ADD_TEST(testMTETagsTaggedHeapObjectsAreTagged());
        ADD_TEST(testMTETagsUntaggedHeapObjectsAreZeroTagged());
        ADD_TEST(testMTETagsAtAndBelowCeilingAreTagged());
        ADD_TEST(testMTETagsAboveCeilingAreUntagged());

        // Segregated-heap adjacent-tag exclusion. Hardened processes force the tagged
        // heap into bitfit, so we have to force segregated allocation explicitly.
        {
            ForceSegregated seg;
            ADD_TEST(testMTESegregatedAdjacentObjectsHaveDifferentTags());
        }

        // Realloc tests (exercise the TCO-guarded copy path).
        ADD_TEST(testMTEReallocPreservesDataAndUsesNonzeroTag());
        ADD_TEST(testMTECompactReallocGivesZeroTag());

        // Bitfit-heap tests (force all tagged_bmalloc allocations through the bitfit path).
        {
            ForceBitfit bitfit;

            ADD_TEST(testMTEBitfitAdjacentObjectsHaveDifferentTags());

            // Bitfit retag-on-free requires the scavenger to be quiescent so we can read
            // the freshly retagged granule before the page is potentially decommitted.
            {
                SuspendScavengerScope noScav;
                ADD_TEST(testMTEBitfitObjectsGetRetaggedOnFreeManyIterations());
            }
        }

        // Segregated-heap retag-on-scavenge: free objects, then verify retagged
        // after pas_scavenger_clear_all_caches() flushes the deallocation log.
        {
            ForceSegregated seg;
            SuspendScavengerScope noScav;
            ADD_TEST(testMTESegregatedObjectsGetRetaggedOnScavenge());
        }
    }
#endif // PAS_ENABLE_MTE
#endif // defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
}
