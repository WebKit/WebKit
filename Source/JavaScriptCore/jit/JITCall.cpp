/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include "JIT.h"

#include "BaselineJITRegisters.h"
#include "BytecodeOperandsForCheckpoint.h"
#include "CacheableIdentifierInlines.h"
#include "CallFrameShuffler.h"
#include "CodeBlock.h"
#include "JITInlines.h"
#include "JITThunks.h"
#include "ScratchRegisterAllocator.h"
#include "SetupVarargsFrame.h"
#include "SlowPathCall.h"
#include "StackAlignment.h"
#include "ThunkGenerators.h"

namespace JSC {

void JIT::emit_op_ret(const JSInstruction* currentInstruction)
{
    static_assert(noOverlap(returnValueJSR, callFrameRegister));

    // Return the result in returnValueGPR (returnValueGPR2/returnValueGPR on 32-bit).
    auto bytecode = currentInstruction->as<OpRet>();
    emitGetVirtualRegister(bytecode.m_value, returnValueJSR);
    jumpThunk(CodeLocationLabel { vm().getCTIStub(CommonJITThunkID::ReturnFromBaseline).retaggedCode<NoPtrTag>() });
}

template<typename Op>
void JIT::emitPutCallResult(const Op& bytecode)
{
    emitValueProfilingSite(bytecode, returnValueJSR);
    emitPutVirtualRegister(destinationFor(bytecode, m_bytecodeIndex.checkpoint()).virtualRegister(), returnValueJSR);
}

template<typename Op>
void JIT::compileSetupFrame(const Op& bytecode)
{
    constexpr auto opcodeID = Op::opcodeID;

    if constexpr (opcodeID == op_call_varargs || opcodeID == op_construct_varargs || opcodeID == op_super_construct_varargs || opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments) {
        VirtualRegister thisValue = bytecode.m_thisValue;
        VirtualRegister arguments = bytecode.m_arguments;
        int firstFreeRegister = bytecode.m_firstFree.offset(); // FIXME: Why is this a virtual register if we never use it as one...
        int firstVarArgOffset = bytecode.m_firstVarArg;

        {
            constexpr GPRReg globalObjectGPR = preferredArgumentGPR<S_JITOperation_GJZZ, 0>();
            constexpr JSValueRegs argumentsJSR = preferredArgumentJSR<S_JITOperation_GJZZ, 1>();

            S_JITOperation_GJZZ sizeOperation;
            if constexpr (opcodeID == op_tail_call_forward_arguments)
                sizeOperation = operationSizeFrameForForwardArguments;
            else
                sizeOperation = operationSizeFrameForVarargs;

            loadGlobalObject(globalObjectGPR);
            emitGetVirtualRegister(arguments, argumentsJSR);
            callOperation(sizeOperation, globalObjectGPR, argumentsJSR, -firstFreeRegister, firstVarArgOffset);
            move(TrustedImm32(-firstFreeRegister), regT1);
            emitSetVarargsFrame(*this, returnValueGPR, false, regT1, regT1);
        }

#if USE(JSVALUE64)
        addPtr(TrustedImm32(-static_cast<int32_t>(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf<stackAlignmentBytes()>(5 * sizeof(void*)))), regT1, stackPointerRegister);
#elif USE(JSVALUE32_64)
        addPtr(TrustedImm32(-(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf<stackAlignmentBytes()>(6 * sizeof(void*)))), regT1, stackPointerRegister);
#endif

        {
            emitGetVirtualRegister(arguments, jsRegT32);
            F_JITOperation_GFJZZ setupOperation;
            if constexpr (opcodeID == op_tail_call_forward_arguments)
                setupOperation = operationSetupForwardArgumentsFrame;
            else
                setupOperation = operationSetupVarargsFrame;
            loadGlobalObject(regT4);
            callOperation(setupOperation, regT4, regT1, jsRegT32, firstVarArgOffset, regT0);
            move(returnValueGPR, regT5);
        }

        // Profile the argument count.
        load32(Address(regT5, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset), regT2);
        move(TrustedImm32(CallLinkInfo::maxProfiledArgumentCountIncludingThisForVarargs), regT0);
#if CPU(ARM64) || CPU(X86_64)
        moveConditionally32(Above, regT2, regT0, regT0, regT2, regT2);
#else
        auto lower = branch32(BelowOrEqual, regT2, regT0);
        move(regT0, regT2);
        lower.link(this);
#endif
        materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfCallLinkInfo(), regT0);
        Jump notBiggest = branch32(Above, Address(regT0, CallLinkInfo::offsetOfMaxArgumentCountIncludingThisForVarargs()), regT2);
        store8(regT2, Address(regT0, CallLinkInfo::offsetOfMaxArgumentCountIncludingThisForVarargs()));
        notBiggest.link(this);

        // Initialize 'this'.
        constexpr JSValueRegs thisJSR = jsRegT10;
        emitGetVirtualRegister(thisValue, thisJSR);
        storeValue(thisJSR, Address(regT5, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));

        addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), regT5, stackPointerRegister);
    } else {
        unsigned checkpoint = m_bytecodeIndex.checkpoint();
        int argCountIncludingThis = argumentCountIncludingThisFor(bytecode, checkpoint);
        int registerOffset = -static_cast<int>(stackOffsetInRegistersForCall(bytecode, checkpoint));


        if constexpr (opcodeID == op_call || opcodeID == op_tail_call || opcodeID == op_iterator_open || opcodeID == op_call_ignore_result) {
            if (shouldEmitProfiling()) {
                constexpr JSValueRegs tmpJSR = returnValueJSR;
                constexpr GPRReg tmpGPR = tmpJSR.payloadGPR();
                emitGetVirtualRegister(VirtualRegister(registerOffset + CallFrame::argumentOffsetIncludingThis(0)), tmpJSR);
                Jump done = branchIfNotCell(tmpJSR);
                load32(Address(tmpJSR.payloadGPR(), JSCell::structureIDOffset()), tmpGPR);
                store32ToMetadata(tmpGPR, bytecode, Op::Metadata::offsetOfArrayProfile() + ArrayProfile::offsetOfLastSeenStructureID());
                done.link(this);
            }
        }

        addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);
        store32(TrustedImm32(argCountIncludingThis), Address(stackPointerRegister, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset - sizeof(CallerFrameAndPC)));
    }
}

