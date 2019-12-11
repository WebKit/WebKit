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

#include "FreeList.h"

namespace bmalloc {

template<typename Config, typename Func>
void* FreeList::allocate(const Func& slowPath)
{
    unsigned remaining = m_remaining;
    if (remaining) {
        remaining -= Config::objectSize;
        m_remaining = remaining;
        return m_payloadEnd - remaining - Config::objectSize;
    }
    
    FreeCell* result = head();
    if (!result)
        return slowPath();
    
    m_scrambledHead = result->scrambledNext;
    return result;
}

template<typename Config, typename Func>
void FreeList::forEach(const Func& func) const
{
    if (m_remaining) {
        for (unsigned remaining = m_remaining; remaining; remaining -= Config::objectSize)
            func(static_cast<void*>(m_payloadEnd - remaining));
    } else {
        for (FreeCell* cell = head(); cell;) {
            // We can use this to overwrite free objects before destroying the free list. So, we need
            // to get next before proceeding further.
            FreeCell* next = cell->next(m_secret);
            func(static_cast<void*>(cell));
            cell = next;
        }
    }
}

} // namespace bmalloc

