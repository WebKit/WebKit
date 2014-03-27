/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef CopiedBlockInlines_h
#define CopiedBlockInlines_h

#include "ClassInfo.h"
#include "CopiedBlock.h"
#include "Heap.h"
#include "MarkedBlock.h"

namespace JSC {
    
inline bool CopiedBlock::shouldReportLiveBytes(SpinLockHolder&, JSCell* owner)
{
    // We want to add to live bytes if the owner isn't part of the remembered set or
    // if this block was allocated during the last cycle. 
    // If we always added live bytes we would double count for elements in the remembered
    // set across collections. 
    // If we didn't always add live bytes to new blocks, we'd get too few.
    return !Heap::isRemembered(owner) || !m_isOld;
}

inline void CopiedBlock::reportLiveBytes(SpinLockHolder&, JSCell* owner, CopyToken token, unsigned bytes)
{
    checkConsistency();
#ifndef NDEBUG
    m_liveObjects++;
#endif
    m_liveBytes += bytes;
    checkConsistency();
    ASSERT(m_liveBytes <= CopiedBlock::blockSize);

    if (isPinned())
        return;

    if (!shouldEvacuate()) {
        pin();
        return;
    }

    if (!m_workList)
        m_workList = adoptPtr(new CopyWorkList(Heap::heap(owner)->blockAllocator()));

    m_workList->append(CopyWorklistItem(owner, token));
}

inline void CopiedBlock::reportLiveBytesDuringCopying(unsigned bytes)
{
    checkConsistency();
    // This doesn't need to be locked because the thread that calls this function owns the current block.
    m_isOld = true;
#ifndef NDEBUG
    m_liveObjects++;
#endif
    m_liveBytes += bytes;
    checkConsistency();
    ASSERT(m_liveBytes <= CopiedBlock::blockSize);
}

} // namespace JSC

#endif // CopiedBlockInlines_h
