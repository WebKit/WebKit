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

#include "BoundaryTagInlines.h"
#include "Heap.h"
#include "LargeChunk.h"
#include "Line.h"
#include "PerProcess.h"
#include <thread>

namespace bmalloc {

VMHeap::VMHeap()
{
}

void VMHeap::allocateXSmallChunk()
{
    XSmallChunk* chunk = XSmallChunk::create();
    for (auto* it = chunk->begin(); it != chunk->end(); ++it)
        m_xSmallPages.push(it);
}

void VMHeap::allocateSmallChunk()
{
    SmallChunk* chunk = SmallChunk::create();
    for (auto* it = chunk->begin(); it != chunk->end(); ++it)
        m_smallPages.push(it);
}

void VMHeap::allocateMediumChunk()
{
    MediumChunk* chunk = MediumChunk::create();
    for (auto* it = chunk->begin(); it != chunk->end(); ++it)
        m_mediumPages.push(it);
}

Range VMHeap::allocateLargeChunk()
{
    LargeChunk* chunk = LargeChunk::create();
    Range result = BoundaryTag::init(chunk);
    return result;
}

} // namespace bmalloc
