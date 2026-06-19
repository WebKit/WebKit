/*
 * Copyright (C) 2026 Anthropic PBC. All rights reserved.
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

#include <wtf/Vector.h>

namespace TestWebKitAPI {

constexpr size_t overAlignment = 64;

struct alignas(overAlignment) OverAlignedFastAllocated {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(OverAlignedFastAllocated);
    char contents[176];
};

struct alignas(overAlignment) OverAlignedFastCompactAllocated {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_COMPACT_ALLOCATED(OverAlignedFastCompactAllocated);
    char contents[176];
};

struct alignas(overAlignment) OverAlignedConfigurableAllocated {
    WTF_MAKE_STRUCT_CONFIGURABLE_ALLOCATED(FastMalloc);
    char contents[176];
};

template<typename T>
static void testOverAlignedAllocations()
{
    Vector<T*> allocations;
    for (size_t i = 0; i < 5000; ++i) {
        T* pointer = new T;
        EXPECT_FALSE(reinterpret_cast<uintptr_t>(pointer) & (overAlignment - 1));
        allocations.append(pointer);
    }
    for (T* pointer : allocations)
        delete pointer;
}

TEST(WTF_FastMalloc, OverAlignedFastAllocated)
{
    testOverAlignedAllocations<OverAlignedFastAllocated>();
}

TEST(WTF_FastMalloc, OverAlignedFastCompactAllocated)
{
    testOverAlignedAllocations<OverAlignedFastCompactAllocated>();
}

TEST(WTF_FastMalloc, OverAlignedConfigurableAllocated)
{
    testOverAlignedAllocations<OverAlignedConfigurableAllocated>();
}

} // namespace TestWebKitAPI
