/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#if USE(JSVALUE64)
#include "JIT.h"

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

template<typename Op>
void JIT::emitPutCallResult(const Op& bytecode)
{
    emitValueProfilingSite(bytecode, regT0);
    emitPutVirtualRegister(destinationFor(bytecode, m_bytecodeIndex.checkpoint()).virtualRegister(), regT0);
}

template<typename Op>
std::enable_if_t<
    Op::opcodeID != op_call_varargs && Op::opcodeID != op_construct_varargs
    && Op::opcodeID != op_tail_call_varargs && Op::opcodeID != op_tail_call_forward_arguments
, void>
JIT::compileSetupFrame(const Op& bytecode, JITConstantPool::Constant)
{
    unsigned checkpoint = m_bytecodeIndex.checkpoint();
    int argCountIncludingThis = argumentCountIncludingThisFor(bytecode, checkpoint);
    int registerOffset = -static_cast<int>(stackOffsetInRegistersForCall(bytecode, checkpoint));


    if (Op::opcodeID == op_call && shouldEmitProfiling()) {
        emitGetVirtualRegister(VirtualRegister(registerOffset + CallFrame::argumentOffsetIncludingThis(0)), regT0);
        Jump done = branchIfNotCell(regT0);
        load32(Address(regT0, JSCell::structureIDOffset()), regT0);
        store32ToMetadata(regT0, bytecode, OpCall::Metadata::offsetOfCallLinkInfo() + LLIntCallLinkInfo::offsetOfArrayProfile() + ArrayProfile::offsetOfLastSeenStructureID());
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
JIT::compileSetupFrame(const Op& bytecode, JITConstantPool::Constant callLinkInfoConstant)
{
    VirtualRegister thisValue = bytecode.m_thisValue;
    VirtualRegister arguments = bytecode.m_arguments;
    int firstFreeRegister = bytecode.m_firstFree.offset(); // FIXME: Why is this a virtual register if we never use it as one...
    int firstVarArgOffset = bytecode.m_firstVarArg;

    emitGetVirtualRegister(arguments, regT1);
    Z_JITOperation_GJZZ sizeOperation;
    if (Op::opcodeID == op_tail_call_forward_arguments)
        sizeOperation = operationSizeFrameForForwardArguments;
    else
        sizeOperation = operationSizeFrameForVarargs;
    loadGlobalObject(regT0);
    callOperation(sizeOperation, regT0, regT1, -firstFreeRegister, firstVarArgOffset);
    move(TrustedImm32(-firstFreeRegister), regT1);
    emitSetVarargsFrame(*this, returnValueGPR, false, regT1, regT1);
    addPtr(TrustedImm32(-(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf(stackAlignmentBytes(), 5 * sizeof(void*)))), regT1, stackPointerRegister);
    emitGetVirtualRegister(arguments, regT2);
    F_JITOperation_GFJZZ setupOperation;
    if (Op::opcodeID == op_tail_call_forward_arguments)
        setupOperation = operationSetupForwardArgumentsFrame;
    else
        setupOperation = operationSetupVarargsFrame;
    loadGlobalObject(regT3);
    callOperation(setupOperation, regT3, regT1, regT2, firstVarArgOffset, regT0);
    move(returnValueGPR, regT1);

    // Profile the argument count.
    load32(Address(regT1, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset), regT2);
    loadConstant(callLinkInfoConstant, regT0);
    load32(Address(regT0, CallLinkInfo::offsetOfMaxArgumentCountIncludingThis()), regT3);
    Jump notBiggest = branch32(Above, regT3, regT2);
    store32(regT2, Address(regT0, CallLinkInfo::offsetOfMaxArgumentCountIncludingThis()));
    notBiggest.link(this);
    
    // Initialize 'this'.
    emitGetVirtualRegister(thisValue, regT0);
    store64(regT0, Address(regT1, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));

    addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), regT1, stackPointerRegister);
}

template<typename Op>
bool JIT::compileCallEval(const Op&)
{
    return false;
}

