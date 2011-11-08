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
#include "DFGJITCompiler.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGJITCodeGenerator.h"
#include "DFGOperations.h"
#include "DFGRegisterBank.h"
#include "DFGSpeculativeJIT.h"
#include "JSGlobalData.h"
#include "LinkBuffer.h"

namespace JSC { namespace DFG {

void JITCompiler::linkOSRExits(SpeculativeJIT& speculative)
{
    OSRExitVector::Iterator exitsIter = speculative.osrExits().begin();
    OSRExitVector::Iterator exitsEnd = speculative.osrExits().end();
    
    while (exitsIter != exitsEnd) {
        const OSRExit& exit = *exitsIter;
        exitSpeculativeWithOSR(exit, speculative.speculationRecovery(exit.m_recoveryIndex));
        ++exitsIter;
    }
}

void JITCompiler::compileEntry()
{
    m_startOfCode = label();
    
    // This code currently matches the old JIT. In the function header we need to
    // pop the return address (since we do not allow any recursion on the machine
    // stack), and perform a fast register file check.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56292
    // We'll need to convert the remaining cti_ style calls (specifically the register file
    // check) which will be dependent on stack layout. (We'd need to account for this in
    // both normal return code and when jumping to an exception handler).
    preserveReturnAddressAfterCall(GPRInfo::regT2);
    emitPutToCallFrameHeader(GPRInfo::regT2, RegisterFile::ReturnPC);
}

void JITCompiler::compileBody()
{
    // We generate the speculative code path, followed by OSR exit code to return
    // to the old JIT code if speculations fail.

#if DFG_ENABLE(JIT_BREAK_ON_EVERY_FUNCTION)
    // Handy debug tool!
    breakpoint();
#endif
    
    addPtr(Imm32(1), AbsoluteAddress(codeBlock()->addressOfSpeculativeSuccessCounter()));

    Label speculativePathBegin = label();
    SpeculativeJIT speculative(*this);
    bool compiledSpeculative = speculative.compile();
    ASSERT_UNUSED(compiledSpeculative, compiledSpeculative);

    linkOSRExits(speculative);

    // Iterate over the m_calls vector, checking for jumps to link.
    bool didLinkExceptionCheck = false;
    for (unsigned i = 0; i < m_exceptionChecks.size(); ++i) {
        Jump& exceptionCheck = m_exceptionChecks[i].m_exceptionCheck;
        if (exceptionCheck.isSet()) {
            exceptionCheck.link(this);
            didLinkExceptionCheck = true;
        }
    }

    // If any exception checks were linked, generate code to lookup a handler.
    if (didLinkExceptionCheck) {
        // lookupExceptionHandler is passed two arguments, exec (the CallFrame*), and
        // an identifier for the operation that threw the exception, which we can use
        // to look up handler information. The identifier we use is the return address
        // of the call out from JIT code that threw the exception; this is still
        // available on the stack, just below the stack pointer!
        move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        peek(GPRInfo::argumentGPR1, -1);
        m_calls.append(CallLinkRecord(call(), lookupExceptionHandler));
        // lookupExceptionHandler leaves the handler CallFrame* in the returnValueGPR,
        // and the address of the handler in returnValueGPR2.
        jump(GPRInfo::returnValueGPR2);
    }
}

void JITCompiler::link(LinkBuffer& linkBuffer)
{
    // Link the code, populate data in CodeBlock data structures.
#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "JIT code for %p start at [%p, %p). Size = %lu.\n", m_codeBlock, linkBuffer.debugAddress(), static_cast<char*>(linkBuffer.debugAddress()) + linkBuffer.debugSize(), linkBuffer.debugSize());
#endif

    // Link all calls out from the JIT code to their respective functions.
    for (unsigned i = 0; i < m_calls.size(); ++i)
        linkBuffer.link(m_calls[i].m_call, m_calls[i].m_function);

    if (m_codeBlock->needsCallReturnIndices()) {
        m_codeBlock->callReturnIndexVector().reserveCapacity(m_exceptionChecks.size());
        for (unsigned i = 0; i < m_exceptionChecks.size(); ++i) {
            unsigned returnAddressOffset = linkBuffer.returnAddressOffset(m_exceptionChecks[i].m_call);
            CodeOrigin codeOrigin = m_exceptionChecks[i].m_codeOrigin;
            while (codeOrigin.inlineCallFrame)
                codeOrigin = codeOrigin.inlineCallFrame->caller;
            unsigned exceptionInfo = codeOrigin.bytecodeIndex;
            m_codeBlock->callReturnIndexVector().append(CallReturnOffsetToBytecodeOffset(returnAddressOffset, exceptionInfo));
        }
    }
    
    unsigned numCallsFromInlineCode = 0;
    for (unsigned i = 0; i < m_exceptionChecks.size(); ++i) {
        if (m_exceptionChecks[i].m_codeOrigin.inlineCallFrame)
            numCallsFromInlineCode++;
    }

    if (numCallsFromInlineCode) {
        Vector<CodeOriginAtCallReturnOffset>& codeOrigins = m_codeBlock->codeOrigins();
        codeOrigins.resize(numCallsFromInlineCode);
        
        for (unsigned i = 0, j = 0; i < m_exceptionChecks.size(); ++i) {
            CallExceptionRecord& record = m_exceptionChecks[i];
            if (record.m_codeOrigin.inlineCallFrame) {
                unsigned returnAddressOffset = linkBuffer.returnAddressOffset(m_exceptionChecks[i].m_call);
                codeOrigins[j].codeOrigin = record.m_codeOrigin;
                codeOrigins[j].callReturnOffset = returnAddressOffset;
                j++;
            }
        }
    }
    
    m_codeBlock->setNumberOfStructureStubInfos(m_propertyAccesses.size());
    for (unsigned i = 0; i < m_propertyAccesses.size(); ++i) {
        StructureStubInfo& info = m_codeBlock->structureStubInfo(i);
        CodeLocationCall callReturnLocation = linkBuffer.locationOf(m_propertyAccesses[i].m_functionCall);
        info.callReturnLocation = callReturnLocation;
        info.u.unset.deltaCheckImmToCall = differenceBetweenCodePtr(linkBuffer.locationOf(m_propertyAccesses[i].m_deltaCheckImmToCall), callReturnLocation);
        info.deltaCallToStructCheck = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_deltaCallToStructCheck));
#if USE(JSVALUE64)
        info.u.unset.deltaCallToLoadOrStore = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_deltaCallToLoadOrStore));