template<typename Op>
void JIT::compileCallDirectEval(const Op&)
{
}

template<>
void JIT::compileCallDirectEval(const OpCallDirectEval& bytecode)
{
    using BaselineJITRegisters::CallDirectEval::SlowPath::calleeFrameGPR;
    using BaselineJITRegisters::CallDirectEval::SlowPath::thisValueJSR;
    using BaselineJITRegisters::CallDirectEval::SlowPath::scopeGPR;
    using BaselineJITRegisters::CallDirectEval::SlowPath::codeBlockGPR;
    using BaselineJITRegisters::CallDirectEval::SlowPath::bytecodeIndexGPR;

    addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), stackPointerRegister, calleeFrameGPR);
    storePtr(callFrameRegister, Address(calleeFrameGPR, CallFrame::callerFrameOffset()));

    resetSP();

    emitGetVirtualRegister(bytecode.m_thisValue, thisValueJSR);
    emitGetVirtualRegisterPayload(bytecode.m_scope, scopeGPR);
    loadPtr(addressFor(CallFrameSlot::codeBlock), codeBlockGPR);
    move(TrustedImm32(m_bytecodeIndex.asBits()), bytecodeIndexGPR);
    callOperation(selectCallDirectEvalOperation(bytecode.m_lexicallyScopedFeatures), calleeFrameGPR, scopeGPR, thisValueJSR, codeBlockGPR, bytecodeIndexGPR);
    addSlowCase(branchIfEmpty(returnValueJSR));

    setFastPathResumePoint();
    emitPutCallResult(bytecode);
}

void JIT::compileCallDirectEvalSlowCase(const JSInstruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = instruction->as<OpCallDirectEval>();
    int registerOffset = -bytecode.m_argv;

    addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);

    static_assert(noOverlap(BaselineJITRegisters::Call::calleeJSR, BaselineJITRegisters::Call::callLinkInfoGPR, regT3));
    loadValue(Address(stackPointerRegister, sizeof(Register) * CallFrameSlot::callee - sizeof(CallerFrameAndPC)), BaselineJITRegisters::Call::calleeJSR);
    materializePointerIntoMetadata(bytecode, OpCallDirectEval::Metadata::offsetOfCallLinkInfo(), BaselineJITRegisters::Call::callLinkInfoGPR);
    emitVirtualCallWithoutMovingGlobalObject(*m_vm, BaselineJITRegisters::Call::callLinkInfoGPR, CallMode::Regular);
    resetSP();
}

