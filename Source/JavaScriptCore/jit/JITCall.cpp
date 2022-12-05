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
    emitNakedNearJump(vm().getCTIStub(returnFromBaselineGenerator).code());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::returnFromBaselineGenerator(VM&)
{
    CCallHelpers jit;

    jit.checkStackPointerAlignment();
    jit.emitRestoreCalleeSavesFor(&RegisterAtOffsetList::llintBaselineCalleeSaveRegisters());
    jit.emitFunctionEpilogue();
    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: op_ret_handler");
}

template<typename Op>
void JIT::emitPutCallResult(const Op& bytecode)
{
    emitValueProfilingSite(bytecode, returnValueJSR);
    emitPutVirtualRegister(destinationFor(bytecode, m_bytecodeIndex.checkpoint()).virtualRegister(), returnValueJSR);
}


template<typename Op>
std::enable_if_t<
    Op::opcodeID != op_call_varargs && Op::opcodeID != op_construct_varargs
    && Op::opcodeID != op_tail_call_varargs && Op::opcodeID != op_tail_call_forward_arguments
, void>
JIT::compileSetupFrame(const Op& bytecode)
{
    unsigned checkpoint = m_bytecodeIndex.checkpoint();
    int argCountIncludingThis = argumentCountIncludingThisFor(bytecode, checkpoint);
    int registerOffset = -static_cast<int>(stackOffsetInRegistersForCall(bytecode, checkpoint));


    if (Op::opcodeID == op_call && shouldEmitProfiling()) {
        constexpr JSValueRegs tmpJSR = returnValueJSR;
        constexpr GPRReg tmpGPR = tmpJSR.payloadGPR();
        emitGetVirtualRegister(VirtualRegister(registerOffset + CallFrame::argumentOffsetIncludingThis(0)), tmpJSR);
        Jump done = branchIfNotCell(tmpJSR);
        load32(Address(tmpJSR.payloadGPR(), JSCell::structureIDOffset()), tmpGPR);
        store32ToMetadata(tmpGPR, bytecode, Op::Metadata::offsetOfArrayProfile() + ArrayProfile::offsetOfLastSeenStructureID());
        done.link(this);
    }

    addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);
    store32(TrustedImm32(argCountIncludingThis), Address(stackPointerRegister, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset - sizeof(CallerFrameAndPC)));
}

template<typename Op>
std::enable_if_t<
    Op::opcodeID == op_call_varargs || Op::opcodeID == op_construct_varargs
    || Op::opcodeID == op_tail_call_varargs || Op::opcodeID == op_tail_call_forward_arguments
