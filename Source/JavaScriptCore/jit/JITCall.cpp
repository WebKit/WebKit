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
#include "JSSentinel.h"
#include "ScratchRegisterAllocator.h"
#include "SetupVarargsFrame.h"
#include "SlowPathCall.h"
#include "StackAlignment.h"
#include "ThunkGenerators.h"

namespace JSC {

void JIT::emit_op_ret(const JSInstruction* currentInstruction)
{
    static_assert(noOverlap(returnValueGPR, callFrameRegister));

    // Return the result in returnValueGPR.
    auto bytecode = currentInstruction->as<OpRet>();
    emitGetVirtualRegister(bytecode.m_value, returnValueGPR);
    jumpThunk(CodeLocationLabel { vm().getCTIStub(CommonJITThunkID::ReturnFromBaseline).retaggedCode<NoPtrTag>() });
}

template<typename Op>
void JIT::emitPutCallResult(const Op& bytecode)
{
    emitValueProfilingSite(bytecode, returnValueGPR);
    emitPutVirtualRegister(destinationFor(bytecode, m_bytecodeIndex.checkpoint()).virtualRegister(), returnValueGPR);
}

template<typename Op>
void JIT::compileSetupFrame(const Op& bytecode)
{
    constexpr auto opcodeID = Op::opcodeID;

    if constexpr (opcodeID == op_call_varargs || opcodeID == op_construct_varargs || opcodeID == op_super_construct_varargs || opcodeID == op_tail_call_varargs) {
        VirtualRegister thisValue = bytecode.m_thisValue;
        VirtualRegister arguments = bytecode.m_arguments;
        int firstFreeRegister = bytecode.m_firstFree.offset(); // FIXME: Why is this a virtual register if we never use it as one...
        int firstVarArgOffset = bytecode.m_firstVarArg;

        JumpList slowVarargs;
        std::optional<Jump> fastVarargsDone;
        // Fast path: when 'arguments' is a small dense Int32/Contiguous JSArray, build the callee frame
        // inline (hot in spread-heavy code). See emitInlineVarargsFrameForContiguousArray.
        if (!firstVarArgOffset) {
            emitGetVirtualRegister(arguments, JSValueRegs(regT2));
            move(TrustedImm32(-firstFreeRegister), regT1);
            emitInlineVarargsFrameForContiguousArray(vm(), *this, regT2 /*array*/, regT1 /*numUsedSlots*/, regT5 /*resultFrame*/, regT0, regT3, regT4, slowVarargs);
            fastVarargsDone = jump();
        }
        slowVarargs.link(this);
        {
            constexpr GPRReg globalObjectGPR = preferredArgumentGPR<S_JITOperation_GJZZ, 0>();
            constexpr GPRReg argumentsGPR = preferredArgumentGPR<S_JITOperation_GJZZ, 1>();

            S_JITOperation_GJZZ sizeOperation = operationSizeFrameForVarargs;

            loadGlobalObject(globalObjectGPR);
            emitGetVirtualRegister(arguments, argumentsGPR);
            callOperation(sizeOperation, globalObjectGPR, argumentsGPR, -firstFreeRegister, firstVarArgOffset);
            move(TrustedImm32(-firstFreeRegister), regT1);
            emitSetVarargsFrame(*this, returnValueGPR, false, regT1, regT1);
        }

        addPtr(TrustedImm32(-static_cast<int32_t>(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf<stackAlignmentBytes()>(5 * sizeof(void*)))), regT1, stackPointerRegister);

        {
            emitGetVirtualRegister(arguments, regT2);
            F_JITOperation_GFJZZ setupOperation = operationSetupVarargsFrame;
            loadGlobalObject(regT4);
            callOperation(setupOperation, regT4, regT1, regT2, firstVarArgOffset, regT0);
            move(returnValueGPR, regT5);
        }

        if (fastVarargsDone)
            fastVarargsDone->link(this);

        // Profile the argument count.
        load32(Address(regT5, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + LowWordOffset), regT2);
        move(TrustedImm32(CallLinkInfo::maxProfiledArgumentCountIncludingThisForVarargs), regT0);
        moveConditionally32(Above, regT2, regT0, regT0, regT2, regT2);
        materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfCallLinkInfo(), regT0);
        load8(Address(regT0, CallLinkInfo::offsetOfMaxArgumentCountIncludingThisForVarargs()), regT1);
        Jump notBiggest = branch32(Above, regT1, regT2);
        store8(regT2, Address(regT0, CallLinkInfo::offsetOfMaxArgumentCountIncludingThisForVarargs()));
        notBiggest.link(this);

        // Initialize 'this'.
        constexpr JSValueRegs thisJSR = jsRegT10;
        emitGetVirtualRegister(thisValue, thisJSR);
        storeValue(thisJSR, Address(regT5, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));

        addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), regT5, stackPointerRegister);
    } else if constexpr (opcodeID == op_call_varargs_with_spread) {
        VirtualRegister thisValue = bytecode.m_thisValue;
        int firstFreeRegister = bytecode.m_firstFree.offset();
        int argvOffset = bytecode.m_argv.offset();
        int argc = bytecode.m_argc;
        const BitVector* bitVector = &m_unlinkedCodeBlock->bitVector(bytecode.m_bitVector);

        {
            loadGlobalObject(regT3);
            callOperation(operationSizeFrameForVarargsWithSpread, regT3, argvOffset, argc, TrustedImmPtr(bitVector), -firstFreeRegister);
            move(TrustedImm32(-firstFreeRegister), regT1);
            emitSetVarargsFrame(*this, returnValueGPR, false, regT1, regT1);
        }

        addPtr(TrustedImm32(-static_cast<int32_t>(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf<stackAlignmentBytes()>(5 * sizeof(void*)))), regT1, stackPointerRegister);

        {
            loadGlobalObject(regT4);
            callOperation(operationSetupVarargsFrameWithSpread, regT4, regT1, argvOffset, argc, TrustedImmPtr(bitVector), regT0);
            move(returnValueGPR, regT5);
        }

        // Profile the argument count.
        load32(Address(regT5, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + LowWordOffset), regT2);
        move(TrustedImm32(CallLinkInfo::maxProfiledArgumentCountIncludingThisForVarargs), regT0);
        moveConditionally32(Above, regT2, regT0, regT0, regT2, regT2);
        materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfCallLinkInfo(), regT0);
        load8(Address(regT0, CallLinkInfo::offsetOfMaxArgumentCountIncludingThisForVarargs()), regT1);
        Jump notBiggest = branch32(Above, regT1, regT2);
        store8(regT2, Address(regT0, CallLinkInfo::offsetOfMaxArgumentCountIncludingThisForVarargs()));
        notBiggest.link(this);

        // Initialize 'this'.
        constexpr GPRReg thisGPR = regT0;
        emitGetVirtualRegister(thisValue, thisGPR);
        storeValue(thisGPR, Address(regT5, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));

        addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), regT5, stackPointerRegister);
    } else {
        unsigned checkpoint = m_bytecodeIndex.checkpoint();
        int argCountIncludingThis = argumentCountIncludingThisFor(bytecode, checkpoint);
        int registerOffset = -static_cast<int>(stackOffsetInRegistersForCall(bytecode, checkpoint));


        if constexpr (opcodeID == op_call || opcodeID == op_tail_call || opcodeID == op_iterator_open || opcodeID == op_call_ignore_result) {
            if (shouldEmitProfiling()) {
                constexpr GPRReg tmpGPR = returnValueGPR;
                emitGetVirtualRegister(VirtualRegister(registerOffset + CallFrame::argumentOffsetIncludingThis(0)), tmpGPR);
                Jump done = branchIfNotCell(tmpGPR);
                load32(Address(tmpGPR, JSCell::structureIDOffset()), tmpGPR);
                store32ToMetadata(tmpGPR, bytecode, Op::Metadata::offsetOfArrayProfile() + ArrayProfile::offsetOfLastSeenStructureID());
                done.link(this);
            }
        }

        addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);
        store32(TrustedImm32(argCountIncludingThis), Address(stackPointerRegister, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + LowWordOffset - sizeof(CallerFrameAndPC)));
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
    using BaselineJITRegisters::CallDirectEval::SlowPath::thisValueGPR;
    using BaselineJITRegisters::CallDirectEval::SlowPath::scopeGPR;
    using BaselineJITRegisters::CallDirectEval::SlowPath::codeBlockGPR;
    using BaselineJITRegisters::CallDirectEval::SlowPath::bytecodeIndexGPR;

    addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), stackPointerRegister, calleeFrameGPR);
    storePtr(callFrameRegister, Address(calleeFrameGPR, CallFrame::callerFrameOffset()));

    resetSP();

    emitGetVirtualRegister(bytecode.m_thisValue, thisValueGPR);
    emitGetVirtualRegister(bytecode.m_scope, scopeGPR);
    loadPtr(addressFor(CallFrameSlot::codeBlock), codeBlockGPR);
    move(TrustedImm32(m_bytecodeIndex.asBits()), bytecodeIndexGPR);
    callOperation(selectCallDirectEvalOperation(bytecode.m_lexicallyScopedFeatures), calleeFrameGPR, scopeGPR, thisValueGPR, codeBlockGPR, bytecodeIndexGPR);
    addSlowCase(branchIfEmpty(returnValueGPR));

    setFastPathResumePoint();
    emitPutCallResult(bytecode);
}