template<typename Op>
bool JIT::compileTailCall(const Op&, BaselineUnlinkedCallLinkInfo*, unsigned)
{
    return false;
}

template<>
bool JIT::compileTailCall(const OpTailCall& bytecode, BaselineUnlinkedCallLinkInfo* callLinkInfo, unsigned callLinkInfoIndex)
{
    CallLinkInfo::emitTailCallFastPath(*this, callLinkInfo, scopedLambda<void()>([&] {
        CallFrameShuffleData shuffleData = CallFrameShuffleData::createForBaselineOrLLIntTailCall(bytecode, m_unlinkedCodeBlock->numParameters());
        CallFrameShuffler shuffler { *this, shuffleData };
        shuffler.setCalleeJSValueRegs(BaselineJITRegisters::Call::calleeJSR);
        shuffler.lockGPR(BaselineJITRegisters::Call::callLinkInfoGPR);
        shuffler.lockGPR(BaselineJITRegisters::Call::callTargetGPR);
        shuffler.prepareForTailCall();
    }));

    auto doneLocation = label();
    m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;

    return true;
}

template<typename Op>
void JIT::compileOpCall(const JSInstruction* instruction)
{
    auto bytecode = instruction->as<Op>();
    VirtualRegister callee = calleeFor(bytecode, m_bytecodeIndex.checkpoint());

    /* Caller always:
        - Updates callFrameRegister to callee callFrame.
        - Initializes ArgumentCount; CallerFrame; Callee.

       For a JS call:
        - Callee initializes ReturnPC; CodeBlock.
        - Callee restores callFrameRegister before return.

       For a non-JS call:
        - Caller initializes ReturnPC; CodeBlock.
        - Caller restores callFrameRegister after return.
    */

    unsigned callLinkInfoIndex = 0;
    BaselineUnlinkedCallLinkInfo* callLinkInfo = nullptr;
    if constexpr (Op::opcodeID != op_call_direct_eval) {
        callLinkInfo = addUnlinkedCallLinkInfo();
        callLinkInfo->bytecodeIndex = m_bytecodeIndex;
        callLinkInfoIndex = m_callCompilationInfo.size();
        m_callCompilationInfo.append(CallCompilationInfo());
        m_callCompilationInfo[callLinkInfoIndex].unlinkedCallLinkInfo = callLinkInfo;
    }
    compileSetupFrame(bytecode);

    // SP holds newCallFrame + sizeof(CallerFrameAndPC), with ArgumentCount initialized.
    uint32_t locationBits = CallSiteIndex(m_bytecodeIndex).bits();
    store32(TrustedImm32(locationBits), tagFor(CallFrameSlot::argumentCountIncludingThis));

    emitGetVirtualRegister(callee, BaselineJITRegisters::Call::calleeJSR);
    storeValue(BaselineJITRegisters::Call::calleeJSR, calleeFrameSlot(CallFrameSlot::callee));

    if constexpr (Op::opcodeID == op_call_direct_eval) {
        compileCallDirectEval(bytecode);
        return;
    } else if constexpr (Op::opcodeID == op_super_construct || Op::opcodeID == op_super_construct_varargs) {
#if USE(JSVALUE64)
        loadPtr(calleeFramePayloadSlot(CallFrameSlot::thisArgument), BaselineJITRegisters::Call::callTargetGPR);
        loadPtrFromMetadata(bytecode, Op::Metadata::offsetOfCachedCallee(), BaselineJITRegisters::Call::callLinkInfoGPR);
        auto done = branchPtr(Equal, BaselineJITRegisters::Call::callTargetGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
        auto store = branchTestPtr(Zero, BaselineJITRegisters::Call::callLinkInfoGPR);
        move(TrustedImmPtr(JSCell::seenMultipleCalleeObjects()), BaselineJITRegisters::Call::callTargetGPR);
        store.link(this);
        storePtrToMetadata(BaselineJITRegisters::Call::callLinkInfoGPR, bytecode, Op::Metadata::offsetOfCachedCallee());
        done.link(this);
#endif
    }

    materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfCallLinkInfo(), BaselineJITRegisters::Call::callLinkInfoGPR);

    if constexpr (Op::opcodeID == op_tail_call)
        compileTailCall(bytecode, callLinkInfo, callLinkInfoIndex);
    else {
        if constexpr (Op::opcodeID == op_tail_call_varargs || Op::opcodeID == op_tail_call_forward_arguments) {
            CallLinkInfo::emitTailCallFastPath(*this, callLinkInfo, scopedLambda<void()>([&] {
                emitRestoreCalleeSaves();
                prepareForTailCallSlow(RegisterSet {
                    BaselineJITRegisters::Call::calleeJSR.payloadGPR(),
#if USE(JSVALUE32_64)
                    BaselineJITRegisters::Call::calleeJSR.tagGPR(),
#endif
                    BaselineJITRegisters::Call::callLinkInfoGPR,
                    BaselineJITRegisters::Call::callTargetGPR,
                });
            }));
            auto doneLocation = label();
            m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;
        } else {
            CallLinkInfo::emitFastPath(*this, callLinkInfo);
            auto doneLocation = label();
            m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;
            if constexpr (Op::opcodeID != op_iterator_open && Op::opcodeID != op_iterator_next)
                setFastPathResumePoint();
            resetSP();
            if constexpr (Op::opcodeID != op_call_ignore_result)
                emitPutCallResult(bytecode);
        }
    }
}

