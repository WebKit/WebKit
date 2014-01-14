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

#ifndef CopiedBlock_h
#define CopiedBlock_h

#include "BlockAllocator.h"
#include "CopyWorkList.h"
#include "HeapBlock.h"
#include "JSCJSValue.h"
#include "Options.h"
#include <wtf/Atomics.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace JSC {

class CopiedSpace;

class CopiedBlock : public HeapBlock<CopiedBlock> {
    friend class CopiedSpace;
    friend class CopiedAllocator;
public:
    static CopiedBlock* create(DeadBlock*);
    static CopiedBlock* createNoZeroFill(DeadBlock*);

    void pin();
    bool isPinned();

    bool isOld();
    bool isOversize();
    void didPromote();

    unsigned liveBytes();
    bool shouldReportLiveBytes(SpinLockHolder&, JSCell* owner);
    void reportLiveBytes(SpinLockHolder&, JSCell*, CopyToken, unsigned);
    void reportLiveBytesDuringCopying(unsigned);
    void didSurviveGC();
    void didEvacuateBytes(unsigned);
    bool shouldEvacuate();
    bool canBeRecycled();

    // The payload is the region of the block that is usable for allocations.
    char* payload();
    char* payloadEnd();
    size_t payloadCapacity();
    
    // The data is the region of the block that has been used for allocations.
    char* data();
    char* dataEnd();
    size_t dataSize();
    
    // The wilderness is the region of the block that is usable for allocations
    // but has not been so used.
    char* wilderness();
    char* wildernessEnd();
    size_t wildernessSize();
    
    size_t size();
    size_t capacity();

    static const size_t blockSize = 32 * KB;

    bool hasWorkList();
    CopyWorkList& workList();
    SpinLock& workListLock() { return m_workListLock; }

private:
    CopiedBlock(Region*);
    void zeroFillWilderness(); // Can be called at any time to zero-fill to the end of the block.

    void checkConsistency();

    SpinLock m_workListLock;
    OwnPtr<CopyWorkList> m_workList;

    size_t m_remaining;
    bool m_isPinned : 1;
    bool m_isOld : 1;
    unsigned m_liveBytes;
#ifndef NDEBUG
    unsigned m_liveObjects;
#endif
};

inline CopiedBlock* CopiedBlock::createNoZeroFill(DeadBlock* block)
{
    Region* region = block->region();
    return new(NotNull, block) CopiedBlock(region);
}

inline CopiedBlock* CopiedBlock::create(DeadBlock* block)
{
    CopiedBlock* newBlock = createNoZeroFill(block);
    newBlock->zeroFillWilderness();
    return newBlock;
}

inline void CopiedBlock::zeroFillWilderness()
{
#if USE(JSVALUE64)
    memset(wilderness(), 0, wildernessSize());
#else
    JSValue emptyValue;
    JSValue* limit = reinterpret_cast_ptr<JSValue*>(wildernessEnd());
    for (JSValue* currentValue = reinterpret_cast_ptr<JSValue*>(wilderness()); currentValue < limit; currentValue++)
        *currentValue = emptyValue;
#endif
}

inline CopiedBlock::CopiedBlock(Region* region)
    : HeapBlock<CopiedBlock>(region)
    , m_remaining(payloadCapacity())
    , m_isPinned(false)
    , m_isOld(false)
    , m_liveBytes(0)
#ifndef NDEBUG
    , m_liveObjects(0)
#endif
{
    m_workListLock.Init();
    ASSERT(is8ByteAligned(reinterpret_cast<void*>(m_remaining)));
}

inline void CopiedBlock::didSurviveGC()
{
    checkConsistency();
    m_liveBytes = 0;
#ifndef NDEBUG
    m_liveObjects = 0;
#endif
    m_isPinned = false;
    if (m_workList)
        m_workList.clear();
}

inline void CopiedBlock::didEvacuateBytes(unsigned bytes)
{
    ASSERT(m_liveBytes >= bytes);
    ASSERT(m_liveObjects);
    checkConsistency();
    m_liveBytes -= bytes;
#ifndef NDEBUG
    m_liveObjects--;
#endif
    checkConsistency();
}

inline bool CopiedBlock::canBeRecycled()
{
    checkConsistency();
    return !m_liveBytes;
}

inline bool CopiedBlock::shouldEvacuate()
{
    checkConsistency();
    return static_cast<double>(m_liveBytes) / static_cast<double>(payloadCapacity()) <= Options::minCopiedBlockUtilization();
}

inline void CopiedBlock::pin()
{
    m_isPinned = true;
    if (m_workList)
        m_workList.clear();
}

inline bool CopiedBlock::isPinned()
{
    return m_isPinned;
}

inline bool CopiedBlock::isOld()
{
    return m_isOld;
}

inline void CopiedBlock::didPromote()
{
    m_isOld = true;
}

inline bool CopiedBlock::isOversize()
{
    return region()->isCustomSize();
}

inline unsigned CopiedBlock::liveBytes()
{
    checkConsistency();
    return m_liveBytes;
}

inline char* CopiedBlock::payload()
{
    return reinterpret_cast<char*>(this) + ((sizeof(CopiedBlock) + 7) & ~7);
}

inline char* CopiedBlock::payloadEnd()
{
    return reinterpret_cast<char*>(this) + region()->blockSize();
}

inline size_t CopiedBlock::payloadCapacity()
{
    return payloadEnd() - payload();
}

inline char* CopiedBlock::data()
{
    return payload();
}

inline char* CopiedBlock::dataEnd()
{
    return payloadEnd() - m_remaining;
}

inline size_t CopiedBlock::dataSize()
{
    return dataEnd() - data();
}

inline char* CopiedBlock::wilderness()
{
    return dataEnd();
}

inline char* CopiedBlock::wildernessEnd()
{
    return payloadEnd();
}

inline size_t CopiedBlock::wildernessSize()
{
    return wildernessEnd() - wilderness();
}

inline size_t CopiedBlock::size()
{
    return dataSize();
}

inline size_t CopiedBlock::capacity()
{
    return region()->blockSize();
}

inline bool CopiedBlock::hasWorkList()
{
    return !!m_workList;
}

inline CopyWorkList& CopiedBlock::workList()
{
    return *m_workList;
}

inline void CopiedBlock::checkConsistency()
{
    ASSERT(!!m_liveBytes == !!m_liveObjects);
}

} // namespace JSC

#endif
