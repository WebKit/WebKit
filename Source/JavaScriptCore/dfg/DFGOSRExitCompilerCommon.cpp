/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "DFGOSRExitCompilerCommon.h"

#if ENABLE(DFG_JIT)

#include "DFGJITCode.h"
#include "DFGOperations.h"
#include "JIT.h"
#include "JSCJSValueInlines.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

void handleExitCounts(CCallHelpers& jit, const OSRExitBase& exit)
{
    jit.add32(AssemblyHelpers::TrustedImm32(1), AssemblyHelpers::AbsoluteAddress(&exit.m_count));
    
    jit.move(AssemblyHelpers::TrustedImmPtr(jit.codeBlock()), GPRInfo::regT0);
    
    AssemblyHelpers::Jump tooFewFails;
    
    jit.load32(AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfOSRExitCounter()), GPRInfo::regT2);
    jit.add32(AssemblyHelpers::TrustedImm32(1), GPRInfo::regT2);
    jit.store32(GPRInfo::regT2, AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfOSRExitCounter()));
    
    jit.move(AssemblyHelpers::TrustedImmPtr(jit.baselineCodeBlock()), GPRInfo::regT0);
    AssemblyHelpers::Jump reoptimizeNow = jit.branch32(
        AssemblyHelpers::GreaterThanOrEqual,
        AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfJITExecuteCounter()),
        AssemblyHelpers::TrustedImm32(0));
    
    // We want to figure out if there's a possibility that we're in a loop. For the outermost
    // code block in the inline stack, we handle this appropriately by having the loop OSR trigger
    // check the exit count of the replacement of the CodeBlock from which we are OSRing. The
    // problem is the inlined functions, which might also have loops, but whose baseline versions
    // don't know where to look for the exit count. Figure out if those loops are severe enough
    // that we had tried to OSR enter. If so, then we should use the loop reoptimization trigger.
    // Otherwise, we should use the normal reoptimization trigger.
    
    AssemblyHelpers::JumpList loopThreshold;
    
    for (InlineCallFrame* inlineCallFrame = exit.m_codeOrigin.inlineCallFrame; inlineCallFrame; inlineCallFrame = inlineCallFrame->caller.inlineCallFrame) {
        loopThreshold.append(
            jit.branchTest8(
                AssemblyHelpers::NonZero,
                AssemblyHelpers::AbsoluteAddress(
                    inlineCallFrame->executable->addressOfDidTryToEnterInLoop())));
    }
    
    jit.move(
        AssemblyHelpers::TrustedImm32(jit.codeBlock()->exitCountThresholdForReoptimization()),
        GPRInfo::regT1);
    
    if (!loopThreshold.empty()) {
        AssemblyHelpers::Jump done = jit.jump();

        loopThreshold.link(&jit);
        jit.move(
            AssemblyHelpers::TrustedImm32(
                jit.codeBlock()->exitCountThresholdForReoptimizationFromLoop()),
            GPRInfo::regT1);
        
        done.link(&jit);
    }
    
    tooFewFails = jit.branch32(AssemblyHelpers::BelowOrEqual, GPRInfo::regT2, GPRInfo::regT1);
    
    reoptimizeNow.link(&jit);
    
    // Reoptimize as soon as possible.
#if !NUMBER_OF_ARGUMENT_REGISTERS
    jit.poke(GPRInfo::regT0);
    jit.poke(AssemblyHelpers::TrustedImmPtr(&exit), 1);
#else
    jit.move(GPRInfo::regT0, GPRInfo::argumentGPR0);
    jit.move(AssemblyHelpers::TrustedImmPtr(&exit), GPRInfo::argumentGPR1);
#endif
    jit.move(AssemblyHelpers::TrustedImmPtr(bitwise_cast<void*>(triggerReoptimizationNow)), GPRInfo::nonArgGPR0);
    jit.call(GPRInfo::nonArgGPR0);
    AssemblyHelpers::Jump doneAdjusting = jit.jump();
    
    tooFewFails.link(&jit);
    
    // Adjust the execution counter such that the target is to only optimize after a while.
    int32_t activeThreshold =
        jit.baselineCodeBlock()->adjustedCounterValue(
            Options::thresholdForOptimizeAfterLongWarmUp());
    int32_t targetValue = applyMemoryUsageHeuristicsAndConvertToInt(
        activeThreshold, jit.baselineCodeBlock());
    int32_t clippedValue;
    switch (jit.codeBlock()->jitType()) {
    case JITCode::DFGJIT:
        clippedValue = BaselineExecutionCounter::clippedThreshold(jit.codeBlock()->globalObject(), targetValue);
        break;
    case JITCode::FTLJIT:
        clippedValue = UpperTierExecutionCounter::clippedThreshold(jit.codeBlock()->globalObject(), targetValue);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
        clippedValue = 0; // Make some compilers, and mhahnenberg, happy.
#endif
        break;
    }
    jit.store32(AssemblyHelpers::TrustedImm32(-clippedValue), AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfJITExecuteCounter()));
    jit.store32(AssemblyHelpers::TrustedImm32(activeThreshold), AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfJITExecutionActiveThreshold()));
    jit.store32(AssemblyHelpers::TrustedImm32(formattedTotalExecutionCount(clippedValue)), AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfJITExecutionTotalCount()));
    
    doneAdjusting.link(&jit);
}