#else
        info.u.unset.deltaCallToTagLoadOrStore = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_deltaCallToTagLoadOrStore));
        info.u.unset.deltaCallToPayloadLoadOrStore = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_deltaCallToPayloadLoadOrStore));
#endif
        info.deltaCallToSlowCase = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_deltaCallToSlowCase));
        info.deltaCallToDone = differenceBetweenCodePtr(callReturnLocation, linkBuffer.locationOf(m_propertyAccesses[i].m_deltaCallToDone));
        info.baseGPR = m_propertyAccesses[i].m_baseGPR;
#if USE(JSVALUE64)
        info.valueGPR = m_propertyAccesses[i].m_valueGPR;
#else
        info.valueTagGPR = m_propertyAccesses[i].m_valueTagGPR;
        info.valueGPR = m_propertyAccesses[i].m_valueGPR;
#endif
        info.scratchGPR = m_propertyAccesses[i].m_scratchGPR;
    }
    
    m_codeBlock->setNumberOfCallLinkInfos(m_jsCalls.size());
    for (unsigned i = 0; i < m_jsCalls.size(); ++i) {
        CallLinkInfo& info = m_codeBlock->callLinkInfo(i);
        info.isCall = m_jsCalls[i].m_isCall;
        info.isDFG = true;
        info.callReturnLocation = CodeLocationLabel(linkBuffer.locationOf(m_jsCalls[i].m_slowCall));
        info.hotPathBegin = linkBuffer.locationOf(m_jsCalls[i].m_targetToCheck);
        info.hotPathOther = linkBuffer.locationOfNearCall(m_jsCalls[i].m_fastCall);
    }
    
    m_codeBlock->addMethodCallLinkInfos(m_methodGets.size());
    for (unsigned i = 0; i < m_methodGets.size(); ++i) {
        MethodCallLinkInfo& info = m_codeBlock->methodCallLinkInfo(i);
        info.cachedStructure.setLocation(linkBuffer.locationOf(m_methodGets[i].m_structToCompare));
        info.cachedPrototypeStructure.setLocation(linkBuffer.locationOf(m_methodGets[i].m_protoStructToCompare));
        info.cachedFunction.setLocation(linkBuffer.locationOf(m_methodGets[i].m_putFunction));
        info.cachedPrototype.setLocation(linkBuffer.locationOf(m_methodGets[i].m_protoObj));
        info.callReturnLocation = linkBuffer.locationOf(m_methodGets[i].m_slowCall);
    }
}

