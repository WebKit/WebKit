/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGOSRExitCompiler.h"

#if ENABLE(DFG_JIT) && USE(JSVALUE64)

#include "DFGOperations.h"

namespace JSC { namespace DFG {

void OSRExitCompiler::compileExit(const OSRExit& exit, SpeculationRecovery* recovery)
{
    // 1) Pro-forma stuff.
#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "OSR exit for Node @%d (", (int)exit.m_nodeIndex);
    for (CodeOrigin codeOrigin = exit.m_codeOrigin; ; codeOrigin = codeOrigin.inlineCallFrame->caller) {
        fprintf(stderr, "bc#%u", codeOrigin.bytecodeIndex);
        if (!codeOrigin.inlineCallFrame)
            break;
        fprintf(stderr, " -> %p ", codeOrigin.inlineCallFrame->executable.get());
    }
    fprintf(stderr, ")  ");
    exit.dump(stderr);
#endif
#if DFG_ENABLE(VERBOSE_SPECULATION_FAILURE)
    SpeculationFailureDebugInfo* debugInfo = new SpeculationFailureDebugInfo;
    debugInfo->codeBlock = m_jit.codeBlock();
    debugInfo->nodeIndex = exit.m_nodeIndex;
    
    m_jit.debugCall(debugOperationPrintSpeculationFailure, debugInfo);
#endif
    
#if DFG_ENABLE(JIT_BREAK_ON_SPECULATION_FAILURE)
    m_jit.breakpoint();
#endif
    
#if DFG_ENABLE(SUCCESS_STATS)
    static SamplingCounter counter("SpeculationFailure");
    m_jit.emitCount(counter);
#endif
    
    // 2) Perform speculation recovery. This only comes into play when an operation
    //    starts mutating state before verifying the speculation it has already made.
    
    GPRReg alreadyBoxed = InvalidGPRReg;
    
    if (recovery) {
        switch (recovery->type()) {
        case SpeculativeAdd:
            m_jit.sub32(recovery->src(), recovery->dest());
            m_jit.orPtr(GPRInfo::tagTypeNumberRegister, recovery->dest());
            alreadyBoxed = recovery->dest();
            break;
            
        case BooleanSpeculationCheck:
            m_jit.xorPtr(AssemblyHelpers::TrustedImm32(static_cast<int32_t>(ValueFalse)), recovery->dest());
            break;
            
        default:
            break;
        }
    }

    // 3) Refine some value profile, if appropriate.
    
    if (!!exit.m_jsValueSource && !!exit.m_valueProfile) {
        if (exit.m_jsValueSource.isAddress()) {
            // We can't be sure that we have a spare register. So use the tagTypeNumberRegister,
            // since we know how to restore it.
            m_jit.loadPtr(AssemblyHelpers::Address(exit.m_jsValueSource.asAddress()), GPRInfo::tagTypeNumberRegister);
            m_jit.storePtr(GPRInfo::tagTypeNumberRegister, exit.m_valueProfile->specFailBucket(0));
            m_jit.move(AssemblyHelpers::TrustedImmPtr(bitwise_cast<void*>(TagTypeNumber)), GPRInfo::tagTypeNumberRegister);
        } else
            m_jit.storePtr(exit.m_jsValueSource.gpr(), exit.m_valueProfile->specFailBucket(0));
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
    bool haveUnboxedInt32s = false;
    bool haveFPRs = false;
    bool haveConstants = false;
    bool haveUndefined = false;
    bool haveUInt32s = false;
    
    for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
        const ValueRecovery& recovery = exit.valueRecovery(index);
        switch (recovery.technique()) {
        case Int32DisplacedInRegisterFile:
        case DoubleDisplacedInRegisterFile:
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
                case UInt32InGPR:
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
            
        case UnboxedInt32InGPR:
        case AlreadyInRegisterFileAsUnboxedInt32:
            haveUnboxedInt32s = true;
            break;
            
        case UInt32InGPR:
            haveUInt32s = true;
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
    
#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "  ");
    if (numberOfPoisonedVirtualRegisters)
        fprintf(stderr, "Poisoned=%u ", numberOfPoisonedVirtualRegisters);
    if (numberOfDisplacedVirtualRegisters)
        fprintf(stderr, "Displaced=%u ", numberOfDisplacedVirtualRegisters);
    if (haveUnboxedInt32s)
        fprintf(stderr, "UnboxedInt32 ");
    if (haveUInt32s)
        fprintf(stderr, "UInt32 ");
    if (haveFPRs)
        fprintf(stderr, "FPR ");
    if (haveConstants)
        fprintf(stderr, "Constants ");
    if (haveUndefined)
        fprintf(stderr, "Undefined ");
    fprintf(stderr, " ");
#endif
    
    EncodedJSValue* scratchBuffer = static_cast<EncodedJSValue*>(m_jit.globalData()->scratchBufferForSize(sizeof(EncodedJSValue) * std::max(haveUInt32s ? 2u : 0u, numberOfPoisonedVirtualRegisters + (numberOfDisplacedVirtualRegisters <= GPRInfo::numberOfRegisters ? 0 : numberOfDisplacedVirtualRegisters))));

    // From here on, the code assumes that it is profitable to maximize the distance
    // between when something is computed and when it is stored.
    
    // 5) Perform all reboxing of integers.
    
    if (haveUnboxedInt32s || haveUInt32s) {
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            switch (recovery.technique()) {
            case UnboxedInt32InGPR:
                if (recovery.gpr() != alreadyBoxed)
                    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, recovery.gpr());
                break;
                
            case AlreadyInRegisterFileAsUnboxedInt32:
                m_jit.store32(AssemblyHelpers::Imm32(static_cast<uint32_t>(TagTypeNumber >> 32)), AssemblyHelpers::tagFor(static_cast<VirtualRegister>(exit.operandForIndex(index))));
                break;
                
            case UInt32InGPR: {
                // This occurs when the speculative JIT left an unsigned 32-bit integer
                // in a GPR. If it's positive, we can just box the int. Otherwise we
                // need to turn it into a boxed double.
                
                // We don't try to be clever with register allocation here; we assume
                // that the program is using FPRs and we don't try to figure out which
                // ones it is using. Instead just temporarily save fpRegT0 and then
                // restore it. This makes sense because this path is not cheap to begin
                // with, and should happen very rarely.
                
                GPRReg addressGPR = GPRInfo::regT0;
                if (addressGPR == recovery.gpr())
                    addressGPR = GPRInfo::regT1;
                
                m_jit.storePtr(addressGPR, scratchBuffer);
                m_jit.move(AssemblyHelpers::TrustedImmPtr(scratchBuffer + 1), addressGPR);
                m_jit.storeDouble(FPRInfo::fpRegT0, addressGPR);
                
                AssemblyHelpers::Jump positive = m_jit.branch32(AssemblyHelpers::GreaterThanOrEqual, recovery.gpr(), AssemblyHelpers::TrustedImm32(0));

                m_jit.convertInt32ToDouble(recovery.gpr(), FPRInfo::fpRegT0);
                m_jit.addDouble(AssemblyHelpers::AbsoluteAddress(&AssemblyHelpers::twoToThe32), FPRInfo::fpRegT0);
                m_jit.boxDouble(FPRInfo::fpRegT0, recovery.gpr());
                
                AssemblyHelpers::Jump done = m_jit.jump();
                
                positive.link(&m_jit);
                
                m_jit.orPtr(GPRInfo::tagTypeNumberRegister, recovery.gpr());
                
                done.link(&m_jit);
                
                m_jit.loadDouble(addressGPR, FPRInfo::fpRegT0);
                m_jit.loadPtr(scratchBuffer, addressGPR);
                break;
            }
                
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
        case UInt32InGPR:
            if (exit.isVariable(index) && poisonedVirtualRegisters[exit.variableForIndex(index)])
                m_jit.storePtr(recovery.gpr(), scratchBuffer + scratchIndex++);
            else
                m_jit.storePtr(recovery.gpr(), AssemblyHelpers::addressFor((VirtualRegister)operand));
            break;
        default:
            break;
        }
    }
    
    // At this point all GPRs are available for scratch use.
    
    if (haveFPRs) {
        // 7) Box all doubles (relies on there being more GPRs than FPRs)
        
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            if (recovery.technique() != InFPR)
                continue;
            FPRReg fpr = recovery.fpr();
            GPRReg gpr = GPRInfo::toRegister(FPRInfo::toIndex(fpr));
            m_jit.boxDouble(fpr, gpr);
        }
        
        // 8) Dump all doubles into the register file, or to the scratch storage if
        //    the destination virtual register is poisoned.
        
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            if (recovery.technique() != InFPR)
                continue;
            GPRReg gpr = GPRInfo::toRegister(FPRInfo::toIndex(recovery.fpr()));
            if (exit.isVariable(index) && poisonedVirtualRegisters[exit.variableForIndex(index)])
                m_jit.storePtr(gpr, scratchBuffer + scratchIndex++);
            else
                m_jit.storePtr(gpr, AssemblyHelpers::addressFor((VirtualRegister)exit.operandForIndex(index)));
        }
    }
    
