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

#include <assert.h>
#include "memalign.h"
#include <memory>
#include <stddef.h>
#include "CommandLine.h"

#include "mbmalloc.h"

void test(size_t alignment, size_t size)
{
    void* result = mbmemalign(alignment, size);

    assert(result);
    assert(!((uintptr_t)result & (alignment - 1)));
    
    mbfree(result, size);
}

void benchmark_memalign(CommandLine&)
{
    for (size_t alignment = 2; alignment < 4096; alignment *= 2) {
        for (size_t size = 0; size < 4096; ++size)
            test(alignment, size);
    }

    test(1 * 1024 * 1024, 8);
    test(8 * 1024 * 1024, 8);
    test(32 * 1024 * 1024, 8);
    test(64 * 1024 * 1024, 8);
    test(1 * 1024 * 1024, 8 * 1024 * 1024);
    test(1 * 1024 * 1024, 16 * 1024 * 1024);
}
