/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "AlignedMemoryAllocator.h"

#include "BlockDirectory.h"
#include "Subspace.h"

namespace JSC { 

AlignedMemoryAllocator::AlignedMemoryAllocator() = default;

AlignedMemoryAllocator::~AlignedMemoryAllocator() = default;

void AlignedMemoryAllocator::addDirectoryWithEmptyBlocks(BlockDirectory* directory)
{
    ASSERT(directory->subspace()->alignedMemoryAllocator() == this);

    // Sweeping announces every block it finishes with, so the already-listed case is the common one
    // and skips the lock. Reading the flag unlocked is sound because directories join and leave this
    // list on the mutator, or on a sweeper holding the API lock, never from two threads at once. A
    // wrong read would cost reuse rather than correctness anyway: a directory left off the list is
    // simply not stealable from until its next empty block or the next prepareForAllocation re-adds
    // it. See the concurrency FIXME in LocalAllocator::tryAllocateWithoutCollecting before allocating
    // from a second thread.
    if (directory->m_isOnEmptyBlocksList.loadRelaxed())
        return;

    Locker locker { m_directoriesWithEmptyBlocksLock };
    if (directory->m_isOnEmptyBlocksList.loadRelaxed())
        return;
    directory->m_isOnEmptyBlocksList.storeRelaxed(true);
    directory->m_nextDirectoryWithEmptyBlocks = m_firstDirectoryWithEmptyBlocks;
    m_firstDirectoryWithEmptyBlocks = directory;
}

BlockDirectory* AlignedMemoryAllocator::takeDirectoryWithEmptyBlocks()
{
    Locker locker { m_directoriesWithEmptyBlocksLock };
    BlockDirectory* directory = m_firstDirectoryWithEmptyBlocks;
    if (!directory)
        return nullptr;
    m_firstDirectoryWithEmptyBlocks = directory->m_nextDirectoryWithEmptyBlocks;
    directory->m_nextDirectoryWithEmptyBlocks = nullptr;
    directory->m_isOnEmptyBlocksList.storeRelaxed(false);
    return directory;
}

MarkedBlock::Handle* AlignedMemoryAllocator::findEmptyBlockToSteal()
{
    // Popped before it is searched, not after: a directory that gains a block mid-search must be able
    // to put itself back on, which it cannot do while the search still claims it. Directories
    // announce themselves from under their own lock, so the list lock has to be dropped either way.
    while (BlockDirectory* directory = takeDirectoryWithEmptyBlocks()) {
        if (MarkedBlock::Handle* block = directory->findEmptyBlockToSteal()) {
            addDirectoryWithEmptyBlocks(directory);
            return block;
        }
    }
    return nullptr;
}

} // namespace JSC