    ASSERT(scratchIndex == numberOfPoisonedVirtualRegisters);
    
    // 9) Reshuffle displaced virtual registers. Optimize for the case that
    //    the number of displaced virtual registers is not more than the number
    //    of available physical registers.
    
    if (numberOfDisplacedVirtualRegisters) {
        if (numberOfDisplacedVirtualRegisters <= GPRInfo::numberOfRegisters) {
            // So far this appears to be the case that triggers all the time, but
            // that is far from guaranteed.
        
            unsigned displacementIndex = 0;
            for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
                const ValueRecovery& recovery = exit.valueRecovery(index);
                switch (recovery.technique()) {
                case DisplacedInRegisterFile:
                    m_jit.loadPtr(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::toRegister(displacementIndex++));
                    break;
                    
                case Int32DisplacedInRegisterFile: {
                    GPRReg gpr = GPRInfo::toRegister(displacementIndex++);
                    m_jit.load32(AssemblyHelpers::addressFor(recovery.virtualRegister()), gpr);
                    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, gpr);
                    break;
                }
                    
                case DoubleDisplacedInRegisterFile: {
                    GPRReg gpr = GPRInfo::toRegister(displacementIndex++);
                    m_jit.loadPtr(AssemblyHelpers::addressFor(recovery.virtualRegister()), gpr);
                    m_jit.subPtr(GPRInfo::tagTypeNumberRegister, gpr);
                    break;
                }
                    
                default:
                    break;
                }
            }
        
