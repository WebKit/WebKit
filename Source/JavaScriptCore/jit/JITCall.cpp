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
#include "SetupVarargsFrame.h"
#include "SlowPathCall.h"
#include "StackAlignment.h"
#include "ThunkGenerators.h"

namespace JSC {

template<typename Op>
void JIT::emitPutCallResult(const Op& bytecode)
{
    emitValueProfilingSite(bytecode.metadata(m_codeBlock), regT0);
    emitPutVirtualRegister(destinationFor(bytecode, m_bytecodeIndex.checkpoint()).virtualRegister(), regT0);
}

template<typename Op>
std::enable_if_t<
    Op::opcodeID != op_call_varargs && Op::opcodeID != op_construct_varargs
    && Op::opcodeID != op_tail_call_varargs && Op::opcodeID != op_tail_call_forward_arguments
, void>
JIT::compileSetupFrame(const Op& bytecode, CallLinkInfo*)
{
    unsigned checkpoint = m_bytecodeIndex.checkpoint();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int argCountIncludingThis = argumentCountIncludingThisFor(bytecode, checkpoint);
    int registerOffset = -static_cast<int>(stackOffsetInRegistersForCall(bytecode, checkpoint));

    if (Op::opcodeID == op_call && shouldEmitProfiling()) {
        emitGetVirtualRegister(VirtualRegister(registerOffset + CallFrame::argumentOffsetIncludingThis(0)), regT0);
        Jump done = branchIfNotCell(regT0);
        load32(Address(regT0, JSCell::structureIDOffset()), regT0);
        store32(regT0, arrayProfileFor(metadata, checkpoint).addressOfLastSeenStructureID());
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
JIT::compileSetupFrame(const Op& bytecode, CallLinkInfo* info)
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
    callOperation(sizeOperation, TrustedImmPtr(m_codeBlock->globalObject()), regT1, -firstFreeRegister, firstVarArgOffset);
    move(TrustedImm32(-firstFreeRegister), regT1);
    emitSetVarargsFrame(*this, returnValueGPR, false, regT1, regT1);
    addPtr(TrustedImm32(-(sizeof(CallerFrameAndPC) + WTF::roundUpToMultipleOf(stackAlignmentBytes(), 5 * sizeof(void*)))), regT1, stackPointerRegister);
    emitGetVirtualRegister(arguments, regT2);
    F_JITOperation_GFJZZ setupOperation;
    if (Op::opcodeID == op_tail_call_forward_arguments)
        setupOperation = operationSetupForwardArgumentsFrame;
    else
        setupOperation = operationSetupVarargsFrame;
    callOperation(setupOperation, TrustedImmPtr(m_codeBlock->globalObject()), regT1, regT2, firstVarArgOffset, regT0);
    move(returnValueGPR, regT1);

    // Profile the argument count.
    load32(Address(regT1, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset), regT2);
    load32(info->addressOfMaxArgumentCountIncludingThis(), regT0);
    Jump notBiggest = branch32(Above, regT0, regT2);
    store32(regT2, info->addressOfMaxArgumentCountIncludingThis());
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
    addPtr(TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), stackPointerRegister, regT1);
    storePtr(callFrameRegister, Address(regT1, CallFrame::callerFrameOffset()));

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    move(TrustedImm32(bytecode.m_ecmaMode.value()), regT2);
    callOperation(operationCallEval, m_codeBlock->globalObject(), regT1, regT2);

    addSlowCase(branchIfEmpty(regT0));

    emitPutCallResult(bytecode);

    return true;
}

void JIT::compileCallEvalSlowCase(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = instruction->as<OpCallEval>();
    CallLinkInfo* info = m_codeBlock->addCallLinkInfo(CodeOrigin(m_bytecodeIndex));
    info->setUpCall(CallLinkInfo::Call, regT0);

    int registerOffset = -bytecode.m_argv;

    addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);

