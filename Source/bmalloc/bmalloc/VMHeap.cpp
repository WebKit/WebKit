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
#include "Line.h"
#include "PerProcess.h"
#include "SuperChunk.h"
#include "VMHeap.h"
#include <thread>

namespace bmalloc {

VMHeap::VMHeap()
    : m_largeObjects(Owner::VMHeap)
{
}

void VMHeap::grow()
{
    SuperChunk* superChunk = SuperChunk::create();
#if BOS(DARWIN)
    m_zone.addSuperChunk(superChunk);
#endif

    SmallChunk* smallChunk = superChunk->smallChunk();
    for (auto* it = smallChunk->begin(); it != smallChunk->end(); ++it)
        m_smallPages.push(it);

    MediumChunk* mediumChunk = superChunk->mediumChunk();
    for (auto* it = mediumChunk->begin(); it != mediumChunk->end(); ++it)
        m_mediumPages.push(it);

    LargeChunk* largeChunk = superChunk->largeChunk();
    LargeObject result(LargeObject::init(largeChunk).begin());
    BASSERT(result.size() == largeMax);
    m_largeObjects.insert(result);
}

} // namespace bmalloc
