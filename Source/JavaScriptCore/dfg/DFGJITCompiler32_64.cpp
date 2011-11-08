/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Intel Corporation. All rights reserved.
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
#include "DFGJITCompiler.h"

#if ENABLE(DFG_JIT)
#if USE(JSVALUE32_64)

#include "CodeBlock.h"
#include "DFGJITCodeGenerator.h"
#include "DFGJITCompilerInlineMethods.h"
#include "DFGOperations.h"
#include "DFGRegisterBank.h"
#include "DFGSpeculativeJIT.h"
#include "JSGlobalData.h"
#include "LinkBuffer.h"

namespace JSC { namespace DFG {

void JITCompiler::exitSpeculativeWithOSR(const OSRExit& exit, SpeculationRecovery* recovery)
{
    // 1) Pro-forma stuff.
    exit.m_check.link(this);

#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "OSR exit for Node @%d (", (int)exit.m_nodeIndex);
    for (CodeOrigin codeOrigin = exit.m_codeOrigin; ; codeOrigin = codeOrigin.inlineCallFrame->caller) {
        fprintf(stderr, "bc#%u", codeOrigin.bytecodeIndex);
        if (!codeOrigin.inlineCallFrame)
            break;
        fprintf(stderr, " -> %p ", codeOrigin.inlineCallFrame->executable.get());
    }
    fprintf(stderr, ") at JIT offset 0x%x  ", debugOffset());
    exit.dump(stderr);
#endif
#if DFG_ENABLE(VERBOSE_SPECULATION_FAILURE)
    SpeculationFailureDebugInfo* debugInfo = new SpeculationFailureDebugInfo;
    debugInfo->codeBlock = m_codeBlock;
    debugInfo->debugOffset = debugOffset();
    
    debugCall(debugOperationPrintSpeculationFailure, debugInfo);
#endif
    
#if DFG_ENABLE(JIT_BREAK_ON_SPECULATION_FAILURE)
    breakpoint();
#endif
    
#if DFG_ENABLE(SUCCESS_STATS)
    static SamplingCounter counter("SpeculationFailure");
    emitCount(counter);
#endif

    // 2) Perform speculation recovery. This only comes into play when an operation
    //    starts mutating state before verifying the speculation it has already made.
    
    if (recovery) {
        switch (recovery->type()) {
        case SpeculativeAdd:
            sub32(recovery->src(), recovery->dest());
            break;
            
        case BooleanSpeculationCheck:
            break;
            
        default:
            break;
        }
    }

    // 3) Refine some value profile, if appropriate.
    
    if (!!exit.m_jsValueSource && !!exit.m_valueProfile) {
        if (exit.m_jsValueSource.isAddress()) {
            // Save a register so we can use it.
            GPRReg scratch = GPRInfo::regT0;
            if (scratch == exit.m_jsValueSource.base())
                scratch = GPRInfo::regT1;
            EncodedJSValue* scratchBuffer = static_cast<EncodedJSValue*>(globalData()->scratchBufferForSize(sizeof(uint32_t)));
            store32(scratch, scratchBuffer);
            load32(exit.m_jsValueSource.asAddress(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), scratch);
            store32(scratch, &bitwise_cast<EncodedValueDescriptor*>(exit.m_valueProfile->specFailBucket(0))->asBits.tag);
            load32(exit.m_jsValueSource.asAddress(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), scratch);
            store32(scratch, &bitwise_cast<EncodedValueDescriptor*>(exit.m_valueProfile->specFailBucket(0))->asBits.payload);
            load32(scratchBuffer, scratch);
        } else if (exit.m_jsValueSource.hasKnownTag()) {
            store32(Imm32(exit.m_jsValueSource.tag()), &bitwise_cast<EncodedValueDescriptor*>(exit.m_valueProfile->specFailBucket(0))->asBits.tag);
            store32(exit.m_jsValueSource.payloadGPR(), &bitwise_cast<EncodedValueDescriptor*>(exit.m_valueProfile->specFailBucket(0))->asBits.payload);
        } else {
            store32(exit.m_jsValueSource.tagGPR(), &bitwise_cast<EncodedValueDescriptor*>(exit.m_valueProfile->specFailBucket(0))->asBits.tag);
            store32(exit.m_jsValueSource.payloadGPR(), &bitwise_cast<EncodedValueDescriptor*>(exit.m_valueProfile->specFailBucket(0))->asBits.payload);
        }
    }
    
    // 4) Figure out how many scratch slots we'll need. We need one for every GPR/FPR
    //    whose destination is now occupied by a DFG virtual register, and we need
    //    one for every displaced virtual register if there are more than
    //    GPRInfo::numberOfRegisters of them. Also see if there are any constants,
    //    any undefined slots, any FPR slots, and any unboxed ints.
            
    Vector<bool> poisonedVirtualRegisters(exit.m_variables.size());
    for (unsigned i = 0; i < poisonedVirtualRegisters.size(); ++i)
        poisonedVirtualRegisters[i] = false;

    unsigned numberOfPoisonedVirtualRegisters = 0;
    unsigned numberOfDisplacedVirtualRegisters = 0;
    
    // Booleans for fast checks. We expect that most OSR exits do not have to rebox
    // Int32s, have no FPRs, and have no constants. If there are constants, we
    // expect most of them to be jsUndefined(); if that's true then we handle that
    // specially to minimize code size and execution time.
    bool haveUnboxedInt32InRegisterFile = false;
    bool haveUnboxedCellInRegisterFile = false;
    bool haveUnboxedBooleanInRegisterFile = false;
    bool haveFPRs = false;
    bool haveConstants = false;
    bool haveUndefined = false;
    
    for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
        const ValueRecovery& recovery = exit.valueRecovery(index);
        switch (recovery.technique()) {
        case DisplacedInRegisterFile:
            numberOfDisplacedVirtualRegisters++;
            ASSERT((int)recovery.virtualRegister() >= 0);
            
            // See if we might like to store to this virtual register before doing
            // virtual register shuffling. If so, we say that the virtual register
            // is poisoned: it cannot be stored to until after displaced virtual
            // registers are handled. We track poisoned virtual register carefully
            // to ensure this happens efficiently. Note that we expect this case
            // to be rare, so the handling of it is optimized for the cases in
            // which it does not happen.
            if (recovery.virtualRegister() < (int)exit.m_variables.size()) {
                switch (exit.m_variables[recovery.virtualRegister()].technique()) {
                case InGPR:
                case UnboxedInt32InGPR:
                case UnboxedBooleanInGPR:
                case InPair:
                case InFPR:
                    if (!poisonedVirtualRegisters[recovery.virtualRegister()]) {
                        poisonedVirtualRegisters[recovery.virtualRegister()] = true;
                        numberOfPoisonedVirtualRegisters++;
                    }
                    break;
                default:
                    break;
                }
            }
            break;
            
        case AlreadyInRegisterFileAsUnboxedInt32:
            haveUnboxedInt32InRegisterFile = true;
            break;
            
        case AlreadyInRegisterFileAsUnboxedCell:
            haveUnboxedCellInRegisterFile = true;
            break;
            
        case AlreadyInRegisterFileAsUnboxedBoolean:
            haveUnboxedBooleanInRegisterFile = true;
            break;
            
        case InFPR:
            haveFPRs = true;
            break;
            
        case Constant:
            haveConstants = true;
            if (recovery.constant().isUndefined())
                haveUndefined = true;
            break;
            
        default:
            break;
        }
    }
    
