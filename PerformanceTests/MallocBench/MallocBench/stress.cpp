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
#include "stress.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <stddef.h>
#include <vector>

#include "mbmalloc.h"

static const size_t kB = 1024;
static const size_t MB = kB * kB;

struct Object {
    Object(void* pointer, size_t size)
        : pointer(pointer)
        , size(size)
    {
    }

    void* pointer;
    size_t size;
};

class SizeStream {
public:
    SizeStream()
        : m_state(Small)
        , m_count(0)
    {
    }

    size_t next()
    {
        switch (m_state) {
        case Small: {
            if (++m_count == smallCount) {
                m_state = Medium;
                m_count = 0;
            }
            return random() % smallMax;
        }
                
        case Medium: {
            if (++m_count == mediumCount) {
                m_state = Large;
                m_count = 0;
            }
            return random() % mediumMax;
        }
                
        case Large: {
            if (++m_count == largeCount) {
                m_state = Small;
                m_count = 0;
            }
            return random() % largeMax;
        }
        }
    }

private:
    static const size_t smallCount = 1000;
    static const size_t smallMax = 16 * kB;

    static const size_t mediumCount = 100;
    static const size_t mediumMax = 512 * kB;
    
    static const size_t largeCount = 10;
    static const size_t largeMax = 4 * MB;

    enum { Small, Medium, Large } m_state;
    size_t m_count;
};

void benchmark_stress(bool isParallel)
{
    const size_t heapSize = 100 * MB;
    const size_t churnSize = .05 * heapSize;
    const size_t churnCount = 10000;
    
    srandom(1); // For consistency between runs.

    std::vector<Object> objects;
    
    SizeStream sizeStream;
    
    size_t lastSize = 0;
    for (size_t remaining = heapSize; remaining; remaining -= std::min(remaining, lastSize)) {
        lastSize = sizeStream.next();
        Object object(mbmalloc(lastSize), lastSize);
        objects.push_back(object);
    }
    
    for (size_t i = 0; i < churnCount; ++i) {
        std::vector<Object> objectsToFree;
        for (size_t remaining = churnSize; remaining; remaining -= std::min(remaining, lastSize)) {
            lastSize = sizeStream.next();
            Object object(mbmalloc(lastSize), lastSize);

            size_t index = random() % objects.size();
            objectsToFree.push_back(objects[index]);
            objects[index] = object;
        }

        for (auto& object : objectsToFree)
            mbfree(object.pointer, object.size);
    }
    
    for (auto& object : objects)
        mbfree(object.pointer, object.size);
}
