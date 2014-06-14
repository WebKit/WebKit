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
#include "DFGOSRExitCompilerCommon.h"

#if ENABLE(DFG_JIT)

#include "Arguments.h"
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
        clippedValue = 0; // Make some compilers, and mhahnenberg, happy.
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
        unsigned callBytecodeIndex = inlineCallFrame->caller.bytecodeIndex;
        CallLinkInfo* callLinkInfo =
            baselineCodeBlockForCaller->getCallLinkInfoForBytecodeIndex(callBytecodeIndex);
        RELEASE_ASSERT(callLinkInfo);
        
        void* jumpTarget = callLinkInfo->callReturnLocation.executableAddress();

        GPRReg callerFrameGPR;
        if (inlineCallFrame->caller.inlineCallFrame) {
            jit.addPtr(AssemblyHelpers::TrustedImm32(inlineCallFrame->caller.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT3);
            callerFrameGPR = GPRInfo::regT3;
        } else
            callerFrameGPR = GPRInfo::callFrameRegister;
        
#if USE(JSVALUE64)
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(baselineCodeBlock), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::CodeBlock)));
        if (!inlineCallFrame->isClosureCall)
            jit.store64(AssemblyHelpers::TrustedImm64(JSValue::encode(JSValue(inlineCallFrame->calleeConstant()->scope()))), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ScopeChain)));
        jit.store64(callerFrameGPR, AssemblyHelpers::addressForByteOffset(inlineCallFrame->callerFrameOffset()));
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(jumpTarget), AssemblyHelpers::addressForByteOffset(inlineCallFrame->returnPCOffset()));
        uint32_t locationBits = CallFrame::Location::encodeAsBytecodeOffset(codeOrigin.bytecodeIndex);
        jit.store32(AssemblyHelpers::TrustedImm32(locationBits), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        jit.store32(AssemblyHelpers::TrustedImm32(inlineCallFrame->arguments.size()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        if (!inlineCallFrame->isClosureCall)
            jit.store64(AssemblyHelpers::TrustedImm64(JSValue::encode(JSValue(inlineCallFrame->calleeConstant()))), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));
        
        // Leave the captured arguments in regT3.
        if (baselineCodeBlock->usesArguments())
            jit.loadPtr(AssemblyHelpers::addressFor(VirtualRegister(inlineCallFrame->stackOffset + unmodifiedArgumentsRegister(baselineCodeBlock->argumentsRegister()).offset())), GPRInfo::regT3);
#else // USE(JSVALUE64) // so this is the 32-bit part
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(baselineCodeBlock), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::CodeBlock)));
        jit.store32(AssemblyHelpers::TrustedImm32(JSValue::CellTag), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ScopeChain)));
        if (!inlineCallFrame->isClosureCall)
            jit.storePtr(AssemblyHelpers::TrustedImmPtr(inlineCallFrame->calleeConstant()->scope()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ScopeChain)));
        jit.storePtr(callerFrameGPR, AssemblyHelpers::addressForByteOffset(inlineCallFrame->callerFrameOffset()));
        jit.storePtr(AssemblyHelpers::TrustedImmPtr(jumpTarget), AssemblyHelpers::addressForByteOffset(inlineCallFrame->returnPCOffset()));
        Instruction* instruction = baselineCodeBlock->instructions().begin() + codeOrigin.bytecodeIndex;
        uint32_t locationBits = CallFrame::Location::encodeAsBytecodeInstruction(instruction);
        jit.store32(AssemblyHelpers::TrustedImm32(locationBits), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        jit.store32(AssemblyHelpers::TrustedImm32(inlineCallFrame->arguments.size()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::ArgumentCount)));
        jit.store32(AssemblyHelpers::TrustedImm32(JSValue::CellTag), AssemblyHelpers::tagFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));
        if (!inlineCallFrame->isClosureCall)
            jit.storePtr(AssemblyHelpers::TrustedImmPtr(inlineCallFrame->calleeConstant()), AssemblyHelpers::payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + JSStack::Callee)));

        // Leave the captured arguments in regT3.
        if (baselineCodeBlock->usesArguments())
            jit.loadPtr(AssemblyHelpers::payloadFor(VirtualRegister(inlineCallFrame->stackOffset + unmodifiedArgumentsRegister(baselineCodeBlock->argumentsRegister()).offset())), GPRInfo::regT3);
#endif // USE(JSVALUE64) // ending the #else part, so directly above is the 32-bit part
        
        if (baselineCodeBlock->usesArguments()) {
            AssemblyHelpers::Jump noArguments = jit.branchTestPtr(AssemblyHelpers::Zero, GPRInfo::regT3);
            jit.addPtr(AssemblyHelpers::TrustedImm32(inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT0);
            jit.storePtr(GPRInfo::regT0, AssemblyHelpers::Address(GPRInfo::regT3, Arguments::offsetOfRegisters()));
            noArguments.link(&jit);
        }
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
    AssemblyHelpers::Jump ownerNotMarkedOrAlreadyRemembered = jit.checkMarkByte(owner);

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

    ownerNotMarkedOrAlreadyRemembered.link(&jit);
}
#endif // ENABLE(GGC)

