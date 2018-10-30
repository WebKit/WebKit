/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "Mutex.h"
#include <mutex>
#include <unordered_map>

#if BOS(DARWIN)
#include <malloc/malloc.h>
#endif

namespace bmalloc {
    
class DebugHeap {
public:
    DebugHeap(std::lock_guard<Mutex>&);
    
    void* malloc(size_t);
    void* memalign(size_t alignment, size_t, bool crashOnFailure);
    void* realloc(void*, size_t, bool crashOnFailure);
    void free(void*);
    
    void* memalignLarge(size_t alignment, size_t);
    void freeLarge(void* base);

private:
#if BOS(DARWIN)
    malloc_zone_t* m_zone;
#endif
    
    // This is the debug heap. We can use whatever data structures we like. It doesn't matter.
    size_t m_pageSize { 0 };
    std::mutex m_lock;
    std::unordered_map<void*, size_t> m_sizeMap;
};

} // namespace bmalloc