void JIT::compileCallDirectEvalSlowCase(const JSInstruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = instruction->as<OpCallDirectEval>();
    int registerOffset = -bytecode.m_argv;

    addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);

    static_assert(noOverlap(BaselineJITRegisters::Call::calleeGPR, BaselineJITRegisters::Call::callLinkInfoGPR, regT3));
    loadValue(Address(stackPointerRegister, sizeof(Register) * CallFrameSlot::callee - sizeof(CallerFrameAndPC)), BaselineJITRegisters::Call::calleeGPR);
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
    CallLinkInfo::emitTailCallFastPath(*this, callLinkInfo, [&] {
        CallFrameShuffleData shuffleData = CallFrameShuffleData::createForBaselineOrLLIntTailCall(bytecode, m_unlinkedCodeBlock->numParameters());
        CallFrameShuffler shuffler { *this, shuffleData };
        shuffler.setCalleeGPR(BaselineJITRegisters::Call::calleeGPR);
        shuffler.lockGPR(BaselineJITRegisters::Call::callLinkInfoGPR);
        shuffler.lockGPR(BaselineJITRegisters::Call::callTargetGPR);
        shuffler.prepareForTailCall();
    });

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
    store32(TrustedImm32(locationBits), highWordFor(CallFrameSlot::argumentCountIncludingThis));

    emitGetVirtualRegister(callee, BaselineJITRegisters::Call::calleeGPR);
    storeValue(BaselineJITRegisters::Call::calleeGPR, calleeFrameSlot(CallFrameSlot::callee));

    if constexpr (Op::opcodeID == op_call_direct_eval) {
        compileCallDirectEval(bytecode);
        return;
    } else if constexpr (Op::opcodeID == op_super_construct || Op::opcodeID == op_super_construct_varargs) {
        loadPtr(calleeFrameLowWordSlot(CallFrameSlot::thisArgument), BaselineJITRegisters::Call::callTargetGPR);
        loadPtrFromMetadata(bytecode, Op::Metadata::offsetOfCachedCallee(), BaselineJITRegisters::Call::callLinkInfoGPR);
        auto done = branchPtr(Equal, BaselineJITRegisters::Call::callTargetGPR, BaselineJITRegisters::Call::callLinkInfoGPR);
        auto store = branchTestPtr(Zero, BaselineJITRegisters::Call::callLinkInfoGPR);
        move(TrustedImmPtr(JSCell::seenMultipleCalleeObjects()), BaselineJITRegisters::Call::callTargetGPR);
        store.link(this);
        storePtrToMetadata(BaselineJITRegisters::Call::callLinkInfoGPR, bytecode, Op::Metadata::offsetOfCachedCallee());
        done.link(this);
    }

    materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfCallLinkInfo(), BaselineJITRegisters::Call::callLinkInfoGPR);

    if constexpr (Op::opcodeID == op_tail_call)
        compileTailCall(bytecode, callLinkInfo, callLinkInfoIndex);
    else {
        if constexpr (Op::opcodeID == op_tail_call_varargs) {
            CallLinkInfo::emitTailCallFastPath(*this, callLinkInfo, [&] {
                emitRestoreCalleeSaves();
                prepareForTailCallSlow(RegisterSet {
                    BaselineJITRegisters::Call::calleeGPR,
                    BaselineJITRegisters::Call::callLinkInfoGPR,
                    BaselineJITRegisters::Call::callTargetGPR,
                });
            });
            auto doneLocation = label();
            m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;
        } else {
            CallLinkInfo::emitFastPath(*this, callLinkInfo);
            auto doneLocation = label();
            m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;
            if constexpr (Op::opcodeID != op_iterator_open && Op::opcodeID != op_iterator_next && Op::opcodeID != op_async_iterator_open)
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

void JIT::emit_op_call_varargs_with_spread(const JSInstruction* currentInstruction)
{
    compileOpCall<OpCallVarargsWithSpread>(currentInstruction);
}

void JIT::emit_op_tail_call_varargs(const JSInstruction* currentInstruction)
{
    compileOpCall<OpTailCallVarargs>(currentInstruction);
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

template<typename Op>
void JIT::emitIteratorOpenGeneric(const JSInstruction* instruction)
{
    auto bytecode = instruction->as<Op>();
    auto* tryFastFunction = ([&] () {
        if constexpr (std::is_same_v<Op, OpIteratorOpen>) {
            switch (instruction->width()) {
            case Narrow: return iterator_open_try_fast_narrow;
            case Wide16: return iterator_open_try_fast_wide16;
            case Wide32: return iterator_open_try_fast_wide32;
            default: RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            switch (instruction->width()) {
            case Narrow: return async_iterator_open_try_fast_narrow;
            case Wide16: return async_iterator_open_try_fast_wide16;
            case Wide32: return async_iterator_open_try_fast_wide32;
            default: RELEASE_ASSERT_NOT_REACHED();
            }
        }
    })();
    GetByIdModeMetadata modeMetadata = bytecode.metadata(m_profiledCodeBlock).m_modeMetadata;

    CacheType cacheType = CacheType::GetByIdSelf;
    if (modeMetadata.mode == GetByIdMode::ProtoLoad)
        cacheType = CacheType::GetByIdPrototype;

    JITSlowPathCall slowPathCall(this, tryFastFunction);
    slowPathCall.call();
    Jump fastCase = branch32(NotEqual, GPRInfo::returnValueGPR2, TrustedImm32(static_cast<uint32_t>(IterationMode::Generic)));

    compileOpCall<Op>(instruction);
    advanceToNextCheckpoint();

    // call result (iterator) is in returnValueGPR

    using BaselineJITRegisters::GetById::baseGPR;
    using BaselineJITRegisters::GetById::resultGPR;
    using BaselineJITRegisters::GetById::propertyCacheGPR;

    move(returnValueGPR, baseGPR);
    auto [ propertyCache, propertyCacheIndex ] = addUnlinkedPropertyInlineCache();
    loadPropertyInlineCache(propertyCacheIndex, propertyCacheGPR);

    emitJumpSlowCaseIfNotJSCell(baseGPR);

    addSlowCase(branchIfNotObject(baseGPR));

    static_assert(noOverlap(returnValueGPR, propertyCacheGPR));

    const Identifier* ident = &vm().propertyNames->next;

    JITGetByIdGenerator gen(
        nullptr, propertyCache, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromImmortalIdentifier(ident->impl()), baseGPR, resultGPR, propertyCacheGPR, AccessType::GetById, cacheType);

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIds.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultGPR);
    emitPutVirtualRegister(bytecode.m_next, resultGPR);

    fastCase.link(this);
}

void JIT::emit_op_iterator_open(const JSInstruction* instruction)
{
    emitIteratorOpenGeneric<OpIteratorOpen>(instruction);
}

template<typename Op>
void JIT::emitSlowIteratorOpenGeneric(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCasesUpToBytecodeIndex(m_slowCases, iter, m_bytecodeIndex.withCheckpoint(Op::numberOfCheckpoints));

    using BaselineJITRegisters::GetById::baseGPR;
    using BaselineJITRegisters::GetById::propertyCacheGPR;

    JumpList notObject;
    notObject.append(branchIfNotCell(baseGPR));
    notObject.append(branchIfNotObject(baseGPR));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    gen.generateDataICSlowPath(*this);
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
    static_assert(BaselineJITRegisters::GetById::resultGPR == returnValueGPR);
    jump().linkTo(fastPathResumePoint(), this);

    notObject.link(this);
    loadGlobalObject(argumentGPR0);
    callOperation(operationThrowIteratorResultIsNotObject, argumentGPR0);
}

void JIT::emitSlow_op_iterator_open(const JSInstruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitSlowIteratorOpenGeneric<OpIteratorOpen>(instruction, iter);
}

void JIT::emit_op_async_iterator_open(const JSInstruction* instruction)
{
    emitIteratorOpenGeneric<OpAsyncIteratorOpen>(instruction);
}

void JIT::emitSlow_op_async_iterator_open(const JSInstruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    emitSlowIteratorOpenGeneric<OpAsyncIteratorOpen>(instruction, iter);
}

void JIT::emit_op_iterator_next(const JSInstruction* instruction)
{
    auto bytecode = instruction->as<OpIteratorNext>();
    using BaselineJITRegisters::GetById::baseGPR;
    using BaselineJITRegisters::GetById::resultGPR;
    using BaselineJITRegisters::GetById::propertyCacheGPR;

    constexpr GPRReg nextGPR = baseGPR; // Used as temporary register
    emitGetVirtualRegister(bytecode.m_next, nextGPR);
    JumpList genericCases;
    genericCases.append(branchIfNotCell(nextGPR));
    genericCases.append(branchIfNotType(nextGPR, SentinelType));

    JumpList doneCases;
    loadGlobalObject(argumentGPR0);
    emitGetVirtualRegister(bytecode.m_iterator, argumentGPR1);
    emitGetVirtualRegister(bytecode.m_iterable, argumentGPR2);
    materializePointerIntoMetadata(bytecode, 0, argumentGPR3);
    callOperation(operationIteratorNextTryFast, argumentGPR0, argumentGPR1, argumentGPR2, argumentGPR3);
    emitPutVirtualRegister(bytecode.m_done, returnValueGPR);
    emitPutVirtualRegister(bytecode.m_value, returnValueGPR2);
    doneCases.append(branchIfEmpty(returnValueGPR2));
    emitValueProfilingSite(bytecode, m_bytecodeIndex.withCheckpoint(OpIteratorNext::getValue), returnValueGPR2);
    doneCases.append(jump());

    genericCases.link(this);
    load16FromMetadata(bytecode, OpIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes(), regT0);
    or32(TrustedImm32(static_cast<uint16_t>(IterationMode::Generic)), regT0);
    store16ToMetadata(regT0, bytecode, OpIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes());
    compileOpCall<OpIteratorNext>(instruction);
    advanceToNextCheckpoint();

    // call result ({ done, value } JSObject) in regT0
    static_assert(noOverlap(resultGPR, propertyCacheGPR));

    move(returnValueGPR, baseGPR);

    addSlowCase(branchIfNotCell(baseGPR));
    addSlowCase(branchIfNotObject(baseGPR));
    {
        auto [ propertyCache, propertyCacheIndex ] = addUnlinkedPropertyInlineCache();
        loadPropertyInlineCache(propertyCacheIndex, propertyCacheGPR);

        JITGetByIdGenerator gen(
            nullptr, propertyCache, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->done.impl()), baseGPR, resultGPR, propertyCacheGPR, AccessType::GetById, CacheType::GetByIdSelf);

        gen.generateDataICFastPath(*this);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        BytecodeIndex bytecodeIndex = m_bytecodeIndex;
        advanceToNextCheckpoint();
        emitValueProfilingSite(bytecode, bytecodeIndex, resultGPR);
        emitPutVirtualRegister(bytecode.m_done, resultGPR);
    }

    {
        auto usedRegisters = RegisterSet(resultGPR);
        ScratchRegisterAllocator scratchAllocator(usedRegisters);
        GPRReg scratch1 = scratchAllocator.allocateScratchGPR();
        GPRReg scratch2 = scratchAllocator.allocateScratchGPR();
        const bool shouldCheckMasqueradesAsUndefined = false;
        JumpList iterationDone = branchIfTruthy(vm(), resultGPR, scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, CCallHelpers::LazyBaselineGlobalObject);

        emitGetVirtualRegister(bytecode.m_value, baseGPR);
        auto [ propertyCache, propertyCacheIndex ] = addUnlinkedPropertyInlineCache();
        loadPropertyInlineCache(propertyCacheIndex, propertyCacheGPR);

        JITGetByIdGenerator gen(
            nullptr, propertyCache, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->value.impl()), baseGPR, resultGPR, propertyCacheGPR, AccessType::GetById, CacheType::GetByIdSelf);

        gen.generateDataICFastPath(*this);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        setFastPathResumePoint();
        emitValueProfilingSite(bytecode, resultGPR);
        emitPutVirtualRegister(bytecode.m_value, resultGPR);

        iterationDone.link(this);
    }

    doneCases.link(this);
}

void JIT::emitSlow_op_iterator_next(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    using BaselineJITRegisters::GetById::baseGPR;
    using BaselineJITRegisters::GetById::resultGPR;
    using BaselineJITRegisters::GetById::propertyCacheGPR;

    // JIT will only get here with m_bytecodeIndex.checkpoint() == OpIteratorNext::getDone already but LOLJIT will call this on the first checkpoint.
    ASSERT_WITH_MESSAGE(!hasAnySlowCases(m_slowCases, iter, m_bytecodeIndex.withCheckpoint(OpIteratorNext::computeNext)), "iterator next computeNext checkpoint should have no slow cases");
    m_bytecodeIndex = m_bytecodeIndex.withCheckpoint(OpIteratorNext::getDone);
    linkAllSlowCases(iter);
    loadGlobalObject(argumentGPR0);
    callOperation(operationThrowIteratorResultIsNotObject, argumentGPR0);

    {
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        gen.generateDataICSlowPath(*this);
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
        static_assert(BaselineJITRegisters::GetById::resultGPR == returnValueGPR);
        emitJumpSlowToHotForCheckpoint(jump());
    }

    {
        linkAllSlowCases(iter);
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        gen.generateDataICSlowPath(*this);
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
        static_assert(BaselineJITRegisters::GetById::resultGPR == returnValueGPR);
    }
}

// dst = next.call(iterator), or -- when next is the fast async generator driver sentinel --
// enqueue onto the producer instead. See op_async_iterator_next in BytecodeList.rb.
void JIT::emit_op_async_iterator_next(const JSInstruction* instruction)
{
    auto bytecode = instruction->as<OpAsyncIteratorNext>();
    using BaselineJITRegisters::GetById::baseGPR;
    using BaselineJITRegisters::GetById::resultGPR;

    constexpr GPRReg nextGPR = baseGPR; // Used as temporary register
    emitGetVirtualRegister(bytecode.m_next, nextGPR);
    JumpList genericCases;
    genericCases.append(branchIfNotCell(nextGPR));
    genericCases.append(branchIfNotType(nextGPR, SentinelType));

    load16FromMetadata(bytecode, OpAsyncIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes(), regT0);
    or32(TrustedImm32(static_cast<uint16_t>(IterationMode::FastAsyncGenerator)), regT0);
    store16ToMetadata(regT0, bytecode, OpAsyncIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes());

    using SlowOperation = decltype(operationAsyncIteratorNextWithDriver);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg iteratorGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr GPRReg driverGPR = preferredArgumentGPR<SlowOperation, 2>();
    constexpr GPRReg resumeValueGPR = preferredArgumentGPR<SlowOperation, 3>();
    emitGetVirtualRegister(bytecode.m_iterator, iteratorGPR);
    emitGetVirtualRegister(bytecode.m_driver, driverGPR);
    if (bytecode.m_hasValue)
        emitGetVirtualRegister(resumeValueOperandFor(bytecode), resumeValueGPR);
    else
        moveValue(JSValue(), resumeValueGPR);
    loadGlobalObject(globalObjectGPR);
    callOperation(operationAsyncIteratorNextWithDriver, globalObjectGPR, iteratorGPR, driverGPR, resumeValueGPR, TrustedImmPtr(&vm().syncResumeCallCache()));
    emitPutVirtualRegister(bytecode.m_dst, returnValueGPR);
    Jump doneCase = jump();

    genericCases.link(this);
    load16FromMetadata(bytecode, OpAsyncIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes(), regT0);
    or32(TrustedImm32(static_cast<uint16_t>(IterationMode::Generic)), regT0);
    store16ToMetadata(regT0, bytecode, OpAsyncIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes());
    compileOpCall<OpAsyncIteratorNext>(instruction);

    doneCase.link(this);
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
        emitGetVirtualRegister(bytecode.m_constructor, GetById::baseGPR);
        addSlowCase(branchIfNotCell(GetById::baseGPR));
        addSlowCase(branchIfNotObject(GetById::baseGPR));
    }

    // 1.2 Get hasInstance from the constructor.
    {
        auto [ propertyCache, propertyCacheIndex ] = addUnlinkedPropertyInlineCache();
        loadPropertyInlineCache(propertyCacheIndex, GetById::propertyCacheGPR);

        JITGetByIdGenerator gen(
            nullptr, propertyCache, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex),
            CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->hasInstanceSymbol.impl()),
            GetById::baseGPR, GetById::resultGPR, GetById::propertyCacheGPR, AccessType::GetById, CacheType::GetByIdSelf);

        gen.generateDataICFastPath(*this);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        BytecodeIndex bytecodeIndex = m_bytecodeIndex;
        advanceToNextCheckpoint();
        emitValueProfilingSite(bytecode, bytecodeIndex, GetById::resultGPR);
    }

    // 2. Get prototype.
    // 2.1 Check whether the constructor has a custom hasInstance.
    {
        shuffleRegisters<GPRReg, 1>({ GetById::resultGPR }, { Instanceof::Custom::hasInstanceGPR });
        loadGlobalObject(Instanceof::Custom::globalObjectGPR);
        emitGetVirtualRegister(bytecode.m_value, Instanceof::Custom::valueGPR);
        emitGetVirtualRegister(bytecode.m_constructor, Instanceof::Custom::constructorGPR);

        addSlowCase(branchPtr(NotEqual,
            Instanceof::Custom::hasInstanceGPR,
            Address(Instanceof::Custom::globalObjectGPR, JSGlobalObject::offsetOfFunctionProtoHasInstanceSymbolFunction())));
        addSlowCase(branchTest8(Zero,
            Address(Instanceof::Custom::constructorGPR, JSCell::typeInfoFlagsOffset()),
            TrustedImm32(ImplementsDefaultHasInstance)));
    }

    // 2.2 Check whether the value is an object.
    {
        falseCases.append(branchIfNotCell(Instanceof::valueGPR));
        falseCases.append(branchIfNotObject(Instanceof::valueGPR));
    }

    // 2.3 Get prototype from the constructor.
    {
        emitGetVirtualRegister(bytecode.m_constructor, GetById::baseGPR);

        auto [ propertyCache, propertyCacheIndex ] = addUnlinkedPropertyInlineCache();
        loadPropertyInlineCache(propertyCacheIndex, GetById::propertyCacheGPR);

        JITGetByIdGenerator gen(
            nullptr, propertyCache, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex),
            CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->prototype.impl()),
            GetById::baseGPR, GetById::resultGPR, GetById::propertyCacheGPR, AccessType::GetById, CacheType::GetByIdSelf);

        gen.generateDataICFastPath(*this);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        BytecodeIndex bytecodeIndex = m_bytecodeIndex;
        advanceToNextCheckpoint();
        emitValueProfilingSite(bytecode, bytecodeIndex, GetById::resultGPR);
    }

    // 3. Do value instanceof prototype.
    {
        shuffleRegisters<GPRReg, 1>({ GetById::resultGPR }, { Instanceof::protoGPR });
        emitGetVirtualRegister(bytecode.m_value, Instanceof::valueGPR);

        auto [propertyCache, propertyCacheIndex] = addUnlinkedPropertyInlineCache();
        loadPropertyInlineCache(propertyCacheIndex, Instanceof::propertyCacheGPR);

        // Check that proto are cells. baseVal must be a cell - this is checked by the get_by_id for Symbol.hasInstance.
        emitJumpSlowCaseIfNotJSCell(Instanceof::valueGPR, bytecode.m_value);
        addSlowCase(branchIfNotCell(Instanceof::protoGPR));

        JITInstanceOfGenerator gen(
            nullptr, propertyCache, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())),
            RegisterSet::stubUnavailableRegisters(),
            Instanceof::resultGPR,
            Instanceof::valueGPR,
            Instanceof::protoGPR,
            Instanceof::propertyCacheGPR);

        gen.generateDataICFastPath(*this);
        addSlowCase();
        m_instanceOfs.append(gen);

        setFastPathResumePoint();
        done = jump();
    }

    falseCases.link(this);
    moveTrustedValue(jsBoolean(false), Instanceof::resultGPR);

    done.link(this);
    emitPutVirtualRegister(bytecode.m_dst, Instanceof::resultGPR);
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
        gen.generateDataICSlowPath(*this);
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
        static_assert(GetById::resultGPR == returnValueGPR);
        emitJumpSlowToHotForCheckpoint(jump());
    }

    // 2. Get prototype.
    linkAllSlowCases(iter);
    // 2.1 The constructor is not an object.
    {
        auto bytecode = instruction->as<OpInstanceof>();
        callOperation(operationInstanceOfCustom,
            Instanceof::Custom::globalObjectGPR,
            Instanceof::Custom::valueGPR,
            Instanceof::Custom::constructorGPR,
            Instanceof::Custom::hasInstanceGPR);
        boxBoolean(Instanceof::resultGPR, Instanceof::resultGPR);
        emitPutVirtualRegister(bytecode.m_dst, Instanceof::resultGPR);
        done = jump();
    }

    // 2.3 Get prototype from the constructor.
    {
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        gen.generateDataICSlowPath(*this);
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
        static_assert(GetById::resultGPR == returnValueGPR);
        emitJumpSlowToHotForCheckpoint(jump());
    }

    // 3. Do value instanceof prototype.
    linkAllSlowCases(iter);
    {
        JITInstanceOfGenerator& gen = m_instanceOfs[m_instanceOfIndex++];
        nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
    }

    done.link(this);
}

} // namespace JSC

#endif // ENABLE(JIT)
