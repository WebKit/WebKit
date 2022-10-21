/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "CallFrameShuffleData.h"

#if ENABLE(JIT)

#include "BaselineJITRegisters.h"
#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "RegisterAtOffsetList.h"

namespace JSC {

void CallFrameShuffleData::setupCalleeSaveRegisters(const RegisterAtOffsetList* registerSaveLocations)
{
    auto calleeSaveRegisters = RegisterSetBuilder::vmCalleeSaveRegisters();

    for (size_t i = 0; i < registerSaveLocations->registerCount(); ++i) {
        RegisterAtOffset entry { registerSaveLocations->at(i) };
        if (!calleeSaveRegisters.contains(entry.reg(), IgnoreVectors))
            continue;

        int saveSlotIndexInCPURegisters = entry.offsetAsIndex();

#if USE(JSVALUE64)
        // CPU registers are the same size as virtual registers
        VirtualRegister saveSlot { saveSlotIndexInCPURegisters };
        registers[entry.reg()]
            = ValueRecovery::displacedInJSStack(saveSlot, DataFormatJS);
#elif USE(JSVALUE32_64)
        // On 32-bit architectures, 2 callee saved GPRs may be packed into the same slot
        if (entry.reg().isGPR()) {
            static_assert(!PayloadOffset || !TagOffset);
            static_assert(PayloadOffset == 4 || TagOffset == 4);
            bool inTag = (saveSlotIndexInCPURegisters & 1) == !!TagOffset;
            if (saveSlotIndexInCPURegisters < 0)
                saveSlotIndexInCPURegisters -= 1; // Round towards -inf
            VirtualRegister saveSlot { saveSlotIndexInCPURegisters / 2 };
            registers[entry.reg()] = ValueRecovery::calleeSaveGPRDisplacedInJSStack(saveSlot, inTag);
        } else {
            ASSERT(!(saveSlotIndexInCPURegisters & 1)); // Should be at an even offset
            VirtualRegister saveSlot { saveSlotIndexInCPURegisters / 2 };
            registers[entry.reg()] = ValueRecovery::displacedInJSStack(saveSlot, DataFormatDouble);
        }
#endif
    }

    for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
        if (!calleeSaveRegisters.contains(reg, IgnoreVectors))
            continue;

        if (registers[reg])
            continue;

#if USE(JSVALUE64)
        registers[reg] = ValueRecovery::inRegister(reg, DataFormatJS);
#elif USE(JSVALUE32_64)
        registers[reg] = ValueRecovery::inRegister(reg, reg.isGPR() ? DataFormatInt32 : DataFormatDouble);
#endif
    }
}

CallFrameShuffleData CallFrameShuffleData::createForBaselineOrLLIntTailCall(const OpTailCall& bytecode, unsigned numParameters)
{
    CallFrameShuffleData shuffleData;
    shuffleData.numPassedArgs = bytecode.m_argc;
    shuffleData.numParameters = numParameters;
#if USE(JSVALUE64)
    shuffleData.numberTagRegister = GPRInfo::numberTagRegister;
#endif
    shuffleData.numLocals = bytecode.m_argv - sizeof(CallerFrameAndPC) / sizeof(Register);
    shuffleData.args.resize(bytecode.m_argc);
    for (unsigned i = 0; i < bytecode.m_argc; ++i) {
        shuffleData.args[i] =
            ValueRecovery::displacedInJSStack(
                virtualRegisterForArgumentIncludingThis(i) - bytecode.m_argv,
                DataFormatJS);
    }
#if USE(JSVALUE64)
    shuffleData.callee = ValueRecovery::inGPR(BaselineJITRegisters::Call::calleeJSR.payloadGPR(), DataFormatJS);
#elif USE(JSVALUE32_64)
    shuffleData.callee = ValueRecovery::inPair(BaselineJITRegisters::Call::calleeJSR.tagGPR(), BaselineJITRegisters::Call::calleeJSR.payloadGPR());
#endif
    shuffleData.setupCalleeSaveRegisters(&RegisterAtOffsetList::llintBaselineCalleeSaveRegisters());
    shuffleData.shrinkToFit();
    return shuffleData;
}

} // namespace JSC

#endif // ENABLE(JIT)
