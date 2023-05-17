/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "FreeList.h"

namespace JSC {

FreeList::FreeList(unsigned cellSize)
    : m_cellSize(cellSize)
{
}

FreeList::~FreeList()
{
}

void FreeList::clear()
{
    m_intervalStart = nullptr;
    m_intervalEnd = nullptr;
    m_nextInterval = bitwise_cast<FreeCell*>(static_cast<uintptr_t>(1));
    m_secret = 0;
    m_originalSize = 0;
}

void FreeList::initialize(FreeCell* start, uint64_t secret, unsigned bytes)
{
    if (UNLIKELY(!start)) {
        clear();
        return;
    }
    m_secret = secret;
    m_nextInterval = start;
    FreeCell::advance(m_secret, m_nextInterval, m_intervalStart, m_intervalEnd);
    m_originalSize = bytes;
}

bool FreeList::contains(HeapCell* target) const
{
    char* targetPtr = bitwise_cast<char*>(target);
    if (m_intervalStart <= targetPtr && targetPtr < m_intervalEnd)
        return true;

    FreeCell* candidate = nextInterval();
    while (!isSentinel(candidate)) {
        char* start;
        char* end;
        FreeCell::advance(m_secret, candidate, start, end);
        if (start <= targetPtr && targetPtr < end)
            return true;
    }

    return false;
}

void FreeList::dump(PrintStream& out) const
{
    out.print("{nextInterval = ", RawPointer(nextInterval()), ", secret = ", m_secret, ", intervalStart = ", RawPointer(m_intervalStart), ", intervalEnd = ", RawPointer(m_intervalEnd), ", originalSize = ", m_originalSize, "}");
}

} // namespace JSC