void reifyInlinedCallFrames(CCallHelpers& jit, const OSRExitBase& exit)
{
    ASSERT(jit.baselineCodeBlock()->jitType() == JITCode::BaselineJIT);
    jit.storePtr(AssemblyHelpers::TrustedImmPtr(jit.baselineCodeBlock()), AssemblyHelpers::addressFor((VirtualRegister)JSStack::CodeBlock));

    CodeOrigin codeOrigin;
    for (codeOrigin = exit.m_codeOrigin; codeOrigin.inlineCallFrame; codeOrigin = codeOrigin.inlineCallFrame->caller) {
        InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame;
        CodeBlock* baselineCodeBlock = jit.baselineCodeBlockFor(codeOrigin);
        CodeBlock* baselineCodeBlockForCaller = jit.baselineCodeBlockFor(inlineCallFrame->caller);
        void* jumpTarget = nullptr;
        void* trueReturnPC = nullptr;
        
        unsigned callBytecodeIndex = inlineCallFrame->caller.bytecodeIndex;
        
        switch (inlineCallFrame->kind) {
        case InlineCallFrame::Call:
        case InlineCallFrame::Construct:
        case InlineCallFrame::CallVarargs:
        case InlineCallFrame::ConstructVarargs: {
            CallLinkInfo* callLinkInfo =
                baselineCodeBlockForCaller->getCallLinkInfoForBytecodeIndex(callBytecodeIndex);
            RELEASE_ASSERT(callLinkInfo);
            
            jumpTarget = callLinkInfo->callReturnLocation().executableAddress();
            break;
        }
            
        case InlineCallFrame::GetterCall:
        case InlineCallFrame::SetterCall: {
            StructureStubInfo* stubInfo =
                baselineCodeBlockForCaller->findStubInfo(CodeOrigin(callBytecodeIndex));
            RELEASE_ASSERT(stubInfo);
            
            switch (inlineCallFrame->kind) {
            case InlineCallFrame::GetterCall:
                jumpTarget = jit.vm()->getCTIStub(baselineGetterReturnThunkGenerator).code().executableAddress();
                break;
            case InlineCallFrame::SetterCall:
                jumpTarget = jit.vm()->getCTIStub(baselineSetterReturnThunkGenerator).code().executableAddress();
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            
            trueReturnPC = stubInfo->callReturnLocation.labelAtOffset(
                stubInfo->patch.deltaCallToDone).executableAddress();
            break;
        } }

        GPRReg callerFrameGPR;
        if (inlineCallFrame->caller.inlineCallFrame) {
            jit.addPtr(AssemblyHelpers::TrustedImm32(inlineCallFrame->caller.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT3);
            callerFrameGPR = GPRInfo::regT3;
        } else
            callerFrameGPR = GPRInfo::callFrameRegister;
        
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(jumpTarget), AssemblyHelpers::addressForByteOffset(inlineCallFrame->returnPCOffset()));
        if (trueReturnPC)
            jit.storePtr(AssemblyHelpers::TrustedImmPtr(trueReturnPC), AssemblyHelpers::addressFor(inlineCallFrame->stackOffset + virtualRegisterForArgument(inlineCallFrame->arguments.size()).offset()));
                         
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(baselineCodeBlock), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::CodeBlock)));
        if (!inlineCallFrame->isVarargs())
            jit.store32(AssemblyHelpers::TrustedImm32(inlineCallFrame->arguments.size()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
#if USE(JSVALUE64)
        jit.store64(callerFrameGPR, AssemblyHelpers::addressForByteOffset(inlineCallFrame->callerFrameOffset()));
        uint32_t locationBits = CallFrame::Location::encodeAsBytecodeOffset(codeOrigin.bytecodeIndex);
        jit.store32(AssemblyHelpers::TrustedImm32(locationBits), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        if (!inlineCallFrame->isClosureCall)
            jit.store64(AssemblyHelpers::TrustedImm64(JSValue::encode(JSValue(inlineCallFrame->calleeConstant()))), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));
#else // USE(JSVALUE64) // so this is the 32-bit part
        jit.storePtr(callerFrameGPR, AssemblyHelpers::addressForByteOffset(inlineCallFrame->callerFrameOffset()));
        Instruction* instruction = baselineCodeBlock->instructions().begin() + codeOrigin.bytecodeIndex;
        uint32_t locationBits = CallFrame::Location::encodeAsBytecodeInstruction(instruction);
        jit.store32(AssemblyHelpers::TrustedImm32(locationBits), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        jit.store32(AssemblyHelpers::TrustedImm32(JSValue::CellTag), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));
        if (!inlineCallFrame->isClosureCall)
            jit.storePtr(AssemblyHelpers::TrustedImmPtr(inlineCallFrame->calleeConstant()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));
#endif // USE(JSVALUE64) // ending the #else part, so directly above is the 32-bit part
    }

#if USE(JSVALUE64)
    uint32_t locationBits = CallFrame::Location::encodeAsBytecodeOffset(codeOrigin.bytecodeIndex);
#else
    Instruction* instruction = jit.baselineCodeBlock()->instructions().begin() + codeOrigin.bytecodeIndex;
    uint32_t locationBits = CallFrame::Location::encodeAsBytecodeInstruction(instruction);
#endif
    jit.store32(AssemblyHelpers::TrustedImm32(locationBits), AssemblyHelpers::tagFor((VirtualRegister)(JSStack::ArgumentCount)));
}

