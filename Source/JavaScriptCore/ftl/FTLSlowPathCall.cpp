/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "FTLSlowPathCall.h"

#if ENABLE(FTL_JIT)

#include "CCallHelpers.h"
#include "FTLState.h"
#include "FTLThunks.h"
#include "GPRInfo.h"

namespace JSC { namespace FTL {

// This code relies on us being 64-bit. FTL is currently always 64-bit.
static constexpr size_t wordSize = 8;

SlowPathCallContext::SlowPathCallContext(
    ScalarRegisterSet originalUsedRegisters, CCallHelpers& jit, unsigned numArgs, GPRReg returnRegister, GPRReg indirectCallTargetRegister)
    : m_jit(jit)
    , m_numArgs(numArgs)
    , m_returnRegister(returnRegister)
{
    RegisterSetBuilder usedRegisters = originalUsedRegisters.toRegisterSet();
    // We don't care that you're using callee-save, stack, or hardware registers.
    usedRegisters.exclude(RegisterSetBuilder::stackRegisters());
    usedRegisters.exclude(RegisterSetBuilder::reservedHardwareRegisters());
    usedRegisters.exclude(RegisterSetBuilder::calleeSaveRegisters().includeWholeRegisterWidth());
        
    // The return register doesn't need to be saved.
    if (m_returnRegister != InvalidGPRReg)
        usedRegisters.remove(m_returnRegister);
        
    size_t stackBytesNeededForReturnAddress = wordSize;
        
    m_offsetToSavingArea =
        (std::max(m_numArgs, NUMBER_OF_ARGUMENT_REGISTERS) - NUMBER_OF_ARGUMENT_REGISTERS) * wordSize;
    
    RegisterSetBuilder callingConventionRegisters = m_callingConventionRegisters.toRegisterSet();
    for (unsigned i = std::min(NUMBER_OF_ARGUMENT_REGISTERS, numArgs); i--;)
        callingConventionRegisters.add(GPRInfo::toArgumentRegister(i), IgnoreVectors);
    callingConventionRegisters.merge(m_argumentRegisters.toRegisterSet());
    if (returnRegister != InvalidGPRReg)
        callingConventionRegisters.add(GPRInfo::returnValueGPR, IgnoreVectors);
    if (indirectCallTargetRegister != InvalidGPRReg)
        callingConventionRegisters.add(indirectCallTargetRegister, IgnoreVectors);
    callingConventionRegisters.filter(usedRegisters);
    m_callingConventionRegisters = callingConventionRegisters.buildScalarRegisterSet();
        
    unsigned numberOfCallingConventionRegisters =
        m_callingConventionRegisters.numberOfSetRegisters();
        
    size_t offsetToThunkSavingArea =
        m_offsetToSavingArea +
        numberOfCallingConventionRegisters * wordSize;
        
    m_stackBytesNeeded =
        offsetToThunkSavingArea +
        stackBytesNeededForReturnAddress +
        (usedRegisters.numberOfSetRegisters() - numberOfCallingConventionRegisters) * wordSize;
        
    m_stackBytesNeeded = (m_stackBytesNeeded + stackAlignmentBytes() - 1) & ~(stackAlignmentBytes() - 1);
        
    m_jit.subPtr(CCallHelpers::TrustedImm32(m_stackBytesNeeded), CCallHelpers::stackPointerRegister);
        
    // This relies on all calling convention registers also being temp registers.
    unsigned stackIndex = 0;
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        GPRReg reg = GPRInfo::toRegister(i);
        if (!m_callingConventionRegisters.contains(reg, IgnoreVectors))
            continue;
        m_jit.storePtr(reg, CCallHelpers::Address(CCallHelpers::stackPointerRegister, m_offsetToSavingArea + (stackIndex++) * wordSize));
        usedRegisters.remove(reg);
    }
    
    m_thunkSaveSet = usedRegisters.buildScalarRegisterSet();
    m_offset = offsetToThunkSavingArea;
}
    
SlowPathCallContext::~SlowPathCallContext()
{
    if (m_returnRegister != InvalidGPRReg)
        m_jit.move(GPRInfo::returnValueGPR, m_returnRegister);
    
    unsigned stackIndex = 0;
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        GPRReg reg = GPRInfo::toRegister(i);
        if (!m_callingConventionRegisters.contains(reg, IgnoreVectors))
            continue;
        m_jit.loadPtr(CCallHelpers::Address(CCallHelpers::stackPointerRegister, m_offsetToSavingArea + (stackIndex++) * wordSize), reg);
    }
    
    m_jit.addPtr(CCallHelpers::TrustedImm32(m_stackBytesNeeded), CCallHelpers::stackPointerRegister);
}

SlowPathCallKey SlowPathCallContext::keyWithTarget(CodePtr<CFunctionPtrTag> callTarget) const
{
    uint8_t numberOfUsedArgumentRegistersIfClobberingCheckIsEnabled = 0;
    if (UNLIKELY(Options::clobberAllRegsInFTLICSlowPath()))
        numberOfUsedArgumentRegistersIfClobberingCheckIsEnabled = std::min(NUMBER_OF_ARGUMENT_REGISTERS, m_numArgs);
    return SlowPathCallKey(m_thunkSaveSet, callTarget, numberOfUsedArgumentRegistersIfClobberingCheckIsEnabled, m_offset, 0);
}

SlowPathCallKey SlowPathCallContext::keyWithTarget(CCallHelpers::Address address) const
{
    uint8_t numberOfUsedArgumentRegistersIfClobberingCheckIsEnabled = 0;
    if (UNLIKELY(Options::clobberAllRegsInFTLICSlowPath()))
        numberOfUsedArgumentRegistersIfClobberingCheckIsEnabled = std::min(NUMBER_OF_ARGUMENT_REGISTERS, m_numArgs);
    return SlowPathCallKey(m_thunkSaveSet, nullptr, numberOfUsedArgumentRegistersIfClobberingCheckIsEnabled, m_offset, address.offset);
}

SlowPathCall SlowPathCallContext::makeCall(VM& vm, CodePtr<CFunctionPtrTag> callTarget)
{
    SlowPathCallKey key = keyWithTarget(callTarget);
    SlowPathCall result = SlowPathCall(m_jit.call(JITThunkPtrTag), key);

    m_jit.addLinkTask(
        [result, &vm] (LinkBuffer& linkBuffer) {
            MacroAssemblerCodeRef<JITThunkPtrTag> thunk =
                vm.ftlThunks->getSlowPathCallThunk(vm, result.key());

            linkBuffer.link(result.call(), CodeLocationLabel<JITThunkPtrTag>(thunk.code()));
        });
    
    return result;
}

SlowPathCall SlowPathCallContext::makeCall(VM& vm, CCallHelpers::Address callTarget)
{
    SlowPathCallKey key = keyWithTarget(callTarget);
    SlowPathCall result = SlowPathCall(m_jit.call(JITThunkPtrTag), key);

    m_jit.addLinkTask(
        [result, &vm] (LinkBuffer& linkBuffer) {
            MacroAssemblerCodeRef<JITThunkPtrTag> thunk =
                vm.ftlThunks->getSlowPathCallThunk(vm, result.key());

            linkBuffer.link(result.call(), CodeLocationLabel<JITThunkPtrTag>(thunk.code()));
        });

    return result;
}

CallSiteIndex callSiteIndexForCodeOrigin(State& state, CodeOrigin codeOrigin)
{
    if (codeOrigin)
        return state.jitCode->common.codeOrigins->addCodeOrigin(codeOrigin);
    return CallSiteIndex();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

