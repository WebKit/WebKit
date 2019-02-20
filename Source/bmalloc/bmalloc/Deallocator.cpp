/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#include "BAssert.h"
#include "BInline.h"
#include "Chunk.h"
#include "Deallocator.h"
#include "Environment.h"
#include "Heap.h"
#include "Object.h"
#include "PerProcess.h"
#include <algorithm>
#include <cstdlib>
#include <sys/mman.h>

namespace bmalloc {

Deallocator::Deallocator(Heap& heap)
    : m_heap(heap)
{
    BASSERT(!PerProcess<Environment>::get()->isDebugHeapEnabled());
}

Deallocator::~Deallocator()
{
    scavenge();
}
    
void Deallocator::scavenge()
{
    std::unique_lock<Mutex> lock(Heap::mutex());

    processObjectLog(lock);
    m_heap.deallocateLineCache(lock, lineCache(lock));
}

void Deallocator::processObjectLog(std::unique_lock<Mutex>& lock)
{
    for (Object object : m_objectLog)
        m_heap.derefSmallLine(lock, object, lineCache(lock));
    m_objectLog.clear();
}

void Deallocator::deallocateSlowCase(void* object)
{
    if (!object)
        return;

    std::unique_lock<Mutex> lock(Heap::mutex());
    if (m_heap.isLarge(lock, object)) {
        m_heap.deallocateLarge(lock, object);
        return;
    }

    if (m_objectLog.size() == m_objectLog.capacity())
        processObjectLog(lock);

    m_objectLog.push(object);
}

} // namespace bmalloc
