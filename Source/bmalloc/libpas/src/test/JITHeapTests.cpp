/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#if PAS_ENABLE_JIT

#include <iostream>
#include "jit_heap.h"
#include "jit_heap_config.h"
#include "pas_utils.h"

using namespace std;

namespace {

void testAllocateShrinkAndAllocate(unsigned initialObjectSize,
                                   unsigned numInitialObjects,
                                   unsigned initialSizeOfObjectToShrink,
                                   unsigned sizeToShrinkTo,
                                   unsigned finalObjectSize,
                                   unsigned expectedAlignmentOfFinalObject)
{
    for (unsigned i = numInitialObjects; i--;) {
        void* ptr = jit_heap_try_allocate(initialObjectSize);
        CHECK(ptr);
    }

    PAS_ASSERT(sizeToShrinkTo <= initialSizeOfObjectToShrink);

    void* ptr = jit_heap_try_allocate(initialSizeOfObjectToShrink);
    CHECK(ptr);

    jit_heap_shrink(ptr, sizeToShrinkTo);

    uintptr_t expectedAddress = pas_round_up_to_power_of_2(
        reinterpret_cast<uintptr_t>(ptr) + (sizeToShrinkTo ? sizeToShrinkTo : 1),
        expectedAlignmentOfFinalObject);

    void* newPtr = jit_heap_try_allocate(finalObjectSize);

    CHECK_EQUAL(
        newPtr,
        reinterpret_cast<void*>(expectedAddress));

    jit_heap_deallocate(ptr);
    jit_heap_deallocate(newPtr);
}

void testAllocationSize(size_t requestedSize, size_t actualSize)
{
    CHECK_EQUAL(jit_heap_get_size(jit_heap_try_allocate(requestedSize)), actualSize);
}

} // anonymous namespace

#endif // PAS_ENABLE_JIT

void addJITHeapTests()
{
#if PAS_ENABLE_JIT
    BootJITHeap bootJITHeap;

    {
        ForceBitfit forceBitfit;
        ADD_TEST(testAllocateShrinkAndAllocate(0, 0, 0, 0, 0, 4));
        ADD_TEST(testAllocateShrinkAndAllocate(0, 0, 128, 64, 64, 4));
        ADD_TEST(testAllocateShrinkAndAllocate(32, 10, 128, 64, 64, 4));
        ADD_TEST(testAllocateShrinkAndAllocate(32, 10, 1000, 500, 1000, 4));
        ADD_TEST(testAllocateShrinkAndAllocate(0, 0, 2048, 512, 1100, 256));
        ADD_TEST(testAllocateShrinkAndAllocate(32, 10, 2048, 512, 1100, 256));
        ADD_TEST(testAllocateShrinkAndAllocate(1100, 10, 2048, 512, 1100, 256));
        ADD_TEST(testAllocateShrinkAndAllocate(0, 0, 100000, 10000, 80000, 4));
    }
    
    ADD_TEST(testAllocationSize(4, 16));
    ADD_TEST(testAllocationSize(8, 16));
    ADD_TEST(testAllocationSize(12, 16));
    ADD_TEST(testAllocationSize(16, 16));
    ADD_TEST(testAllocationSize(20, 32));
    {
        ForceBitfit forceBitfit;
        ADD_TEST(testAllocationSize(4, 4));
        ADD_TEST(testAllocationSize(8, 8));
        ADD_TEST(testAllocationSize(12, 12));
        ADD_TEST(testAllocationSize(16, 16));
        ADD_TEST(testAllocationSize(20, 20));
    }
#endif // PAS_ENABLE_JIT
}
