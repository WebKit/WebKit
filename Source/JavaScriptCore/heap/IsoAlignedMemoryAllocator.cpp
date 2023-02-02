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
#include "IsoAlignedMemoryAllocator.h"
#include "MarkedBlock.h"

namespace JSC {

IsoAlignedMemoryAllocator::IsoAlignedMemoryAllocator(CString name)
    : Base(name)
{
}

IsoAlignedMemoryAllocator::~IsoAlignedMemoryAllocator()
{
    releaseMemoryFromSubclassDestructor();
}

void IsoAlignedMemoryAllocator::dump(PrintStream& out) const
{
    out.print("Iso(", RawPointer(this), ")");
}

void* IsoAlignedMemoryAllocator::tryAllocateMemory(size_t size)
{
    return FastMalloc::tryMalloc(size);
}

void IsoAlignedMemoryAllocator::freeMemory(void* pointer)
{
    FastMalloc::free(pointer);
}

void* IsoAlignedMemoryAllocator::tryReallocateMemory(void*, size_t)
{
    // In IsoSubspace-managed PreciseAllocation, we must not perform realloc.
    RELEASE_ASSERT_NOT_REACHED();
}

void* IsoAlignedMemoryAllocator::tryMallocBlock()
{
    return tryFastAlignedMalloc(MarkedBlock::blockSize, MarkedBlock::blockSize);
}

void IsoAlignedMemoryAllocator::freeBlock(void* block)
{
    fastAlignedFree(block);
}

void IsoAlignedMemoryAllocator::commitBlock(void* block)
{
    WTF::fastCommitAlignedMemory(block, MarkedBlock::blockSize);
}

void IsoAlignedMemoryAllocator::decommitBlock(void* block)
{
    WTF::fastDecommitAlignedMemory(block, MarkedBlock::blockSize);
}

} // namespace JSC

