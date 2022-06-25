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

#include "CPUCount.h"
#include "churn.h"
#include <memory>
#include <stddef.h>

#include "mbmalloc.h"

struct HeapDouble {
    void* operator new(size_t size) { return mbmalloc(size); }
    void operator delete(void* p, size_t size) { mbfree(p, size); }

    HeapDouble(double d) : value(d) { }
    const HeapDouble& operator+=(const HeapDouble& other) { value += other.value; return *this; }
    double value;
};

void benchmark_churn(CommandLine& commandLine)
{
    size_t times = 7000000;
    if (commandLine.isParallel())
        times /= cpuCount();

    auto total = std::unique_ptr<HeapDouble>(new HeapDouble(0.0));
    for (size_t i = 0; i < times; ++i) {
        auto heapDouble = std::unique_ptr<HeapDouble>(new HeapDouble(i));
        *total += *heapDouble;
    }
}