    load64(Address(stackPointerRegister, sizeof(Register) * CallFrameSlot::callee - sizeof(CallerFrameAndPC)), regT0);
    emitVirtualCall(*m_vm, m_codeBlock->globalObject(), info);
    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    emitPutCallResult(bytecode);
}

template<typename Op>
bool JIT::compileTailCall(const Op&, CallLinkInfo*, unsigned)
{
    return false;
}

template<>
bool JIT::compileTailCall(const OpTailCall& bytecode, CallLinkInfo* info, unsigned callLinkInfoIndex)
{
    CallFrameShuffleData shuffleData;
    shuffleData.numPassedArgs = bytecode.m_argc;
    shuffleData.numberTagRegister = GPRInfo::numberTagRegister;
    shuffleData.numLocals =
        bytecode.m_argv - sizeof(CallerFrameAndPC) / sizeof(Register);
    shuffleData.args.resize(bytecode.m_argc);
    for (unsigned i = 0; i < bytecode.m_argc; ++i) {
        shuffleData.args[i] =
            ValueRecovery::displacedInJSStack(
                virtualRegisterForArgumentIncludingThis(i) - bytecode.m_argv,
                DataFormatJS);
    }
    shuffleData.callee =
        ValueRecovery::inGPR(regT0, DataFormatJS);
    shuffleData.setupCalleeSaveRegisters(m_codeBlock);
    info->setFrameShuffleData(shuffleData);

    JumpList slowPaths = info->emitTailCallFastPath(*this, regT0, regT2, CallLinkInfo::UseDataIC::Yes, [&] {
        CallFrameShuffler(*this, shuffleData).prepareForTailCall();
    });
    addSlowCase(slowPaths);
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
    CallLinkInfo* info = nullptr;
    if (opcodeID != op_call_eval)
        info = m_codeBlock->addCallLinkInfo(CodeOrigin(m_bytecodeIndex));
    compileSetupFrame(bytecode, info);

    // SP holds newCallFrame + sizeof(CallerFrameAndPC), with ArgumentCount initialized.
    auto bytecodeIndex = m_codeBlock->bytecodeIndex(instruction);
    uint32_t locationBits = CallSiteIndex(bytecodeIndex).bits();
    store32(TrustedImm32(locationBits), Address(callFrameRegister, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + TagOffset));

    emitGetVirtualRegister(callee, regT0); // regT0 holds callee.
    store64(regT0, Address(stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - sizeof(CallerFrameAndPC)));

    if (compileCallEval(bytecode))
        return;

    ASSERT(m_callCompilationInfo.size() == callLinkInfoIndex);
    info->setUpCall(CallLinkInfo::callTypeFor(opcodeID), regT0);
    m_callCompilationInfo.append(CallCompilationInfo());
    m_callCompilationInfo[callLinkInfoIndex].callLinkInfo = info;

    if (compileTailCall(bytecode, info, callLinkInfoIndex))
        return;

    if (opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments) {
        auto slowPaths = info->emitTailCallFastPath(*this, regT0, regT2, CallLinkInfo::UseDataIC::Yes, [&] {
            emitRestoreCalleeSaves();
            prepareForTailCallSlow(regT2);
        });
        addSlowCase(slowPaths);
        auto doneLocation = label();
        m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;
        return;
    }

    auto slowPaths = info->emitFastPath(*this, regT0, regT2, CallLinkInfo::UseDataIC::Yes);
    auto doneLocation = label();
    addSlowCase(slowPaths);

    m_callCompilationInfo[callLinkInfoIndex].doneLocation = doneLocation;

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    emitPutCallResult(bytecode);
}