void JIT::emit_op_call(const JSInstruction* currentInstruction)
{
    compileOpCall<OpCall>(currentInstruction);
}

void JIT::emit_op_call_ignore_result(const JSInstruction* currentInstruction)
{
    compileOpCall<OpCallIgnoreResult>(currentInstruction);
}

void JIT::emit_op_tail_call(const JSInstruction* currentInstruction)
{
    compileOpCall<OpTailCall>(currentInstruction);
}

void JIT::emit_op_call_direct_eval(const JSInstruction* currentInstruction)
{
    compileOpCall<OpCallDirectEval>(currentInstruction);
}

void JIT::emit_op_call_varargs(const JSInstruction* currentInstruction)
{
    compileOpCall<OpCallVarargs>(currentInstruction);
}

void JIT::emit_op_tail_call_varargs(const JSInstruction* currentInstruction)
{
    compileOpCall<OpTailCallVarargs>(currentInstruction);
}

void JIT::emit_op_tail_call_forward_arguments(const JSInstruction* currentInstruction)
{
    compileOpCall<OpTailCallForwardArguments>(currentInstruction);
}

void JIT::emit_op_construct_varargs(const JSInstruction* currentInstruction)
{
    compileOpCall<OpConstructVarargs>(currentInstruction);
}

void JIT::emit_op_super_construct_varargs(const JSInstruction* currentInstruction)
{
    compileOpCall<OpSuperConstructVarargs>(currentInstruction);
}

void JIT::emit_op_construct(const JSInstruction* currentInstruction)
{
    compileOpCall<OpConstruct>(currentInstruction);
}

void JIT::emit_op_super_construct(const JSInstruction* currentInstruction)
{
    compileOpCall<OpSuperConstruct>(currentInstruction);
}

void JIT::emitSlow_op_call_direct_eval(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileCallDirectEvalSlowCase(currentInstruction, iter);
}

