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

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"
#include "pas_heap.h"
#include "pas_segregated_heap_inlines.h"
#include "pas_segregated_view.h"

namespace {

void testMemalignArray(size_t size, size_t typeSize, size_t typeAlignment)
{
    bmalloc_type type = BMALLOC_TYPE_INITIALIZER(static_cast<unsigned>(typeSize),
                                                 static_cast<unsigned>(typeAlignment),
                                                 "test");
    pas_heap_ref heapRef = BMALLOC_HEAP_REF_INITIALIZER(&type);
    pas_segregated_view view;
    pas_segregated_size_directory* directory;

    void* ptr = bmalloc_iso_allocate_zeroed_array_by_size(&heapRef, size, pas_non_compact_allocation_mode);
    CHECK(ptr);

    view = pas_segregated_view_for_object(reinterpret_cast<uintptr_t>(ptr), &bmalloc_heap_config);
    directory = pas_segregated_view_get_size_directory(view);

    CHECK_EQUAL(pas_segregated_heap_size_directory_for_size(
                    &bmalloc_heap_ref_get_heap(&heapRef)->segregated_heap,
                    size,
                    BMALLOC_HEAP_CONFIG,
                    NULL),
                directory);
}

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addMemalignTests()
{
#if PAS_ENABLE_BMALLOC
    ADD_TEST(testMemalignArray(1523, 16, 4));
    ADD_TEST(testMemalignArray(1523, 128, 128));
#endif // PAS_ENABLE_BMALLOC
}