template<typename Op>
void JIT::compileOpCallSlowCase(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex)
{
    OpcodeID opcodeID = Op::opcodeID;
    ASSERT(opcodeID != op_call_eval);

    linkAllSlowCases(iter);

    m_callCompilationInfo[callLinkInfoIndex].slowPathStart = label();

    if (opcodeID == op_tail_call || opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments)
        emitRestoreCalleeSaves();

    move(TrustedImmPtr(m_codeBlock->globalObject()), regT3);
    m_callCompilationInfo[callLinkInfoIndex].callLinkInfo->emitSlowPath(*m_vm, *this);

    if (opcodeID == op_tail_call || opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments) {
        abortWithReason(JITDidReturnFromTailCall);
        return;
    }

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

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
    
    emitJumpSlowCaseIfNotJSCell(regT0);

    JITGetByIdGenerator gen(
        m_codeBlock, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromImmortalIdentifier(ident->impl()), JSValueRegs(regT0), JSValueRegs(regT0), regT1, AccessType::GetById);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock), regT0);
    emitPutVirtualRegister(bytecode.m_next);

    fastCase.link(this);
}

void JIT::emitSlow_op_iterator_open(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    compileOpCallSlowCase<OpIteratorOpen>(instruction, iter, m_callLinkInfoIndex++);
    emitJumpSlowToHotForCheckpoint(jump());


    linkAllSlowCases(iter);

    GPRReg iteratorGPR = regT0;
    JumpList notObject;
    notObject.append(branchIfNotCell(iteratorGPR));
    notObject.append(branchIfNotObject(iteratorGPR));

    auto bytecode = instruction->as<OpIteratorOpen>();
    VirtualRegister nextVReg = bytecode.m_next;
    UniquedStringImpl* ident = vm().propertyNames->next.impl();

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    
    Label coldPathBegin = label();

    Call call;
    if (JITCode::useDataIC(JITType::BaselineJIT)) {
        gen.stubInfo()->m_slowOperation = operationGetByIdOptimize;
        move(TrustedImmPtr(gen.stubInfo()), GPRInfo::nonArgGPR0);
        callOperationWithProfile<decltype(operationGetByIdOptimize)>(bytecode.metadata(m_codeBlock), Address(GPRInfo::nonArgGPR0, StructureStubInfo::offsetOfSlowOperation()), nextVReg, TrustedImmPtr(m_codeBlock->globalObject()), GPRInfo::nonArgGPR0, iteratorGPR, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
    } else
        call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, nextVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), iteratorGPR, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
    gen.reportSlowPathCall(coldPathBegin, call);

    auto done = jump();

    notObject.link(this);
    callOperation(operationThrowIteratorResultIsNotObject, TrustedImmPtr(m_codeBlock->globalObject()));

    done.link(this);
}

void JIT::emit_op_iterator_next(const Instruction* instruction)
{
    auto bytecode = instruction->as<OpIteratorNext>();
    auto& metadata = bytecode.metadata(m_codeBlock);
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
    or8(TrustedImm32(static_cast<uint8_t>(IterationMode::Generic)), AbsoluteAddress(&metadata.m_iterationMetadata.seenModes));
    compileOpCall<OpIteratorNext>(instruction, m_callLinkInfoIndex++);
    advanceToNextCheckpoint();
    // call result ({ done, value } JSObject) in regT0

    GPRReg valueGPR = regT0;
    GPRReg iterResultGPR = regT2;
    GPRReg doneGPR = regT1;
    // iterResultGPR will get trashed by the first get by id below.
    move(valueGPR, iterResultGPR);

    {
        emitJumpSlowCaseIfNotJSCell(iterResultGPR);

        RegisterSet preservedRegs = RegisterSet::stubUnavailableRegisters();
        preservedRegs.add(valueGPR);
        JITGetByIdGenerator gen(
            m_codeBlock, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), preservedRegs,
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->done.impl()), JSValueRegs(iterResultGPR), JSValueRegs(doneGPR), regT3, AccessType::GetById);
        gen.generateFastPath(*this);
        addSlowCase(gen.slowPathJump());
        m_getByIds.append(gen);

        emitValueProfilingSite(metadata, JSValueRegs { doneGPR });
        emitPutVirtualRegister(bytecode.m_done, doneGPR);
        advanceToNextCheckpoint();
    }


    {
        GPRReg scratch1 = regT2;
        GPRReg scratch2 = regT3;
        const bool shouldCheckMasqueradesAsUndefined = false;
        JumpList iterationDone = branchIfTruthy(vm(), JSValueRegs(doneGPR), scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, m_codeBlock->globalObject());

        JITGetByIdGenerator gen(
            m_codeBlock, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(BytecodeIndex(m_bytecodeIndex.offset())), RegisterSet::stubUnavailableRegisters(),
            CacheableIdentifier::createFromImmortalIdentifier(vm().propertyNames->value.impl()), JSValueRegs(valueGPR), JSValueRegs(valueGPR), regT4, AccessType::GetById);
        gen.generateFastPath(*this);
        addSlowCase(gen.slowPathJump());
        m_getByIds.append(gen);

        emitValueProfilingSite(metadata, JSValueRegs { valueGPR });
        emitPutVirtualRegister(bytecode.m_value, valueGPR);

        iterationDone.link(this);
    }

    fastCase.link(this);
}

