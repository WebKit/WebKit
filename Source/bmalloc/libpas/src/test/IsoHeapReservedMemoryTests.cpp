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

#if PAS_ENABLE_ISO && SEGHEAP

#include "HeapLocker.h"
#include "iso_heap.h"

using namespace std;

namespace {

void testSizeProgression(size_t startSize,
                         size_t maxSize,
                         size_t sizeStep,
                         size_t countForSize,
                         size_t reservationSize,
                         bool shouldSucceed)
{
    static constexpr bool verbose = false;
    
    void* reservation = malloc(reservationSize);
    CHECK(reservation);
    
    pas_primitive_heap_ref heapRef = ISO_PRIMITIVE_HEAP_REF_INITIALIZER;

    uintptr_t begin = reinterpret_cast<uintptr_t>(reservation);
    uintptr_t end = begin + reservationSize;

    iso_force_primitive_heap_into_reserved_memory(&heapRef, begin, end);

    for (size_t size = startSize; size <= maxSize; size += sizeStep) {
        if (verbose)
            cout << "Allocating " << size << "\n";
        
        for (size_t i = countForSize; i--;) {
            void* object = iso_try_allocate_primitive(&heapRef, size);
            
            if (shouldSucceed)
                CHECK(object);
            else {
                if (!object)
                    return;
            }
            
            uintptr_t objectBegin = reinterpret_cast<uintptr_t>(object);
            uintptr_t objectEnd = objectBegin + size;
            
            CHECK_GREATER_EQUAL(objectBegin, begin);
            CHECK_LESS(objectBegin, end);
            CHECK_GREATER_EQUAL(objectEnd, begin);
            CHECK_LESS_EQUAL(objectEnd, end);
        }
    }

    CHECK(shouldSucceed);
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO && SEGHEAP

void addIsoHeapReservedMemoryTests()
{
#if PAS_ENABLE_ISO && SEGHEAP
    ADD_TEST(testSizeProgression(0, 1000000, 1024, 1, 1000000000, true));
    ADD_TEST(testSizeProgression(0, 10000, 64, 1000, 1000000000, true));
    ADD_TEST(testSizeProgression(0, 100000000, 1024, 1, 100000000, false));
#endif // PAS_ENABLE_ISO && SEGHEAP
}