    EncodedJSValue* scratchBuffer = static_cast<EncodedJSValue*>(globalData()->scratchBufferForSize(sizeof(EncodedJSValue) * (numberOfPoisonedVirtualRegisters + ((numberOfDisplacedVirtualRegisters * 2) <= GPRInfo::numberOfRegisters ? 0 : numberOfDisplacedVirtualRegisters))));

    // From here on, the code assumes that it is profitable to maximize the distance
    // between when something is computed and when it is stored.
    
    // 5) Perform all reboxing of integers and cells, except for those in registers.

    if (haveUnboxedInt32InRegisterFile || haveUnboxedCellInRegisterFile || haveUnboxedBooleanInRegisterFile) {
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            switch (recovery.technique()) {
            case AlreadyInRegisterFileAsUnboxedInt32:
                store32(TrustedImm32(JSValue::Int32Tag), tagFor(static_cast<VirtualRegister>(exit.operandForIndex(index))));
                break;

            case AlreadyInRegisterFileAsUnboxedCell:
                store32(TrustedImm32(JSValue::CellTag), tagFor(static_cast<VirtualRegister>(exit.operandForIndex(index))));
                break;

            case AlreadyInRegisterFileAsUnboxedBoolean:
                store32(TrustedImm32(JSValue::BooleanTag), tagFor(static_cast<VirtualRegister>(exit.operandForIndex(index))));
                break;

            default:
                break;
            }
        }
    }

