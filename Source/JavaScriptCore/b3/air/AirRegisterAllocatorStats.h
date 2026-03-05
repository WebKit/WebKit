/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "AirPhaseStats.h"
#include "B3Bank.h"

namespace JSC { namespace B3 { namespace Air {

#define FOR_EACH_REGISTER_ALLOCATOR_STAT(macro) \
    macro(numTmpsIn)                            \
    macro(numFastTmps)                          \
    macro(numUnspillableTmps)                   \
    macro(numSpillTmps)                         \
    macro(numTmpsOut)                           \
    macro(numCoalescedRegisterMoves)            \
    macro(numCoalescedStackSlotMoves)           \
    macro(numCoalescedPinned)                   \
    macro(numSpilledTmps)                       \
    macro(numSpillStackSlots)                   \
    macro(numLoadSpill)                         \
    macro(numStoreSpill)                        \
    macro(numInPlaceSpill)                      \
    macro(numInPlaceSpillGiveUpSpillWidth)      \
    macro(numMoveSpillSpillInsts)               \
    macro(numRematerializeConst)                \
    macro(maxLiveRangeSize)                     \
    macro(maxLiveRangeIntervals)                \
    macro(didSpill)                             \
    macro(numSplitAroundClobbers)               \
    macro(numSplitAroundClobberSpilled)         \
    macro(numSplitIntraBlockNoCluster)          \
    macro(numSplitIntraBlock)                   \
    macro(numSplitIntraBlockClusterTmps)        \
    macro(numSplitIntraBlockClusterTmpsSpilled) \
    macro(numSplitIntraBlockLoad)               \
    macro(numSplitIntraBlockStore)              \
    macro(numInsts)                             \

class AirAllocateRegistersStats {
public:
    AirAllocateRegistersStats(Bank bank)
        : m_bank(bank) { }

    ASCIILiteral name() const
    {
        return m_bank == GP ? "RegAlloc<GP>"_s : "RegAlloc<FP>"_s;
    }

    DEFINE_PHASE_STATS(AirAllocateRegistersStats, FOR_EACH_REGISTER_ALLOCATOR_STAT)

private:
    Bank m_bank;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
