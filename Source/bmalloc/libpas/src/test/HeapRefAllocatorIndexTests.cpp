/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#if PAS_ENABLE_ISO

#include "iso_heap.h"

namespace {

void testIsoAllocate(size_t requestedSize, size_t actualSize)
{
    pas_heap_ref heap = ISO_HEAP_REF_INITIALIZER(requestedSize);

    void* object = iso_allocate(&heap, pas_non_compact_allocation_mode);
    CHECK_EQUAL(iso_get_allocation_size(object), actualSize);
    CHECK_LESS(heap.allocator_index, 10000);

    void* object2 = iso_allocate(&heap, pas_non_compact_allocation_mode);
    CHECK_NOT_EQUAL(object, object2);
    CHECK_EQUAL(iso_get_allocation_size(object2), actualSize);
}

void testAllocatePrimitive(size_t requestedSize, size_t actualSize)
{
    pas_primitive_heap_ref heap = ISO_PRIMITIVE_HEAP_REF_INITIALIZER;

    void* object = iso_allocate_primitive(&heap, requestedSize, pas_non_compact_allocation_mode);
    CHECK_EQUAL(iso_get_allocation_size(object), actualSize);
    CHECK_LESS(heap.base.allocator_index, 10000);

    void* object2 = iso_allocate_primitive(&heap, requestedSize, pas_non_compact_allocation_mode);
    CHECK_NOT_EQUAL(object, object2);
    CHECK_EQUAL(iso_get_allocation_size(object2), actualSize);
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO

void addHeapRefAllocatorIndexTests()
{
#if PAS_ENABLE_ISO
    ADD_TEST(testIsoAllocate(16, 16));
    ADD_TEST(testIsoAllocate(32, 32));
    ADD_TEST(testIsoAllocate(48, 48));
    ADD_TEST(testIsoAllocate(64, 64));
    ADD_TEST(testIsoAllocate(1000, 1008));
    ADD_TEST(testIsoAllocate(1008, 1008));
    ADD_TEST(testIsoAllocate(10000, 10752));
    ADD_TEST(testIsoAllocate(10752, 10752));
    
    ADD_TEST(testAllocatePrimitive(16, 16));
    ADD_TEST(testAllocatePrimitive(32, 32));
    ADD_TEST(testAllocatePrimitive(48, 48));
    ADD_TEST(testAllocatePrimitive(64, 64));
    ADD_TEST(testAllocatePrimitive(1000, 1008));
    ADD_TEST(testAllocatePrimitive(1008, 1008));
    ADD_TEST(testAllocatePrimitive(10000, 10752));
    ADD_TEST(testAllocatePrimitive(10752, 10752));
#endif // PAS_ENABLE_ISO
}