void JIT::emitSlow_op_iterator_next(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    compileOpCallSlowCase<OpIteratorNext>(instruction, iter, m_callLinkInfoIndex++);
    emitJumpSlowToHotForCheckpoint(jump());

    auto bytecode = instruction->as<OpIteratorNext>();
    {
        VirtualRegister doneVReg = bytecode.m_done;
        GPRReg iterResultGPR = regT2;

        linkAllSlowCases(iter);
        JumpList notObject;
        notObject.append(branchIfNotCell(iterResultGPR));

        UniquedStringImpl* ident = vm().propertyNames->done.impl();
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
        
        Label coldPathBegin = label();

        notObject.append(branchIfNotObject(iterResultGPR));

        Call call;
        if (JITCode::useDataIC(JITType::BaselineJIT)) {
            gen.stubInfo()->m_slowOperation = operationGetByIdOptimize;
            move(TrustedImmPtr(gen.stubInfo()), GPRInfo::nonArgGPR0);
            callOperationWithProfile<decltype(operationGetByIdOptimize)>(bytecode.metadata(m_codeBlock), Address(GPRInfo::nonArgGPR0, StructureStubInfo::offsetOfSlowOperation()), doneVReg, TrustedImmPtr(m_codeBlock->globalObject()), GPRInfo::nonArgGPR0, iterResultGPR, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
        } else
            call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, doneVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), iterResultGPR, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
        gen.reportSlowPathCall(coldPathBegin, call);

        emitGetVirtualRegister(doneVReg, regT1);
        emitGetVirtualRegister(bytecode.m_value, regT0);
        emitJumpSlowToHotForCheckpoint(jump());

        notObject.link(this);
        callOperation(operationThrowIteratorResultIsNotObject, TrustedImmPtr(m_codeBlock->globalObject()));
    }

    {   
        linkAllSlowCases(iter);
        VirtualRegister valueVReg = bytecode.m_value;
        GPRReg iterResultGPR = regT0;

        UniquedStringImpl* ident = vm().propertyNames->value.impl();
        JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

        Label coldPathBegin = label();

        Call call;
        if (JITCode::useDataIC(JITType::BaselineJIT)) {
            gen.stubInfo()->m_slowOperation = operationGetByIdOptimize;
            move(TrustedImmPtr(gen.stubInfo()), GPRInfo::nonArgGPR0);
            callOperationWithProfile<decltype(operationGetByIdOptimize)>(bytecode.metadata(m_codeBlock), Address(GPRInfo::nonArgGPR0, StructureStubInfo::offsetOfSlowOperation()), valueVReg, TrustedImmPtr(m_codeBlock->globalObject()), GPRInfo::nonArgGPR0, iterResultGPR, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
        } else
            call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, valueVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), iterResultGPR, CacheableIdentifier::createFromImmortalIdentifier(ident).rawBits());
        gen.reportSlowPathCall(coldPathBegin, call);
    }

}

} // namespace JSC

#endif // USE(JSVALUE64)
#endif // ENABLE(JIT)
