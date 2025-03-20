/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "B3Bank.h"
#include <wtf/DataLog.h>
#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>

namespace JSC { namespace B3 { namespace Air {

#define FOR_EACH_REGISTER_ALLOCATOR_STAT(macro) \
    macro(numTmpsIn)                            \
    macro(numUnspillableTmps)                   \
    macro(numSpillTmps)                         \
    macro(numSplitTmps)                         \
    macro(numTmpsOut)                           \
    macro(numSpillStackSlots)                   \
    macro(numLoadSpill)                         \
    macro(numStoreSpill)                        \
    macro(numMoveSpillSpillInsts)               \
    macro(numRematerializeConst)                \

class AirAllocateRegistersStats {
    WTF_MAKE_NONCOPYABLE(AirAllocateRegistersStats);
public:
    AirAllocateRegistersStats(Bank bank)
        : m_bank(bank) { }

    ~AirAllocateRegistersStats()
    {
        if (Options::airDumpRegAllocStats())
            dataLogLn("Register allocator stats for ", m_bank, " bank:", pointerDump(this));
    }

    void dump(PrintStream& out) const
    {
#define STAT_PRINT(name) out.print("\n   " #name ": ", name);
        FOR_EACH_REGISTER_ALLOCATOR_STAT(STAT_PRINT)
#undef STAT_PRINT
    }

#define STAT_DEF(name) unsigned name { 0 };
    FOR_EACH_REGISTER_ALLOCATOR_STAT(STAT_DEF)
#undef STAT_DEF

private:
    Bank m_bank;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
