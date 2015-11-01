/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "CopiedBlock.h"

#include "JSCInlines.h"

namespace JSC {

static const bool computeBalance = false;
static size_t balance;

CopiedBlock* CopiedBlock::createNoZeroFill(Heap& heap, size_t capacity)
{
    if (computeBalance) {
        balance++;
        if (!(balance % 10))
            dataLog("CopiedBlock Balance: ", balance, "\n");
    }
    CopiedBlock* block = new (NotNull, fastAlignedMalloc(CopiedBlock::blockSize, capacity)) CopiedBlock(capacity);
    heap.didAllocateBlock(capacity);
    return block;
}

void CopiedBlock::destroy(Heap& heap, CopiedBlock* copiedBlock)
{
    if (computeBalance) {
        balance--;
        if (!(balance % 10))
            dataLog("CopiedBlock Balance: ", balance, "\n");
    }
    size_t capacity = copiedBlock->capacity();
    copiedBlock->~CopiedBlock();
    fastAlignedFree(copiedBlock);
    heap.didFreeBlock(capacity);
}

CopiedBlock* CopiedBlock::create(Heap& heap, size_t capacity)
{
    CopiedBlock* newBlock = createNoZeroFill(heap, capacity);
    newBlock->zeroFillWilderness();
    return newBlock;
}

void CopiedBlock::zeroFillWilderness()
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

CopiedBlock::CopiedBlock(size_t capacity)
    : DoublyLinkedListNode<CopiedBlock>()
    , m_capacity(capacity)
    , m_remaining(payloadCapacity())
    , m_isPinned(false)
    , m_isOld(false)
    , m_liveBytes(0)
#ifndef NDEBUG
    , m_liveObjects(0)
#endif
{
    ASSERT(is8ByteAligned(reinterpret_cast<void*>(m_remaining)));
}

} // namespace JSC
