/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef CopiedAllocator_h
#define CopiedAllocator_h

#include "CopiedBlock.h"

namespace JSC {

class CopiedAllocator {
    friend class JIT;
public:
    CopiedAllocator();
    void* allocate(size_t);
    bool fitsInCurrentBlock(size_t);
    bool wasLastAllocation(void*, size_t);
    void startedCopying();
    void resetCurrentBlock(CopiedBlock*);
    size_t currentCapacity();

private:
    CopiedBlock* currentBlock() { return m_currentBlock; }

    char* m_currentOffset;
    CopiedBlock* m_currentBlock; 
};

inline CopiedAllocator::CopiedAllocator()
    : m_currentOffset(0)
    , m_currentBlock(0)
{
}

inline void* CopiedAllocator::allocate(size_t bytes)
{
    ASSERT(m_currentOffset);
    ASSERT(is8ByteAligned(reinterpret_cast<void*>(bytes)));
    ASSERT(fitsInCurrentBlock(bytes));
    void* ptr = static_cast<void*>(m_currentOffset);
    m_currentOffset += bytes;
    ASSERT(is8ByteAligned(ptr));
    return ptr;
}

inline bool CopiedAllocator::fitsInCurrentBlock(size_t bytes)
{
    return m_currentOffset + bytes < reinterpret_cast<char*>(m_currentBlock) + HeapBlock::s_blockSize && m_currentOffset + bytes > m_currentOffset;
}

inline bool CopiedAllocator::wasLastAllocation(void* ptr, size_t size)
{
    return static_cast<char*>(ptr) + size == m_currentOffset && ptr > m_currentBlock && ptr < reinterpret_cast<char*>(m_currentBlock) + HeapBlock::s_blockSize;
}

inline void CopiedAllocator::startedCopying()
{
    if (m_currentBlock)
        m_currentBlock->m_offset = static_cast<void*>(m_currentOffset);
    m_currentOffset = 0;
    m_currentBlock = 0;
}

inline void CopiedAllocator::resetCurrentBlock(CopiedBlock* newBlock)
{
    if (m_currentBlock)
        m_currentBlock->m_offset = static_cast<void*>(m_currentOffset);
    m_currentBlock = newBlock;
    m_currentOffset = static_cast<char*>(newBlock->m_offset);
}

inline size_t CopiedAllocator::currentCapacity()
{
    return m_currentBlock->capacity();
}

} // namespace JSC

#endif
