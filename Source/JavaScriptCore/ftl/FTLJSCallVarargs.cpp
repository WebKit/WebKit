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
#include "FTLJSCallVarargs.h"

#if ENABLE(FTL_JIT)

#include "DFGGraph.h"
#include "DFGNode.h"
#include "DFGOperations.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "ScratchRegisterAllocator.h"
#include "SetupVarargsFrame.h"

namespace JSC { namespace FTL {

using namespace DFG;

JSCallVarargs::JSCallVarargs()
    : m_stackmapID(UINT_MAX)
    , m_node(nullptr)
    , m_instructionOffset(UINT_MAX)
{
}

JSCallVarargs::JSCallVarargs(unsigned stackmapID, Node* node)
    : m_stackmapID(stackmapID)
    , m_node(node)
    , m_callBase(
        node->op() == ConstructVarargs ? CallLinkInfo::ConstructVarargs : CallLinkInfo::CallVarargs,
        node->origin.semantic)
    , m_instructionOffset(0)
{
    ASSERT(node->op() == CallVarargs || node->op() == CallForwardVarargs || node->op() == ConstructVarargs);
}

unsigned JSCallVarargs::numSpillSlotsNeeded()
{
    return 4;
}

void JSCallVarargs::emit(CCallHelpers& jit, Graph& graph, int32_t spillSlotsOffset)
{
    // We are passed three pieces of information:
    // - The callee.
    // - The arguments object.
    // - The "this" value, if it's a constructor call.
    
    bool isCall = m_node->op() != ConstructVarargs;
    
    CallVarargsData* data = m_node->callVarargsData();
    
    GPRReg calleeGPR = GPRInfo::argumentGPR0;
    
    GPRReg argumentsGPR = InvalidGPRReg;
    GPRReg thisGPR = InvalidGPRReg;
    bool argumentsOnStack = false;
    
    switch (m_node->op()) {
    case CallVarargs:
        argumentsGPR = GPRInfo::argumentGPR1;
        thisGPR = GPRInfo::argumentGPR2;
        break;
    case CallForwardVarargs:
        thisGPR = GPRInfo::argumentGPR1;
        argumentsOnStack = true;
        break;
    case ConstructVarargs:
        argumentsGPR = GPRInfo::argumentGPR1;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    const unsigned calleeSpillSlot = 0;
    const unsigned argumentsSpillSlot = 1;
    const unsigned thisSpillSlot = 2;
    const unsigned stackPointerSpillSlot = 3;
    
    // Get some scratch registers.
    RegisterSet usedRegisters;
    usedRegisters.merge(RegisterSet::stackRegisters());
    usedRegisters.merge(RegisterSet::reservedHardwareRegisters());
    usedRegisters.merge(RegisterSet::calleeSaveRegisters());
    usedRegisters.set(calleeGPR);
    if (argumentsGPR != InvalidGPRReg)
        usedRegisters.set(argumentsGPR);
    if (thisGPR != InvalidGPRReg)
        usedRegisters.set(thisGPR);
    ScratchRegisterAllocator allocator(usedRegisters);
    GPRReg scratchGPR1 = allocator.allocateScratchGPR();
    GPRReg scratchGPR2 = allocator.allocateScratchGPR();
    GPRReg scratchGPR3 = allocator.allocateScratchGPR();
    if (argumentsOnStack)
        argumentsGPR = allocator.allocateScratchGPR();
    RELEASE_ASSERT(!allocator.numberOfReusedRegisters());
    
    auto loadArguments = [&] (bool clobbered) {
        if (argumentsOnStack) {
            jit.load64(
                CCallHelpers::addressFor(graph.machineArgumentsRegisterFor(m_node->origin.semantic)),
                argumentsGPR);
        } else if (clobbered) {
            jit.load64(
                CCallHelpers::addressFor(spillSlotsOffset + argumentsSpillSlot), argumentsGPR);
        }
    };
    
    auto computeUsedStack = [&] (GPRReg targetGPR, unsigned extra) {
        if (isARM64()) {
            // Have to do this the weird way because $sp on ARM64 means zero when used in a subtraction.
            jit.move(CCallHelpers::stackPointerRegister, targetGPR);
            jit.negPtr(targetGPR);
            jit.addPtr(GPRInfo::callFrameRegister, targetGPR);
        } else {
            jit.move(GPRInfo::callFrameRegister, targetGPR);
            jit.subPtr(CCallHelpers::stackPointerRegister, targetGPR);
        }
        if (extra)
            jit.subPtr(CCallHelpers::TrustedImm32(extra), targetGPR);
        jit.urshiftPtr(CCallHelpers::Imm32(3), targetGPR);
    };
    
    auto callWithExceptionCheck = [&] (void* callee) {
        jit.move(CCallHelpers::TrustedImmPtr(callee), GPRInfo::nonPreservedNonArgumentGPR);
        jit.call(GPRInfo::nonPreservedNonArgumentGPR);
        m_exceptions.append(jit.emitExceptionCheck(AssemblyHelpers::NormalExceptionCheck, AssemblyHelpers::FarJumpWidth));
    };
    
    loadArguments(false);

    if (isARM64()) {
        jit.move(CCallHelpers::stackPointerRegister, scratchGPR1);
        jit.storePtr(scratchGPR1, CCallHelpers::addressFor(spillSlotsOffset + stackPointerSpillSlot));
    } else
        jit.storePtr(CCallHelpers::stackPointerRegister, CCallHelpers::addressFor(spillSlotsOffset + stackPointerSpillSlot));
    
    // Attempt the forwarding fast path, if it's been requested.
    CCallHelpers::Jump haveArguments;
    if (m_node->op() == CallForwardVarargs) {
        // Do the horrific foo.apply(this, arguments) optimization.
        // FIXME: do this optimization at the IR level.
        
        CCallHelpers::JumpList slowCase;
        slowCase.append(jit.branchTest64(CCallHelpers::NonZero, argumentsGPR));
        
        computeUsedStack(scratchGPR2, 0);
        emitSetupVarargsFrameFastCase(jit, scratchGPR2, scratchGPR1, scratchGPR2, scratchGPR3, m_node->origin.semantic.inlineCallFrame, data->firstVarArgOffset, slowCase);
        
        jit.move(calleeGPR, GPRInfo::regT0);
        haveArguments = jit.jump();
        slowCase.link(&jit);
    }
    
    // Gotta spill the callee, arguments, and this because we will need them later and we will have some
    // calls that clobber them.
    jit.store64(calleeGPR, CCallHelpers::addressFor(spillSlotsOffset + calleeSpillSlot));
    if (!argumentsOnStack)
        jit.store64(argumentsGPR, CCallHelpers::addressFor(spillSlotsOffset + argumentsSpillSlot));
    if (isCall)
        jit.store64(thisGPR, CCallHelpers::addressFor(spillSlotsOffset + thisSpillSlot));
    
    unsigned extraStack = sizeof(CallerFrameAndPC) +
        WTF::roundUpToMultipleOf(stackAlignmentBytes(), 5 * sizeof(void*));
    computeUsedStack(scratchGPR1, 0);
    jit.subPtr(CCallHelpers::TrustedImm32(extraStack), CCallHelpers::stackPointerRegister);
    jit.setupArgumentsWithExecState(argumentsGPR, scratchGPR1, CCallHelpers::TrustedImm32(data->firstVarArgOffset));
    callWithExceptionCheck(bitwise_cast<void*>(operationSizeFrameForVarargs));
    
    jit.move(GPRInfo::returnValueGPR, scratchGPR1);
    computeUsedStack(scratchGPR2, extraStack);
    loadArguments(true);
    emitSetVarargsFrame(jit, scratchGPR1, false, scratchGPR2, scratchGPR2);
    jit.addPtr(CCallHelpers::TrustedImm32(-extraStack), scratchGPR2, CCallHelpers::stackPointerRegister);
    jit.setupArgumentsWithExecState(scratchGPR2, argumentsGPR, CCallHelpers::TrustedImm32(data->firstVarArgOffset), scratchGPR1);
    callWithExceptionCheck(bitwise_cast<void*>(operationSetupVarargsFrame));
    
    jit.move(GPRInfo::returnValueGPR, scratchGPR2);
    
    if (isCall)
        jit.load64(CCallHelpers::addressFor(spillSlotsOffset + thisSpillSlot), thisGPR);
    jit.load64(CCallHelpers::addressFor(spillSlotsOffset + calleeSpillSlot), GPRInfo::regT0);
    
    if (m_node->op() == CallForwardVarargs)
        haveArguments.link(&jit);
    
    jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CallerFrameAndPC)), scratchGPR2, CCallHelpers::stackPointerRegister);
    
    if (isCall)
        jit.store64(thisGPR, CCallHelpers::calleeArgumentSlot(0));
    
    // Henceforth we make the call. The base FTL call machinery expects the callee in regT0 and for the
    // stack frame to already be set up, which it is.
    jit.store64(GPRInfo::regT0, CCallHelpers::calleeFrameSlot(JSStack::Callee));
    
    m_callBase.emit(jit);
    
    // Undo the damage we've done.
    if (isARM64()) {
        GPRReg scratchGPRAtReturn = CCallHelpers::selectScratchGPR(GPRInfo::returnValueGPR);
        jit.loadPtr(CCallHelpers::addressFor(spillSlotsOffset + stackPointerSpillSlot), scratchGPRAtReturn);
        jit.move(scratchGPRAtReturn, CCallHelpers::stackPointerRegister);
    } else
        jit.loadPtr(CCallHelpers::addressFor(spillSlotsOffset + stackPointerSpillSlot), CCallHelpers::stackPointerRegister);
}

void JSCallVarargs::link(VM& vm, LinkBuffer& linkBuffer, CodeLocationLabel exceptionHandler)
{
    m_callBase.link(vm, linkBuffer);
    linkBuffer.link(m_exceptions, exceptionHandler);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

