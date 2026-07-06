/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/FreeList.h>
#include <JavaScriptCore/MarkedBlock.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

template<typename Func>
ALWAYS_INLINE HeapCell* FreeList::allocateWithCellSize(const Func& slowPath, size_t cellSize)
{
    ASSERT(cellSize == m_cellSize);
    if (m_intervalStart < m_intervalEnd) {
        char* result = m_intervalStart;
        m_intervalStart += cellSize;
        return std::bit_cast<HeapCell*>(result);
    }
    if (m_block && findNextInterval(cellSize)) {
        char* result = m_intervalStart;
        m_intervalStart += cellSize;
        return std::bit_cast<HeapCell*>(result);
    }
    return slowPath();
}

template<typename Func>
void FreeList::consumeRemaining(const Func& func)
{
    while (true) {
        while (m_intervalStart < m_intervalEnd) {
            func(std::bit_cast<HeapCell*>(m_intervalStart));
            m_intervalStart += m_cellSize;
        }
        if (!(m_block && findNextInterval(m_cellSize)))
            break;
    }
    clear();
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
