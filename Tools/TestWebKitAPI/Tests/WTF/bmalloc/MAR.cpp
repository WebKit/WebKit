/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if !USE(SYSTEM_MALLOC) && BUSE(LIBPAS) && OS(DARWIN)

#include <bmalloc/bmalloc.h>
#include <bmalloc/pas_mar_registry.h>
#include <cstring>
#include <vector>

namespace TestWebKitAPI {

static unsigned pageIndexOf(void* p)
{
    return static_cast<unsigned>((reinterpret_cast<uintptr_t>(p) >> PAS_MAR_PAGE_SHIFT) % PAS_MAR_PROBABILITY);
}

static void testZeroedAllocationAfterPriming(bool marEnabled)
{
    pas_mar_initialize();

    constexpr size_t size = 128;
    constexpr int warmup = 8192;
    constexpr int retries = 8192;

    // Warm up: prime many equally sized slots with 0xAB, then free them all so
    // their bytes stay at 0xAB in the backing memory. Subsequent allocations
    // at the same size will reuse these slots from bmalloc's free list.
    std::vector<void*> primed;
    primed.reserve(warmup);
    for (int i = 0; i < warmup; ++i) {
        void* p = bmalloc::api::tryZeroedMalloc(size, bmalloc::CompactAllocationMode::NonCompact);
        ASSERT_TRUE(p);
        WTF::memsetSpan(std::span { reinterpret_cast<uint8_t*>(p), size }, 0xAB);
        primed.push_back(p);
    }
    for (void* p : primed)
        bmalloc::api::free(p);

    // Discover the page bmalloc will actually serve subsequent allocations
    // from: ask it for one and see where it lands, then free that slot so
    // it goes back on the free list.
    void* probe = bmalloc::api::tryZeroedMalloc(size, bmalloc::CompactAllocationMode::NonCompact);
    ASSERT_TRUE(probe);
    const unsigned targetPage = pageIndexOf(probe);
    bmalloc::api::free(probe);

    pas_mar_enabled = marEnabled;
    pas_mar_qualifying_page_index = targetPage;

    // Reallocate and inspect. For every allocation that lands in the MAR qualifying page,
    // the MAR branch of bmalloc_try_allocate_zeroed_inline runs (when enabled).
    // If that branch fails to zero, we'll see the 0xAB pattern from the previous occupant.
    int runsInTargetPage = 0;
    for (int i = 0; i < retries; ++i) {
        void* buf = bmalloc::api::tryZeroedMalloc(size, bmalloc::CompactAllocationMode::NonCompact);
        std::span bufSpan { reinterpret_cast<uint8_t*>(buf), size };
        ASSERT_TRUE(buf);

        if (pageIndexOf(buf) == targetPage) {
            for (size_t j = 0; j < size; ++j) {
                uint8_t byte { };
                WTF::memcpySpan(std::span { &byte, 1 }, bufSpan.subspan(j, 1));
                EXPECT_EQ(byte, 0u) << " at offset " << j << " in iteration " << i;
            }
            ++runsInTargetPage;
        }

        bmalloc::api::free(buf);
    }

    // Fail loudly if we never exercised the target page. The test would be vacuous otherwise.
    EXPECT_GT(runsInTargetPage, 0);

    pas_mar_enabled = false;
}

TEST(bmalloc, MARZeroedAllocationWithMARDisabled)
{
    testZeroedAllocationAfterPriming(false);
}

TEST(bmalloc, MARZeroedAllocationWithMAREnabled)
{
    testZeroedAllocationAfterPriming(true);
}

} // namespace TestWebKitAPI

#endif
