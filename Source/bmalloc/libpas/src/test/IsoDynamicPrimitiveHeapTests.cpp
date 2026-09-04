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

// This test is way too slow without segheaps.
#if PAS_ENABLE_ISO && SEGHEAP

#include <functional>
#include "iso_heap.h"
#include "iso_heap_innards.h"
#include "pas_dynamic_primitive_heap_map.h"
#include <set>
#include <vector>

using namespace std;

namespace {

void* allocate42(unsigned sizeIndex, const void* key)
{
    return iso_try_allocate_dynamic_primitive(key, 42 * (sizeIndex + 1));
}

void* allocate42WithAlignment(unsigned sizeIndex, const void* key)
{
    return iso_try_allocate_dynamic_primitive_with_alignment(key, 42 * (sizeIndex + 1), 32);
}

void* allocate42Zeroed(unsigned sizeIndex, const void* key)
{
    char* result = static_cast<char*>(
        iso_try_allocate_dynamic_primitive_zeroed(key, 42 * (sizeIndex + 1)));
    for (unsigned i = 42; i--;)
        CHECK(!result[i]);
    return result;
}

void* reallocate42(unsigned sizeIndex, const void* key)
{
    void* result = iso_try_allocate_common_primitive(16);
    CHECK(result);
    return iso_try_reallocate_dynamic_primitive(
        result, key, 42 * (sizeIndex + 1), pas_reallocate_free_if_successful);
}

void testManySizesAndKeys(
    unsigned numSizes, unsigned numKeys, unsigned maxHeaps,
    unsigned& maxHeapsLimiter,
    function<void*(unsigned sizeIndex, const void* key)> allocate)
{
    maxHeapsLimiter = maxHeaps;

    set<pas_heap*> heaps;
    vector<pas_heap*> heapByKey;

    for (unsigned keyIndex = 0; keyIndex < numKeys; ++keyIndex) {
        void* object = allocate(
            0,
            reinterpret_cast<const void*>(static_cast<uintptr_t>(keyIndex) + 666));
        CHECK(object);
        pas_heap* heap = iso_get_heap(object);
        CHECK(heap);
        CHECK_EQUAL(static_cast<bool>(heaps.count(heap)), keyIndex >= maxHeaps);
        heaps.insert(heap);
        PAS_ASSERT(heapByKey.size() == keyIndex);
        heapByKey.push_back(heap);
    }

    for (unsigned sizeIndex = 1; sizeIndex < numSizes; ++sizeIndex) {
        for (unsigned keyIndex = 0; keyIndex < numKeys; ++keyIndex) {
            void* object = allocate(
                0,
                reinterpret_cast<const void*>(static_cast<uintptr_t>(keyIndex) + 666));
            CHECK(object);
            pas_heap* heap = iso_get_heap(object);
            CHECK(heap);
            CHECK_EQUAL(heap, heapByKey[keyIndex]);
        }
    }
}

void testManySizesAndKeysInTandem(
    unsigned numSizesAndKeys, unsigned maxHeaps,
    unsigned& maxHeapsLimiter,
    bool disallowCollisions,
    function<void*(unsigned sizeIndex, const void* key)> allocate)
{
    iso_primitive_dynamic_heap_map.max_heaps_per_size = UINT_MAX;
    iso_primitive_dynamic_heap_map.max_heaps = UINT_MAX;
    
    maxHeapsLimiter = maxHeaps;

    set<pas_heap*> heaps;

    for (unsigned sizeAndKeyIndex = 0; sizeAndKeyIndex < numSizesAndKeys; ++sizeAndKeyIndex) {
        void* object = allocate(
            sizeAndKeyIndex,
            reinterpret_cast<const void*>(static_cast<uintptr_t>(sizeAndKeyIndex) + 666));
        CHECK(object);
        pas_heap* heap = iso_get_heap(object);
        CHECK(heap);
        if (disallowCollisions || sizeAndKeyIndex < maxHeaps) {
            CHECK(!heaps.count(heap));
            heaps.insert(heap);
        }
    }
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO && SEGHEAP

void addIsoDynamicPrimitiveHeapTests()
{
#if PAS_ENABLE_ISO && SEGHEAP
    static constexpr bool skipTestsBecauseOfTLAExhaustionCrash = true;
    
    ADD_TEST(testManySizesAndKeys(100, 1, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42));
    ADD_TEST(testManySizesAndKeys(100, 1, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeys(100, 1, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeys(100, 1, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, reallocate42));
    ADD_TEST(testManySizesAndKeys(100, 1, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42));
    ADD_TEST(testManySizesAndKeys(100, 1, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeys(100, 1, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeys(100, 1, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, reallocate42));
    ADD_TEST(testManySizesAndKeys(100, 10000, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42));
    ADD_TEST(testManySizesAndKeys(100, 10000, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeys(100, 10000, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeys(100, 10000, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, reallocate42));
    ADD_TEST(testManySizesAndKeys(100, 10000, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42));
    ADD_TEST(testManySizesAndKeys(100, 10000, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeys(100, 10000, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeys(100, 10000, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, reallocate42));
    ADD_TEST(testManySizesAndKeysInTandem(100, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42));
    ADD_TEST(testManySizesAndKeysInTandem(100, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeysInTandem(100, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeysInTandem(100, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, reallocate42));
    if (!skipTestsBecauseOfTLAExhaustionCrash) {
        ADD_TEST(testManySizesAndKeysInTandem(10000, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42WithAlignment));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42Zeroed));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 10, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, reallocate42));
    }
    ADD_TEST(testManySizesAndKeysInTandem(100, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42));
    ADD_TEST(testManySizesAndKeysInTandem(100, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeysInTandem(100, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeysInTandem(100, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, reallocate42));
    if (!skipTestsBecauseOfTLAExhaustionCrash) {
        ADD_TEST(testManySizesAndKeysInTandem(10000, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42WithAlignment));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, allocate42Zeroed));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 1000, iso_primitive_dynamic_heap_map.max_heaps_per_size, true, reallocate42));
    }

    ADD_TEST(testManySizesAndKeys(100, 1, 10, iso_primitive_dynamic_heap_map.max_heaps, allocate42));
    ADD_TEST(testManySizesAndKeys(100, 1, 10, iso_primitive_dynamic_heap_map.max_heaps, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeys(100, 1, 10, iso_primitive_dynamic_heap_map.max_heaps, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeys(100, 1, 10, iso_primitive_dynamic_heap_map.max_heaps, reallocate42));
    ADD_TEST(testManySizesAndKeys(100, 1, 1000, iso_primitive_dynamic_heap_map.max_heaps, allocate42));
    ADD_TEST(testManySizesAndKeys(100, 1, 1000, iso_primitive_dynamic_heap_map.max_heaps, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeys(100, 1, 1000, iso_primitive_dynamic_heap_map.max_heaps, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeys(100, 1, 1000, iso_primitive_dynamic_heap_map.max_heaps, reallocate42));
    ADD_TEST(testManySizesAndKeys(100, 10000, 10, iso_primitive_dynamic_heap_map.max_heaps, allocate42));
    ADD_TEST(testManySizesAndKeys(100, 10000, 10, iso_primitive_dynamic_heap_map.max_heaps, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeys(100, 10000, 10, iso_primitive_dynamic_heap_map.max_heaps, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeys(100, 10000, 10, iso_primitive_dynamic_heap_map.max_heaps, reallocate42));
    ADD_TEST(testManySizesAndKeys(100, 10000, 1000, iso_primitive_dynamic_heap_map.max_heaps, allocate42));
    ADD_TEST(testManySizesAndKeys(100, 10000, 1000, iso_primitive_dynamic_heap_map.max_heaps, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeys(100, 10000, 1000, iso_primitive_dynamic_heap_map.max_heaps, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeys(100, 10000, 1000, iso_primitive_dynamic_heap_map.max_heaps, reallocate42));
    ADD_TEST(testManySizesAndKeysInTandem(100, 10, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42));
    ADD_TEST(testManySizesAndKeysInTandem(100, 10, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeysInTandem(100, 10, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeysInTandem(100, 10, iso_primitive_dynamic_heap_map.max_heaps, false, reallocate42));
    ADD_TEST(testManySizesAndKeysInTandem(10000, 10, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42));
    ADD_TEST(testManySizesAndKeysInTandem(10000, 10, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeysInTandem(10000, 10, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeysInTandem(10000, 10, iso_primitive_dynamic_heap_map.max_heaps, false, reallocate42));
    ADD_TEST(testManySizesAndKeysInTandem(100, 1000, iso_primitive_dynamic_heap_map.max_heaps, true, allocate42));
    ADD_TEST(testManySizesAndKeysInTandem(100, 1000, iso_primitive_dynamic_heap_map.max_heaps, true, allocate42WithAlignment));
    ADD_TEST(testManySizesAndKeysInTandem(100, 1000, iso_primitive_dynamic_heap_map.max_heaps, true, allocate42Zeroed));
    ADD_TEST(testManySizesAndKeysInTandem(100, 1000, iso_primitive_dynamic_heap_map.max_heaps, true, reallocate42));
    if (!skipTestsBecauseOfTLAExhaustionCrash) {
        ADD_TEST(testManySizesAndKeysInTandem(10000, 1000, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 1000, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42WithAlignment));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 1000, iso_primitive_dynamic_heap_map.max_heaps, false, allocate42Zeroed));
        ADD_TEST(testManySizesAndKeysInTandem(10000, 1000, iso_primitive_dynamic_heap_map.max_heaps, false, reallocate42));
    }
#endif // PAS_ENABLE_ISO && SEGHEAP
}