    // 6) Dump all non-poisoned GPRs. For poisoned GPRs, save them into the scratch storage.
    //    Note that GPRs do not have a fast change (like haveFPRs) because we expect that
    //    most OSR failure points will have at least one GPR that needs to be dumped.
    
    unsigned scratchIndex = 0;
    for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
        const ValueRecovery& recovery = exit.valueRecovery(index);
        int operand = exit.operandForIndex(index);
        switch (recovery.technique()) {
        case InGPR:
        case UnboxedInt32InGPR:
        case UnboxedBooleanInGPR:
            if (exit.isVariable(index) && poisonedVirtualRegisters[exit.variableForIndex(index)])
                store32(recovery.gpr(), reinterpret_cast<char*>(scratchBuffer + scratchIndex++) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
            else {
                uint32_t tag = JSValue::EmptyValueTag;
                if (recovery.technique() == InGPR)
                    tag = JSValue::CellTag;
                else if (recovery.technique() == UnboxedInt32InGPR)
                    tag = JSValue::Int32Tag;
                else
                    tag = JSValue::BooleanTag;
                store32(TrustedImm32(tag), tagFor((VirtualRegister)operand));
                store32(recovery.gpr(), payloadFor((VirtualRegister)operand));
            }
            break;
        case InPair:
            if (exit.isVariable(index) && poisonedVirtualRegisters[exit.variableForIndex(index)]) {
                store32(recovery.tagGPR(), reinterpret_cast<char*>(scratchBuffer + scratchIndex) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
                store32(recovery.payloadGPR(), reinterpret_cast<char*>(scratchBuffer + scratchIndex) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
                scratchIndex++;
            } else {
                store32(recovery.tagGPR(), tagFor((VirtualRegister)operand));
                store32(recovery.payloadGPR(), payloadFor((VirtualRegister)operand));
            }
            break;
        default:
            break;
        }
    }
    
    // At this point all GPRs are available for scratch use.
    
    if (haveFPRs) {
        // 7) Box all doubles (relies on there being more GPRs than FPRs)
        //    For JSValue32_64, no need to box doubles.
        
        // 8) Dump all doubles into the register file, or to the scratch storage if
        //    the destination virtual register is poisoned.
        
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            if (recovery.technique() != InFPR)
                continue;
            if (exit.isVariable(index) && poisonedVirtualRegisters[exit.variableForIndex(index)])
                storeDouble(recovery.fpr(), scratchBuffer + scratchIndex++);
            else
                storeDouble(recovery.fpr(), addressFor((VirtualRegister)exit.operandForIndex(index)));
        }
    }
    
    ASSERT(scratchIndex == numberOfPoisonedVirtualRegisters);
    
    // 9) Reshuffle displaced virtual registers. Optimize for the case that
    //    the number of displaced virtual registers is not more than the number
    //    of available physical registers.
    
    if (numberOfDisplacedVirtualRegisters) {
        if (numberOfDisplacedVirtualRegisters * 2 <= GPRInfo::numberOfRegisters) {
            // So far this appears to be the case that triggers all the time, but
            // that is far from guaranteed.
        
            unsigned displacementIndex = 0;
            for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
                const ValueRecovery& recovery = exit.valueRecovery(index);
                if (recovery.technique() != DisplacedInRegisterFile)
                    continue;
                load32(payloadFor(recovery.virtualRegister()), GPRInfo::toRegister(displacementIndex++));
                load32(tagFor(recovery.virtualRegister()), GPRInfo::toRegister(displacementIndex++));
            }
        
            displacementIndex = 0;
            for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
                const ValueRecovery& recovery = exit.valueRecovery(index);
                if (recovery.technique() != DisplacedInRegisterFile)
                    continue;
                store32(GPRInfo::toRegister(displacementIndex++), payloadFor((VirtualRegister)exit.operandForIndex(index)));
                store32(GPRInfo::toRegister(displacementIndex++), tagFor((VirtualRegister)exit.operandForIndex(index)));
            }
        } else {
            // FIXME: This should use the shuffling algorithm that we use
            // for speculative->non-speculative jumps, if we ever discover that
            // some hot code with lots of live values that get displaced and
            // spilled really enjoys frequently failing speculation.
        
            // For now this code is engineered to be correct but probably not
            // super. In particular, it correctly handles cases where for example
            // the displacements are a permutation of the destination values, like
            //
            // 1 -> 2
            // 2 -> 1
            //
            // It accomplishes this by simply lifting all of the virtual registers
            // from their old (DFG JIT) locations and dropping them in a scratch
            // location in memory, and then transferring from that scratch location
            // to their new (old JIT) locations.
        
            for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
                const ValueRecovery& recovery = exit.valueRecovery(index);
                if (recovery.technique() != DisplacedInRegisterFile)
                    continue;
                load32(payloadFor(recovery.virtualRegister()), GPRInfo::regT0);
                load32(tagFor(recovery.virtualRegister()), GPRInfo::regT1);
                store32(GPRInfo::regT0, reinterpret_cast<char*>(scratchBuffer + scratchIndex) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
                store32(GPRInfo::regT1, reinterpret_cast<char*>(scratchBuffer + scratchIndex) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
                scratchIndex++;
            }
        
            scratchIndex = numberOfPoisonedVirtualRegisters;
            for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
                const ValueRecovery& recovery = exit.valueRecovery(index);
                if (recovery.technique() != DisplacedInRegisterFile)
                    continue;
                load32(reinterpret_cast<char*>(scratchBuffer + scratchIndex) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload), GPRInfo::regT0);
                load32(reinterpret_cast<char*>(scratchBuffer + scratchIndex) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag), GPRInfo::regT1);
                store32(GPRInfo::regT0, payloadFor((VirtualRegister)exit.operandForIndex(index)));
                store32(GPRInfo::regT1, tagFor((VirtualRegister)exit.operandForIndex(index)));
                scratchIndex++;
            }
        
            ASSERT(scratchIndex == numberOfPoisonedVirtualRegisters + numberOfDisplacedVirtualRegisters);
        }
    }
    
    // 10) Dump all poisoned virtual registers.
    
    scratchIndex = 0;
    if (numberOfPoisonedVirtualRegisters) {
        for (int virtualRegister = 0; virtualRegister < (int)exit.m_variables.size(); ++virtualRegister) {
            if (!poisonedVirtualRegisters[virtualRegister])
                continue;
            
            const ValueRecovery& recovery = exit.m_variables[virtualRegister];
            switch (recovery.technique()) {
            case InGPR:
            case UnboxedInt32InGPR:
            case UnboxedBooleanInGPR: {
                load32(reinterpret_cast<char*>(scratchBuffer + scratchIndex++) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload), GPRInfo::regT0);
                store32(GPRInfo::regT0, payloadFor((VirtualRegister)virtualRegister));
                uint32_t tag = JSValue::EmptyValueTag;
                if (recovery.technique() == InGPR)
                    tag = JSValue::CellTag;
                else if (recovery.technique() == UnboxedInt32InGPR)
                    tag = JSValue::Int32Tag;
                else
                    tag = JSValue::BooleanTag;
                store32(TrustedImm32(tag), tagFor((VirtualRegister)virtualRegister));
                break;
            }

            case InFPR:
            case InPair:
                load32(reinterpret_cast<char*>(scratchBuffer + scratchIndex) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload), GPRInfo::regT0);
                load32(reinterpret_cast<char*>(scratchBuffer + scratchIndex) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag), GPRInfo::regT1);
                store32(GPRInfo::regT0, payloadFor((VirtualRegister)virtualRegister));
                store32(GPRInfo::regT1, tagFor((VirtualRegister)virtualRegister));
                scratchIndex++;
                break;
                
            default:
                break;
            }
        }
    }
    ASSERT(scratchIndex == numberOfPoisonedVirtualRegisters);
    
    // 11) Dump all constants. Optimize for Undefined, since that's a constant we see
    //     often.

    if (haveConstants) {
        if (haveUndefined) {
            move(TrustedImm32(jsUndefined().payload()), GPRInfo::regT0);
            move(TrustedImm32(jsUndefined().tag()), GPRInfo::regT1);
        }
        
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            if (recovery.technique() != Constant)
                continue;
            if (recovery.constant().isUndefined()) {
                store32(GPRInfo::regT0, payloadFor((VirtualRegister)exit.operandForIndex(index)));
                store32(GPRInfo::regT1, tagFor((VirtualRegister)exit.operandForIndex(index)));
            } else {
                store32(TrustedImm32(recovery.constant().payload()), payloadFor((VirtualRegister)exit.operandForIndex(index)));
                store32(TrustedImm32(recovery.constant().tag()), tagFor((VirtualRegister)exit.operandForIndex(index)));
            }
        }
    }
    
    // 12) Adjust the old JIT's execute counter. Since we are exiting OSR, we know
    //     that all new calls into this code will go to the new JIT, so the execute
    //     counter only affects call frames that performed OSR exit and call frames
    //     that were still executing the old JIT at the time of another call frame's
    //     OSR exit. We want to ensure that the following is true:
    //
    //     (a) Code the performs an OSR exit gets a chance to reenter optimized
    //         code eventually, since optimized code is faster. But we don't
    //         want to do such reentery too aggressively (see (c) below).
    //
    //     (b) If there is code on the call stack that is still running the old
    //         JIT's code and has never OSR'd, then it should get a chance to
    //         perform OSR entry despite the fact that we've exited.
    //
    //     (c) Code the performs an OSR exit should not immediately retry OSR
    //         entry, since both forms of OSR are expensive. OSR entry is
    //         particularly expensive.
    //
    //     (d) Frequent OSR failures, even those that do not result in the code
    //         running in a hot loop, result in recompilation getting triggered.
    //
    //     To ensure (c), we'd like to set the execute counter to
    //     counterValueForOptimizeAfterWarmUp(). This seems like it would endanger
    //     (a) and (b), since then every OSR exit would delay the opportunity for
    //     every call frame to perform OSR entry. Essentially, if OSR exit happens
    //     frequently and the function has few loops, then the counter will never
    //     become non-negative and OSR entry will never be triggered. OSR entry
    //     will only happen if a loop gets hot in the old JIT, which does a pretty
    //     good job of ensuring (a) and (b). But that doesn't take care of (d),
    //     since each speculation failure would reset the execute counter.
    //     So we check here if the number of speculation failures is significantly
    //     larger than the number of successes (we want 90% success rate), and if
    //     there have been a large enough number of failures. If so, we set the
    //     counter to 0; otherwise we set the counter to
    //     counterValueForOptimizeAfterWarmUp().
    
    move(TrustedImmPtr(codeBlock()), GPRInfo::regT0);
    
    load32(Address(GPRInfo::regT0, CodeBlock::offsetOfSpeculativeFailCounter()), GPRInfo::regT2);
    load32(Address(GPRInfo::regT0, CodeBlock::offsetOfSpeculativeSuccessCounter()), GPRInfo::regT1);
    add32(Imm32(1), GPRInfo::regT2);
    add32(Imm32(-1), GPRInfo::regT1);
    store32(GPRInfo::regT2, Address(GPRInfo::regT0, CodeBlock::offsetOfSpeculativeFailCounter()));
    store32(GPRInfo::regT1, Address(GPRInfo::regT0, CodeBlock::offsetOfSpeculativeSuccessCounter()));
    
    move(TrustedImmPtr(codeBlock()->alternative()), GPRInfo::regT0);
    
    Jump fewFails = branch32(BelowOrEqual, GPRInfo::regT2, Imm32(codeBlock()->largeFailCountThreshold()));
    mul32(Imm32(Heuristics::desiredSpeculativeSuccessFailRatio), GPRInfo::regT2, GPRInfo::regT2);
    
    Jump lowFailRate = branch32(BelowOrEqual, GPRInfo::regT2, GPRInfo::regT1);
    
    // Reoptimize as soon as possible.
    store32(Imm32(Heuristics::executionCounterValueForOptimizeNextInvocation), Address(GPRInfo::regT0, CodeBlock::offsetOfExecuteCounter()));
    Jump doneAdjusting = jump();
    
    fewFails.link(this);
    lowFailRate.link(this);
    
    store32(Imm32(codeBlock()->alternative()->counterValueForOptimizeAfterLongWarmUp()), Address(GPRInfo::regT0, CodeBlock::offsetOfExecuteCounter()));
    
    doneAdjusting.link(this);
    
    // 13) Load the result of the last bytecode operation into regT0.
    
    if (exit.m_lastSetOperand != std::numeric_limits<int>::max()) {
        load32(payloadFor((VirtualRegister)exit.m_lastSetOperand), GPRInfo::cachedResultRegister);
        load32(tagFor((VirtualRegister)exit.m_lastSetOperand), GPRInfo::cachedResultRegister2);
    }
    
    // 14) Fix call frame (s).
    
    ASSERT(codeBlock()->alternative()->getJITType() == JITCode::BaselineJIT);
    storePtr(TrustedImmPtr(codeBlock()->alternative()), addressFor((VirtualRegister)RegisterFile::CodeBlock));
    
    for (CodeOrigin codeOrigin = exit.m_codeOrigin; codeOrigin.inlineCallFrame; codeOrigin = codeOrigin.inlineCallFrame->caller) {
        InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame;
        CodeBlock* baselineCodeBlock = baselineCodeBlockFor(codeOrigin);
        CodeBlock* baselineCodeBlockForCaller = baselineCodeBlockFor(inlineCallFrame->caller);
        Vector<BytecodeAndMachineOffset>& decodedCodeMap = decodedCodeMapFor(baselineCodeBlockForCaller);
        unsigned returnBytecodeIndex = inlineCallFrame->caller.bytecodeIndex + OPCODE_LENGTH(op_call);
        BytecodeAndMachineOffset* mapping = binarySearch<BytecodeAndMachineOffset, unsigned, BytecodeAndMachineOffset::getBytecodeIndex>(decodedCodeMap.begin(), decodedCodeMap.size(), returnBytecodeIndex);
        
        ASSERT(mapping);
        ASSERT(mapping->m_bytecodeIndex == returnBytecodeIndex);
        
        void* jumpTarget = baselineCodeBlockForCaller->getJITCode().executableAddressAtOffset(mapping->m_machineCodeOffset);

        GPRReg callerFrameGPR;
        if (inlineCallFrame->caller.inlineCallFrame) {
            add32(Imm32(inlineCallFrame->caller.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT3);
            callerFrameGPR = GPRInfo::regT3;
        } else
            callerFrameGPR = GPRInfo::callFrameRegister;
        
        storePtr(TrustedImmPtr(baselineCodeBlock), addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::CodeBlock)));
        store32(Imm32(JSValue::CellTag), tagFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ScopeChain)));
        storePtr(TrustedImmPtr(inlineCallFrame->callee->scope()), payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ScopeChain)));
        store32(Imm32(JSValue::CellTag), tagFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::CallerFrame)));
        storePtr(callerFrameGPR, payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::CallerFrame)));
        storePtr(TrustedImmPtr(jumpTarget), payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ReturnPC)));
        store32(Imm32(JSValue::Int32Tag), tagFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ArgumentCount)));
        store32(Imm32(inlineCallFrame->arguments.size()), payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ArgumentCount)));
        store32(Imm32(JSValue::CellTag), tagFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::Callee)));
        storePtr(TrustedImmPtr(inlineCallFrame->callee.get()), payloadFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::Callee)));
    }
    
    if (exit.m_codeOrigin.inlineCallFrame)
        addPtr(Imm32(exit.m_codeOrigin.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister);

    // 15) Jump into the corresponding baseline JIT code.
    
    CodeBlock* baselineCodeBlock = baselineCodeBlockFor(exit.m_codeOrigin);
    Vector<BytecodeAndMachineOffset>& decodedCodeMap = decodedCodeMapFor(baselineCodeBlock);
    
    BytecodeAndMachineOffset* mapping = binarySearch<BytecodeAndMachineOffset, unsigned, BytecodeAndMachineOffset::getBytecodeIndex>(decodedCodeMap.begin(), decodedCodeMap.size(), exit.m_codeOrigin.bytecodeIndex);
    
    ASSERT(mapping);
    ASSERT(mapping->m_bytecodeIndex == exit.m_codeOrigin.bytecodeIndex);
    
    void* jumpTarget = baselineCodeBlock->getJITCode().executableAddressAtOffset(mapping->m_machineCodeOffset);
    
    ASSERT(GPRInfo::regT2 != GPRInfo::cachedResultRegister && GPRInfo::regT2 != GPRInfo::cachedResultRegister2);
    
    move(TrustedImmPtr(jumpTarget), GPRInfo::regT2);
    jump(GPRInfo::regT2);

#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "   -> %p\n", jumpTarget);
#endif
}

} } // namespace JSC::DFG

#endif
#endif