void JIT::emit_op_iterator_open(const JSInstruction* instruction)
{
    auto bytecode = instruction->as<OpIteratorOpen>();
    auto* tryFastFunction = ([&] () {
        switch (instruction->width()) {
        case Narrow: return iterator_open_try_fast_narrow;
        case Wide16: return iterator_open_try_fast_wide16;
        case Wide32: return iterator_open_try_fast_wide32;
        default: RELEASE_ASSERT_NOT_REACHED();
        }
    })();
    GetByIdModeMetadata modeMetadata = bytecode.metadata(m_profiledCodeBlock).m_modeMetadata;

    CacheType cacheType = CacheType::GetByIdSelf;
    if (modeMetadata.mode == GetByIdMode::ProtoLoad)
        cacheType = CacheType::GetByIdPrototype;

    JITSlowPathCall slowPathCall(this, tryFastFunction);
    slowPathCall.call();
    Jump fastCase = branch32(NotEqual, GPRInfo::returnValueGPR2, TrustedImm32(static_cast<uint32_t>(IterationMode::Generic)));

    compileOpCall<OpIteratorOpen>(instruction);
    advanceToNextCheckpoint();

    // call result (iterator) is in returnValueJSR

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    moveValueRegs(returnValueJSR, baseJSR);
    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR);

    addSlowCase(branchIfNotObject(baseJSR.payloadGPR()));

    static_assert(noOverlap(returnValueJSR, stubInfoGPR));

    const Identifier* ident = &vm().propertyNames->next;

    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromImmortalIdentifier(ident->impl()), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById, cacheType);

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIds.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(bytecode.m_next, resultJSR);

    fastCase.link(this);
}

void JIT::emitSlow_op_iterator_open(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    JumpList notObject;
    notObject.append(branchIfNotCell(baseJSR));
    notObject.append(branchIfNotObject(baseJSR.payloadGPR()));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
    static_assert(BaselineJITRegisters::GetById::resultJSR == returnValueJSR);
    jump().linkTo(fastPathResumePoint(), this);

    notObject.link(this);
    loadGlobalObject(argumentGPR0);
    callOperation(operationThrowIteratorResultIsNotObject, argumentGPR0);
}

void JIT::emit_op_iterator_next(const JSInstruction* instruction)
{
    auto bytecode = instruction->as<OpIteratorNext>();
    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    constexpr JSValueRegs nextJSR = baseJSR; // Used as temporary register
    emitGetVirtualRegister(bytecode.m_next, nextJSR);
    Jump genericCase = branchIfNotEmpty(nextJSR);

    JumpList doneCases;
#if CPU(ARM64) || CPU(X86_64)
    loadGlobalObject(argumentGPR0);
    emitGetVirtualRegister(bytecode.m_iterator, argumentGPR1);
    emitGetVirtualRegister(bytecode.m_iterable, argumentGPR2);
    materializePointerIntoMetadata(bytecode, 0, argumentGPR3);
    callOperation(operationIteratorNextTryFast, argumentGPR0, argumentGPR1, argumentGPR2, argumentGPR3);
    emitPutVirtualRegister(bytecode.m_done, returnValueGPR);
    emitPutVirtualRegister(bytecode.m_value, returnValueGPR2);
    doneCases.append(branchIfEmpty(JSValueRegs { returnValueGPR2 }));
    emitValueProfilingSite(bytecode, JSValueRegs { returnValueGPR2 });
#else
    auto* tryFastFunction = ([&] () {
        switch (instruction->width()) {
        case Narrow: return iterator_next_try_fast_narrow;
        case Wide16: return iterator_next_try_fast_wide16;
        case Wide32: return iterator_next_try_fast_wide32;
        default: RELEASE_ASSERT_NOT_REACHED();
        }
    })();
    JITSlowPathCall slowPathCall(this, tryFastFunction);
    slowPathCall.call();
#endif
    doneCases.append(jump());

    genericCase.link(this);
    load8FromMetadata(bytecode, OpIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes(), regT0);
    or32(TrustedImm32(static_cast<uint8_t>(IterationMode::Generic)), regT0);
    store8ToMetadata(regT0, bytecode, OpIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes());
    compileOpCall<OpIteratorNext>(instruction);
    advanceToNextCheckpoint();

    // call result ({ done, value } JSObject) in regT0  (regT1/regT0 or 32-bit)
    static_assert(noOverlap(resultJSR, stubInfoGPR));

    moveValueRegs(returnValueJSR, baseJSR);

    addSlowCase(branchIfNotCell(baseJSR));
    addSlowCase(branchIfNotObject(baseJSR.payloadGPR()));
    {
        auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
        loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

        JITGetByIdGenerator gen(
            nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSetBuilder::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->done.impl()), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById, CacheType::GetByIdSelf);

        gen.generateDataICFastPath(*this);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        BytecodeIndex bytecodeIndex = m_bytecodeIndex;
        advanceToNextCheckpoint();
        emitValueProfilingSite(bytecode, bytecodeIndex, resultJSR);
        emitPutVirtualRegister(bytecode.m_done, resultJSR);
    }

    {
        auto usedRegisters = RegisterSetBuilder(resultJSR).buildAndValidate();
        ScratchRegisterAllocator scratchAllocator(usedRegisters);
        GPRReg scratch1 = scratchAllocator.allocateScratchGPR();
        GPRReg scratch2 = scratchAllocator.allocateScratchGPR();
        const bool shouldCheckMasqueradesAsUndefined = false;
        JumpList iterationDone = branchIfTruthy(vm(), resultJSR, scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, CCallHelpers::LazyBaselineGlobalObject);

        emitGetVirtualRegister(bytecode.m_value, baseJSR);
        auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
        loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

        JITGetByIdGenerator gen(
            nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSetBuilder::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->value.impl()), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById, CacheType::GetByIdSelf);

        gen.generateDataICFastPath(*this);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        setFastPathResumePoint();
        emitValueProfilingSite(bytecode, resultJSR);
        emitPutVirtualRegister(bytecode.m_value, resultJSR);

        iterationDone.link(this);
    }

    doneCases.link(this);
}

