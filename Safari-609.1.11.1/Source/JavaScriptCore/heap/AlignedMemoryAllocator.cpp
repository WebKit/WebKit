/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "Heap.h"
#include "Subspace.h"

namespace JSC { 

AlignedMemoryAllocator::AlignedMemoryAllocator()
{
}

AlignedMemoryAllocator::~AlignedMemoryAllocator()
{
}

void AlignedMemoryAllocator::registerDirectory(Heap& heap, BlockDirectory* directory)
{
    RELEASE_ASSERT(!directory->nextDirectoryInAlignedMemoryAllocator());
    
    if (m_directories.isEmpty()) {
        ASSERT_UNUSED(heap, !Thread::mayBeGCThread() || heap.worldIsStopped());
        for (Subspace* subspace = m_subspaces.first(); subspace; subspace = subspace->nextSubspaceInAlignedMemoryAllocator())
            subspace->didCreateFirstDirectory(directory);
    }
    
    m_directories.append(std::mem_fn(&BlockDirectory::setNextDirectoryInAlignedMemoryAllocator), directory);
}

void AlignedMemoryAllocator::registerSubspace(Subspace* subspace)
{
    RELEASE_ASSERT(!subspace->nextSubspaceInAlignedMemoryAllocator());
    m_subspaces.append(std::mem_fn(&Subspace::setNextSubspaceInAlignedMemoryAllocator), subspace);
}

} // namespace JSC


