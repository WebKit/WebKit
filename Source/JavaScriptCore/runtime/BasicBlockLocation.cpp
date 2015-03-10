/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Saam Barati. <saambarati1@gmail.com>
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
#include "BasicBlockLocation.h"

#include "CCallHelpers.h"
#include <climits>
#include <wtf/DataLog.h>

namespace JSC {

BasicBlockLocation::BasicBlockLocation(int startOffset, int endOffset)
    : m_startOffset(startOffset)
    , m_endOffset(endOffset)
    , m_hasExecuted(false)
{
}

void BasicBlockLocation::insertGap(int startOffset, int endOffset)
{
    std::pair<int, int> gap(startOffset, endOffset);
    if (!m_gaps.contains(gap))
        m_gaps.append(gap);
}

Vector<std::pair<int, int>> BasicBlockLocation::getExecutedRanges() const
{
    Vector<Gap> result;
    Vector<Gap> gaps = m_gaps;
    int nextRangeStart = m_startOffset;
    while (gaps.size()) {
        Gap minGap(INT_MAX, 0);
        unsigned minIdx = std::numeric_limits<unsigned>::max();
        for (unsigned idx = 0; idx < gaps.size(); idx++) {
            // Because we know that the Gaps inside m_gaps aren't enclosed within one another, it suffices to just check the first element to test ordering.
            if (gaps[idx].first < minGap.first) {
                minGap = gaps[idx];
                minIdx = idx;
            }
        }
        result.append(Gap(nextRangeStart, minGap.first - 1));
        nextRangeStart = minGap.second + 1;
        gaps.remove(minIdx);
    }

    result.append(Gap(nextRangeStart, m_endOffset));
    return result;
}

void BasicBlockLocation::dumpData() const
{
    Vector<Gap> executedRanges = getExecutedRanges();
    for (Gap gap : executedRanges)
        dataLogF("\tBasicBlock: [%d, %d] hasExecuted: %s\n", gap.first, gap.second, hasExecuted() ? "true" : "false");
}

#if ENABLE(JIT)
void BasicBlockLocation::emitExecuteCode(CCallHelpers& jit, MacroAssembler::RegisterID ptrReg) const
{
    jit.move(CCallHelpers::TrustedImmPtr(&m_hasExecuted), ptrReg);
    jit.store8(CCallHelpers::TrustedImm32(true), CCallHelpers::Address(ptrReg, 0));
}
#endif // ENABLE(JIT)

} // namespace JSC
