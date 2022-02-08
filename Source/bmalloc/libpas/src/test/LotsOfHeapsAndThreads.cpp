/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"
#include "pas_get_heap.h"
#include <thread>

using namespace std;

namespace {

void testLotsOfHeapsAndThreads(unsigned numHeaps, unsigned numThreads, unsigned count)
{
    thread* threads = new thread[numThreads];
    pas_heap_ref* heaps = new pas_heap_ref[numHeaps];

    for (unsigned i = numHeaps; i--;)
        heaps[i] = BMALLOC_HEAP_REF_INITIALIZER(new bmalloc_type(BMALLOC_TYPE_INITIALIZER(16, 16, "test")));

    for (unsigned i = numThreads; i--;) {
        threads[i] = thread([&] () {
            for (unsigned j = count; j--;) {
                for (unsigned k = numHeaps; k--;) {
                    void* ptr = bmalloc_iso_allocate(heaps + k);
                    CHECK_EQUAL(pas_get_heap(ptr, BMALLOC_HEAP_CONFIG),
                                bmalloc_heap_ref_get_heap(heaps + k));
                    bmalloc_deallocate(ptr);
                }
            }
        });
    }

    for (unsigned i = numThreads; i--;)
        threads[i].join();
}

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addLotsOfHeapsAndThreadsTests()
{
#if PAS_ENABLE_BMALLOC
    ForceTLAs forceTLAs;
    ForcePartials forcePartials;
    
    ADD_TEST(testLotsOfHeapsAndThreads(10000, 100, 10));
    ADD_TEST(testLotsOfHeapsAndThreads(25000, 100, 10));
    ADD_TEST(testLotsOfHeapsAndThreads(30000, 100, 10)); // This is about as high as we can reliably go right now!
#endif // PAS_ENABLE_BMALLOC
}