void JITCompiler::compile(JITCode& entry)
{
    // Preserve the return address to the callframe.
    compileEntry();
    // Generate the body of the program.
    compileBody();
    // Link
    LinkBuffer linkBuffer(*m_globalData, this);
    link(linkBuffer);
    entry = JITCode(linkBuffer.finalizeCode(), JITCode::DFGJIT);
}

void JITCompiler::compileFunction(JITCode& entry, MacroAssemblerCodePtr& entryWithArityCheck)
{
    compileEntry();

    // === Function header code generation ===
    // This is the main entry point, without performing an arity check.
    // If we needed to perform an arity check we will already have moved the return address,
    // so enter after this.
    Label fromArityCheck(this);
    // Setup a pointer to the codeblock in the CallFrameHeader.
    emitPutImmediateToCallFrameHeader(m_codeBlock, RegisterFile::CodeBlock);
    // Plant a check that sufficient space is available in the RegisterFile.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=56291
    addPtr(Imm32(m_codeBlock->m_numCalleeRegisters * sizeof(Register)), GPRInfo::callFrameRegister, GPRInfo::regT1);
    Jump registerFileCheck = branchPtr(Below, AbsoluteAddress(m_globalData->interpreter->registerFile().addressOfEnd()), GPRInfo::regT1);
    // Return here after register file check.
    Label fromRegisterFileCheck = label();


    // === Function body code generation ===
    compileBody();

    // === Function footer code generation ===
    //
    // Generate code to perform the slow register file check (if the fast one in
    // the function header fails), and generate the entry point with arity check.
    //
    // Generate the register file check; if the fast check in the function head fails,
    // we need to call out to a helper function to check whether more space is available.
    // FIXME: change this from a cti call to a DFG style operation (normal C calling conventions).
    registerFileCheck.link(this);
    move(stackPointerRegister, GPRInfo::argumentGPR0);
    poke(GPRInfo::callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));
    Call callRegisterFileCheck = call();
    jump(fromRegisterFileCheck);
    
    // The fast entry point into a function does not check the correct number of arguments
    // have been passed to the call (we only use the fast entry point where we can statically
    // determine the correct number of arguments have been passed, or have already checked).
    // In cases where an arity check is necessary, we enter here.
    // FIXME: change this from a cti call to a DFG style operation (normal C calling conventions).
    Label arityCheck = label();
    preserveReturnAddressAfterCall(GPRInfo::regT2);
    emitPutToCallFrameHeader(GPRInfo::regT2, RegisterFile::ReturnPC);
    branch32(Equal, GPRInfo::regT1, Imm32(m_codeBlock->m_numParameters)).linkTo(fromArityCheck, this);
    move(stackPointerRegister, GPRInfo::argumentGPR0);
    poke(GPRInfo::callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));
    Call callArityCheck = call();
    move(GPRInfo::regT0, GPRInfo::callFrameRegister);
    jump(fromArityCheck);


    // === Link ===
    LinkBuffer linkBuffer(*m_globalData, this);
    link(linkBuffer);
    
    // FIXME: switch the register file check & arity check over to DFGOpertaion style calls, not JIT stubs.
    linkBuffer.link(callRegisterFileCheck, cti_register_file_check);
    linkBuffer.link(callArityCheck, m_codeBlock->m_isConstructor ? cti_op_construct_arityCheck : cti_op_call_arityCheck);

    entryWithArityCheck = linkBuffer.locationOf(arityCheck);
    entry = JITCode(linkBuffer.finalizeCode(), JITCode::DFGJIT);
}

