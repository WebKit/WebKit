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
#include "medium.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <strings.h>

#include "mbmalloc.h"

using namespace std;

struct Object {
    double* p;
    size_t size;
};

void benchmark_medium(bool isParallel)
{
    size_t times = 1;

    size_t vmSize = 1ul * 1024 * 1024 * 1024;
    size_t objectSizeMin = 2 * 1024;
    size_t objectSizeMax = 8 * 1024;
    if (isParallel)
        vmSize /= cpuCount();

    size_t objectCount = vmSize / objectSizeMin;

    srandom(0); // For consistency between runs.

    for (size_t i = 0; i < times; ++i) {
        Object* objects = (Object*)mbmalloc(objectCount * sizeof(Object));
        bzero(objects, objectCount * sizeof(Object));

        for (size_t i = 0, remaining = vmSize; remaining > objectSizeMin; ++i) {
            size_t size = min(remaining, max(objectSizeMin, random() % objectSizeMax));
            objects[i] = { (double*)mbmalloc(size), size };
            bzero(objects[i].p, size);
            remaining -= size;
        }

        for (size_t i = 0; i < objectCount && objects[i].p; ++i)
            mbfree(objects[i].p, objects[i].size);

        mbfree(objects, objectCount * sizeof(Object));
    }
}