void JIT::emitSlow_op_iterator_next(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    linkAllSlowCases(iter);
    loadGlobalObject(argumentGPR0);
    callOperation(operationThrowIteratorResultIsNotObject, argumentGPR0);

    {
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        gen.reportBaselineDataICSlowPathBegin(label());
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
        static_assert(BaselineJITRegisters::GetById::resultJSR == returnValueJSR);
        emitJumpSlowToHotForCheckpoint(jump());
    }

    {
        linkAllSlowCases(iter);
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        gen.reportBaselineDataICSlowPathBegin(label());
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
        static_assert(BaselineJITRegisters::GetById::resultJSR == returnValueJSR);
    }
}

void JIT::emit_op_instanceof(const JSInstruction* instruction)
{
    using namespace BaselineJITRegisters;

    auto bytecode = instruction->as<OpInstanceof>();
    JumpList falseCases;
    Jump done;

    // 1. Get hasInstance.
    // 1.1 Check whether the constructor is an object.
    {
        emitGetVirtualRegister(bytecode.m_constructor, GetById::baseJSR);
        addSlowCase(branchIfNotCell(GetById::baseJSR));
        addSlowCase(branchIfNotObject(GetById::baseJSR.payloadGPR()));
    }

    // 1.2 Get hasInstance from the constructor.
    {
        auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
        loadStructureStubInfo(stubInfoIndex, GetById::stubInfoGPR);

        JITGetByIdGenerator gen(
            nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex),
            CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSetBuilder::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->hasInstanceSymbol.impl()),
            GetById::baseJSR, GetById::resultJSR, GetById::stubInfoGPR, AccessType::GetById, CacheType::GetByIdSelf);

        gen.generateDataICFastPath(*this);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        BytecodeIndex bytecodeIndex = m_bytecodeIndex;
        advanceToNextCheckpoint();
        emitValueProfilingSite(bytecode, bytecodeIndex, GetById::resultJSR);
    }

    // 2. Get prototype.
    // 2.1 Check whether the constructor has a custom hasInstance.
    {
        shuffleJSRs<1>({ GetById::resultJSR }, { Instanceof::Custom::hasInstanceJSR });
        loadGlobalObject(Instanceof::Custom::globalObjectGPR);
        emitGetVirtualRegister(bytecode.m_value, Instanceof::Custom::valueJSR);
        emitGetVirtualRegisterPayload(bytecode.m_constructor, Instanceof::Custom::constructorGPR);

        addSlowCase(branchPtr(NotEqual,
            Instanceof::Custom::hasInstanceJSR.payloadGPR(),
            Address(Instanceof::Custom::globalObjectGPR, JSGlobalObject::offsetOfFunctionProtoHasInstanceSymbolFunction())));
        addSlowCase(branchTest8(Zero,
            Address(Instanceof::Custom::constructorGPR, JSCell::typeInfoFlagsOffset()),
            TrustedImm32(ImplementsDefaultHasInstance)));
    }

    // 2.2 Check whether the value is an object.
    {
        falseCases.append(branchIfNotCell(Instanceof::valueJSR));
        falseCases.append(branchIfNotObject(Instanceof::valueJSR.payloadGPR()));
    }

    // 2.3 Get prototype from the constructor.
    {
        emitGetVirtualRegister(bytecode.m_constructor, GetById::baseJSR);

        auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
        loadStructureStubInfo(stubInfoIndex, GetById::stubInfoGPR);

        JITGetByIdGenerator gen(
            nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex),
            CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSetBuilder::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->prototype.impl()),
            GetById::baseJSR, GetById::resultJSR, GetById::stubInfoGPR, AccessType::GetById, CacheType::GetByIdSelf);

        gen.generateDataICFastPath(*this);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        BytecodeIndex bytecodeIndex = m_bytecodeIndex;
        advanceToNextCheckpoint();
        emitValueProfilingSite(bytecode, bytecodeIndex, GetById::resultJSR);
    }

    // 3. Do value instanceof prototype.
    {
        shuffleJSRs<1>({ GetById::resultJSR }, { Instanceof::protoJSR });
        emitGetVirtualRegister(bytecode.m_value, Instanceof::valueJSR);

        auto [stubInfo, stubInfoIndex] = addUnlinkedStructureStubInfo();
        loadStructureStubInfo(stubInfoIndex, Instanceof::stubInfoGPR);

        // Check that proto are cells. baseVal must be a cell - this is checked by the get_by_id for Symbol.hasInstance.
        emitJumpSlowCaseIfNotJSCell(Instanceof::valueJSR, bytecode.m_value);
        addSlowCase(branchIfNotCell(Instanceof::protoJSR));

        JITInstanceOfGenerator gen(
            nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())),
            RegisterSetBuilder::stubUnavailableRegisters(),
            Instanceof::resultJSR.payloadGPR(),
            Instanceof::valueJSR.payloadGPR(),
            Instanceof::protoJSR.payloadGPR(),
            Instanceof::stubInfoGPR);

        gen.generateDataICFastPath(*this);
