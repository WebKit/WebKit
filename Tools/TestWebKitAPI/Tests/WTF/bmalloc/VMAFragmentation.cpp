/*
 * Copyright (C) 2026 Anthropic PBC.
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

#if !USE(SYSTEM_MALLOC) && BUSE(LIBPAS) && OS(LINUX)

#include <cstdio>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

// The number of virtual memory areas the kernel tracks for this process.
static long countVirtualMemoryAreas()
{
    FILE* file = fopen("/proc/self/maps", "r");
    if (!file)
        return -1;
    long lines = 0;
    for (int character = getc_unlocked(file); character != EOF; character = getc_unlocked(file)) {
        if (character == '\n')
            ++lines;
    }
    fclose(file);
    return lines;
}

// Decommitting a scattered live set must not shred the heap into one VMA per chunk,
// or the process eventually hits vm.max_map_count and madvise() starts failing with a
// permanent EAGAIN. https://bugs.webkit.org/show_bug.cgi?id=319025
TEST(bmalloc, DecommitDoesNotFragmentVirtualMemoryMap)
{
    if (!WTF::isFastMallocEnabled())
        return;

    constexpr size_t chunkSize = 16 * KB;
    constexpr size_t chunkCount = 4000;
    constexpr long allowedGrowth = 512;

    // Other tests in this process may have left free memory behind; scavenge
    // first so that the baseline is a settled map.
    WTF::releaseFastMallocFreeMemory();
    long before = countVirtualMemoryAreas();
    ASSERT_GT(before, 0);

    Vector<void*> chunks(chunkCount);
    for (size_t i = 0; i < chunkCount; ++i) {
        auto* chunk = static_cast<char*>(WTF::fastMalloc(chunkSize));
        // Touch the memory so that it is actually committed.
        chunk[0] = static_cast<char>(i);
        chunk[chunkSize - 1] = static_cast<char>(i);
        chunks[i] = chunk;
    }

    for (size_t i = 0; i < chunkCount; i += 2) {
        WTF::fastFree(chunks[i]);
        chunks[i] = nullptr;
    }

    // Synchronous scavenge: this is what decommits the freed chunks.
    WTF::releaseFastMallocFreeMemory();

    long after = countVirtualMemoryAreas();
    ASSERT_GT(after, 0);
    EXPECT_LT(after - before, allowedGrowth);

    for (void* chunk : chunks) {
        if (chunk)
            WTF::fastFree(chunk);
    }
}

} // namespace TestWebKitAPI

#endif // !USE(SYSTEM_MALLOC) && BUSE(LIBPAS) && OS(LINUX)