, void>
JIT::compileSetupFrame(const Op& bytecode)
{
    VirtualRegister thisValue = bytecode.m_thisValue;
    VirtualRegister arguments = bytecode.m_arguments;
    int firstFreeRegister = bytecode.m_firstFree.offset(); // FIXME: Why is this a virtual register if we never use it as one...
    int firstVarArgOffset = bytecode.m_firstVarArg;

    {
        constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Z_JITOperation_GJZZ, 0>();
        constexpr JSValueRegs argumentsJSR = preferredArgumentJSR<Z_JITOperation_GJZZ, 1>();

        Z_JITOperation_GJZZ sizeOperation;
        if (Op::opcodeID == op_tail_call_forward_arguments)
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
    addPtr(TrustedImm32(-static_cast<int32_t>(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf(stackAlignmentBytes(), 5 * sizeof(void*)))), regT1, stackPointerRegister);
#elif USE(JSVALUE32_64)
    addPtr(TrustedImm32(-(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf(stackAlignmentBytes(), 6 * sizeof(void*)))), regT1, stackPointerRegister);
#endif

    {
        emitGetVirtualRegister(arguments, jsRegT32);
        F_JITOperation_GFJZZ setupOperation;
        if (Op::opcodeID == op_tail_call_forward_arguments)
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

    addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), stackPointerRegister, calleeFrameGPR);
    storePtr(callFrameRegister, Address(calleeFrameGPR, CallFrame::callerFrameOffset()));

    resetSP();

    emitGetVirtualRegister(bytecode.m_thisValue, thisValueJSR);
    emitGetVirtualRegisterPayload(bytecode.m_scope, scopeGPR);
    callOperation(bytecode.m_ecmaMode.isStrict() ? operationCallDirectEvalStrict : operationCallDirectEvalSloppy, calleeFrameGPR, scopeGPR, thisValueJSR);
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
    loadGlobalObject(regT3);
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
    materializePointerIntoMetadata(bytecode, OpTailCall::Metadata::offsetOfCallLinkInfo(), BaselineJITRegisters::Call::callLinkInfoGPR);
    JumpList slowPaths = CallLinkInfo::emitTailCallFastPath(*this, callLinkInfo, BaselineJITRegisters::Call::calleeJSR.payloadGPR(), BaselineJITRegisters::Call::callLinkInfoGPR, scopedLambda<void()>([&] {
        CallFrameShuffleData shuffleData = CallFrameShuffleData::createForBaselineOrLLIntTailCall(bytecode, m_unlinkedCodeBlock->numParameters());
        CallFrameShuffler shuffler { *this, shuffleData };
        shuffler.lockGPR(BaselineJITRegisters::Call::callLinkInfoGPR);
        shuffler.prepareForTailCall();
    }));
    addSlowCase(slowPaths);

    auto doneLocation = label();
    m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;

    return true;
}

template<typename Op>
void JIT::compileOpCall(const JSInstruction* instruction, unsigned callLinkInfoIndex)
{
    OpcodeID opcodeID = Op::opcodeID;
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

    BaselineUnlinkedCallLinkInfo* callLinkInfo = nullptr;
    if (opcodeID != op_call_direct_eval) {
        callLinkInfo = addUnlinkedCallLinkInfo();
        callLinkInfo->bytecodeIndex = m_bytecodeIndex;
        ASSERT(m_callCompilationInfo.size() == callLinkInfoIndex);
        m_callCompilationInfo.append(CallCompilationInfo());
        m_callCompilationInfo[callLinkInfoIndex].unlinkedCallLinkInfo = callLinkInfo;
    }
    compileSetupFrame(bytecode);

    // SP holds newCallFrame + sizeof(CallerFrameAndPC), with ArgumentCount initialized.
    uint32_t locationBits = CallSiteIndex(m_bytecodeIndex).bits();
    store32(TrustedImm32(locationBits), Address(callFrameRegister, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + TagOffset));

    emitGetVirtualRegister(callee, BaselineJITRegisters::Call::calleeJSR);
    storeValue(BaselineJITRegisters::Call::calleeJSR, Address(stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - sizeof(CallerFrameAndPC)));

    if (opcodeID == op_call_direct_eval) {
        compileCallDirectEval(bytecode);
        return;
    }

#if USE(JSVALUE32_64)
    // We need this on JSVALUE32_64 only as on JSVALUE64 a pointer comparison in the DataIC fast
    // path catches this.
    addSlowCase(branchIfNotCell(BaselineJITRegisters::Call::calleeJSR));
#endif

    if (compileTailCall(bytecode, callLinkInfo, callLinkInfoIndex))
        return;

    materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfCallLinkInfo(), BaselineJITRegisters::Call::callLinkInfoGPR);
    if (opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments) {
        auto slowPaths = CallLinkInfo::emitTailCallFastPath(*this, callLinkInfo, BaselineJITRegisters::Call::calleeJSR.payloadGPR(), BaselineJITRegisters::Call::callLinkInfoGPR, scopedLambda<void()>([&] {
            emitRestoreCalleeSaves();
            prepareForTailCallSlow(BaselineJITRegisters::Call::callLinkInfoGPR);
        }));
        addSlowCase(slowPaths);
        auto doneLocation = label();
        m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;
        return;
    }

    auto slowPaths = CallLinkInfo::emitFastPath(*this, callLinkInfo, BaselineJITRegisters::Call::calleeJSR.payloadGPR(), BaselineJITRegisters::Call::callLinkInfoGPR);
    auto doneLocation = label();
    addSlowCase(slowPaths);

    m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;

    if constexpr (Op::opcodeID != op_iterator_open && Op::opcodeID != op_iterator_next)
        setFastPathResumePoint();
    resetSP();
    emitPutCallResult(bytecode);
}

