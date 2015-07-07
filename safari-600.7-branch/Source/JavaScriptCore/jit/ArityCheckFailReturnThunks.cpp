/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "ArityCheckFailReturnThunks.h"

#if ENABLE(JIT)

#include "AssemblyHelpers.h"
#include "LinkBuffer.h"
#include "JSCInlines.h"
#include "StackAlignment.h"

namespace JSC {

ArityCheckFailReturnThunks::ArityCheckFailReturnThunks()
    : m_nextSize(0)
{
}

ArityCheckFailReturnThunks::~ArityCheckFailReturnThunks() { }

CodeLocationLabel* ArityCheckFailReturnThunks::returnPCsFor(
    VM& vm, unsigned numExpectedArgumentsIncludingThis)
{
    ASSERT(numExpectedArgumentsIncludingThis >= 1);
    
    numExpectedArgumentsIncludingThis = WTF::roundUpToMultipleOf(
        stackAlignmentRegisters(), numExpectedArgumentsIncludingThis);
    
    {
        ConcurrentJITLocker locker(m_lock);
        if (numExpectedArgumentsIncludingThis < m_nextSize)
            return m_returnPCArrays.last().get();
    }
    
    ASSERT(!isCompilationThread());
    
    numExpectedArgumentsIncludingThis = std::max(numExpectedArgumentsIncludingThis, m_nextSize * 2);
    
    AssemblyHelpers jit(&vm, 0);
    
    Vector<AssemblyHelpers::Label> labels;
    
    for (unsigned size = m_nextSize; size <= numExpectedArgumentsIncludingThis; size += stackAlignmentRegisters()) {
        labels.append(jit.label());
        
        jit.load32(
            AssemblyHelpers::Address(
                AssemblyHelpers::stackPointerRegister,
                (JSStack::ArgumentCount - JSStack::CallerFrameAndPCSize) * sizeof(Register) +
                PayloadOffset),
            GPRInfo::regT4);
        jit.add32(
            AssemblyHelpers::TrustedImm32(
                JSStack::CallFrameHeaderSize - JSStack::CallerFrameAndPCSize + size - 1),
            GPRInfo::regT4, GPRInfo::regT2);
        jit.lshift32(AssemblyHelpers::TrustedImm32(3), GPRInfo::regT2);
        jit.addPtr(AssemblyHelpers::stackPointerRegister, GPRInfo::regT2);
        jit.loadPtr(GPRInfo::regT2, GPRInfo::regT2);
        
        jit.addPtr(
            AssemblyHelpers::TrustedImm32(size * sizeof(Register)),
            AssemblyHelpers::stackPointerRegister);
        
        // Thunks like ours want to use the return PC to figure out where things
        // were saved. So, we pay it forward.
        jit.store32(
            GPRInfo::regT4,
            AssemblyHelpers::Address(
                AssemblyHelpers::stackPointerRegister,
                (JSStack::ArgumentCount - JSStack::CallerFrameAndPCSize) * sizeof(Register) +
                PayloadOffset));
        
        jit.jump(GPRInfo::regT2);
    }
    
    LinkBuffer linkBuffer(vm, jit, GLOBAL_THUNK_ID);
    
    unsigned returnPCsSize = numExpectedArgumentsIncludingThis / stackAlignmentRegisters() + 1;
    std::unique_ptr<CodeLocationLabel[]> returnPCs =
        std::make_unique<CodeLocationLabel[]>(returnPCsSize);
    for (unsigned size = 0; size <= numExpectedArgumentsIncludingThis; size += stackAlignmentRegisters()) {
        unsigned index = size / stackAlignmentRegisters();
        RELEASE_ASSERT(index < returnPCsSize);
        if (size < m_nextSize)
            returnPCs[index] = m_returnPCArrays.last()[index];
        else
            returnPCs[index] = linkBuffer.locationOf(labels[(size - m_nextSize) / stackAlignmentRegisters()]);
    }

    CodeLocationLabel* result = returnPCs.get();

    {
        ConcurrentJITLocker locker(m_lock);
        m_returnPCArrays.append(WTF::move(returnPCs));
        m_refs.append(FINALIZE_CODE(linkBuffer, ("Arity check fail return thunks for up to numArgs = %u", numExpectedArgumentsIncludingThis)));
        m_nextSize = numExpectedArgumentsIncludingThis + stackAlignmentRegisters();
    }
    
    return result;
}

CodeLocationLabel ArityCheckFailReturnThunks::returnPCFor(VM& vm, unsigned slotsToAdd)
{
    return returnPCsFor(vm, slotsToAdd)[slotsToAdd / stackAlignmentRegisters()];
}

} // namespace JSC

#endif // ENABLE(JIT)

