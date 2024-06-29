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
#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"

#include <cstdlib>

using namespace std;

namespace {

void testBmallocAllocate()
{
    void* mem = bmalloc_try_allocate(100, pas_non_compact_allocation_mode);
    CHECK(mem);
}

void testBmallocDeallocate()
{
    void* mem = bmalloc_try_allocate(100, pas_non_compact_allocation_mode);
    CHECK(mem);
    bmalloc_deallocate(mem);
}

void testBmallocForceBitfitAfterAlloc()
{
    void* mem0 = bmalloc_try_allocate(28616, pas_non_compact_allocation_mode);
    CHECK(mem0);

    void* mem1 = bmalloc_try_allocate(20768, pas_non_compact_allocation_mode);
    CHECK(mem1);

    // Simulate entering mini mode by forcing bitfit only.
    bmalloc_intrinsic_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_intrinsic_runtime_config.base.max_bitfit_object_size = UINT_MAX;
    bmalloc_primitive_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_primitive_runtime_config.base.max_bitfit_object_size = UINT_MAX;

    void* mem2 = bmalloc_try_allocate(20648, pas_non_compact_allocation_mode);
    CHECK(mem2);
}

} // anonymous namespace

void addBmallocTests()
{
    ADD_TEST(testBmallocAllocate());
    ADD_TEST(testBmallocDeallocate());
    ADD_TEST(testBmallocForceBitfitAfterAlloc());
}