template<typename Op>
void JIT::compileOpCallSlowCase(const JSInstruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex)
{
    OpcodeID opcodeID = Op::opcodeID;
    auto bytecode = instruction->as<Op>();
    ASSERT(opcodeID != op_call_direct_eval);
    auto* callLinkInfo = m_callCompilationInfo[callLinkInfoIndex].unlinkedCallLinkInfo;

    linkAllSlowCases(iter);

    loadGlobalObject(regT3);
    materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfCallLinkInfo(), regT2);

    if (opcodeID == op_tail_call || opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments)
        emitRestoreCalleeSaves();

    CallLinkInfo::emitSlowPath(*m_vm, *this, callLinkInfo, regT2);

    if (opcodeID == op_tail_call || opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments) {
        abortWithReason(JITDidReturnFromTailCall);
        return;
    }
}

void JIT::emit_op_call(const JSInstruction* currentInstruction)
{
    compileOpCall<OpCall>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_tail_call(const JSInstruction* currentInstruction)
{
    compileOpCall<OpTailCall>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_call_direct_eval(const JSInstruction* currentInstruction)
{
    compileOpCall<OpCallDirectEval>(currentInstruction, m_callLinkInfoIndex);
}

void JIT::emit_op_call_varargs(const JSInstruction* currentInstruction)
{
    compileOpCall<OpCallVarargs>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_tail_call_varargs(const JSInstruction* currentInstruction)
{
    compileOpCall<OpTailCallVarargs>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_tail_call_forward_arguments(const JSInstruction* currentInstruction)
{
    compileOpCall<OpTailCallForwardArguments>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_construct_varargs(const JSInstruction* currentInstruction)
{
    compileOpCall<OpConstructVarargs>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_construct(const JSInstruction* currentInstruction)
{
    compileOpCall<OpConstruct>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_call(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpCall>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_tail_call(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpTailCall>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_call_direct_eval(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileCallDirectEvalSlowCase(currentInstruction, iter);
}

void JIT::emitSlow_op_call_varargs(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpCallVarargs>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_tail_call_varargs(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpTailCallVarargs>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_tail_call_forward_arguments(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpTailCallForwardArguments>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_construct_varargs(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpConstructVarargs>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_construct(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpConstruct>(currentInstruction, iter, m_callLinkInfoIndex++);
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

    JITSlowPathCall slowPathCall(this, tryFastFunction);
    slowPathCall.call();
    Jump fastCase = branch32(NotEqual, GPRInfo::returnValueGPR2, TrustedImm32(static_cast<uint32_t>(IterationMode::Generic)));

    compileOpCall<OpIteratorOpen>(instruction, m_callLinkInfoIndex++);
    advanceToNextCheckpoint();

    // call result (iterator) is in returnValueJSR

    emitJumpSlowCaseIfNotJSCell(returnValueJSR);

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::FastPath::stubInfoGPR;

    static_assert(noOverlap(returnValueJSR, stubInfoGPR));
    static_assert(returnValueJSR == baseJSR); // Otherwise will need move(returnValueJSR, baseJSR)
    static_assert(baseJSR == resultJSR);

    const Identifier* ident = &vm().propertyNames->next;

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromImmortalIdentifier(ident->impl()), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(bytecode.m_next, resultJSR);

    fastCase.link(this);
}

void JIT::emitSlow_op_iterator_open(const JSInstruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = instruction->as<OpIteratorOpen>();

    linkAllSlowCases(iter);
    compileOpCallSlowCase<OpIteratorOpen>(instruction, iter, m_callLinkInfoIndex++);
    resetSP();
    emitPutCallResult(bytecode);
    emitJumpSlowToHotForCheckpoint(jump());

    linkAllSlowCases(iter);
    JSValueRegs iteratorJSR = BaselineJITRegisters::GetById::baseJSR;
    JumpList notObject;
    notObject.append(branchIfNotCell(iteratorJSR));
    notObject.append(branchIfNotObject(iteratorJSR.payloadGPR()));

    VirtualRegister nextVReg = bytecode.m_next;
    UniquedStringImpl* ident = vm().propertyNames->next.impl();

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    using SlowOperation = decltype(operationGetByIdOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs arg2JSR = preferredArgumentJSR<SlowOperation, 2>();

    moveValueRegs(iteratorJSR, arg2JSR);
    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    callOperationWithProfile<SlowOperation>(
        bytecode,
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        nextVReg,
        globalObjectGPR, stubInfoGPR, arg2JSR,
        CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
    gen.reportSlowPathCall(coldPathBegin, Call());

    auto done = jump();

    notObject.link(this);
    loadGlobalObject(argumentGPR0);
    callOperation(operationThrowIteratorResultIsNotObject, argumentGPR0);

    done.link(this);
}

void JIT::emit_op_iterator_next(const JSInstruction* instruction)
{
    auto bytecode = instruction->as<OpIteratorNext>();
    auto* tryFastFunction = ([&] () {
        switch (instruction->width()) {
        case Narrow: return iterator_next_try_fast_narrow;
        case Wide16: return iterator_next_try_fast_wide16;
        case Wide32: return iterator_next_try_fast_wide32;
        default: RELEASE_ASSERT_NOT_REACHED();
        }
    })();

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::FastPath::dontClobberJSR;
    using BaselineJITRegisters::GetById::FastPath::stubInfoGPR;

    constexpr JSValueRegs nextJSR = baseJSR; // Used as temporary register
    emitGetVirtualRegister(bytecode.m_next, nextJSR);
    Jump genericCase = branchIfNotEmpty(nextJSR);

    JITSlowPathCall slowPathCall(this, tryFastFunction);
    slowPathCall.call();
    Jump fastCase = branch32(NotEqual, GPRInfo::returnValueGPR2, TrustedImm32(static_cast<uint32_t>(IterationMode::Generic)));

    genericCase.link(this);
    load8FromMetadata(bytecode, OpIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes(), regT0);
    or32(TrustedImm32(static_cast<uint8_t>(IterationMode::Generic)), regT0);
    store8ToMetadata(regT0, bytecode, OpIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes());
    compileOpCall<OpIteratorNext>(instruction, m_callLinkInfoIndex++);
    advanceToNextCheckpoint();

    // call result ({ done, value } JSObject) in regT0  (regT1/regT0 or 32-bit)
    static_assert(noOverlap(resultJSR, stubInfoGPR));

    constexpr JSValueRegs iterCallResultJSR = dontClobberJSR;
    moveValueRegs(returnValueJSR, iterCallResultJSR);

    constexpr JSValueRegs doneJSR = resultJSR;
    {
        emitJumpSlowCaseIfNotJSCell(returnValueJSR);

        auto preservedRegs = RegisterSetBuilder::stubUnavailableRegisters();
        preservedRegs.add(iterCallResultJSR, IgnoreVectors);
        auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
        JITGetByIdGenerator gen(
            nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), preservedRegs,
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->done.impl()), returnValueJSR, doneJSR, stubInfoGPR, AccessType::GetById);
        gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

        gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        emitValueProfilingSite(bytecode, doneJSR);
        emitPutVirtualRegister(bytecode.m_done, doneJSR);
        advanceToNextCheckpoint();
    }

    {
        auto usedRegisters = RegisterSetBuilder(doneJSR, iterCallResultJSR).buildAndValidate();
        ScratchRegisterAllocator scratchAllocator(usedRegisters);
        GPRReg scratch1 = scratchAllocator.allocateScratchGPR();
        GPRReg scratch2 = scratchAllocator.allocateScratchGPR();
        GPRReg globalGPR = scratchAllocator.allocateScratchGPR();
        const bool shouldCheckMasqueradesAsUndefined = false;
        loadGlobalObject(globalGPR);
        JumpList iterationDone = branchIfTruthy(vm(), doneJSR, scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, globalGPR);

        moveValueRegs(iterCallResultJSR, baseJSR);

        auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
        JITGetByIdGenerator gen(
            nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSetBuilder::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->value.impl()), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById);
        gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

        gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        emitValueProfilingSite(bytecode, resultJSR);
        emitPutVirtualRegister(bytecode.m_value, resultJSR);

        iterationDone.link(this);
    }

    fastCase.link(this);
}

void JIT::emitSlow_op_iterator_next(const JSInstruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = instruction->as<OpIteratorNext>();

    linkAllSlowCases(iter);
    compileOpCallSlowCase<OpIteratorNext>(instruction, iter, m_callLinkInfoIndex++);
    resetSP();
    emitPutCallResult(bytecode);
    emitJumpSlowToHotForCheckpoint(jump());

    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::FastPath::dontClobberJSR;

    constexpr JSValueRegs iterCallResultJSR = dontClobberJSR;

    {
        VirtualRegister doneVReg = bytecode.m_done;

        linkAllSlowCases(iter);
        JumpList notObject;
        notObject.append(branchIfNotCell(iterCallResultJSR));

        UniquedStringImpl* ident = vm().propertyNames->done.impl();
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        
        Label coldPathBegin = label();

        notObject.append(branchIfNotObject(iterCallResultJSR.payloadGPR()));

        using SlowOperation = decltype(operationGetByIdOptimize);
        constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
        constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
        constexpr JSValueRegs arg2JSR = preferredArgumentJSR<SlowOperation, 2>();

        moveValueRegs(iterCallResultJSR, arg2JSR);
        loadGlobalObject(globalObjectGPR);
        loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
        callOperationWithProfile<SlowOperation>(
            bytecode,
            Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
            doneVReg,
            globalObjectGPR, stubInfoGPR, arg2JSR,
            CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
        gen.reportSlowPathCall(coldPathBegin, Call());

        constexpr JSValueRegs doneJSR = resultJSR;
        emitGetVirtualRegister(doneVReg, doneJSR);
        emitGetVirtualRegister(bytecode.m_value, iterCallResultJSR);
        emitJumpSlowToHotForCheckpoint(jump());

        notObject.link(this);
        loadGlobalObject(argumentGPR0);
        callOperation(operationThrowIteratorResultIsNotObject, argumentGPR0);
    }

    {   
        linkAllSlowCases(iter);
        VirtualRegister valueVReg = bytecode.m_value;

        UniquedStringImpl* ident = vm().propertyNames->value.impl();
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

        Label coldPathBegin = label();

        using SlowOperation = decltype(operationGetByIdOptimize);
        constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
        constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
        constexpr JSValueRegs arg2JSR = preferredArgumentJSR<SlowOperation, 2>();

        moveValueRegs(iterCallResultJSR, arg2JSR);
        loadGlobalObject(globalObjectGPR);
        loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
        callOperationWithProfile<decltype(operationGetByIdOptimize)>(
            bytecode,
            Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
            valueVReg,
            globalObjectGPR, stubInfoGPR, arg2JSR,
            CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
        gen.reportSlowPathCall(coldPathBegin, Call());
    }
}

} // namespace JSC

#endif // ENABLE(JIT)
