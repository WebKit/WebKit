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
#include <wtf/FastMalloc.h>

#include <cstdint>
#include <limits>
#include <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

static constexpr size_t alignment = 32;
static constexpr size_t sizesNotMultipleOfAlignment[] = { 760, 1112, 8208 };

static void expectAlignedAndWritable(void* memory, size_t size)
{
    EXPECT_EQ(reinterpret_cast<uintptr_t>(memory) % alignment, 0u);
    auto bytes = unsafeMakeSpan(static_cast<uint8_t*>(memory), size);
    memsetSpan(bytes, 0xA5);
    EXPECT_EQ(bytes.front(), 0xA5);
    EXPECT_EQ(bytes.back(), 0xA5);
}

TEST(WTF_FastMalloc, TryFastAlignedMallocSizeNotMultipleOfAlignment)
{
    for (size_t size : sizesNotMultipleOfAlignment) {
        void* memory = tryFastAlignedMalloc(alignment, size);
        EXPECT_NE(memory, nullptr) << "size " << size;
        if (!memory)
            continue;
        expectAlignedAndWritable(memory, size);
        fastFree(memory);
    }
}

TEST(WTF_FastMalloc, TryFastCompactAlignedMallocSizeNotMultipleOfAlignment)
{
    for (size_t size : sizesNotMultipleOfAlignment) {
        void* memory = tryFastCompactAlignedMalloc(alignment, size);
        EXPECT_NE(memory, nullptr) << "size " << size;
        if (!memory)
            continue;
        expectAlignedAndWritable(memory, size);
        fastFree(memory);
    }
}

TEST(WTF_FastMalloc, FastAlignedMallocSizeNotMultipleOfAlignment)
{
    for (size_t size : sizesNotMultipleOfAlignment) {
        void* memory = fastAlignedMalloc(alignment, size);
        expectAlignedAndWritable(memory, size);
        fastFree(memory);
    }
}

TEST(WTF_FastMalloc, TryFastAlignedMallocFailsWhenSizeRoundedUpToAlignmentOverflows)
{
    void* memory = tryFastAlignedMalloc(alignment, std::numeric_limits<size_t>::max() - 8);
    EXPECT_EQ(memory, nullptr);
    if (memory)
        fastFree(memory);
}

} // namespace TestWebKitAPI
