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

#include "BAssert.h"
#include "BeginTag.h"
#include "LargeChunk.h"
#include "Deallocator.h"
#include "Heap.h"
#include "Inline.h"
#include "PerProcess.h"
#include "SmallChunk.h"
#include "XSmallChunk.h"
#include <algorithm>
#include <sys/mman.h>

using namespace std;

namespace bmalloc {

Deallocator::Deallocator()
    : m_objectLog()
    , m_smallLineCache()
    , m_mediumLineCache()
{
}

Deallocator::~Deallocator()
{
    scavenge();
}
    
void Deallocator::scavenge()
{
    processObjectLog();
    
    std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
    Heap* heap = PerProcess<Heap>::getFastCase();
    
    while (m_xSmallLineCache.size())
        heap->deallocateXSmallLine(lock, m_xSmallLineCache.pop());
    while (m_smallLineCache.size())
        heap->deallocateSmallLine(lock, m_smallLineCache.pop());
    while (m_mediumLineCache.size())
        heap->deallocateMediumLine(lock, m_mediumLineCache.pop());
}

void Deallocator::deallocateLarge(void* object)
{
    std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
    PerProcess<Heap>::getFastCase()->deallocateLarge(lock, object);
}

void Deallocator::deallocateXLarge(void* object)
{
    std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
    PerProcess<Heap>::getFastCase()->deallocateXLarge(lock, object);
}

void Deallocator::processObjectLog()
{
    std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
    
    for (auto object : m_objectLog) {
        if (isXSmall(object)) {
            XSmallLine* line = XSmallLine::get(object);
            if (!line->deref(lock))
                continue;
            deallocateXSmallLine(lock, line);
        } else if (isSmall(object)) {
            SmallLine* line = SmallLine::get(object);
            if (!line->deref(lock))
                continue;
            deallocateSmallLine(lock, line);
        } else {
            BASSERT(isMedium(object));
            MediumLine* line = MediumLine::get(object);
            if (!line->deref(lock))
                continue;
            deallocateMediumLine(lock, line);
        }
    }
    
    m_objectLog.clear();
}

void Deallocator::deallocateSlowCase(void* object)
{
    BASSERT(!deallocateFastCase(object));

    if (!object)
        return;

    if (!isLarge(object)) {
        processObjectLog();
        m_objectLog.push(object);
        return;
    }

    BeginTag* beginTag = LargeChunk::beginTag(object);
    if (!beginTag->isXLarge())
        return deallocateLarge(object);
    
    return deallocateXLarge(object);
}

void Deallocator::deallocateSmallLine(std::lock_guard<Mutex>& lock, SmallLine* line)
{
    if (m_smallLineCache.size() == m_smallLineCache.capacity())
        return PerProcess<Heap>::getFastCase()->deallocateSmallLine(lock, line);

    m_smallLineCache.push(line);
}

void Deallocator::deallocateXSmallLine(std::lock_guard<Mutex>& lock, XSmallLine* line)
{
    if (m_xSmallLineCache.size() == m_xSmallLineCache.capacity())
        return PerProcess<Heap>::getFastCase()->deallocateXSmallLine(lock, line);

    m_xSmallLineCache.push(line);
}

SmallLine* Deallocator::allocateSmallLine()
{
    if (!m_smallLineCache.size()) {
        std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
        Heap* heap = PerProcess<Heap>::getFastCase();

        while (m_smallLineCache.size() != m_smallLineCache.capacity())
            m_smallLineCache.push(heap->allocateSmallLine(lock));
    }

    return m_smallLineCache.pop();
}

XSmallLine* Deallocator::allocateXSmallLine()
{
    if (!m_xSmallLineCache.size()) {
        std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
        Heap* heap = PerProcess<Heap>::getFastCase();

        while (m_xSmallLineCache.size() != m_xSmallLineCache.capacity())
            m_xSmallLineCache.push(heap->allocateXSmallLine(lock));
    }

    return m_xSmallLineCache.pop();
}

void Deallocator::deallocateMediumLine(std::lock_guard<Mutex>& lock, MediumLine* line)
{
    if (m_mediumLineCache.size() == m_mediumLineCache.capacity())
        return PerProcess<Heap>::getFastCase()->deallocateMediumLine(lock, line);

    m_mediumLineCache.push(line);
}

MediumLine* Deallocator::allocateMediumLine()
{
    if (!m_mediumLineCache.size()) {
        std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
        Heap* heap = PerProcess<Heap>::getFastCase();

        while (m_mediumLineCache.size() != m_mediumLineCache.capacity())
            m_mediumLineCache.push(heap->allocateMediumLine(lock));
    }

    return m_mediumLineCache.pop();
}

} // namespace bmalloc
