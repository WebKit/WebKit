/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "LargeObject.h"
#include "PerProcess.h"
#include "SuperChunk.h"
#include "VMHeap.h"
#include <thread>

namespace bmalloc {

VMHeap::VMHeap()
    : m_largeObjects(VMState::HasPhysical::False)
{
}

void VMHeap::allocateSmallChunk(std::lock_guard<StaticMutex>& lock)
{
    if (!m_smallChunks.size())
        allocateSuperChunk(lock);

    // We initialize chunks lazily to avoid dirtying their metadata pages.
    SmallChunk* smallChunk = new (m_smallChunks.pop()->smallChunk()) SmallChunk(lock);
    for (auto* it = smallChunk->begin(); it < smallChunk->end(); ++it)
        m_smallRuns.push(it);
}

LargeObject VMHeap::allocateLargeChunk(std::lock_guard<StaticMutex>& lock)
{
    if (!m_largeChunks.size())
        allocateSuperChunk(lock);

    // We initialize chunks lazily to avoid dirtying their metadata pages.
    LargeChunk* largeChunk = new (m_largeChunks.pop()->largeChunk()) LargeChunk;
    return LargeObject(largeChunk->begin());
}

void VMHeap::allocateSuperChunk(std::lock_guard<StaticMutex>&)
{
    SuperChunk* superChunk =
        new (vmAllocate(superChunkSize, superChunkSize)) SuperChunk;
    m_smallChunks.push(superChunk);
    m_largeChunks.push(superChunk);
#if BOS(DARWIN)
    m_zone.addSuperChunk(superChunk);
#endif
}

} // namespace bmalloc
