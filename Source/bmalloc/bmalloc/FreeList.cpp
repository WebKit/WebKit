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

#include "FreeList.h"

#include "FreeListInlines.h"

#if !BUSE(LIBPAS)

namespace bmalloc {

FreeList::FreeList()
{
}

FreeList::~FreeList()
{
}

void FreeList::clear()
{
    *this = FreeList();
}

void FreeList::initializeList(FreeCell* head, uintptr_t secret, unsigned bytes)
{
    // It's *slightly* more optimal to use a scrambled head. It saves a register on the fast path.
    m_scrambledHead = FreeCell::scramble(head, secret);
    m_secret = secret;
    m_payloadEnd = nullptr;
    m_remaining = 0;
    m_originalSize = bytes;
}

void FreeList::initializeBump(char* payloadEnd, unsigned remaining)
{
    m_scrambledHead = 0;
    m_secret = 0;
    m_payloadEnd = payloadEnd;
    m_remaining = remaining;
    m_originalSize = remaining;
}

bool FreeList::contains(void* target) const
{
    if (m_remaining) {
        const void* start = (m_payloadEnd - m_remaining);
        const void* end = m_payloadEnd;
        return (start <= target) && (target < end);
    }

    FreeCell* candidate = head();
    while (candidate) {
        if (static_cast<void*>(candidate) == target)
            return true;
        candidate = candidate->next(m_secret);
    }

    return false;
}

} // namespace JSC

#endif
