/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "GPRInfo.h"
#include "JSCInlines.h"

namespace JSC { namespace FTL {

namespace {

// This code relies on us being 64-bit. FTL is currently always 64-bit.
static const size_t wordSize = 8;

// This will be an RAII thingy that will set up the necessary stack sizes and offsets and such.
class CallContext {
public:
    CallContext(
        State& state, const RegisterSet& usedRegisters, CCallHelpers& jit,
        unsigned numArgs, GPRReg returnRegister)
        : m_state(state)
        , m_usedRegisters(usedRegisters)
        , m_jit(jit)
        , m_numArgs(numArgs)
        , m_returnRegister(returnRegister)
    {
        // We don't care that you're using callee-save, stack, or hardware registers.
        m_usedRegisters.exclude(RegisterSet::stackRegisters());
        m_usedRegisters.exclude(RegisterSet::reservedHardwareRegisters());
        m_usedRegisters.exclude(RegisterSet::calleeSaveRegisters());
        
        // The return register doesn't need to be saved.
        if (m_returnRegister != InvalidGPRReg)
            m_usedRegisters.clear(m_returnRegister);
        
        size_t stackBytesNeededForReturnAddress = wordSize;
        
        m_offsetToSavingArea =
            (std::max(m_numArgs, NUMBER_OF_ARGUMENT_REGISTERS) - NUMBER_OF_ARGUMENT_REGISTERS) * wordSize;
        
        for (unsigned i = std::min(NUMBER_OF_ARGUMENT_REGISTERS, numArgs); i--;)
            m_argumentRegisters.set(GPRInfo::toArgumentRegister(i));
        m_callingConventionRegisters.merge(m_argumentRegisters);
        if (returnRegister != InvalidGPRReg)
            m_callingConventionRegisters.set(GPRInfo::returnValueGPR);
        m_callingConventionRegisters.filter(m_usedRegisters);
        
        unsigned numberOfCallingConventionRegisters =
            m_callingConventionRegisters.numberOfSetRegisters();
        
        size_t offsetToThunkSavingArea =
            m_offsetToSavingArea +
            numberOfCallingConventionRegisters * wordSize;
        
        m_stackBytesNeeded =
            offsetToThunkSavingArea +
            stackBytesNeededForReturnAddress +
            (m_usedRegisters.numberOfSetRegisters() - numberOfCallingConventionRegisters) * wordSize;
        
        m_stackBytesNeeded = (m_stackBytesNeeded + stackAlignmentBytes() - 1) & ~(stackAlignmentBytes() - 1);
        
        m_jit.subPtr(CCallHelpers::TrustedImm32(m_stackBytesNeeded), CCallHelpers::stackPointerRegister);
        
        m_thunkSaveSet = m_usedRegisters;
        
        // This relies on all calling convention registers also being temp registers.
        unsigned stackIndex = 0;
        for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
            GPRReg reg = GPRInfo::toRegister(i);
            if (!m_callingConventionRegisters.get(reg))
                continue;
            m_jit.storePtr(reg, CCallHelpers::Address(CCallHelpers::stackPointerRegister, m_offsetToSavingArea + (stackIndex++) * wordSize));
            m_thunkSaveSet.clear(reg);
        }
        
        m_offset = offsetToThunkSavingArea;
    }
    
    ~CallContext()
    {
        if (m_returnRegister != InvalidGPRReg)
            m_jit.move(GPRInfo::returnValueGPR, m_returnRegister);
        
        unsigned stackIndex = 0;
        for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
            GPRReg reg = GPRInfo::toRegister(i);
            if (!m_callingConventionRegisters.get(reg))
                continue;
            m_jit.loadPtr(CCallHelpers::Address(CCallHelpers::stackPointerRegister, m_offsetToSavingArea + (stackIndex++) * wordSize), reg);
        }
        
        m_jit.addPtr(CCallHelpers::TrustedImm32(m_stackBytesNeeded), CCallHelpers::stackPointerRegister);
    }
    
    RegisterSet usedRegisters() const
    {
        return m_thunkSaveSet;
    }
    
    ptrdiff_t offset() const
    {
        return m_offset;
    }
    
    SlowPathCallKey keyWithTarget(void* callTarget) const
    {
        return SlowPathCallKey(usedRegisters(), callTarget, m_argumentRegisters, offset());
    }
    
    MacroAssembler::Call makeCall(void* callTarget, MacroAssembler::JumpList* exceptionTarget)
    {
        MacroAssembler::Call result = m_jit.call();
        m_state.finalizer->slowPathCalls.append(SlowPathCall(
            result, keyWithTarget(callTarget)));
        if (exceptionTarget)
            exceptionTarget->append(m_jit.emitExceptionCheck());
        return result;
    }
    
private:
    State& m_state;
    RegisterSet m_usedRegisters;
    RegisterSet m_argumentRegisters;
    RegisterSet m_callingConventionRegisters;
    CCallHelpers& m_jit;
    unsigned m_numArgs;
    GPRReg m_returnRegister;
    size_t m_offsetToSavingArea;
    size_t m_stackBytesNeeded;
    RegisterSet m_thunkSaveSet;
    ptrdiff_t m_offset;
};

} // anonymous namespace

void storeCodeOrigin(State& state, CCallHelpers& jit, CodeOrigin codeOrigin)
{
    if (!codeOrigin.isSet())
        return;
    
    unsigned index = state.jitCode->common.addCodeOrigin(codeOrigin);
    unsigned locationBits = CallFrame::Location::encodeAsCodeOriginIndex(index);
    jit.store32(
        CCallHelpers::TrustedImm32(locationBits),
        CCallHelpers::tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));
}

MacroAssembler::Call callOperation(
    State& state, const RegisterSet& usedRegisters, CCallHelpers& jit,
    CodeOrigin codeOrigin, MacroAssembler::JumpList* exceptionTarget,
    J_JITOperation_ESsiJI operation, GPRReg result, StructureStubInfo* stubInfo,
    GPRReg object, StringImpl* uid)
{
    storeCodeOrigin(state, jit, codeOrigin);
    CallContext context(state, usedRegisters, jit, 4, result);
    jit.setupArgumentsWithExecState(
        CCallHelpers::TrustedImmPtr(stubInfo), object,
        CCallHelpers::TrustedImmPtr(uid));
    return context.makeCall(bitwise_cast<void*>(operation), exceptionTarget);
}

MacroAssembler::Call callOperation(
    State& state, const RegisterSet& usedRegisters, CCallHelpers& jit, 
    CodeOrigin codeOrigin, MacroAssembler::JumpList* exceptionTarget,
    V_JITOperation_ESsiJJI operation, StructureStubInfo* stubInfo, GPRReg value,
    GPRReg object, StringImpl* uid)
{
    storeCodeOrigin(state, jit, codeOrigin);
    CallContext context(state, usedRegisters, jit, 5, InvalidGPRReg);
    jit.setupArgumentsWithExecState(
        CCallHelpers::TrustedImmPtr(stubInfo), value, object,
        CCallHelpers::TrustedImmPtr(uid));
    return context.makeCall(bitwise_cast<void*>(operation), exceptionTarget);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

