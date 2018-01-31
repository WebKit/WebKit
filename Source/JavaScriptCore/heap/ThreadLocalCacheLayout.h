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

#pragma once

#include <wtf/FastMalloc.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace JSC {

class BlockDirectory;

// Each Heap has a ThreadLocalCacheLayout that helps us figure out how to allocate a ThreadLocalCache
// for that Heap.

class ThreadLocalCacheLayout {
    WTF_MAKE_NONCOPYABLE(ThreadLocalCacheLayout);
    WTF_MAKE_FAST_ALLOCATED;
    
public:
    ThreadLocalCacheLayout();
    ~ThreadLocalCacheLayout();

    // BlockDirectory calls this during creation and this fills in BlockDirectory::offset.
    void allocateOffset(BlockDirectory*);
    
    struct Snapshot {
        size_t size;
        Vector<BlockDirectory*> directories;
    };
    
    Snapshot snapshot();
    
    BlockDirectory* directory(unsigned offset);
    
private:
    Lock m_lock;
    size_t m_size { 0 };
    Vector<BlockDirectory*> m_directories;
};

} // namespace JSC

