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
#include "pas_segregated_size_directory.h"
#include <set>

using namespace std;

namespace {

void setupConfig()
{
    // If these assertions ever fail we could just fix it by replacing them with code that mutates the
    // config to have the settings we want.
    CHECK_EQUAL(bmalloc_intrinsic_runtime_config.base.view_cache_capacity_for_object_size,
                pas_heap_runtime_config_aggressive_view_cache_capacity);
    CHECK_EQUAL(bmalloc_intrinsic_runtime_config.base.directory_size_bound_for_no_view_cache, 0);
}

void testDisableViewCacheUsingBoundForNoViewCache()
{
    setupConfig();

    bmalloc_intrinsic_runtime_config.base.directory_size_bound_for_no_view_cache = UINT_MAX;

    bmalloc_deallocate(bmalloc_allocate(42));
}

void testEnableViewCacheAtSomeBoundForNoViewCache(unsigned bound)
{
    setupConfig();

    bmalloc_intrinsic_runtime_config.base.directory_size_bound_for_no_view_cache = bound;

    void* ptr = bmalloc_allocate(42);
    pas_segregated_view view = pas_segregated_view_for_object(
        reinterpret_cast<uintptr_t>(ptr), &bmalloc_heap_config);
    pas_segregated_size_directory* theDirectory = pas_segregated_view_get_size_directory(view);

    CHECK_EQUAL(theDirectory->view_cache_index, UINT_MAX);

    set<pas_segregated_view> views;
    views.insert(view);

    for (;;) {
        ptr = bmalloc_allocate(42);
        view = pas_segregated_view_for_object(reinterpret_cast<uintptr_t>(ptr), &bmalloc_heap_config);
        pas_segregated_size_directory* directory = pas_segregated_view_get_size_directory(view);

        CHECK_EQUAL(directory, theDirectory);

        views.insert(view);

        if (views.size() > bound) {
            CHECK_EQUAL(views.size(), bound + 1);
            CHECK_LESS(theDirectory->view_cache_index, UINT_MAX);
            CHECK_GREATER(theDirectory->view_cache_index, 0);

            pas_segregated_page* page = pas_segregated_view_get_page(view);
            CHECK_EQUAL(page->view_cache_index, theDirectory->view_cache_index);
            return;
        }
    }
}

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addViewCacheTests()
{
#if PAS_ENABLE_BMALLOC
    ForceExclusives forceExclusives;
    ForceTLAs forceTLAs;
    
    ADD_TEST(testDisableViewCacheUsingBoundForNoViewCache());
    ADD_TEST(testEnableViewCacheAtSomeBoundForNoViewCache(1));
    ADD_TEST(testEnableViewCacheAtSomeBoundForNoViewCache(10));
    ADD_TEST(testEnableViewCacheAtSomeBoundForNoViewCache(100));
#endif // PAS_ENABLE_BMALLOC
}

