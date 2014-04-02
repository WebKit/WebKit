/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "Benchmark.h"
#include "CPUCount.h"
#include "balloon.h"
#include <array>
#include <chrono>
#include <memory>
#include <stddef.h>

#include "mbmalloc.h"

void benchmark_balloon(bool isParallel)
{
    const size_t chunkSize = 1 * 1024;
    const size_t balloonSize = 100 * 1024 * 1024;
    const size_t steadySize = 10 * 1024 * 1024;
    
    std::array<void*, balloonSize / chunkSize> balloon;
    std::array<void*, balloonSize / chunkSize> steady;

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < balloon.size(); ++i) {
        balloon[i] = mbmalloc(chunkSize);
        bzero(balloon[i], chunkSize);
    }

    for (size_t i = 0; i < balloon.size(); ++i)
        mbfree(balloon[i], chunkSize);

    auto stop = std::chrono::steady_clock::now();
    
    auto benchmarkTime = stop - start;

    start = std::chrono::steady_clock::now();

    // Converts bytes to time -- for reporting's sake -- by waiting a while until
    // the heap shrinks back down. This isn't great for pooling with other
    // benchmarks in a geometric mean of throughput, but it's OK for basic testing.
    while (Benchmark::currentMemoryBytes().resident > 2 * steadySize
        && std::chrono::steady_clock::now() - start < 8 * benchmarkTime) {
        for (size_t i = 0; i < steady.size(); ++i) {
            steady[i] = mbmalloc(chunkSize);
            bzero(steady[i], chunkSize);
        }

        for (size_t i = 0; i < steady.size(); ++i)
            mbfree(steady[i], chunkSize);
    }
}