#if ENABLE(GGC)
static void osrWriteBarrier(CCallHelpers& jit, GPRReg owner, GPRReg scratch)
{
    AssemblyHelpers::Jump ownerIsRememberedOrInEden = jit.jumpIfIsRememberedOrInEden(owner);

    // We need these extra slots because setupArgumentsWithExecState will use poke on x86.
#if CPU(X86)
    jit.subPtr(MacroAssembler::TrustedImm32(sizeof(void*) * 3), MacroAssembler::stackPointerRegister);
#endif

    jit.setupArgumentsWithExecState(owner);
    jit.move(MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(operationOSRWriteBarrier)), scratch);
    jit.call(scratch);

#if CPU(X86)
    jit.addPtr(MacroAssembler::TrustedImm32(sizeof(void*) * 3), MacroAssembler::stackPointerRegister);
#endif

    ownerIsRememberedOrInEden.link(&jit);
}
#endif // ENABLE(GGC)

void adjustAndJumpToTarget(CCallHelpers& jit, const OSRExitBase& exit)
{
#if ENABLE(GGC) 
    jit.move(AssemblyHelpers::TrustedImmPtr(jit.codeBlock()->ownerExecutable()), GPRInfo::nonArgGPR0);
    osrWriteBarrier(jit, GPRInfo::nonArgGPR0, GPRInfo::nonArgGPR1);
    InlineCallFrameSet* inlineCallFrames = jit.codeBlock()->jitCode()->dfgCommon()->inlineCallFrames.get();
    if (inlineCallFrames) {
        for (InlineCallFrame* inlineCallFrame : *inlineCallFrames) {
            ScriptExecutable* ownerExecutable = inlineCallFrame->executable.get();
            jit.move(AssemblyHelpers::TrustedImmPtr(ownerExecutable), GPRInfo::nonArgGPR0); 
            osrWriteBarrier(jit, GPRInfo::nonArgGPR0, GPRInfo::nonArgGPR1);
        }
    }
#endif

    if (exit.m_codeOrigin.inlineCallFrame)
        jit.addPtr(AssemblyHelpers::TrustedImm32(exit.m_codeOrigin.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister);

    CodeBlock* baselineCodeBlock = jit.baselineCodeBlockFor(exit.m_codeOrigin);
    Vector<BytecodeAndMachineOffset>& decodedCodeMap = jit.decodedCodeMapFor(baselineCodeBlock);
    
    BytecodeAndMachineOffset* mapping = binarySearch<BytecodeAndMachineOffset, unsigned>(decodedCodeMap, decodedCodeMap.size(), exit.m_codeOrigin.bytecodeIndex, BytecodeAndMachineOffset::getBytecodeIndex);
    
    ASSERT(mapping);
    ASSERT(mapping->m_bytecodeIndex == exit.m_codeOrigin.bytecodeIndex);
    
    void* jumpTarget = baselineCodeBlock->jitCode()->executableAddressAtOffset(mapping->m_machineCodeOffset);

    jit.addPtr(AssemblyHelpers::TrustedImm32(JIT::stackPointerOffsetFor(baselineCodeBlock) * sizeof(Register)), GPRInfo::callFrameRegister, AssemblyHelpers::stackPointerRegister);
    
    jit.jitAssertTagsInPlace();

    jit.move(AssemblyHelpers::TrustedImmPtr(jumpTarget), GPRInfo::regT2);
    jit.jump(GPRInfo::regT2);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

