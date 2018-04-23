/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "PerProcess.h"

#include "VMAllocate.h"
#include <stdio.h>

namespace bmalloc {

static constexpr unsigned tableSize = 100;
static constexpr bool verbose = false;

static Mutex s_mutex;

static char* s_bumpBase;
static size_t s_bumpOffset;
static size_t s_bumpLimit;

static PerProcessData* s_table[tableSize];

// Returns zero-filled memory by virtual of using vmAllocate.
static void* allocate(size_t size, size_t alignment)
{
    s_bumpOffset = roundUpToMultipleOf(alignment, s_bumpOffset);
    void* result = s_bumpBase + s_bumpOffset;
    s_bumpOffset += size;
    if (s_bumpOffset <= s_bumpLimit)
        return result;
    
    size_t allocationSize = vmSize(size);
    void* allocation = vmAllocate(allocationSize);
    s_bumpBase = static_cast<char*>(allocation);
    s_bumpOffset = 0;
    s_bumpLimit = allocationSize;
    return allocate(size, alignment);
}

PerProcessData* getPerProcessData(unsigned hash, const char* disambiguator, size_t size, size_t alignment)
{
    std::lock_guard<Mutex> lock(s_mutex);

    PerProcessData*& bucket = s_table[hash % tableSize];
    
    for (PerProcessData* data = bucket; data; data = data->next) {
        if (!strcmp(data->disambiguator, disambiguator)) {
            if (verbose)
                fprintf(stderr, "%d: Using existing %s\n", getpid(), disambiguator);
            RELEASE_BASSERT(data->size == size);
            RELEASE_BASSERT(data->alignment == alignment);
            return data;
        }
    }
    
    if (verbose)
        fprintf(stderr, "%d: Creating new %s\n", getpid(), disambiguator);
    void* rawDataPtr = allocate(sizeof(PerProcessData), std::alignment_of<PerProcessData>::value);
    PerProcessData* data = static_cast<PerProcessData*>(rawDataPtr);
    data->disambiguator = disambiguator;
    data->memory = allocate(size, alignment);
    data->size = size;
    data->alignment = alignment;
    data->next = bucket;
    bucket = data;
    return data;
}

} // namespace bmalloc

