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

#include "CallFrameShuffler.h"
#include "CodeBlock.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "OpcodeInlines.h"
#include "ResultType.h"
#include "SetupVarargsFrame.h"
#include "StackAlignment.h"
#include "ThunkGenerators.h"
#include <wtf/StringPrintStream.h>


namespace JSC {

template<typename Op>
void JIT::emitPutCallResult(const Op& bytecode)
{
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(bytecode.m_dst);
}

template<typename Op>
std::enable_if_t<
    Op::opcodeID != op_call_varargs && Op::opcodeID != op_construct_varargs
    && Op::opcodeID != op_tail_call_varargs && Op::opcodeID != op_tail_call_forward_arguments
, void>
JIT::compileSetupFrame(const Op& bytecode, CallLinkInfo*)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    int argCount = bytecode.m_argc;
    int registerOffset = -static_cast<int>(bytecode.m_argv);

    if (Op::opcodeID == op_call && shouldEmitProfiling()) {
        emitGetVirtualRegister(VirtualRegister(registerOffset + CallFrame::argumentOffsetIncludingThis(0)), regT0);
        Jump done = branchIfNotCell(regT0);
        load32(Address(regT0, JSCell::structureIDOffset()), regT0);
        store32(regT0, metadata.m_callLinkInfo.m_arrayProfile.addressOfLastSeenStructureID());
        done.link(this);
    }

    addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);
    store32(TrustedImm32(argCount), Address(stackPointerRegister, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + PayloadOffset - sizeof(CallerFrameAndPC)));
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

    callOperation(operationCallEval, m_codeBlock->globalObject(), regT1);

    addSlowCase(branchIfEmpty(regT0));

    sampleCodeBlock(m_codeBlock);
    
    emitPutCallResult(bytecode);

    return true;
}

void JIT::compileCallEvalSlowCase(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = instruction->as<OpCallEval>();
    CallLinkInfo* info = m_codeBlock->addCallLinkInfo();
    info->setUpCall(CallLinkInfo::Call, CodeOrigin(m_bytecodeIndex), regT0);

    int registerOffset = -bytecode.m_argv;

    addPtr(TrustedImm32(registerOffset * sizeof(Register) + sizeof(CallerFrameAndPC)), callFrameRegister, stackPointerRegister);

    load64(Address(stackPointerRegister, sizeof(Register) * CallFrameSlot::callee - sizeof(CallerFrameAndPC)), regT0);
    emitDumbVirtualCall(vm(), m_codeBlock->globalObject(), info);
    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    sampleCodeBlock(m_codeBlock);
    
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
    CallFrameShuffler(*this, shuffleData).prepareForTailCall();
    m_callCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedTailCall();
    return true;
}

template<typename Op>
void JIT::compileOpCall(const Instruction* instruction, unsigned callLinkInfoIndex)
{
    OpcodeID opcodeID = Op::opcodeID;
    auto bytecode = instruction->as<Op>();
    VirtualRegister callee = bytecode.m_callee;

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
        info = m_codeBlock->addCallLinkInfo();
    compileSetupFrame(bytecode, info);

    // SP holds newCallFrame + sizeof(CallerFrameAndPC), with ArgumentCount initialized.
    auto bytecodeIndex = m_codeBlock->bytecodeIndex(instruction);
    uint32_t locationBits = CallSiteIndex(bytecodeIndex).bits();
    store32(TrustedImm32(locationBits), Address(callFrameRegister, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + TagOffset));

    emitGetVirtualRegister(callee, regT0); // regT0 holds callee.
    store64(regT0, Address(stackPointerRegister, CallFrameSlot::callee * static_cast<int>(sizeof(Register)) - sizeof(CallerFrameAndPC)));

    if (compileCallEval(bytecode)) {
        return;
    }

    DataLabelPtr addressOfLinkedFunctionCheck;
    Jump slowCase = branchPtrWithPatch(NotEqual, regT0, addressOfLinkedFunctionCheck, TrustedImmPtr(nullptr));
    addSlowCase(slowCase);

    ASSERT(m_callCompilationInfo.size() == callLinkInfoIndex);
    info->setUpCall(CallLinkInfo::callTypeFor(opcodeID), CodeOrigin(m_bytecodeIndex), regT0);
    m_callCompilationInfo.append(CallCompilationInfo());
    m_callCompilationInfo[callLinkInfoIndex].hotPathBegin = addressOfLinkedFunctionCheck;
    m_callCompilationInfo[callLinkInfoIndex].callLinkInfo = info;

    if (compileTailCall(bytecode, info, callLinkInfoIndex)) {
        return;
    }

    if (opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments) {
        emitRestoreCalleeSaves();
        prepareForTailCallSlow();
        m_callCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedTailCall();
        return;
    }

    m_callCompilationInfo[callLinkInfoIndex].hotPathOther = emitNakedCall();

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    sampleCodeBlock(m_codeBlock);
    
    emitPutCallResult(bytecode);
}

template<typename Op>
void JIT::compileOpCallSlowCase(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter, unsigned callLinkInfoIndex)
{
    OpcodeID opcodeID = Op::opcodeID;
    ASSERT(opcodeID != op_call_eval);

    linkAllSlowCases(iter);

    if (opcodeID == op_tail_call || opcodeID == op_tail_call_varargs || opcodeID == op_tail_call_forward_arguments)
        emitRestoreCalleeSaves();

    move(TrustedImmPtr(m_codeBlock->globalObject()), regT3);
    move(TrustedImmPtr(m_callCompilationInfo[callLinkInfoIndex].callLinkInfo), regT2);

    m_callCompilationInfo[callLinkInfoIndex].callReturnLocation =
        emitNakedCall(m_vm->getCTIStub(linkCallThunkGenerator).retaggedCode<NoPtrTag>());

    if (opcodeID == op_tail_call || opcodeID == op_tail_call_varargs) {
        abortWithReason(JITDidReturnFromTailCall);
        return;
    }

    addPtr(TrustedImm32(stackPointerOffsetFor(m_codeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);
    checkStackPointerAlignment();

    sampleCodeBlock(m_codeBlock);
    
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

} // namespace JSC

#endif // USE(JSVALUE64)
#endif // ENABLE(JIT)