template<>
bool JIT::compileCallEval(const OpCallEval& bytecode)
{
    addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), stackPointerRegister, argumentGPR1);
    storePtr(callFrameRegister, Address(argumentGPR1, CallFrame::callerFrameOffset()));

    resetSP();

    move(TrustedImm32(bytecode.m_ecmaMode.value()), argumentGPR2);
    loadGlobalObject(argumentGPR0);
    callOperation(operationCallEval, argumentGPR0, argumentGPR1, argumentGPR2);
    addSlowCase(branchIfEmpty(returnValueGPR));

    emitPutCallResult(bytecode);

    return true;
}

void JIT::compileCallEvalSlowCase(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = instruction->as<OpCallEval>();
    CallLinkInfo* info = m_evalCallLinkInfos.add(CodeOrigin(m_bytecodeIndex));
    info->setUpCall(CallLinkInfo::Call, regT0);

    int registerOffset = -bytecode.m_argv;

    addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);

    load64(Address(stackPointerRegister, sizeof(Register) * CallFrameSlot::callee - sizeof(CallerFrameAndPC)), regT0);
    loadGlobalObject(regT3);
    emitVirtualCallWithoutMovingGlobalObject(*m_vm, info);
    resetSP();

    emitPutCallResult(bytecode);
}

template<typename Op>
bool JIT::compileTailCall(const Op&, UnlinkedCallLinkInfo*, unsigned, JITConstantPool::Constant)
{
    return false;
}

template<>
bool JIT::compileTailCall(const OpTailCall& bytecode, UnlinkedCallLinkInfo* info, unsigned callLinkInfoIndex, JITConstantPool::Constant callLinkInfoConstant)
{
    std::unique_ptr<CallFrameShuffleData> shuffleData = makeUnique<CallFrameShuffleData>();
    shuffleData->numPassedArgs = bytecode.m_argc;
    shuffleData->numParameters = m_unlinkedCodeBlock->numParameters();
    shuffleData->numberTagRegister = GPRInfo::numberTagRegister;
    shuffleData->numLocals =
        bytecode.m_argv - sizeof(CallerFrameAndPC) / sizeof(Register);
    shuffleData->args.resize(bytecode.m_argc);
    for (unsigned i = 0; i < bytecode.m_argc; ++i) {
        shuffleData->args[i] =
            ValueRecovery::displacedInJSStack(
                virtualRegisterForArgumentIncludingThis(i) - bytecode.m_argv,
                DataFormatJS);
    }
    shuffleData->callee = ValueRecovery::inGPR(regT0, DataFormatJS);
    shuffleData->setupCalleeSaveRegisters(&RegisterAtOffsetList::llintBaselineCalleeSaveRegisters());

    loadConstant(callLinkInfoConstant, regT2);
    JumpList slowPaths = CallLinkInfo::emitTailCallDataICFastPath(*this, regT0, regT2, [&] {
        CallFrameShuffler(*this, *shuffleData).prepareForTailCall();
    });
    addSlowCase(slowPaths);

    shuffleData->shrinkToFit();
    info->frameShuffleData = WTFMove(shuffleData);

    auto doneLocation = label();
    m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;
    
    return true;
}

template<typename Op>
void JIT::compileOpCall(const Instruction* instruction, unsigned callLinkInfoIndex)
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

    UnlinkedCallLinkInfo* info = nullptr;
    JITConstantPool::Constant infoConstant = UINT_MAX;
    if (opcodeID != op_call_eval) {
        info = m_unlinkedCalls.add();
        info->bytecodeIndex = m_bytecodeIndex;
        info->callType = CallLinkInfo::callTypeFor(opcodeID);

        infoConstant = addToConstantPool(JITConstantPool::Type::CallLinkInfo, info);

        ASSERT(m_callCompilationInfo.size() == callLinkInfoIndex);
        m_callCompilationInfo.append(CallCompilationInfo());
        m_callCompilationInfo[callLinkInfoIndex].unlinkedCallLinkInfo = info;
        m_callCompilationInfo[callLinkInfoIndex].callLinkInfoConstant = infoConstant;
    }
    compileSetupFrame(bytecode, infoConstant);

    // SP holds newCallFrame + sizeof(CallerFrameAndPC), with ArgumentCount initialized.
    uint32_t locationBits = CallSiteIndex(m_bytecodeIndex).bits();
    store32(TrustedImm32(locationBits), Address(callFrameRegister, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + TagOffset));

    emitGetVirtualRegister(callee, regT0); // regT0 holds callee.
    store64(regT0, Address(stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - sizeof(CallerFrameAndPC)));

    if (compileCallEval(bytecode))
        return;

    if (compileTailCall(bytecode, info, callLinkInfoIndex, infoConstant))
        return;

    loadConstant(infoConstant, regT2);
    if (opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments) {
        auto slowPaths = CallLinkInfo::emitTailCallDataICFastPath(*this, regT0, regT2, [&] {
            emitRestoreCalleeSaves();
            prepareForTailCallSlow(regT2);
        });
        addSlowCase(slowPaths);
        auto doneLocation = label();
        m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;
        return;
    }

    auto slowPaths = CallLinkInfo::emitDataICFastPath(*this, regT0, regT2);
    auto doneLocation = label();
    addSlowCase(slowPaths);

    m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;

    resetSP();

    emitPutCallResult(bytecode);
}

