/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BlockAllocator.h"

#include "MarkedBlock.h"
#include <wtf/CurrentTime.h>

namespace JSC {

BlockAllocator::BlockAllocator()
    : m_numberOfFreeBlocks(0)
    , m_isCurrentlyAllocating(false)
    , m_blockFreeingThreadShouldQuit(false)
    , m_blockFreeingThread(createThread(blockFreeingThreadStartFunc, this, "JavaScriptCore::BlockFree"))
{
    ASSERT(m_blockFreeingThread);
}

BlockAllocator::~BlockAllocator()
{
    releaseFreeBlocks();
    {
        MutexLocker locker(m_freeBlockLock);
        m_blockFreeingThreadShouldQuit = true;
        m_freeBlockCondition.broadcast();
    }
    waitForThreadCompletion(m_blockFreeingThread);
}

void BlockAllocator::releaseFreeBlocks()
{
    while (true) {
        MarkedBlock* block;
        {
            MutexLocker locker(m_freeBlockLock);
            if (!m_numberOfFreeBlocks)
                block = 0;
            else {
                // FIXME: How do we know this is a MarkedBlock? It could be a CopiedBlock.
                block = static_cast<MarkedBlock*>(m_freeBlocks.removeHead());
                ASSERT(block);
                m_numberOfFreeBlocks--;
            }
        }
        
        if (!block)
            break;
        
        MarkedBlock::destroy(block);
    }
}

void BlockAllocator::waitForRelativeTimeWhileHoldingLock(double relative)
{
    if (m_blockFreeingThreadShouldQuit)
        return;
    m_freeBlockCondition.timedWait(m_freeBlockLock, currentTime() + relative);
}

void BlockAllocator::waitForRelativeTime(double relative)
{
    // If this returns early, that's fine, so long as it doesn't do it too
    // frequently. It would only be a bug if this function failed to return
    // when it was asked to do so.
    
    MutexLocker locker(m_freeBlockLock);
    waitForRelativeTimeWhileHoldingLock(relative);
}

void BlockAllocator::blockFreeingThreadStartFunc(void* blockAllocator)
{
    static_cast<BlockAllocator*>(blockAllocator)->blockFreeingThreadMain();
}

void BlockAllocator::blockFreeingThreadMain()
{
    while (!m_blockFreeingThreadShouldQuit) {
        // Generally wait for one second before scavenging free blocks. This
        // may return early, particularly when we're being asked to quit.
        waitForRelativeTime(1.0);
        if (m_blockFreeingThreadShouldQuit)
            break;
        
        if (m_isCurrentlyAllocating) {
            m_isCurrentlyAllocating = false;
            continue;
        }

        // Now process the list of free blocks. Keep freeing until half of the
        // blocks that are currently on the list are gone. Assume that a size_t
        // field can be accessed atomically.
        size_t currentNumberOfFreeBlocks = m_numberOfFreeBlocks;
        if (!currentNumberOfFreeBlocks)
            continue;
        
        size_t desiredNumberOfFreeBlocks = currentNumberOfFreeBlocks / 2;
        
        while (!m_blockFreeingThreadShouldQuit) {
            MarkedBlock* block;
            {
                MutexLocker locker(m_freeBlockLock);
                if (m_numberOfFreeBlocks <= desiredNumberOfFreeBlocks)
                    block = 0;
                else {
                    // FIXME: How do we know this is a MarkedBlock? It could be a CopiedBlock.
                    block = static_cast<MarkedBlock*>(m_freeBlocks.removeHead());
                    ASSERT(block);
                    m_numberOfFreeBlocks--;
                }
            }
            
            if (!block)
                break;
            
            MarkedBlock::destroy(block);
        }
    }
}

} // namespace JSC