            displacementIndex = 0;
            for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
                const ValueRecovery& recovery = exit.valueRecovery(index);
                switch (recovery.technique()) {
                case DisplacedInRegisterFile:
                case Int32DisplacedInRegisterFile:
                case DoubleDisplacedInRegisterFile:
                    m_jit.storePtr(GPRInfo::toRegister(displacementIndex++), AssemblyHelpers::addressFor((VirtualRegister)exit.operandForIndex(index)));
                    break;
                    
                default:
                    break;
                }
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
                
                switch (recovery.technique()) {
                case DisplacedInRegisterFile:
                    m_jit.loadPtr(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::regT0);
                    m_jit.storePtr(GPRInfo::regT0, scratchBuffer + scratchIndex++);
                    break;
                    
                case Int32DisplacedInRegisterFile: {
                    m_jit.load32(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::regT0);
                    m_jit.orPtr(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
                    m_jit.storePtr(GPRInfo::regT0, scratchBuffer + scratchIndex++);
                    break;
                }
                    
                case DoubleDisplacedInRegisterFile: {
                    m_jit.loadPtr(AssemblyHelpers::addressFor(recovery.virtualRegister()), GPRInfo::regT0);
                    m_jit.subPtr(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
                    m_jit.storePtr(GPRInfo::regT0, scratchBuffer + scratchIndex++);
                    break;
                }
                    
                default:
                    break;
                }
            }
        
            scratchIndex = numberOfPoisonedVirtualRegisters;
            for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
                const ValueRecovery& recovery = exit.valueRecovery(index);
                switch (recovery.technique()) {
                case DisplacedInRegisterFile:
                case Int32DisplacedInRegisterFile:
                case DoubleDisplacedInRegisterFile:
                    m_jit.loadPtr(scratchBuffer + scratchIndex++, GPRInfo::regT0);
                    m_jit.storePtr(GPRInfo::regT0, AssemblyHelpers::addressFor((VirtualRegister)exit.operandForIndex(index)));
                    break;
                    
                default:
                    break;
                }
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
            case UInt32InGPR:
            case InFPR:
                m_jit.loadPtr(scratchBuffer + scratchIndex++, GPRInfo::regT0);
                m_jit.storePtr(GPRInfo::regT0, AssemblyHelpers::addressFor((VirtualRegister)virtualRegister));
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
        if (haveUndefined)
            m_jit.move(AssemblyHelpers::TrustedImmPtr(JSValue::encode(jsUndefined())), GPRInfo::regT0);
        
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            if (recovery.technique() != Constant)
                continue;
            if (recovery.constant().isUndefined())
                m_jit.storePtr(GPRInfo::regT0, AssemblyHelpers::addressFor((VirtualRegister)exit.operandForIndex(index)));
            else
                m_jit.storePtr(AssemblyHelpers::TrustedImmPtr(JSValue::encode(recovery.constant())), AssemblyHelpers::addressFor((VirtualRegister)exit.operandForIndex(index)));
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
    
    m_jit.add32(AssemblyHelpers::Imm32(1), AssemblyHelpers::AbsoluteAddress(&exit.m_count));
    
    m_jit.move(AssemblyHelpers::TrustedImmPtr(m_jit.codeBlock()), GPRInfo::regT0);
    
    m_jit.load32(AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfSpeculativeFailCounter()), GPRInfo::regT2);
    m_jit.load32(AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfSpeculativeSuccessCounter()), GPRInfo::regT1);
    m_jit.add32(AssemblyHelpers::Imm32(1), GPRInfo::regT2);
    m_jit.add32(AssemblyHelpers::Imm32(-1), GPRInfo::regT1);
    m_jit.store32(GPRInfo::regT2, AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfSpeculativeFailCounter()));
    m_jit.store32(GPRInfo::regT1, AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfSpeculativeSuccessCounter()));
    
    m_jit.move(AssemblyHelpers::TrustedImmPtr(m_jit.baselineCodeBlock()), GPRInfo::regT0);
    
    AssemblyHelpers::Jump fewFails = m_jit.branch32(AssemblyHelpers::BelowOrEqual, GPRInfo::regT2, AssemblyHelpers::Imm32(m_jit.codeBlock()->largeFailCountThreshold()));
    m_jit.mul32(AssemblyHelpers::Imm32(Heuristics::desiredSpeculativeSuccessFailRatio), GPRInfo::regT2, GPRInfo::regT2);
    
    AssemblyHelpers::Jump lowFailRate = m_jit.branch32(AssemblyHelpers::BelowOrEqual, GPRInfo::regT2, GPRInfo::regT1);
    
    // Reoptimize as soon as possible.
    m_jit.store32(AssemblyHelpers::Imm32(Heuristics::executionCounterValueForOptimizeNextInvocation), AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfExecuteCounter()));
    AssemblyHelpers::Jump doneAdjusting = m_jit.jump();
    
    fewFails.link(&m_jit);
    lowFailRate.link(&m_jit);
    
    m_jit.store32(AssemblyHelpers::Imm32(m_jit.baselineCodeBlock()->counterValueForOptimizeAfterLongWarmUp()), AssemblyHelpers::Address(GPRInfo::regT0, CodeBlock::offsetOfExecuteCounter()));
    
    doneAdjusting.link(&m_jit);
    
    // 13) Load the result of the last bytecode operation into regT0.
    
    if (exit.m_lastSetOperand != std::numeric_limits<int>::max())
        m_jit.loadPtr(AssemblyHelpers::addressFor((VirtualRegister)exit.m_lastSetOperand), GPRInfo::cachedResultRegister);
    
    // 14) Fix call frame(s).
    
    ASSERT(m_jit.baselineCodeBlock()->getJITType() == JITCode::BaselineJIT);
    m_jit.storePtr(AssemblyHelpers::TrustedImmPtr(m_jit.baselineCodeBlock()), AssemblyHelpers::addressFor((VirtualRegister)RegisterFile::CodeBlock));
    
    for (CodeOrigin codeOrigin = exit.m_codeOrigin; codeOrigin.inlineCallFrame; codeOrigin = codeOrigin.inlineCallFrame->caller) {
        InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame;
        CodeBlock* baselineCodeBlock = m_jit.baselineCodeBlockFor(codeOrigin);
        CodeBlock* baselineCodeBlockForCaller = m_jit.baselineCodeBlockFor(inlineCallFrame->caller);
        Vector<BytecodeAndMachineOffset>& decodedCodeMap = m_jit.decodedCodeMapFor(baselineCodeBlockForCaller);
        unsigned returnBytecodeIndex = inlineCallFrame->caller.bytecodeIndex + OPCODE_LENGTH(op_call);
        BytecodeAndMachineOffset* mapping = binarySearch<BytecodeAndMachineOffset, unsigned, BytecodeAndMachineOffset::getBytecodeIndex>(decodedCodeMap.begin(), decodedCodeMap.size(), returnBytecodeIndex);
        
        ASSERT(mapping);
        ASSERT(mapping->m_bytecodeIndex == returnBytecodeIndex);
        
        void* jumpTarget = baselineCodeBlockForCaller->getJITCode().executableAddressAtOffset(mapping->m_machineCodeOffset);

        GPRReg callerFrameGPR;
        if (inlineCallFrame->caller.inlineCallFrame) {
            m_jit.addPtr(AssemblyHelpers::Imm32(inlineCallFrame->caller.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT3);
            callerFrameGPR = GPRInfo::regT3;
        } else
            callerFrameGPR = GPRInfo::callFrameRegister;
        
        m_jit.storePtr(AssemblyHelpers::TrustedImmPtr(baselineCodeBlock), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::CodeBlock)));
        m_jit.storePtr(AssemblyHelpers::TrustedImmPtr(inlineCallFrame->callee->scope()), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ScopeChain)));
        m_jit.storePtr(callerFrameGPR, AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::CallerFrame)));
        m_jit.storePtr(AssemblyHelpers::TrustedImmPtr(jumpTarget), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ReturnPC)));
        m_jit.storePtr(AssemblyHelpers::TrustedImmPtr(JSValue::encode(jsNumber(inlineCallFrame->arguments.size()))), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ArgumentCount)));
        m_jit.storePtr(AssemblyHelpers::TrustedImmPtr(inlineCallFrame->callee.get()), AssemblyHelpers::addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::Callee)));
    }
    
    if (exit.m_codeOrigin.inlineCallFrame)
        m_jit.addPtr(AssemblyHelpers::Imm32(exit.m_codeOrigin.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister);
    
    // 15) Jump into the corresponding baseline JIT code.
    
    CodeBlock* baselineCodeBlock = m_jit.baselineCodeBlockFor(exit.m_codeOrigin);
    Vector<BytecodeAndMachineOffset>& decodedCodeMap = m_jit.decodedCodeMapFor(baselineCodeBlock);
    
    BytecodeAndMachineOffset* mapping = binarySearch<BytecodeAndMachineOffset, unsigned, BytecodeAndMachineOffset::getBytecodeIndex>(decodedCodeMap.begin(), decodedCodeMap.size(), exit.m_codeOrigin.bytecodeIndex);
    
    ASSERT(mapping);
    ASSERT(mapping->m_bytecodeIndex == exit.m_codeOrigin.bytecodeIndex);
    
    void* jumpTarget = baselineCodeBlock->getJITCode().executableAddressAtOffset(mapping->m_machineCodeOffset);
    
    ASSERT(GPRInfo::regT1 != GPRInfo::cachedResultRegister);
    
    m_jit.move(AssemblyHelpers::TrustedImmPtr(jumpTarget), GPRInfo::regT1);
    m_jit.jump(GPRInfo::regT1);

#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "-> %p\n", jumpTarget);
#endif
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT) && USE(JSVALUE64)
