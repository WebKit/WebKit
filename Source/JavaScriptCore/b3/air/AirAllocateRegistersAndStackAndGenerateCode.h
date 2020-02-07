/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "AirLiveness.h"
#include "AirTmpMap.h"
#include <wtf/Nonmovable.h>

namespace JSC { 

class CCallHelpers;

namespace B3 { namespace Air {

class Code;

class GenerateAndAllocateRegisters {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONMOVABLE(GenerateAndAllocateRegisters);

    struct TmpData {
        StackSlot* spillSlot { nullptr };
        Reg reg;
    };

public:
    GenerateAndAllocateRegisters(Code&);

    void prepareForGeneration();
    void generate(CCallHelpers&);

private:
    void insertBlocksForFlushAfterTerminalPatchpoints();
    void release(Tmp, Reg);
    void flush(Tmp, Reg);
    void spill(Tmp, Reg);
    void alloc(Tmp, Reg, bool isDef);
    void freeDeadTmpsIfNeeded();
    bool assignTmp(Tmp&, Bank, bool isDef);
    void buildLiveRanges(UnifiedTmpLiveness&);
    bool isDisallowedRegister(Reg);

    void checkConsistency();

    Code& m_code;
    CCallHelpers* m_jit { nullptr };

    TmpMap<TmpData> m_map;

#if ASSERT_ENABLED
    Vector<Tmp> m_allTmps[numBanks];
#endif

    Vector<Reg> m_registers[numBanks];
    RegisterSet m_availableRegs[numBanks];
    size_t m_globalInstIndex;
    IndexMap<Reg, Tmp>* m_currentAllocation { nullptr };
    bool m_didAlreadyFreeDeadSlots;
    TmpMap<size_t> m_liveRangeEnd;
    RegisterSet m_namedUsedRegs;
    RegisterSet m_namedDefdRegs;
    RegisterSet m_allowedRegisters;
    std::unique_ptr<UnifiedTmpLiveness> m_liveness;

    struct PatchSpillData {
        MacroAssembler::Jump jump;
        MacroAssembler::Label continueLabel;
        HashMap<Tmp, Arg*> defdTmps;
    };

    HashMap<BasicBlock*, PatchSpillData> m_blocksAfterTerminalPatchForSpilling;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