template<typename Op>
void JIT::compileOpCallSlowCase(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex)
{
    OpcodeID opcodeID = Op::opcodeID;
    ASSERT(opcodeID != op_call_eval);

    linkAllSlowCases(iter);

    loadGlobalObject(regT3);
    loadConstant(m_callCompilationInfo[callLinkInfoIndex].callLinkInfoConstant, regT2);

    if (opcodeID == op_tail_call || opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments)
        emitRestoreCalleeSaves();

    CallLinkInfo::emitDataICSlowPath(*m_vm, *this, regT2);

    if (opcodeID == op_tail_call || opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments) {
        abortWithReason(JITDidReturnFromTailCall);
        return;
    }

    resetSP();

    auto bytecode = instruction->as<Op>();
    emitPutCallResult(bytecode);
}

void JIT::emit_op_call(const Instruction* currentInstruction)
{
    compileOpCall<OpCall>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_tail_call(const Instruction* currentInstruction)
{
    compileOpCall<OpTailCall>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_call_eval(const Instruction* currentInstruction)
{
    compileOpCall<OpCallEval>(currentInstruction, m_callLinkInfoIndex);
}

void JIT::emit_op_call_varargs(const Instruction* currentInstruction)
{
    compileOpCall<OpCallVarargs>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_tail_call_varargs(const Instruction* currentInstruction)
{
    compileOpCall<OpTailCallVarargs>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_tail_call_forward_arguments(const Instruction* currentInstruction)
{
    compileOpCall<OpTailCallForwardArguments>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_construct_varargs(const Instruction* currentInstruction)
{
    compileOpCall<OpConstructVarargs>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_construct(const Instruction* currentInstruction)
{
    compileOpCall<OpConstruct>(currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_call(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpCall>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_tail_call(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpTailCall>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_call_eval(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileCallEvalSlowCase(currentInstruction, iter);
}
 
void JIT::emitSlow_op_call_varargs(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpCallVarargs>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_tail_call_varargs(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpTailCallVarargs>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emitSlow_op_tail_call_forward_arguments(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpTailCallForwardArguments>(currentInstruction, iter, m_callLinkInfoIndex++);
}
    
void JIT::emitSlow_op_construct_varargs(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpConstructVarargs>(currentInstruction, iter, m_callLinkInfoIndex++);
}
    
void JIT::emitSlow_op_construct(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase<OpConstruct>(currentInstruction, iter, m_callLinkInfoIndex++);
}

void JIT::emit_op_iterator_open(const Instruction* instruction)
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

    JITSlowPathCall slowPathCall(this, instruction, tryFastFunction);
    slowPathCall.call();
    Jump fastCase = branch32(NotEqual, GPRInfo::returnValueGPR2, TrustedImm32(static_cast<uint32_t>(IterationMode::Generic)));

    compileOpCall<OpIteratorOpen>(instruction, m_callLinkInfoIndex++);
    advanceToNextCheckpoint();
    // call result (iterator) is in regT0

    const Identifier* ident = &vm().propertyNames->next;
    
    constexpr GPRReg baseGPR = BaselineGetByIdRegisters::base;
    constexpr GPRReg resultGPR = BaselineGetByIdRegisters::result;
    constexpr GPRReg stubInfoGPR = BaselineGetByIdRegisters::stubInfo;

    move(regT0, baseGPR);
    emitJumpSlowCaseIfNotJSCell(baseGPR);

    JITGetByIdGenerator gen(
        nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromImmortalIdentifier(ident->impl()), JSValueRegs(baseGPR), JSValueRegs(resultGPR), stubInfoGPR, AccessType::GetById);

    UnlinkedStructureStubInfo* stubInfo = m_unlinkedStubInfos.add();
    stubInfo->accessType = AccessType::GetById;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    JITConstantPool::Constant stubInfoIndex = addToConstantPool(JITConstantPool::Type::StructureStubInfo, stubInfo);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode, resultGPR);
    emitPutVirtualRegister(bytecode.m_next, JSValueRegs(resultGPR));

    fastCase.link(this);
}

void JIT::emitSlow_op_iterator_open(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    compileOpCallSlowCase<OpIteratorOpen>(instruction, iter, m_callLinkInfoIndex++);
    emitJumpSlowToHotForCheckpoint(jump());

    linkAllSlowCases(iter);
    GPRReg iteratorGPR = BaselineGetByIdRegisters::base;
    JumpList notObject;
    notObject.append(branchIfNotCell(iteratorGPR));
    notObject.append(branchIfNotObject(iteratorGPR));

    auto bytecode = instruction->as<OpIteratorOpen>();
    VirtualRegister nextVReg = bytecode.m_next;
    UniquedStringImpl* ident = vm().propertyNames->next.impl();

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    
    Label coldPathBegin = label();

    move(iteratorGPR, argumentGPR2);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, argumentGPR1);
    loadGlobalObject(argumentGPR0);
    callOperationWithProfile<decltype(operationGetByIdOptimize)>(bytecode, Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), nextVReg, argumentGPR0, argumentGPR1, argumentGPR2, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
    gen.reportSlowPathCall(coldPathBegin, Call());

    auto done = jump();

    notObject.link(this);
    loadGlobalObject(argumentGPR0);
    callOperation(operationThrowIteratorResultIsNotObject, argumentGPR0);

    done.link(this);
}

void JIT::emit_op_iterator_next(const Instruction* instruction)
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

    emitGetVirtualRegister(bytecode.m_next, regT0);
    Jump genericCase = branchIfNotEmpty(regT0);

    JITSlowPathCall slowPathCall(this, instruction, tryFastFunction);
    slowPathCall.call();
    Jump fastCase = branch32(NotEqual, GPRInfo::returnValueGPR2, TrustedImm32(static_cast<uint32_t>(IterationMode::Generic)));

    genericCase.link(this);
    load8FromMetadata(bytecode, OpIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes(), regT0);
    or32(TrustedImm32(static_cast<uint8_t>(IterationMode::Generic)), regT0);
    store8ToMetadata(regT0, bytecode, OpIteratorNext::Metadata::offsetOfIterationMetadata() + IterationModeMetadata::offsetOfSeenModes());
    compileOpCall<OpIteratorNext>(instruction, m_callLinkInfoIndex++);
    advanceToNextCheckpoint();
    // call result ({ done, value } JSObject) in regT0

    constexpr GPRReg stubInfoGPR = BaselineGetByIdRegisters::stubInfo;
    constexpr GPRReg iterCallResultGPR = BaselineGetByIdRegisters::dontClobberRegister;
    move(returnValueGPR, iterCallResultGPR);

    constexpr GPRReg doneGPR = BaselineGetByIdRegisters::result;
    {
        constexpr GPRReg baseGPR = BaselineGetByIdRegisters::base;
        static_assert(returnValueGPR == baseGPR);

        emitJumpSlowCaseIfNotJSCell(baseGPR);

        RegisterSet preservedRegs = RegisterSet::stubUnavailableRegisters();
        preservedRegs.add(iterCallResultGPR);
        JITGetByIdGenerator gen(
            nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), preservedRegs,
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->done.impl()), JSValueRegs(baseGPR), JSValueRegs(doneGPR), stubInfoGPR, AccessType::GetById);

        UnlinkedStructureStubInfo* stubInfo = m_unlinkedStubInfos.add();
        stubInfo->accessType = AccessType::GetById;
        stubInfo->bytecodeIndex = m_bytecodeIndex;
        JITConstantPool::Constant stubInfoIndex = addToConstantPool(JITConstantPool::Type::StructureStubInfo, stubInfo);
        gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
        gen.m_unlinkedStubInfo = stubInfo;

        gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        emitValueProfilingSite(bytecode, JSValueRegs { doneGPR });
        emitPutVirtualRegister(bytecode.m_done, doneGPR);
        advanceToNextCheckpoint();
    }

    {
        RegisterSet usedRegisters(doneGPR, iterCallResultGPR);
        ScratchRegisterAllocator scratchAllocator(usedRegisters);
        GPRReg scratch1 = scratchAllocator.allocateScratchGPR();
        GPRReg scratch2 = scratchAllocator.allocateScratchGPR();
        GPRReg globalGPR = scratchAllocator.allocateScratchGPR();
        const bool shouldCheckMasqueradesAsUndefined = false;
        loadGlobalObject(globalGPR);
        JumpList iterationDone = branchIfTruthy(vm(), JSValueRegs(doneGPR), scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, globalGPR);

        constexpr GPRReg baseGPR = BaselineGetByIdRegisters::base;
        constexpr GPRReg resultGPR = BaselineGetByIdRegisters::result;
        move(iterCallResultGPR, baseGPR);

        JITGetByIdGenerator gen(
            nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->value.impl()), JSValueRegs(baseGPR), JSValueRegs(resultGPR), stubInfoGPR, AccessType::GetById);

        UnlinkedStructureStubInfo* stubInfo = m_unlinkedStubInfos.add();
        stubInfo->accessType = AccessType::GetById;
        stubInfo->bytecodeIndex = m_bytecodeIndex;
        JITConstantPool::Constant stubInfoIndex = addToConstantPool(JITConstantPool::Type::StructureStubInfo, stubInfo);
        gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
        gen.m_unlinkedStubInfo = stubInfo;

        gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP
        addSlowCase();
        m_getByIds.append(gen);

        emitValueProfilingSite(bytecode, JSValueRegs { resultGPR });
        emitPutVirtualRegister(bytecode.m_value, resultGPR);

        iterationDone.link(this);
    }

    fastCase.link(this);
}

void JIT::emitSlow_op_iterator_next(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    compileOpCallSlowCase<OpIteratorNext>(instruction, iter, m_callLinkInfoIndex++);
    emitJumpSlowToHotForCheckpoint(jump());

    constexpr GPRReg iterCallResultGPR = BaselineGetByIdRegisters::dontClobberRegister;

    auto bytecode = instruction->as<OpIteratorNext>();
    {
        VirtualRegister doneVReg = bytecode.m_done;

        linkAllSlowCases(iter);
        JumpList notObject;
        notObject.append(branchIfNotCell(iterCallResultGPR));

        UniquedStringImpl* ident = vm().propertyNames->done.impl();
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        
        Label coldPathBegin = label();

        notObject.append(branchIfNotObject(iterCallResultGPR));

        move(iterCallResultGPR, argumentGPR2);
        loadGlobalObject(argumentGPR0);
        loadConstant(gen.m_unlinkedStubInfoConstantIndex, argumentGPR1);
        callOperationWithProfile<decltype(operationGetByIdOptimize)>(bytecode, Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), doneVReg, argumentGPR0, argumentGPR1, argumentGPR2, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());

        gen.reportSlowPathCall(coldPathBegin, Call());

        constexpr GPRReg doneGPR = BaselineGetByIdRegisters::result;
        emitGetVirtualRegister(doneVReg, doneGPR);
        emitGetVirtualRegister(bytecode.m_value, iterCallResultGPR);
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

        move(iterCallResultGPR, argumentGPR2);
        loadGlobalObject(argumentGPR0);
        loadConstant(gen.m_unlinkedStubInfoConstantIndex, argumentGPR1);
        callOperationWithProfile<decltype(operationGetByIdOptimize)>(bytecode, Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), valueVReg, argumentGPR0, argumentGPR1, argumentGPR2, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
        gen.reportSlowPathCall(coldPathBegin, Call());
    }
}

} // namespace JSC

#endif // USE(JSVALUE64)
#endif // ENABLE(JIT)
