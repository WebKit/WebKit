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
#include "pas_bootstrap_heap_page_provider.h"
#include "pas_memory_flavor.h"
#include "pas_mte.h"
#include "pas_mte_config.h"
#include "pas_page_malloc.h"
#include "tagged_bmalloc_heap_config.h"
#if PAS_ENABLE_JIT
#include "jit_heap_config.h"
#endif

using namespace std;

namespace {

void testHeapConfigFlavors()
{
    CHECK_EQUAL(bmalloc_heap_config.memory_flavor, pas_memory_flavor_plain);
    CHECK_EQUAL(tagged_bmalloc_heap_config.memory_flavor, pas_memory_flavor_mte);
#if PAS_ENABLE_JIT
    CHECK_EQUAL(jit_heap_config.memory_flavor, pas_memory_flavor_rwx);
    CHECK(pas_memory_flavor_client_owns_permissions(jit_heap_config.memory_flavor));
    CHECK(pas_memory_flavor_is_executable(jit_heap_config.memory_flavor));
#endif
}

void testPageCachesUseHeapConfigFlavor()
{
    /* Every page cache of a heap config, including the one backing large allocations, must get its
       memory from the bootstrap page provider of the heap config's flavor. */
    CHECK(bmalloc_page_caches.large_heap_cache.provider == pas_bootstrap_heap_page_provider);
    CHECK(bmalloc_page_caches.small_exclusive_segregated_megapage_cache.provider == pas_bootstrap_heap_page_provider);
    CHECK(bmalloc_page_caches.small_other_megapage_cache.provider == pas_bootstrap_heap_page_provider);
    CHECK(bmalloc_page_caches.medium_megapage_cache.provider == pas_bootstrap_heap_page_provider);

    CHECK(tagged_bmalloc_page_caches.large_heap_cache.provider == pas_mte_bootstrap_heap_page_provider);
    CHECK(tagged_bmalloc_page_caches.small_exclusive_segregated_megapage_cache.provider == pas_mte_bootstrap_heap_page_provider);
    CHECK(tagged_bmalloc_page_caches.small_other_megapage_cache.provider == pas_mte_bootstrap_heap_page_provider);
    CHECK(tagged_bmalloc_page_caches.medium_megapage_cache.provider == pas_mte_bootstrap_heap_page_provider);

    CHECK(!PAS_BOOTSTRAP_HEAP_PAGE_PROVIDER_FOR_FLAVOR(pas_memory_flavor_rwx));
}

#if PAS_ENABLE_MTE
// Returns whether the granule at ptr has MTE tag storage, by storing a non-zero tag to it and reading
// it back: on memory without tag storage, tag stores are ignored and tag loads read as zero. Leaves
// the granule's tag as zero. Must only be called when MTE is enabled.
bool hasMTETagStorage(void* ptr)
{
    static constexpr uintptr_t probeTag = 5;

    uintptr_t untagged = reinterpret_cast<uintptr_t>(ptr) & ~PAS_MTE_TAG_MASK;
    uintptr_t tagged = untagged | (probeTag << PAS_MTE_TAG_SHIFT);
    PAS_MTE_SET_TAG(tagged);

    uintptr_t loaded = untagged;
    PAS_MTE_GET_MTAG(loaded);

    PAS_MTE_SET_TAG(untagged);

    return ((loaded & PAS_MTE_TAG_MASK) >> PAS_MTE_TAG_SHIFT) == probeTag;
}
#endif

// Checks that every page of [begin, begin + size) has MTE tag storage exactly when memory of this
// flavor should.
void checkMTETagStorage(void* begin, size_t size, pas_memory_flavor flavor)
{
#if PAS_ENABLE_MTE
    pas_mte_ensure_initialized();
    if (!PAS_USE_MTE)
        return;
    bool expected = pas_memory_flavor_needs_mte_tag_storage(flavor);
    for (size_t offset = 0; offset < size; offset += pas_page_malloc_alignment())
        CHECK_EQUAL(hasMTETagStorage(static_cast<char*>(begin) + offset), expected);
#else
    PAS_UNUSED_PARAM(begin);
    PAS_UNUSED_PARAM(size);
    PAS_UNUSED_PARAM(flavor);
#endif
}

void testAllocateAndZeroFill(pas_memory_flavor flavor, size_t numPages)
{
    size_t size = numPages * pas_page_malloc_alignment();

    pas_aligned_allocation_result result = pas_page_malloc_try_allocate_without_deallocating_padding(
        size, pas_alignment_create_trivial(), flavor);
    CHECK(result.result);
    CHECK_EQUAL(result.result_size, size);
    CHECK_EQUAL(result.zero_mode, pas_zero_mode_is_all_zero);

    char* bytes = static_cast<char*>(result.result);
    for (size_t index = 0; index < size; ++index)
        CHECK_EQUAL(bytes[index], 0);
    checkMTETagStorage(bytes, size, flavor);

    memset(bytes, 42, size);

    pas_page_malloc_zero_fill(bytes, size, flavor);

    // Zero-filling must not change the memory's flavor; in particular, mte memory must keep its tag
    // storage.
    for (size_t index = 0; index < size; ++index)
        CHECK_EQUAL(bytes[index], 0);
    checkMTETagStorage(bytes, size, flavor);

    pas_page_malloc_deallocate(result.left_padding, result.left_padding_size + size + result.right_padding_size);
}

} // anonymous namespace

void addMemoryFlavorTests()
{
    ADD_TEST(testHeapConfigFlavors());
    ADD_TEST(testPageCachesUseHeapConfigFlavor());
    ADD_TEST(testAllocateAndZeroFill(pas_memory_flavor_plain, 1));
    ADD_TEST(testAllocateAndZeroFill(pas_memory_flavor_plain, 16));
    ADD_TEST(testAllocateAndZeroFill(pas_memory_flavor_mte, 1));
    ADD_TEST(testAllocateAndZeroFill(pas_memory_flavor_mte, 16));
}