void adjustAndJumpToTarget(CCallHelpers& jit, const OSRExitBase& exit)
{
#if ENABLE(GGC) 
    // 11) Write barrier the owner executables because we're jumping into a different block.
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

ArgumentsRecoveryGenerator::ArgumentsRecoveryGenerator() { }
ArgumentsRecoveryGenerator::~ArgumentsRecoveryGenerator() { }

void ArgumentsRecoveryGenerator::generateFor(
    int operand, CodeOrigin codeOrigin, CCallHelpers& jit)
{
    // Find the right inline call frame.
    InlineCallFrame* inlineCallFrame = 0;
    for (InlineCallFrame* current = codeOrigin.inlineCallFrame;
         current;
         current = current->caller.inlineCallFrame) {
        if (current->stackOffset >= operand) {
            inlineCallFrame = current;
            break;
        }
    }

    if (!jit.baselineCodeBlockFor(inlineCallFrame)->usesArguments())
        return;
    VirtualRegister argumentsRegister = jit.baselineArgumentsRegisterFor(inlineCallFrame);
    if (m_didCreateArgumentsObject.add(inlineCallFrame).isNewEntry) {
        // We know this call frame optimized out an arguments object that
        // the baseline JIT would have created. Do that creation now.
#if USE(JSVALUE64)
        if (inlineCallFrame) {
            jit.addPtr(AssemblyHelpers::TrustedImm32(inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT0);
            jit.setupArguments(GPRInfo::regT0);
        } else
            jit.setupArgumentsExecState();
        jit.move(
            AssemblyHelpers::TrustedImmPtr(
                bitwise_cast<void*>(operationCreateArgumentsDuringOSRExit)),
            GPRInfo::nonArgGPR0);
        jit.call(GPRInfo::nonArgGPR0);
        jit.store64(GPRInfo::returnValueGPR, AssemblyHelpers::addressFor(argumentsRegister));
        jit.store64(
            GPRInfo::returnValueGPR,
            AssemblyHelpers::addressFor(unmodifiedArgumentsRegister(argumentsRegister)));
        jit.move(GPRInfo::returnValueGPR, GPRInfo::regT0); // no-op move on almost all platforms.
#else // USE(JSVALUE64) -> so the 32_64 part
        if (inlineCallFrame) {
            jit.setupArgumentsWithExecState(
                AssemblyHelpers::TrustedImmPtr(inlineCallFrame));
            jit.move(
                AssemblyHelpers::TrustedImmPtr(
                    bitwise_cast<void*>(operationCreateInlinedArgumentsDuringOSRExit)),
                GPRInfo::nonArgGPR0);
        } else {
            jit.setupArgumentsExecState();
            jit.move(
                AssemblyHelpers::TrustedImmPtr(
                    bitwise_cast<void*>(operationCreateArgumentsDuringOSRExit)),
                GPRInfo::nonArgGPR0);
        }
        jit.call(GPRInfo::nonArgGPR0);
        jit.store32(
            AssemblyHelpers::TrustedImm32(JSValue::CellTag),
            AssemblyHelpers::tagFor(argumentsRegister));
        jit.store32(
            GPRInfo::returnValueGPR,
            AssemblyHelpers::payloadFor(argumentsRegister));
        jit.store32(
            AssemblyHelpers::TrustedImm32(JSValue::CellTag),
            AssemblyHelpers::tagFor(unmodifiedArgumentsRegister(argumentsRegister)));
        jit.store32(
            GPRInfo::returnValueGPR,
            AssemblyHelpers::payloadFor(unmodifiedArgumentsRegister(argumentsRegister)));
        jit.move(GPRInfo::returnValueGPR, GPRInfo::regT0); // no-op move on almost all platforms.
#endif // USE(JSVALUE64)
    }

#if USE(JSVALUE64)
    jit.load64(AssemblyHelpers::addressFor(argumentsRegister), GPRInfo::regT0);
    jit.store64(GPRInfo::regT0, AssemblyHelpers::addressFor(operand));
#else // USE(JSVALUE64) -> so the 32_64 part
    jit.load32(AssemblyHelpers::payloadFor(argumentsRegister), GPRInfo::regT0);
    jit.store32(
        AssemblyHelpers::TrustedImm32(JSValue::CellTag),
        AssemblyHelpers::tagFor(operand));
    jit.store32(GPRInfo::regT0, AssemblyHelpers::payloadFor(operand));
#endif // USE(JSVALUE64)
}
    
} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