#if USE(JSVALUE64)

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
    
    GPRReg alreadyBoxed = InvalidGPRReg;
    
    if (recovery) {
        switch (recovery->type()) {
        case SpeculativeAdd:
            sub32(recovery->src(), recovery->dest());
            orPtr(GPRInfo::tagTypeNumberRegister, recovery->dest());
            alreadyBoxed = recovery->dest();
            break;
            
        case BooleanSpeculationCheck:
            xorPtr(TrustedImm32(static_cast<int32_t>(ValueFalse)), recovery->dest());
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
            loadPtr(Address(exit.m_jsValueSource.asAddress()), GPRInfo::tagTypeNumberRegister);
            storePtr(GPRInfo::tagTypeNumberRegister, exit.m_valueProfile->specFailBucket(0));
            move(TrustedImmPtr(bitwise_cast<void*>(TagTypeNumber)), GPRInfo::tagTypeNumberRegister);
        } else
            storePtr(exit.m_jsValueSource.gpr(), exit.m_valueProfile->specFailBucket(0));
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
    if (haveFPRs)
        fprintf(stderr, "FPR ");
    if (haveConstants)
        fprintf(stderr, "Constants ");
    if (haveUndefined)
        fprintf(stderr, "Undefined ");
    fprintf(stderr, " ");
#endif
    
    EncodedJSValue* scratchBuffer = static_cast<EncodedJSValue*>(globalData()->scratchBufferForSize(sizeof(EncodedJSValue) * (numberOfPoisonedVirtualRegisters + (numberOfDisplacedVirtualRegisters <= GPRInfo::numberOfRegisters ? 0 : numberOfDisplacedVirtualRegisters))));

    // From here on, the code assumes that it is profitable to maximize the distance
    // between when something is computed and when it is stored.
    
    // 5) Perform all reboxing of integers.
    
    if (haveUnboxedInt32s) {
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            switch (recovery.technique()) {
            case UnboxedInt32InGPR:
                if (recovery.gpr() != alreadyBoxed)
                    orPtr(GPRInfo::tagTypeNumberRegister, recovery.gpr());
                break;
                
            case AlreadyInRegisterFileAsUnboxedInt32:
                store32(Imm32(static_cast<uint32_t>(TagTypeNumber >> 32)), tagFor(static_cast<VirtualRegister>(exit.operandForIndex(index))));
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
            if (exit.isVariable(index) && poisonedVirtualRegisters[exit.variableForIndex(index)])
                storePtr(recovery.gpr(), scratchBuffer + scratchIndex++);
            else
                storePtr(recovery.gpr(), addressFor((VirtualRegister)operand));
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
            boxDouble(fpr, gpr);
        }
        
        // 8) Dump all doubles into the register file, or to the scratch storage if
        //    the destination virtual register is poisoned.
        
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            if (recovery.technique() != InFPR)
                continue;
            GPRReg gpr = GPRInfo::toRegister(FPRInfo::toIndex(recovery.fpr()));
            if (exit.isVariable(index) && poisonedVirtualRegisters[exit.variableForIndex(index)])
                storePtr(gpr, scratchBuffer + scratchIndex++);
            else
                storePtr(gpr, addressFor((VirtualRegister)exit.operandForIndex(index)));
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
                    loadPtr(addressFor(recovery.virtualRegister()), GPRInfo::toRegister(displacementIndex++));
                    break;
                    
                case Int32DisplacedInRegisterFile: {
                    GPRReg gpr = GPRInfo::toRegister(displacementIndex++);
                    load32(addressFor(recovery.virtualRegister()), gpr);
                    orPtr(GPRInfo::tagTypeNumberRegister, gpr);
                    break;
                }
                    
                case DoubleDisplacedInRegisterFile: {
                    GPRReg gpr = GPRInfo::toRegister(displacementIndex++);
                    loadPtr(addressFor(recovery.virtualRegister()), gpr);
                    subPtr(GPRInfo::tagTypeNumberRegister, gpr);
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
                    storePtr(GPRInfo::toRegister(displacementIndex++), addressFor((VirtualRegister)exit.operandForIndex(index)));
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
                    loadPtr(addressFor(recovery.virtualRegister()), GPRInfo::regT0);
                    storePtr(GPRInfo::regT0, scratchBuffer + scratchIndex++);
                    break;
                    
                case Int32DisplacedInRegisterFile: {
                    load32(addressFor(recovery.virtualRegister()), GPRInfo::regT0);
                    orPtr(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
                    storePtr(GPRInfo::regT0, scratchBuffer + scratchIndex++);
                    break;
                }
                    
                case DoubleDisplacedInRegisterFile: {
                    loadPtr(addressFor(recovery.virtualRegister()), GPRInfo::regT0);
                    subPtr(GPRInfo::tagTypeNumberRegister, GPRInfo::regT0);
                    storePtr(GPRInfo::regT0, scratchBuffer + scratchIndex++);
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
                    loadPtr(scratchBuffer + scratchIndex++, GPRInfo::regT0);
                    storePtr(GPRInfo::regT0, addressFor((VirtualRegister)exit.operandForIndex(index)));
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
            case InFPR:
                loadPtr(scratchBuffer + scratchIndex++, GPRInfo::regT0);
                storePtr(GPRInfo::regT0, addressFor((VirtualRegister)virtualRegister));
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
            move(TrustedImmPtr(JSValue::encode(jsUndefined())), GPRInfo::regT0);
        
        for (int index = 0; index < exit.numberOfRecoveries(); ++index) {
            const ValueRecovery& recovery = exit.valueRecovery(index);
            if (recovery.technique() != Constant)
                continue;
            if (recovery.constant().isUndefined())
                storePtr(GPRInfo::regT0, addressFor((VirtualRegister)exit.operandForIndex(index)));
            else
                storePtr(TrustedImmPtr(JSValue::encode(recovery.constant())), addressFor((VirtualRegister)exit.operandForIndex(index)));
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
    
    if (exit.m_lastSetOperand != std::numeric_limits<int>::max())
        loadPtr(addressFor((VirtualRegister)exit.m_lastSetOperand), GPRInfo::cachedResultRegister);
    
    // 14) Fix call frame(s).
    
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
            addPtr(Imm32(inlineCallFrame->caller.inlineCallFrame->stackOffset * sizeof(EncodedJSValue)), GPRInfo::callFrameRegister, GPRInfo::regT3);
            callerFrameGPR = GPRInfo::regT3;
        } else
            callerFrameGPR = GPRInfo::callFrameRegister;
        
        storePtr(TrustedImmPtr(baselineCodeBlock), addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::CodeBlock)));
        storePtr(TrustedImmPtr(inlineCallFrame->callee->scope()), addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ScopeChain)));
        storePtr(callerFrameGPR, addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::CallerFrame)));
        storePtr(TrustedImmPtr(jumpTarget), addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ReturnPC)));
        storePtr(TrustedImmPtr(JSValue::encode(jsNumber(inlineCallFrame->arguments.size()))), addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::ArgumentCount)));
        storePtr(TrustedImmPtr(inlineCallFrame->callee.get()), addressFor((VirtualRegister)(inlineCallFrame->stackOffset + RegisterFile::Callee)));
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
    
    ASSERT(GPRInfo::regT1 != GPRInfo::cachedResultRegister);
    
    move(TrustedImmPtr(jumpTarget), GPRInfo::regT1);
    jump(GPRInfo::regT1);

#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "-> %p\n", jumpTarget);
#endif
}

#endif // USE(JSVALUE64)

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
