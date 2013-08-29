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
#include "DFGOSRExitCompilerCommon.h"

#if ENABLE(DFG_JIT)

#include "DFGOperations.h"
#include "JSCJSValueInlines.h"
#include "Operations.h"

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
        
    tooFewFails = jit.branch32(AssemblyHelpers::BelowOrEqual, GPRInfo::regT2, AssemblyHelpers::TrustedImm32(jit.codeBlock()->exitCountThresholdForReoptimization()));
    
    reoptimizeNow.link(&jit);
    
    // Reoptimize as soon as possible.
#if !NUMBER_OF_ARGUMENT_REGISTERS
    jit.poke(GPRInfo::regT0);
#else
    jit.move(GPRInfo::regT0, GPRInfo::argumentGPR0);
    ASSERT(GPRInfo::argumentGPR0 != GPRInfo::regT1);
#endif
    jit.move(AssemblyHelpers::TrustedImmPtr(bitwise_cast<void*>(triggerReoptimizationNow)), GPRInfo::regT1);
    jit.call(GPRInfo::regT1);
    AssemblyHelpers::Jump doneAdjusting = jit.jump();
    
    tooFewFails.link(&jit);
    
    // Adjust the execution counter such that the target is to only optimize after a while.
    int32_t activeThreshold =
        jit.baselineCodeBlock()->adjustedCounterValue(
            Options::thresholdForOptimizeAfterLongWarmUp());
    int32_t targetValue = ExecutionCounter::applyMemoryUsageHeuristicsAndConvertToInt(
        activeThreshold, jit.baselineCodeBlock());
    int32_t clippedValue =
        ExecutionCounter::clippedThreshold(jit.codeBlock()->globalObject(), targetValue);
    jit.store32(AssemblyHelpers::TrustedImm32(-clippedValue), AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfJITExecuteCounter()));
    jit.store32(AssemblyHelpers::TrustedImm32(activeThreshold), AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfJITExecutionActiveThreshold()));
    jit.store32(AssemblyHelpers::TrustedImm32(ExecutionCounter::formattedTotalCount(clippedValue)), AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfJITExecutionTotalCount()));
    
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
        unsigned callBytecodeIndex = inlineCallFrame->caller.bytecodeIndex;
        CallLinkInfo& callLinkInfo = baselineCodeBlockForCaller->getCallLinkInfo(callBytecodeIndex);
        
        void* jumpTarget = callLinkInfo.callReturnLocation.executableAddress();

#if USE(JSVALUE64)
        GPRReg callerFrameGPR;
        if (inlineCallFrame->caller.inlineCallFrame) {
            jit.addPtr(AssemblyHelpers::TrustedImm32(inlineCallFrame->caller.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT3);
            callerFrameGPR = GPRInfo::regT3;
        } else
            callerFrameGPR = GPRInfo::callFrameRegister;
        
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(baselineCodeBlock), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::CodeBlock)));
        if (!inlineCallFrame->isClosureCall())
            jit.store64(AssemblyHelpers::TrustedImm64(JSValue::encode(JSValue(inlineCallFrame->callee->scope()))), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ScopeChain)));
        jit.store64(callerFrameGPR, AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::CallerFrame)));
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(jumpTarget), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ReturnPC)));
        uint32_t locationBits = CallFrame::Location::encodeAsBytecodeOffset(codeOrigin.bytecodeIndex);
        jit.store32(AssemblyHelpers::TrustedImm32(locationBits), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        jit.store32(AssemblyHelpers::TrustedImm32(inlineCallFrame->arguments.size()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        if (!inlineCallFrame->isClosureCall())
            jit.store64(AssemblyHelpers::TrustedImm64(JSValue::encode(JSValue(inlineCallFrame->callee.get()))), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));
#else // USE(JSVALUE64) // so this is the 32-bit part
        GPRReg callerFrameGPR;
        if (inlineCallFrame->caller.inlineCallFrame) {
            jit.add32(AssemblyHelpers::TrustedImm32(inlineCallFrame->caller.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT3);
            callerFrameGPR = GPRInfo::regT3;
        } else
            callerFrameGPR = GPRInfo::callFrameRegister;
        
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(baselineCodeBlock), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::CodeBlock)));
        jit.store32(AssemblyHelpers::TrustedImm32(JSValue::CellTag), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ScopeChain)));
        if (!inlineCallFrame->isClosureCall())
            jit.storePtr(AssemblyHelpers::TrustedImmPtr(inlineCallFrame->callee->scope()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ScopeChain)));
        jit.store32(AssemblyHelpers::TrustedImm32(JSValue::CellTag), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::CallerFrame)));
        jit.storePtr(callerFrameGPR, AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::CallerFrame)));
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(jumpTarget), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ReturnPC)));
        Instruction* instruction = baselineCodeBlock->instructions().begin() + codeOrigin.bytecodeIndex;
        uint32_t locationBits = CallFrame::Location::encodeAsBytecodeInstruction(instruction);
        jit.store32(AssemblyHelpers::TrustedImm32(locationBits), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        jit.store32(AssemblyHelpers::TrustedImm32(inlineCallFrame->arguments.size()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        jit.store32(AssemblyHelpers::TrustedImm32(JSValue::CellTag), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));
        if (!inlineCallFrame->isClosureCall())
            jit.storePtr(AssemblyHelpers::TrustedImmPtr(inlineCallFrame->callee.get()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));
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

void adjustAndJumpToTarget(CCallHelpers& jit, const OSRExitBase& exit)
{
    if (exit.m_codeOrigin.inlineCallFrame)
        jit.addPtr(AssemblyHelpers::TrustedImm32(exit.m_codeOrigin.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister);

    CodeBlock* baselineCodeBlock = jit.baselineCodeBlockFor(exit.m_codeOrigin);
    Vector<BytecodeAndMachineOffset>& decodedCodeMap = jit.decodedCodeMapFor(baselineCodeBlock);
    
    BytecodeAndMachineOffset* mapping = binarySearch<BytecodeAndMachineOffset, unsigned>(decodedCodeMap, decodedCodeMap.size(), exit.m_codeOrigin.bytecodeIndex, BytecodeAndMachineOffset::getBytecodeIndex);
    
    ASSERT(mapping);
    ASSERT(mapping->m_bytecodeIndex == exit.m_codeOrigin.bytecodeIndex);
    
    void* jumpTarget = baselineCodeBlock->jitCode()->executableAddressAtOffset(mapping->m_machineCodeOffset);
    
    jit.move(AssemblyHelpers::TrustedImmPtr(jumpTarget), GPRInfo::regT2);
    jit.jump(GPRInfo::regT2);

#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLogF("   -> %p\n", jumpTarget);
#endif
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

