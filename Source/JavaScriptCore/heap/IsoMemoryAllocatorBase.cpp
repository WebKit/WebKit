/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IsoMemoryAllocatorBase.h"

#include "MarkedBlock.h"

namespace JSC {

IsoMemoryAllocatorBase::IsoMemoryAllocatorBase(CString name)
{
    UNUSED_PARAM(name);
}

IsoMemoryAllocatorBase::~IsoMemoryAllocatorBase()
{
}

// We need to call this from the derived class's destructor because it's undefined behavior to call pure virtual methods from within a destructor.
void IsoMemoryAllocatorBase::releaseMemoryFromSubclassDestructor()
{
#if !ENABLE(MALLOC_HEAP_BREAKDOWN)
    for (unsigned i = 0; i < m_blocks.size(); ++i) {
        void* block = m_blocks[i];
        if (!m_committed.quickGet(i))
            commitBlock(block);
        freeBlock(block);
    }
#endif
}

void* IsoMemoryAllocatorBase::tryAllocateAlignedMemory(size_t alignment, size_t size)
{
    // Since this is designed specially for IsoSubspace, we know that we will only be asked to
    // allocate MarkedBlocks.
    RELEASE_ASSERT(alignment == MarkedBlock::blockSize);
    RELEASE_ASSERT(size == MarkedBlock::blockSize);

    Locker locker { m_lock };
    
    m_firstUncommitted = m_committed.findBit(m_firstUncommitted, false);
    if (m_firstUncommitted < m_blocks.size()) {
        m_committed.quickSet(m_firstUncommitted);
        void* result = m_blocks[m_firstUncommitted];
        commitBlock(result);
        return result;
    }
    
    void* result = tryMallocBlock();
    if (!result)
        return nullptr;
    unsigned index = m_blocks.size();
    m_blocks.append(result);
    m_blockIndices.add(result, index);
    if (m_blocks.capacity() != m_committed.size())
        m_committed.resize(m_blocks.capacity());
    m_committed.quickSet(index);
    return result;
}

void IsoMemoryAllocatorBase::freeAlignedMemory(void* basePtr)
{
    Locker locker { m_lock };
    
    auto iter = m_blockIndices.find(basePtr);
    RELEASE_ASSERT(iter != m_blockIndices.end());
    unsigned index = iter->value;
    m_committed.quickClear(index);
    m_firstUncommitted = std::min(index, m_firstUncommitted);
    decommitBlock(basePtr);
}

} // namespace JSC