#if USE(JSVALUE32_64)
        boxBoolean(Instanceof::resultJSR.payloadGPR(), Instanceof::resultJSR);
#endif
        addSlowCase();
        m_instanceOfs.append(gen);

        setFastPathResumePoint();
        done = jump();
    }

    falseCases.link(this);
    moveTrustedValue(jsBoolean(false), Instanceof::resultJSR);

    done.link(this);
    emitPutVirtualRegister(bytecode.m_dst, Instanceof::resultJSR);
}

void JIT::emitSlow_op_instanceof(const JSInstruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    using namespace BaselineJITRegisters;
    Jump done;

    // 1. Get hasInstance
    linkAllSlowCases(iter);
    // 1.1 The constructor is not an object.
    {
        JITSlowPathCall slowPathCall(this, slow_path_throw_static_error_from_instanceof);
        slowPathCall.call();
    }

    // 1.2 Get hasInstance from the constructor.
    {
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        gen.reportBaselineDataICSlowPathBegin(label());
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
        static_assert(GetById::resultJSR == returnValueJSR);
        emitJumpSlowToHotForCheckpoint(jump());
    }

    // 2. Get prototype.
    linkAllSlowCases(iter);
    // 2.1 The constructor is not an object.
    {
        auto bytecode = instruction->as<OpInstanceof>();
        callOperation(operationInstanceOfCustom,
            Instanceof::Custom::globalObjectGPR,
            Instanceof::Custom::valueJSR,
            Instanceof::Custom::constructorGPR,
            Instanceof::Custom::hasInstanceJSR);
        boxBoolean(Instanceof::resultJSR.payloadGPR(), Instanceof::resultJSR);
        emitPutVirtualRegister(bytecode.m_dst, Instanceof::resultJSR);
        done = jump();
    }

    // 2.3 Get prototype from the constructor.
    {
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        gen.reportBaselineDataICSlowPathBegin(label());
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
        static_assert(GetById::resultJSR == returnValueJSR);
        emitJumpSlowToHotForCheckpoint(jump());
    }

    // 3. Do value instanceof prototype.
    linkAllSlowCases(iter);
    {
        JITInstanceOfGenerator& gen = m_instanceOfs[m_instanceOfIndex++];
        gen.reportBaselineDataICSlowPathBegin(label());
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
    }

    done.link(this);
}

} // namespace JSC

#endif // ENABLE(JIT)
